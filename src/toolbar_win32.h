#ifndef TOOLBAR_WIN32_H
#define TOOLBAR_WIN32_H

#include "toolbar.h"
#include "viewer_settings.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <QHash>
#include <QString>
#include <memory>

// Win32 toolbar backend: a child-HWND strip hosting individual BUTTON
// (owner-drawn icon buttons), EDIT (page/find boxes) and STATIC (labels/status)
// controls, laid out manually at DPI-aware sizes (design D1/D3). State changes
// flow in from the shared ToolbarPresenter; user actions flow out through the
// same presenter, so both native and Qt backends share one behavior.
class ToolbarWin32 : public toolbar::ToolbarBackend {
public:
    explicit ToolbarWin32(HWND hParent);
    ~ToolbarWin32() override;

    int heightPx() const;
    HWND hwnd() const { return m_hwnd; }

    // --- ToolbarBackend ---
    void setEnabled(toolbar::Control c, bool on) override;
    void setChecked(toolbar::Control c, bool on) override;
    void setIcon(toolbar::Control c, toolbar::Icon icon) override;
    void setText(toolbar::Control c, const QString& text) override;
    void setEditText(toolbar::Control c, const QString& text) override;
    QString editText(toolbar::Control c) const override;
    bool isEditFocused() const override;

    void relayout(); // re-run after DPI change or parent resize
    void setDpiScale(float scale);
    // Returns keyboard focus to the viewer window unless the user is editing a
    // toolbar box, so arrows/Esc/etc. are never swallowed by toolbar children.
    void restoreViewerFocus() const;

private:
    struct Btn {
        int id = 0;
        toolbar::Control control = toolbar::Control::_Count;
        toolbar::Icon icon = toolbar::Icon::Print;
        int widthPx = 0;
        bool checkable = false;
    };

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handleMsg(UINT msg, WPARAM wp, LPARAM lp);
    static LRESULT CALLBACK editProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void createControls();
    void createTooltips();
    HWND addButton(const Btn& def, const wchar_t* tooltip);
    HWND addEdit(int id, int widthPx, bool rightAlign = false);
    HWND addStatic(int id, int widthPx);
    void layout();
    void onCommand(int id);
    void drawButton(HDC parentDc, const DRAWITEMSTRUCT& dis);
    void onEditCommit(int id);
    bool buildButtonBitmaps(int id, toolbar::Icon icon);

    int dpi(int v) const { return static_cast<int>(v * m_dpiScale + 0.5f); }
    int iconSizePx() const { return dpi(viewer_settings::kIconBaseSize); }
    int step() const { return dpi(8); }       // gap between control groups
    int gap() const { return dpi(4); }        // gap inside a group
    int buttonH() const { return dpi(viewer_settings::kIconBaseSize) + dpi(6); }
    int editH() const { return dpi(viewer_settings::kIconBaseSize - 6); }

    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;
    HWND m_tooltip = nullptr;
    float m_dpiScale = 1.0f;

    QHash<int, HWND> m_controls;         // child control id -> hwnd
    QHash<toolbar::Control, int> m_ctrlId; // toolbar::Control -> child id
    QHash<int, int> m_ctrlWidth;

    QHash<int, toolbar::Icon> m_btnIcon;
    QHash<int, HBITMAP> m_btnBitmap;
    QHash<int, HBITMAP> m_btnBitmapGrey;
    QHash<int, bool> m_btnEnabled;
    QHash<int, bool> m_btnCheckable;
    QHash<int, bool> m_manualCheck; // our own checked state for toggle buttons

    int m_editPage = 0;
    int m_editFind = 0;
};

#endif // Q_OS_WIN
#endif // TOOLBAR_WIN32_H