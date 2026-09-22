#ifndef SIDEBAR_H
#define SIDEBAR_H

#include "document.h"
#include "favorites.h"
#include "ui_strings.h"
#include "viewercontroller.h"

#include <QString>
#include <QVector>
#include <functional>

// Toggleable outline sidebar shared core (Task 3.4). The presenter flattens the
// engine's OutlineItem tree into pre-order entries (id == flat index), tracks
// the active entry from the reading position, and hands activation events back
// to the controller. The native backends (WC_TREEVIEW / QTreeWidget) render the
// entries; because trees can be thousands of nodes deep, the backends populate
// lazily per expanded node via childrenOf().

struct SidebarEntry {
    int id = -1;        // flat pre-order index; stable while a document is open
    int parentId = -1;  // -1 for top-level
    int level = 0;
    int pageNo = 1;
    bool resolved = true;  // false: container/dangling link, activation falls back to page 1
    QString title;
    // Normalized (0..1) vertical position of the destination inside pageNo's
    // page; 0 = page top. A CHM TOC entry targeting a fragment can land
    // mid-page after the topic is paginated into A5 pages.
    float anchorY = 0.0f;
};

class SidebarPresenter;

// Abstract native sidebar panel. addEntry is called in pre-order so top-level
// entries appear before their descendants; backends that populate lazily can
// skip inserting descendants and instead insert them from childrenOf() when a
// node is expanded.
class SidebarBackend {
public:
    virtual ~SidebarBackend() = default;

    void setPresenter(SidebarPresenter* p) { m_presenter = p; }
    SidebarPresenter* presenter() const { return m_presenter; }

    // Called by the backend with the candidate sidebar width in logical px while
    // the user drags the resize grip; the viewer clamps and re-runs its chrome
    // chain (mirrors SidebarPresenter::setScrollApplier).
    void setWidthChangedHandler(std::function<void(int logicalPx)> fn) {
        m_widthChanged = std::move(fn);
    }

    virtual void clearEntries() = 0;
    virtual void addEntry(int id, int parentId, const QString& title) = 0;
    virtual void selectEntry(int id) = 0; // highlight + auto-expand the path
    virtual void setVisible(bool on) = 0;

    // Collapse-state preservation across a reload() rebuild: capture which
    // entry ids are currently expanded before clearEntries(), re-apply them
    // after the new entries are added. Ids that are absent keep the backend's
    // default (collapsed), so a user-collapsed Favorites section stays closed.
    virtual QVector<int> expandedEntryIds() const { return {}; }
    virtual void restoreExpandedEntries(const QVector<int>&) {}

protected:
    void notifyWidthChanged(int logicalPx) {
        if (m_widthChanged)
            m_widthChanged(logicalPx);
    }

private:
    SidebarPresenter* m_presenter = nullptr;
    std::function<void(int)> m_widthChanged;
};

class SidebarPresenter {
public:
    void attach(ViewerController* controller, SidebarBackend* backend) {
        m_controller = controller;
        m_backend = backend;
        if (backend)
            backend->setPresenter(this);
    }

    // Applier for scrolled navigation (continuous mode), mirroring the toolbar.
    void setScrollApplier(std::function<void(int)> fn) { m_applyScroll = std::move(fn); }

    void reload() {
        // Capture the expanded set first so a reload triggered by (e.g.) a
        // favorites toggle keeps the Favorites section and any expanded outline
        // branches open. Ids are only meaningful within one document, so the
        // capture is discarded when the open document changed.
        const QString docPath = m_controller ? m_controller->currentPath() : QString();
        QVector<int> expanded;
        if (m_backend && !docPath.isEmpty() && docPath == m_expandDocPath)
            expanded = m_backend->expandedEntryIds();
        m_entries.clear();
        m_activeEntry = -1;
        m_favoritesHeaderId = -1;
        m_hasOutlineEntries = false;
        if (m_backend)
            m_backend->clearEntries();
        if (!m_controller || !m_controller->hasDocument()) {
            m_expandDocPath.clear();
            return;
        }
        const QVector<OutlineItem> items = m_controller->engine()->outline();
        flatten(items, -1, 0);
        m_hasOutlineEntries = !m_entries.isEmpty();
        // Favorites: a synthetic collapsible section below the outline when the
        // current document has any marked pages. The section header is a
        // destination-less container (resolved=false) so it can never be
        // highlighted or activated; the rows are real resolved entries.
        const QString path = m_controller->currentPath();
        const QVector<FavoriteEntry> favs =
            path.isEmpty() ? QVector<FavoriteEntry>()
                           : FavoritesStore::get().favoritesFor(path);
        if (!favs.isEmpty()) {
            m_favoritesHeaderId = m_entries.size();
            m_entries.append(SidebarEntry{m_favoritesHeaderId, -1, 0, 1, false,
                                          ui_strings::sidebarFavoritesHeader()});
            for (const FavoriteEntry& f : favs) {
                const int id = m_entries.size();
                const QString title = f.label.isEmpty()
                    ? ui_strings::sidebarFavoritePageFallback().arg(f.page)
                    : f.label;
                m_entries.append(SidebarEntry{id, m_favoritesHeaderId, 1, f.page,
                                              true, title});
            }
        }
        if (m_backend) {
            for (const SidebarEntry& e : m_entries)
                m_backend->addEntry(e.id, e.parentId, e.title);
            if (!expanded.isEmpty())
                m_backend->restoreExpandedEntries(expanded);
        }
        m_expandDocPath = path;
        onPageChanged(m_controller->currentPage());
    }

    void onPageChanged(int page) {
        // Deepest resolved entry whose page is at or before the reading
        // position. Destination-less entries (pure TOC containers, dangling
        // links) are excluded: they would otherwise masquerade as page-1
        // candidates and steal the highlight from the actual section.
        int best = -1;
        int bestLevel = -1;
        // The scan is limited to real outline rows; the favorites section (when
        // present) sits past m_favoritesHeaderId and has its own exact-match rule.
        const int outlineEnd = (m_favoritesHeaderId < 0) ? m_entries.size()
                                                         : m_favoritesHeaderId;
        for (int i = 0; i < outlineEnd; ++i) {
            const SidebarEntry& e = m_entries[i];
            if (e.resolved && e.pageNo <= page && e.level >= bestLevel) {
                bestLevel = e.level;
                best = i;
            }
        }
        // A favorite row whose page equals the reading position overrides the
        // outline highlight: pages are unique per favorite, so at most one match.
        if (m_favoritesHeaderId >= 0) {
            for (int i = m_favoritesHeaderId + 1; i < m_entries.size(); ++i) {
                const SidebarEntry& e = m_entries[i];
                if (e.resolved && e.pageNo == page) {
                    best = i;
                    break;
                }
            }
        }
        m_activeEntry = best;
        if (m_backend && best >= 0)
            m_backend->selectEntry(best);
    }

    void onEntryActivated(int id) {
        if (!m_controller || !m_controller->hasDocument())
            return;
        // The favorites header is a destination-less container.
        if (id == m_favoritesHeaderId)
            return;
        const SidebarEntry* e = entry(id);
        if (!e)
            return;
        m_controller->goToPage(e->pageNo);
        // Entries with a fragment anchor land on the heading (works in paged
        // mode as an in-page offset, in continuous mode as an absolute canvas
        // offset); plain entries keep the old page-top behavior.
        if (e->anchorY > 0.0f && m_applyScroll) {
            m_applyScroll(m_controller->anchorScrollOffset(e->pageNo, e->anchorY));
        } else if (!m_controller->isPagedMode() && m_applyScroll) {
            m_applyScroll(m_controller->scrollOffsetForPage(e->pageNo));
        }
    }

    const SidebarEntry* entry(int id) const {
        return (id >= 0 && id < m_entries.size()) ? &m_entries[id] : nullptr;
    }

    QVector<int> childrenOf(int id) const {
        QVector<int> out;
        for (int i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].parentId == id)
                out.append(i);
        }
        return out;
    }

    int activeEntry() const { return m_activeEntry; }
    // Sidebar availability: the panel has content if the document has an
    // outline AND/OR favorites.
    bool hasOutline() const { return !m_entries.isEmpty(); }
    bool hasSidebarContent() const { return !m_entries.isEmpty(); }
    // The document's own outline rows (excludes the synthetic Favorites
    // section) and whether that section was appended.
    bool hasOutlineEntries() const { return m_hasOutlineEntries; }
    bool hasFavoritesSection() const { return m_favoritesHeaderId >= 0; }

private:
    void flatten(const QVector<OutlineItem>& items, int parentId, int level) {
        for (const OutlineItem& it : items) {
            const int id = m_entries.size();
            m_entries.append(SidebarEntry{id, parentId, level, it.pageNo,
                                          it.resolved, it.title, it.anchorY});
            flatten(it.children, id, level + 1);
        }
    }

    ViewerController* m_controller = nullptr;
    SidebarBackend* m_backend = nullptr;
    std::function<void(int)> m_applyScroll;
    QVector<SidebarEntry> m_entries;
    int m_activeEntry = -1;
    // Flat id of the synthetic "Favorites" section header, or -1 when the
    // current document has no favorites (no section appended).
    int m_favoritesHeaderId = -1;
    // Whether the flattened document outline contributed any rows (before the
    // synthetic Favorites section is appended).
    bool m_hasOutlineEntries = false;
    // Document whose expanded-entry ids the backend currently holds; a reload
    // only restores the captured collapse state while this still matches.
    QString m_expandDocPath;
};

#endif // SIDEBAR_H