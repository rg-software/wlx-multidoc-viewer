#include "sidebar_win32.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <commctrl.h>
#include <windowsx.h>

#define WLX_SIDEBAR_CLASS L"WLXDocSidebar"

#pragma comment(lib, "comctl32.lib")

SidebarWin32::SidebarWin32(HWND hParent)
    : m_parent(hParent)
{
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icc);

    HINSTANCE hInst = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = WLX_SIDEBAR_CLASS;

    static bool registered = false;
    if (!registered) {
        RegisterClassExW(&wc);
        registered = true;
    }

    m_hwnd = CreateWindowExW(
        0, WLX_SIDEBAR_CLASS, L"",
        WS_CHILD | WS_CLIPSIBLINGS,
        0, 0, widthPx(), 0, hParent, nullptr, hInst, this);
    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    m_tree = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASBUTTONS | TVS_HASLINES |
        TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_TRACKSELECT,
        0, 0, 10, 10, m_hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(1)),
        hInst, nullptr);
}

SidebarWin32::~SidebarWin32() {
    if (m_tree)
        DestroyWindow(m_tree);
    if (m_hwnd)
        DestroyWindow(m_hwnd);
}

void SidebarWin32::setDpiScale(float scale) {
    m_dpiScale = std::max(0.25f, std::min(scale, 8.0f));
}

int SidebarWin32::widthPx() const {
    return std::max(1, static_cast<int>(viewer_settings::kSidebarBaseWidth * m_dpiScale + 0.5f));
}

void SidebarWin32::setVisible(bool on) {
    if (!m_hwnd)
        return;
    ShowWindow(m_hwnd, on ? SW_SHOW : SW_HIDE);
}

void SidebarWin32::clearEntries() {
    m_lastSelected = -1;
    if (m_tree)
        TreeView_DeleteAllItems(m_tree);
    m_items.clear();
    m_materialized.clear();
}

bool SidebarWin32::hasChildren(int id) const {
    return !presenter()->childrenOf(id).isEmpty();
}

HTREEITEM SidebarWin32::insertItem(HTREEITEM parent, int id, const QString& title, bool placeholder) {
    TVINSERTSTRUCTW ins = {};
    ins.hParent = parent;
    ins.hInsertAfter = TVI_LAST;
    ins.item.mask = TVIF_TEXT | TVIF_PARAM;
    ins.item.pszText = const_cast<wchar_t*>(placeholder ? L"" : reinterpret_cast<const wchar_t*>(title.utf16()));
    ins.item.lParam = placeholder ? -1 : id;
    HTREEITEM h = (HTREEITEM)SendMessageW(m_tree, TVM_INSERTITEMW, 0, (LPARAM)&ins);
    if (h && !placeholder)
        m_items[id] = h;
    return h;
}

void SidebarWin32::materializeChildren(int parentId, HTREEITEM parentItem) {
    if (m_materialized.contains(parentId))
        return;
    m_materialized.insert(parentId);

    // Drop the placeholder item (lParam == -1) so real children can appear.
    HTREEITEM child = TreeView_GetChild(m_tree, parentItem);
    QVector<HTREEITEM> placeholders;
    while (child) {
        if (itemId(child) < 0)
            placeholders.append(child);
        child = TreeView_GetNextSibling(m_tree, child);
    }
    for (HTREEITEM ph : placeholders)
        TreeView_DeleteItem(m_tree, ph);

    const QVector<int> kids = presenter()->childrenOf(parentId);
    for (int kid : kids) {
        const SidebarEntry* e = presenter()->entry(kid);
        if (!e)
            continue;
        HTREEITEM h = insertItem(parentItem, kid, e->title, false);
        if (hasChildren(kid))
            insertItem(h, -1, QString(), true); // expand affordance
    }
}

void SidebarWin32::ensureMaterialized(int id) {
    const SidebarEntry* e = presenter()->entry(id);
    if (!e)
        return;
    QVector<int> path; // root..target
    int cur = id;
    while (cur >= 0) {
        const SidebarEntry* c = presenter()->entry(cur);
        if (!c)
            break;
        path.prepend(cur);
        cur = c->parentId;
    }

    // Suppress intermediate repaints while inserting/expanding the path so the
    // tree does not flicker when the reading position changes across pages.
    SendMessageW(m_tree, WM_SETREDRAW, FALSE, 0);
    m_internalMutation = true;
    for (int nodeId : path) {
        if (!m_items.contains(nodeId))
            continue;
        HTREEITEM h = m_items[nodeId];
        materializeChildren(nodeId, h);
        if (nodeId != path.last())
            TreeView_Expand(m_tree, h, TVE_EXPAND);
    }
    m_internalMutation = false;
    SendMessageW(m_tree, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_tree, nullptr, FALSE);
}

int SidebarWin32::itemId(HTREEITEM h) const {
    TVITEMEXW it = {};
    it.hItem = h;
    it.mask = TVIF_PARAM;
    SendMessageW(m_tree, TVM_GETITEMW, 0, (LPARAM)&it);
    return (int)it.lParam;
}

void SidebarWin32::addEntry(int id, int parentId, const QString& title) {
    if (!m_tree)
        return;
    if (parentId < 0) {
        // Top level: materialize immediately (its real children stay lazy).
        const HTREEITEM h = insertItem(TVI_ROOT, id, title, false);
        if (hasChildren(id))
            insertItem(h, -1, QString(), true);
        return;
    }
    // A descendant whose parent may not be materialized yet: skipped here, it
    // is inserted lazily when its parent expands (see materializeChildren).
    Q_UNUSED(parentId)
}

void SidebarWin32::selectEntry(int id) {
    if (!m_tree || id < 0)
        return;
    // Skip repaint when the entry did not change so navigating between pages
    // whose TOC entry stays the same does not redraw (flicker) the tree.
    if (id == m_lastSelected)
        return;
    m_lastSelected = id;
    ensureMaterialized(id);
    const QHash<int, HTREEITEM>::iterator it = m_items.find(id);
    if (it != m_items.end()) {
        // Select without scrolling if the item is already visible.
        TreeView_SelectSetFirstVisible(m_tree, TreeView_GetVisibleCount(m_tree));
        SendMessageW(m_tree, TVM_SELECTITEM, TVGN_CARET, (LPARAM)it.value());
    }
}

LRESULT CALLBACK SidebarWin32::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* self = reinterpret_cast<SidebarWin32*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self)
        return self->handleMsg(msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT SidebarWin32::handleMsg(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE: {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        if (m_tree)
            MoveWindow(m_tree, 0, 0, rc.right, rc.bottom, TRUE);
        return 0;
    }
    case WM_NOTIFY: {
        NMHDR* nm = reinterpret_cast<NMHDR*>(lp);
        if (nm->hwndFrom != m_tree)
            break;
        switch (nm->code) {
        case TVN_SELCHANGED: {
            auto* tv = reinterpret_cast<NMTREEVIEWW*>(lp);
            const int id = static_cast<int>(tv->itemNew.lParam);
            if (!m_internalMutation && id >= 0 && presenter())
                presenter()->onEntryActivated(id);
            break;
        }
        case TVN_ITEMEXPANDING: {
            auto* tv = reinterpret_cast<NMTREEVIEWW*>(lp);
            if (m_internalMutation)
                break;
            if (tv->action == TVE_EXPAND) {
                const int id = static_cast<int>(tv->itemNew.lParam);
                if (id >= 0) {
                    m_internalMutation = true;
                    materializeChildren(id, tv->itemNew.hItem);
                    m_internalMutation = false;
                }
            }
            break;
        }
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
    return DefWindowProcW(m_hwnd, msg, wp, lp);
}

#endif // Q_OS_WIN