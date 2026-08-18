#ifndef VIEWERSTATE_H
#define VIEWERSTATE_H

#include <algorithm>

class ViewerState {
public:
    // Page navigation
    bool nextPage() {
        if (m_currentPage < m_pageCount) {
            ++m_currentPage;
            return true;
        }
        return false;
    }

    bool prevPage() {
        if (m_currentPage > 1) {
            --m_currentPage;
            return true;
        }
        return false;
    }

    bool firstPage() {
        if (m_currentPage != 1) {
            m_currentPage = 1;
            return true;
        }
        return false;
    }

    bool lastPage() {
        if (m_currentPage < m_pageCount) {
            m_currentPage = m_pageCount;
            return true;
        }
        return false;
    }

    bool goToPage(int page) {
        if (page >= 1 && page <= m_pageCount && page != m_currentPage) {
            m_currentPage = page;
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
        m_autoFit = false;
    }

    void zoomOut() {
        m_zoom = std::max(m_zoom * 0.8f, 0.1f);
        m_autoFit = false;
    }

    void setZoom(float z) { m_zoom = std::max(0.1f, std::min(z, 5.0f)); }

    // Mode
    void setPagedMode(bool paged) { m_pagedMode = paged; }
    bool isPagedMode() const { return m_pagedMode; }

    // Auto-fit
    void setFitToWidth() { m_fitToWidth = true; m_autoFit = true; }
    void setFitToPage() { m_fitToWidth = false; m_autoFit = true; }
    void setManualZoom(float z) { m_zoom = z; m_autoFit = false; }
    bool autoFit() const { return m_autoFit; }
    bool fitToWidth() const { return m_fitToWidth; }

    // State accessors
    int currentPage() const { return m_currentPage; }
    int pageCount() const { return m_pageCount; }
    float zoom() const { return m_zoom; }

    void setPageCount(int count) { m_pageCount = count; }
    void resetPage() { m_currentPage = 1; }
    void resetZoom() { m_zoom = 1.0f; m_autoFit = true; m_fitToWidth = false; }

private:
    int m_currentPage = 1;
    int m_pageCount = 0;
    float m_zoom = 1.0f;
    bool m_fitToWidth = false;
    bool m_autoFit = true;
    bool m_pagedMode = true;
};

#endif // VIEWERSTATE_H
