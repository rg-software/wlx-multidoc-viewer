#ifndef SIDEBAR_H
#define SIDEBAR_H

#include "document.h"
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
    QString title;
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

    virtual void clearEntries() = 0;
    virtual void addEntry(int id, int parentId, const QString& title) = 0;
    virtual void selectEntry(int id) = 0; // highlight + auto-expand the path
    virtual void setVisible(bool on) = 0;

private:
    SidebarPresenter* m_presenter = nullptr;
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
        m_entries.clear();
        m_activeEntry = -1;
        if (m_backend)
            m_backend->clearEntries();
        if (!m_controller || !m_controller->hasDocument())
            return;
        const QVector<OutlineItem> items = m_controller->engine()->outline();
        flatten(items, -1, 0);
        if (m_backend) {
            for (const SidebarEntry& e : m_entries)
                m_backend->addEntry(e.id, e.parentId, e.title);
        }
        onPageChanged(m_controller->currentPage());
    }

    void onPageChanged(int page) {
        // Deepest entry whose page is at or before the reading position.
        int best = -1;
        int bestLevel = -1;
        for (int i = 0; i < m_entries.size(); ++i) {
            const SidebarEntry& e = m_entries[i];
            if (e.pageNo <= page && e.level >= bestLevel) {
                bestLevel = e.level;
                best = i;
            }
        }
        m_activeEntry = best;
        if (m_backend && best >= 0)
            m_backend->selectEntry(best);
    }

    void onEntryActivated(int id) {
        if (!m_controller || !m_controller->hasDocument())
            return;
        const SidebarEntry* e = entry(id);
        if (!e)
            return;
        m_controller->goToPage(e->pageNo);
        if (!m_controller->isPagedMode() && m_applyScroll)
            m_applyScroll(m_controller->scrollOffsetForPage(e->pageNo));
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
    bool hasOutline() const { return !m_entries.isEmpty(); }

private:
    void flatten(const QVector<OutlineItem>& items, int parentId, int level) {
        for (const OutlineItem& it : items) {
            const int id = m_entries.size();
            m_entries.append(SidebarEntry{id, parentId, level, it.pageNo, it.title});
            flatten(it.children, id, level + 1);
        }
    }

    ViewerController* m_controller = nullptr;
    SidebarBackend* m_backend = nullptr;
    std::function<void(int)> m_applyScroll;
    QVector<SidebarEntry> m_entries;
    int m_activeEntry = -1;
};

#endif // SIDEBAR_H