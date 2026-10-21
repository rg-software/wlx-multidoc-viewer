#include "toolbar.h"
#include "ui_strings.h"

#include <QString>
#include <algorithm>

namespace toolbar {

void ToolbarBackend::notifyFindTextChanged() {
    if (m_presenter)
        m_presenter->onFindTextChanged();
}

void ToolbarPresenter::refreshState() {
    if (!m_backend)
        return;

    const bool hasDoc = m_controller && m_controller->hasDocument();
    const int page = hasDoc ? m_controller->currentPage() : 1;
    const int count = hasDoc ? m_controller->pageCount() : 0;
    const bool paged = hasDoc && m_controller->isPagedMode();

    // Prev/next step whole screens (units). In a double-page state the last
    // unit is reached while currentPage < pageCount, so enablement must reflect
    // whether an adjacent screen exists: use hasPrev/hasNext in paged mode, and
    // in continuous mode check whether a whole-unit step from the page at the
    // top of the viewport can actually move.
    if (hasDoc) {
        if (paged) {
            m_backend->setEnabled(Control::PrevPage, m_controller->hasPrevPage());
            m_backend->setEnabled(Control::NextPage, m_controller->hasNextPage());
        } else {
            const int base = m_controller->pageAtScrollOffset(m_controller->scrollAnchor());
            const int stepPrev = m_controller->scrollStepPage(base, -1);
            const int stepNext = m_controller->scrollStepPage(base, +1);
            m_backend->setEnabled(Control::PrevPage, stepPrev != base || m_controller->hasPrevSibling());
            m_backend->setEnabled(Control::NextPage, stepNext != base || m_controller->hasNextSibling());
        }
    } else {
        m_backend->setEnabled(Control::PrevPage, false);
        m_backend->setEnabled(Control::NextPage, false);
    }
    m_backend->setEditText(Control::PageBox, hasDoc ? QString::number(page) : QString());
    m_backend->setText(Control::PageCount, hasDoc ? ui_strings::pageCountSuffix().arg(count) : QString());

    // A standalone image document is a single page whose "pages" are really its
    // sibling images: show the image position/total instead of a page count.
    if (hasDoc) {
        const int imgPos = m_controller->imagePosition();
        if (imgPos > 0) {
            m_backend->setEditText(Control::PageBox, QString::number(imgPos));
            m_backend->setText(Control::PageCount,
                               ui_strings::pageCountSuffix().arg(m_controller->imageCount()));
        }
    }

    // Paged/continuous and single/double presentation are meaningless for a
    // single-page document (a standalone image), so disable those toggles.
    const bool singlePage = hasDoc && count <= 1;
    m_backend->setChecked(Control::ModeToggle, hasDoc && !singlePage && !m_controller->isPagedMode());
    m_backend->setIcon(Control::ModeToggle,
                       hasDoc && !singlePage
                           ? (m_controller->isPagedMode() ? Icon::ModePaged : Icon::ModeContinuous)
                           : Icon::ModePaged);

    // Presentation is a three-state cycle reflected via its current icon.
    m_backend->setEnabled(Control::PresentationToggle, hasDoc && !singlePage);
    if (hasDoc) {
        switch (m_controller->pagePresentation()) {
        case ViewerState::PagePresentation::Single:          m_backend->setIcon(Control::PresentationToggle, Icon::PresentationSingle); break;
        case ViewerState::PagePresentation::Double:          m_backend->setIcon(Control::PresentationToggle, Icon::PresentationDouble); break;
        case ViewerState::PagePresentation::DoubleWithCover: m_backend->setIcon(Control::PresentationToggle, Icon::PresentationDoubleWithCover); break;
        }
    } else {
        m_backend->setIcon(Control::PresentationToggle, Icon::PresentationSingle);
    }

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
    updateFindControls();

    QString findInfo;
    if (canSearch) {
        if (m_controller->searchInProgress())
            findInfo = ui_strings::findSearching();
        else if (m_controller->searchNoMatch())
            findInfo = ui_strings::findNoMatch();
        else if (hasMatches)
            findInfo = ui_strings::matchCountFraction()
                           .arg(m_controller->activeMatchIndex() + 1)
                           .arg(matchCount);
    }
    m_backend->setText(Control::FindStatus, findInfo);

    // Copy copies the current selection, so it is enabled whenever a selection is
// active. (This replaces the earlier always-disabled placeholder now that text
// selection works.)
    m_backend->setEnabled(Control::Copy, hasDoc && m_controller->hasSelection());

    m_backend->setEnabled(Control::ToggleFavorite, hasDoc);
    // Reflect the current-page state with the icon itself (outlined bookmark vs
    // filled bookmark_star) rather than a pressed-button background.
    m_backend->setIcon(Control::ToggleFavorite,
                       (hasDoc && m_controller->isCurrentPageFavorite())
                           ? Icon::FavoritesStar
                           : Icon::Favorites);

    m_backend->setEnabled(Control::Print, hasDoc);
    m_backend->setEnabled(Control::SidebarToggle, hasDoc && (!sidebarAvailable || sidebarAvailable()));
    m_backend->setChecked(Control::SidebarToggle, sidebarVisible && sidebarVisible());
    m_backend->setEnabled(Control::RotateLeft, hasDoc);
    m_backend->setEnabled(Control::RotateRight, hasDoc);
    m_backend->setEnabled(Control::ModeToggle, hasDoc && !singlePage);
    m_backend->setEnabled(Control::FitButton, hasDoc);
}

void ToolbarPresenter::updateFindControls() {
    if (!m_backend)
        return;
    // Enablement is driven by the box contents plus search capability, NOT by
    // whether a result set exists: with a term present the buttons are usable
    // immediately and initiate the search themselves (issue #12, design
    // decision 3).
    const bool canSearch =
        m_controller && m_controller->hasDocument() && m_controller->searchAvailable();
    const bool termPresent =
        canSearch && !m_backend->editText(Control::FindBox).trimmed().isEmpty();
    m_backend->setEnabled(Control::FindPrev, termPresent);
    m_backend->setEnabled(Control::FindNext, termPresent);
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
    // at the top of the viewport and scroll in the top of the neighbouring
    // screen (whole unit in a double-page presentation).
    const int base = m_controller->pageAtScrollOffset(m_controller->scrollAnchor());
    const int target = m_controller->scrollStepPage(base, -1);
    if (target != base && m_applyScroll) {
        m_applyScroll(m_controller->scrollOffsetForPage(target));
        return;
    }
    // No whole-view step possible (single-page document at a boundary): fall
    // back to controller navigation, which opens a sibling image when one exists.
    m_controller->prevPage();
}

void ToolbarPresenter::onNextPage() {
    if (!m_controller)
        return;
    if (m_controller->isPagedMode()) {
        m_controller->nextPage();
        return;
    }
    const int base = m_controller->pageAtScrollOffset(m_controller->scrollAnchor());
    const int target = m_controller->scrollStepPage(base, +1);
    if (target != base && m_applyScroll) {
        m_applyScroll(m_controller->scrollOffsetForPage(target));
        return;
    }
    m_controller->nextPage();
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

void ToolbarPresenter::onPresentationCycled() {
    if (!m_controller)
        return;
    // Mirror the keyboard toggle: keep the current page, and restore the
    // scroll so the view does not reset to page 1 when switching presentation.
    m_controller->cyclePagePresentation();
    if (!m_applyScroll)
        return;
    if (m_controller->isPagedMode())
        m_applyScroll(0);
    else
        // Re-target to the POST-cycle unit top (issue #20): the pre-cycle
        // current page is truncated to the old unit's first page, which under
        // the new pairing can sit in an earlier unit. Scrolling there strands
        // the anchor and scroll tracking then truncates it, so every press
        // walks the view toward page 1.
        m_applyScroll(m_controller->scrollOffsetForPage(m_controller->currentPage()));
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

void ToolbarPresenter::onToggleFavorite() {
    if (m_controller)
        m_controller->toggleCurrentPageFavorite();
}

void ToolbarPresenter::onFindCommitted(const QString& text) {
    if (!m_controller)
        return;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        m_controller->clearSearch();
        updateFindControls();
        return;
    }
    // Pressing Enter again on the already-active term should cycle through the
    // matches, not restart the scan: a completed search keeps its query and
    // highlights, so re-committing the same text is a "next match" request.
    if (trimmed == m_controller->searchQuery() && m_controller->searchActive()) {
        applyAnchored([this](int s) { return m_controller->nextMatch(s); });
        return;
    }
    m_controller->startSearch(trimmed, m_matchCase);
    updateFindControls();
}

void ToolbarPresenter::onMatchCaseToggled(bool on) {
    m_matchCase = on;
    // Re-run only an already-active query so the toggle updates matches
    // immediately; with an empty/never-searched box it merely constrains the
    // next search (design decision 5).
    if (m_controller && !m_controller->searchQuery().isEmpty()) {
        const QString boxText =
            m_backend ? m_backend->editText(Control::FindBox).trimmed() : QString();
        if (!boxText.isEmpty())
            m_controller->startSearch(boxText, on);
    }
    refreshState();
}

void ToolbarPresenter::onFindPrev() {
    if (!m_controller || !m_backend)
        return;
    const QString boxText = m_backend->editText(Control::FindBox).trimmed();
    if (boxText.isEmpty())
        return;
    // No active search for the current box text: this activation initiates it
    // (same semantics as Enter) instead of navigating a stale result set.
    // Keyed on the committed query, not the hit list, so a zero-match term is
    // never re-scanned (design decision 2).
    if (boxText != m_controller->searchQuery()) {
        m_controller->startSearch(boxText, m_matchCase);
        updateFindControls();
        return;
    }
    applyAnchored([this](int s) { return m_controller->prevMatch(s); });
}

void ToolbarPresenter::onFindNext() {
    if (!m_controller || !m_backend)
        return;
    const QString boxText = m_backend->editText(Control::FindBox).trimmed();
    if (boxText.isEmpty())
        return;
    if (boxText != m_controller->searchQuery()) {
        m_controller->startSearch(boxText, m_matchCase);
        updateFindControls();
        return;
    }
    applyAnchored([this](int s) { return m_controller->nextMatch(s); });
}

void ToolbarPresenter::onFindTextChanged() {
    if (m_applyingFindBox)
        return; // programmatic box write, not a user edit (decision 7)
    // The box is the single source of truth for the current term: any edit
    // invalidates the committed query and its results immediately, without a
    // commit (issue #12, decision 1). clearSearch() early-returns when nothing
    // is active, so editing before any search is free.
    if (m_controller)
        m_controller->clearSearch();
    updateFindControls();
}

void ToolbarPresenter::onDocumentLoaded() {
    // No term or highlights leak across documents (decision 6). The guarded
    // write does not raise onFindTextChanged.
    setFindBoxText(QString());
    if (m_controller)
        m_controller->clearSearch();
    updateFindControls();
}

void ToolbarPresenter::setFindBoxText(const QString& text) {
    if (!m_backend)
        return;
    m_applyingFindBox = true;
    m_backend->setEditText(Control::FindBox, text);
    m_applyingFindBox = false;
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