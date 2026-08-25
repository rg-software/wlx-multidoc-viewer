#include "toolbar.h"

#include <QString>
#include <algorithm>

namespace toolbar {

void ToolbarPresenter::refreshState() {
    if (!m_backend)
        return;

    const bool hasDoc = m_controller && m_controller->hasDocument();
    const int page = hasDoc ? m_controller->currentPage() : 1;
    const int count = hasDoc ? m_controller->pageCount() : 0;

    m_backend->setEnabled(Control::PrevPage, hasDoc && page > 1);
    m_backend->setEnabled(Control::NextPage, hasDoc && page < count);
    m_backend->setEditText(Control::PageBox, hasDoc ? QString::number(page) : QString());
    m_backend->setText(Control::PageCount, hasDoc ? QStringLiteral("/ %1").arg(count) : QString());
    m_backend->setChecked(Control::ModeToggle, hasDoc && !m_controller->isPagedMode());
    m_backend->setIcon(Control::ModeToggle,
                       hasDoc ? (m_controller->isPagedMode() ? Icon::ModePaged : Icon::ModeContinuous)
                              : Icon::ModePaged);

    Icon fitIcon = Icon::FitPage;
    if (hasDoc) {
        switch (m_controller->fitMode()) {
        case ViewerController::FitMode::Manual:     fitIcon = Icon::FitManual; break;
        case ViewerController::FitMode::FitToPage:  fitIcon = Icon::FitPage; break;
        case ViewerController::FitMode::FitToWidth: fitIcon = Icon::FitWidth; break;
        }
    }
    m_backend->setIcon(Control::FitButton, fitIcon);

    const float zoom = hasDoc ? m_controller->zoom() : 0.0f;
    m_backend->setEnabled(Control::ZoomIn, zoom < 4.99f);
    m_backend->setEnabled(Control::ZoomOut, zoom > 0.11f);

    const bool canSearch = hasDoc && m_controller->searchAvailable();
    m_backend->setEnabled(Control::FindBox, canSearch);
    m_backend->setEnabled(Control::MatchCase, canSearch);
    m_backend->setChecked(Control::MatchCase, m_matchCase);
    // Reflect OFF/ON via a different glyph (Material has no Aa-case icon; use
    // the font/text glyph pair for the two states).
    m_backend->setIcon(Control::MatchCase,
                       m_matchCase ? Icon::MatchCase : Icon::MatchCaseOff);

    const int matchCount = hasDoc ? m_controller->searchMatchCount() : 0;
    const bool hasMatches = matchCount > 0;
    m_backend->setEnabled(Control::FindPrev, canSearch && hasMatches);
    m_backend->setEnabled(Control::FindNext, canSearch && hasMatches);

    QString findInfo;
    if (canSearch) {
        if (m_controller->searchInProgress())
            findInfo = QStringLiteral("Searching");
        else if (m_controller->searchNoMatch())
            findInfo = QStringLiteral("No matches");
        else if (hasMatches)
            findInfo = QStringLiteral("%1 / %2")
                           .arg(m_controller->activeMatchIndex() + 1)
                           .arg(matchCount);
    }
    m_backend->setText(Control::FindStatus, findInfo);

    // Copy copies the current selection, so it is enabled whenever a selection is
// active. (This replaces the earlier always-disabled placeholder now that text
// selection works.)
    m_backend->setEnabled(Control::Copy, hasDoc && m_controller->hasSelection());

    m_backend->setEnabled(Control::Print, hasDoc);
    m_backend->setEnabled(Control::SidebarToggle, hasDoc && (!sidebarAvailable || sidebarAvailable()));
    m_backend->setChecked(Control::SidebarToggle, sidebarVisible && sidebarVisible());
    m_backend->setEnabled(Control::RotateLeft, hasDoc);
    m_backend->setEnabled(Control::RotateRight, hasDoc);
    m_backend->setEnabled(Control::ModeToggle, hasDoc);
    m_backend->setEnabled(Control::FitButton, hasDoc);
}

void ToolbarPresenter::applyAnchored(std::function<int(int)> step) {
    if (!m_controller)
        return;
    const int next = step(m_controller->scrollAnchor());
    if (m_applyScroll)
        m_applyScroll(next);
}

void ToolbarPresenter::onPrevPage() {
    if (!m_controller)
        return;
    if (m_controller->isPagedMode()) {
        m_controller->prevPage();
        return;
    }
    // Continuous: navigate the VIEW, not just the counter. Start from the page
    // at the top of the viewport, move one page, and scroll its top in.
    const int base = m_controller->pageAtScrollOffset(m_controller->scrollAnchor());
    const int target = (std::max)(1, base - 1);
    if (m_applyScroll)
        m_applyScroll(m_controller->scrollOffsetForPage(target));
}

void ToolbarPresenter::onNextPage() {
    if (!m_controller)
        return;
    if (m_controller->isPagedMode()) {
        m_controller->nextPage();
        return;
    }
    const int base = m_controller->pageAtScrollOffset(m_controller->scrollAnchor());
    const int target = (std::min)(base + 1, m_controller->pageCount());
    if (m_applyScroll)
        m_applyScroll(m_controller->scrollOffsetForPage(target));
}

void ToolbarPresenter::onGoToPageCommitted(const QString& raw) {
    if (!m_controller || !m_controller->hasDocument())
        return;
    bool ok = false;
    const int parsed = raw.trimmed().toInt(&ok);
    if (!ok || parsed < 1) {
        refreshState(); // revert the edit box to the actual page
        return;
    }
    const int clamped = (std::min)(parsed, m_controller->pageCount());
    m_controller->goToPage(clamped);
    if (!m_controller->isPagedMode() && m_applyScroll)
        m_applyScroll(m_controller->scrollOffsetForPage(clamped));
}

void ToolbarPresenter::onModeToggled() {
    if (!m_controller)
        return;
    // Mirror the keyboard toggle: keep the current page, and restore the
    // scroll so the view does not reset to page 1 when switching modes.
    const int page = m_controller->currentPage();
    m_controller->toggleMode();
    if (!m_applyScroll)
        return;
    if (m_controller->isPagedMode())
        m_applyScroll(0);
    else
        m_applyScroll(m_controller->scrollOffsetForPage(page));
}

void ToolbarPresenter::onFitCycled() {
    applyAnchored([this](int s) { return m_controller->cycleFitMode(s); });
}

void ToolbarPresenter::onRotateLeft() {
    applyAnchored([this](int s) { return m_controller->rotateCcw(s); });
}

void ToolbarPresenter::onRotateRight() {
    applyAnchored([this](int s) { return m_controller->rotateCw(s); });
}

void ToolbarPresenter::onZoomIn() {
    applyAnchored([this](int s) { return m_controller->zoomIn(s); });
}

void ToolbarPresenter::onZoomOut() {
    applyAnchored([this](int s) { return m_controller->zoomOut(s); });
}

void ToolbarPresenter::onFindCommitted(const QString& text) {
    if (!m_controller)
        return;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        m_controller->clearSearch();
        return;
    }
    m_controller->startSearch(trimmed, m_matchCase);
}

void ToolbarPresenter::onMatchCaseToggled(bool on) {
    m_matchCase = on;
    // Match-case is a pure checkbox: it only affects the next search, it does
    // not re-run the current one.
    refreshState();
}

void ToolbarPresenter::onFindPrev() {
    if (m_controller)
        applyAnchored([this](int s) { return m_controller->prevMatch(s); });
}

void ToolbarPresenter::onFindNext() {
    if (m_controller)
        applyAnchored([this](int s) { return m_controller->nextMatch(s); });
}

void ToolbarPresenter::onPrint() {
    if (printHandler)
        printHandler();
}

void ToolbarPresenter::onCopy() {
    // Copy the current selection to the clipboard.
    if (m_controller && m_controller->hasSelection()) {
        const QString text = m_controller->selectedText();
        if (copyHandler && !text.isEmpty())
            copyHandler(text);
    }
}

void ToolbarPresenter::onSidebarToggled() {
    if (sidebarToggleHandler)
        sidebarToggleHandler();
}

} // namespace toolbar