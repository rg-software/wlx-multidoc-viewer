#ifndef TOOLBAR_H
#define TOOLBAR_H

#include "viewercontroller.h"

#include <QString>
#include <functional>

// Shared on-window toolbar definition (Task 3.1). One control set, one
// presenter; two native backends (Win32 common controls, Qt widgets) with
// behavioral parity. The toolbar is a thin wrapper: it never owns document
// logic, it just maps controller state -> backend widgets and backend events ->
// controller calls.
//
// The presenter pushes state on every controller change (via the state-changed
// callback), so keyboard and toolbar stay synchronized by construction.

namespace toolbar {

enum class Control {
    PrevPage,
    NextPage,
    PageBox,       // editable current page number
    PageCount,     // read-only "/ N" label
    ModeToggle,    // paged / continuous
    FitButton,     // cycles Manual -> FitToPage -> FitToWidth; icon follows
    RotateLeft,
    RotateRight,
    ZoomIn,
    ZoomOut,
    FindBox,       // editable search term
    FindPrev,
    FindNext,
    MatchCase,
    FindStatus,    // "n / m" or no-match label
    Print,
    Copy,          // permanently disabled placeholder (deferred selection)
    SidebarToggle,
    _Count
};

enum class Icon {
    Prev, Next, ModePaged, ModeContinuous, FitManual, FitPage, FitWidth,
    RotateLeft, RotateRight, ZoomIn, ZoomOut,
    Find, FindPrev, FindNext, MatchCase, MatchCaseOff,
    Print, Copy, SidebarToggle,
};

class ToolbarPresenter;

// Abstract native implementation of the control strip. The backend owns the
// actual OS widgets and renders images/text; the presenter owns all behavior.
class ToolbarBackend {
public:
    virtual ~ToolbarBackend() = default;

    // Presenter -> backend updates.
    virtual void setEnabled(Control c, bool on) = 0;
    virtual void setChecked(Control c, bool on) = 0;
    virtual void setIcon(Control c, Icon icon) = 0;
    virtual void setText(Control c, const QString& text) = 0;
    virtual void setEditText(Control c, const QString& text) = 0;

    // Backend -> presenter queries (edit box contents).
    virtual QString editText(Control c) const = 0;

    // Focus neutrality: true while keyboard focus is inside an edit control,
    // meaning typed characters must go to the control, never to the viewer.
    virtual bool isEditFocused() const = 0;

    void setPresenter(ToolbarPresenter* p) { m_presenter = p; }
    ToolbarPresenter* presenter() const { return m_presenter; }

private:
    ToolbarPresenter* m_presenter = nullptr;
};

// Maps controller state -> backend widgets and backend events -> controller
// calls, plus the small cross-platform helpers (scroll anchoring for zoom/fit/
// rotate/match navigation, which the viewer owns).
class ToolbarPresenter {
public:
    ToolbarPresenter() = default;
    ~ToolbarPresenter() = default;
    ToolbarPresenter(const ToolbarPresenter&) = delete;
    ToolbarPresenter& operator=(const ToolbarPresenter&) = delete;

    void attach(ViewerController* controller, ToolbarBackend* backend) {
        m_controller = controller;
        m_backend = backend;
        if (backend)
            backend->setPresenter(this);
    }

    // Applier for anchored scroll commands (zoom/fit/rotate/match). The viewer
    // installs this to move its own scroll position, keeping the scroll anchor
    // pattern used by keyboard shortcuts.
    void setScrollApplier(std::function<void(int)> fn) { m_applyScroll = std::move(fn); }

    ViewerController* controller() const { return m_controller; }
    ToolbarBackend* backend() const { return m_backend; }

    bool matchCaseOn() const { return m_matchCase; }

    // Refresh all control state from the controller (state-changed callback).
    void refreshState();

    // ---- Backend event handlers (called from the native UI) ----
    void onPrevPage();
    void onNextPage();
    void onGoToPageCommitted(const QString& text);
    void onModeToggled();
    void onFitCycled();
    void onRotateLeft();
    void onRotateRight();
    void onZoomIn();
    void onZoomOut();
    void onFindCommitted(const QString& text);
    void onFindPrev();
    void onFindNext();
    void onMatchCaseToggled(bool on);
    void onPrint();
    void onCopy();
    void onSidebarToggled();

    // Host hooks: the viewer installs the print/sidebar entry points so the
    // shared presenter stays platform-independent. onSidebarToggle is set by
    // the sidebar host (the toolbar itself only reflects availability).
    std::function<void()> printHandler;
    std::function<void()> sidebarToggleHandler;
    std::function<bool()> sidebarAvailable; // null = assume available
    std::function<void(const QString&)> copyHandler; // platform clipboard write
    std::function<bool()> sidebarVisible;   // null = assume hidden

private:
    void applyAnchored(std::function<int(int)> step);

    ViewerController* m_controller = nullptr;
    ToolbarBackend* m_backend = nullptr;
    std::function<void(int)> m_applyScroll;
    bool m_matchCase = false;
};

} // namespace toolbar

#endif // TOOLBAR_H