#ifndef SIDEBAR_WIN32_H
#define SIDEBAR_WIN32_H

#include "sidebar.h"
#include "viewer_settings.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

// Win32 outline sidebar backend: a child panel hosting a WC_TREEVIEW with lazy
// per-level population (design D10). Only the root level is inserted up front;
// a placeholder child renders the expand (+) affordance, and real descendants
// are inserted when the node expands or when the active entry needs its path.
// A regular document outline can be thousands of nodes, so unexpanded branches
// never create native items.
class SidebarWin32 : public SidebarBackend {
public:
    explicit SidebarWin32(HWND hParent);
    ~SidebarWin32() override;

    HWND hwnd() const { return m_hwnd; }
    void setDpiScale(float scale);
    int widthPx() const;

    // --- SidebarBackend ---
    void clearEntries() override;
    void addEntry(int id, int parentId, const QString& title) override;
    void selectEntry(int id) override;
    void setVisible(bool on) override;

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    static LRESULT CALLBACK treeProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handleMsg(UINT msg, WPARAM wp, LPARAM lp);
    void forwardEscape();

    HTREEITEM insertItem(HTREEITEM parent, int id, const QString& title, bool placeholder);
    void ensureMaterialized(int id);                 // inserts + expands the path to id
    void materializeChildren(int parentId, HTREEITEM parentItem);
    int itemId(HTREEITEM h) const;
    bool hasChildren(int id) const;

    HWND m_hwnd = nullptr;
    HWND m_tree = nullptr;
    HWND m_parent = nullptr;
    float m_dpiScale = 1.0f;

    QHash<int, HTREEITEM> m_items;      // materialized entry id -> native item
    QSet<int> m_materialized;           // ids whose real children are inserted
    bool m_internalMutation = false;
    int m_lastSelected = -1;            // last tree entry highlighted (redraw guard)
};

#endif // Q_OS_WIN
#endif // SIDEBAR_WIN32_H