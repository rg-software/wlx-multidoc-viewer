#ifndef TEXTSELECTION_H
#define TEXTSELECTION_H

#include <QVector>

// Platform-agnostic text-selection model for the viewer.
//
// A selection is an ordered (anchor, focus) pair of (page, wordIndex,
// charIndex) boundaries. charIndex is a character offset inside a word's text
// (0..wordLen); the granularity is a single character, not a whole word.

struct SelectionPoint {
    int page = 1;
    int wordIndex = -1; // -1 = "start of page"
    int charIndex = 0;  // char offset within the word (-1 = end of the word)
};

// One contribution to the selected extent on a page, per word. from/to are
// character offsets (boundaries); to == -1 means "through the end of the word".
struct SelectionCharSpan {
    int wordIndex = -1;
    int from = 0;
    int to = -1;
};

class TextSelection {
public:
    void begin(int page, int wordIndex, int charIndex = 0) {
        anchor = {page, wordIndex, charIndex};
        focus = anchor;
        m_active = true;
    }

    void setFocus(int page, int wordIndex, int charIndex = 0) {
        if (m_active)
            focus = {page, wordIndex, charIndex};
    }

    void clear() { m_active = false; }

    bool isActive() const { return m_active; }
    SelectionPoint anchorPoint() const { return anchor; }
    SelectionPoint focusPoint() const { return focus; }

    int firstPage() const;
    int lastPage() const;

    // True when the anchor boundary precedes the focus boundary (document
    // order). The controller uses this to build forward ranges.
    bool anchorFirst() const;

    // Ordered char spans covering the selection on `page`. wordCount is the
    // page's word count (from the controller's page text); spans never exceed
    // it. Empty when the selection does not touch that page.
    QVector<SelectionCharSpan> charSpansOnPage(int page, int wordCount) const;

private:
    bool m_active = false;
    SelectionPoint anchor;
    SelectionPoint focus;
};

#endif // TEXTSELECTION_H