#ifndef VIEWERSTATE_H
#define VIEWERSTATE_H

#include <algorithm>

class ViewerState {
public:
    // How pages are grouped into display units. Single shows one page per view
    // unit; Double pairs consecutive pages (1,2),(3,4),...; DoubleWithCover
    // isolates page 1 as its own unit and pairs the remainder (2,3),(4,5),...
    enum class PagePresentation { Single, Double, DoubleWithCover };

    // Page navigation
    bool nextPage() {
        if (m_currentPage < m_pageCount) {
            ++m_currentPage;
            m_anchorPage = m_currentPage;
            return true;
        }
        return false;
    }

    bool prevPage() {
        if (m_currentPage > 1) {
            --m_currentPage;
            m_anchorPage = m_currentPage;
            return true;
        }
        return false;
    }

    bool firstPage() {
        if (m_currentPage != 1) {
            m_currentPage = 1;
            m_anchorPage = 1;
            return true;
        }
        return false;
    }

    bool lastPage() {
        if (m_currentPage < m_pageCount) {
            m_currentPage = m_pageCount;
            m_anchorPage = m_pageCount;
            return true;
        }
        return false;
    }

    bool goToPage(int page) {
        if (page >= 1 && page <= m_pageCount && page != m_currentPage) {
            m_currentPage = page;
            m_anchorPage = page;
            return true;
        }
        return false;
    }

    // Zoom calculations
    float fitToWidthZoom(int pageW, int viewportW) const {
        if (pageW <= 0 || viewportW <= 0)
            return 1.0f;
        return static_cast<float>(viewportW) / pageW;
    }

    float fitToPageZoom(int pageW, int pageH, int viewportW, int viewportH) const {
        if (pageW <= 0 || pageH <= 0 || viewportW <= 0 || viewportH <= 0)
            return 1.0f;
        float zx = static_cast<float>(viewportW) / pageW;
        float zy = static_cast<float>(viewportH) / pageH;
        return std::min(zx, zy);
    }

    void zoomIn() {
        m_zoom = std::min(m_zoom * 1.25f, 5.0f);
    }

    void zoomOut() {
        m_zoom = std::max(m_zoom * 0.8f, 0.1f);
    }

    void setZoom(float z) { m_zoom = std::max(0.1f, std::min(z, 5.0f)); }

    // Mode
    void setPagedMode(bool paged) { m_pagedMode = paged; }
    bool isPagedMode() const { return m_pagedMode; }

    // Presentation
    void setPagePresentation(PagePresentation presentation) { m_pagePresentation = presentation; }
    PagePresentation pagePresentation() const { return m_pagePresentation; }

    // Reading anchor independent of unit pairing (issue #20). currentPage is
    // truncated to the unit's first page in double-page states, so cycling the
    // presentation off currentPage alone accumulates unitFirst() rounding and
    // drifts toward page 1. anchorPage is the page actually being read and is
    // only rewritten by real navigation, never by unit resolution.
    int anchorPage() const { return m_anchorPage; }
    void setAnchorPage(int page) {
        if (page >= 1 && (m_pageCount <= 0 || page <= m_pageCount))
            m_anchorPage = page;
    }
    // Unit resolution only: moves current without touching the anchor. Used by
    // cyclePagePresentation and trackCurrentPage.
    void resolveCurrentPage(int page) {
        if (page >= 1 && page <= m_pageCount)
            m_currentPage = page;
    }

    // State accessors
    int currentPage() const { return m_currentPage; }
    int pageCount() const { return m_pageCount; }
    float zoom() const { return m_zoom; }

    void setPageCount(int count) {
        m_pageCount = count;
        if (m_pageCount > 0) {
            m_currentPage = (std::clamp)(m_currentPage, 1, m_pageCount);
            m_anchorPage = (std::clamp)(m_anchorPage, 1, m_pageCount);
        }
    }
    void resetPage() { m_currentPage = 1; m_anchorPage = 1; }

private:
    int m_currentPage = 1;
    int m_anchorPage = 1;
    int m_pageCount = 0;
    float m_zoom = 1.0f;
    bool m_pagedMode = false;
    PagePresentation m_pagePresentation = PagePresentation::Single;
};

#endif // VIEWERSTATE_H
