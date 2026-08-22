#include "textselection.h"

#include <algorithm>
#include <climits>

int TextSelection::firstPage() const {
    return std::min(anchor.page, focus.page);
}

int TextSelection::lastPage() const {
    return std::max(anchor.page, focus.page);
}

SelectionWordRange TextSelection::wordRangeOnPage(int page) const {
    SelectionWordRange out; // {first=-1,last=-1} = empty
    if (!m_active)
        return out;
    if (page < firstPage() || page > lastPage())
        return out;

    // Word indices are half-open in the model's choice of -1 = "before first
    // word". A focus of -1 means "start of page" (only reachable as the anchor
    // when the press fell between words before the first word). Clamp both
    // endpoints into [0, +inf] and let the controller clamp the upper bound to
    // the page's actual word count.
    const int loRaw = std::min(anchor.wordIndex, focus.wordIndex);
    const int hiRaw = std::max(anchor.wordIndex, focus.wordIndex);

    if (page == firstPage()) {
        // First page of the span: starts at the lower endpoint (clamped to 0),
        // extends to the end unless this is also the last page.
        out.first = std::max(0, loRaw);
        out.last = (page == lastPage()) ? std::max(0, hiRaw) : INT_MAX;
        if (out.first > out.last) {
            std::swap(out.first, out.last);
        }
    } else if (page == lastPage()) {
        // Last page of the span: from word 0 to the higher endpoint.
        out.first = 0;
        out.last = std::max(0, hiRaw);
    } else {
        // Middle page: whole page selected.
        out.first = 0;
        out.last = INT_MAX;
    }
    return out;
}