#ifndef TEXTSELECTION_H
#define TEXTSELECTION_H

// Platform-agnostic text-selection model for the viewer.
//
// A selection is an ordered (anchor, focus) pair of (page, wordIndex)
// positions. The model is intentionally free of page-text and coordinate
// data; the controller supplies PageText for text assembly and the page
// transform for highlight rectangles.

struct SelectionPoint {
    int page = 1;
    int wordIndex = -1; // -1 .. words.size()-1; -1 = "start of page"
};

struct SelectionWordRange {
    int first = -1; // inclusive, -1 = empty
    int last = -1;  // inclusive, -1 = empty
};

class TextSelection {
public:
    void begin(int page, int wordIndex) {
        anchor = {page, wordIndex};
        focus = anchor;
        m_active = true;
    }

    void setFocus(int page, int wordIndex) {
        if (m_active)
            focus = {page, wordIndex};
    }

    void clear() { m_active = false; }

    bool isActive() const { return m_active; }
    SelectionPoint anchorPoint() const { return anchor; }
    SelectionPoint focusPoint() const { return focus; }

    int firstPage() const;
    int lastPage() const;

    // Inclusive word range selected on `page`, or an empty range when the
    // selection does not touch that page.
    SelectionWordRange wordRangeOnPage(int page) const;

private:
    bool m_active = false;
    SelectionPoint anchor;
    SelectionPoint focus;
};

#endif // TEXTSELECTION_H