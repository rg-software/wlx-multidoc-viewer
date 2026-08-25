#include "textselection.h"

#include <algorithm>
#include <climits>

namespace {

// charIndex -1 means "end of word", which orders after any concrete offset.
int pos(int c) { return c < 0 ? INT_MAX : c; }

bool pointLess(const SelectionPoint& a, const SelectionPoint& b) {
    if (a.page != b.page)
        return a.page < b.page;
    if (a.wordIndex != b.wordIndex)
        return a.wordIndex < b.wordIndex;
    return pos(a.charIndex) < pos(b.charIndex);
}

} // namespace

int TextSelection::firstPage() const {
    return std::min(anchor.page, focus.page);
}

int TextSelection::lastPage() const {
    return std::max(anchor.page, focus.page);
}

bool TextSelection::anchorFirst() const {
    return pointLess(anchor, focus);
}

QVector<SelectionCharSpan> TextSelection::charSpansOnPage(int page, int wordCount) const {
    QVector<SelectionCharSpan> out;
    if (!m_active || wordCount <= 0)
        return out;
    if (page < firstPage() || page > lastPage())
        return out;

    const bool aFirst = pointLess(anchor, focus);
    const SelectionPoint& lo = aFirst ? anchor : focus;
    const SelectionPoint& hi = aFirst ? focus : anchor;

    int startWord = 0;
    int startChar = 0;
    if (page == lo.page) {
        startWord = std::max(0, lo.wordIndex);
        startChar = (startWord == lo.wordIndex) ? lo.charIndex : 0;
    }
    if (startWord >= wordCount)
        return out;

    int endWord = wordCount - 1;
    int endChar = -1; // -1 = through end of word
    if (page == hi.page) {
        endWord = std::max(0, hi.wordIndex);
        endChar = (endWord == hi.wordIndex) ? hi.charIndex : -1;
    }
    endWord = std::min(endWord, wordCount - 1);
    if (endWord < startWord)
        return out;

    // Both boundaries in the same word: a sub-word (possibly empty) span.
    if (startWord == endWord) {
        const int to = (endChar < 0) ? -1 : std::max(startChar, endChar);
        if (to < 0 || startChar < to)
            out.append({startWord, startChar, to});
        return out;
    }

    // The first word is cut from startChar.
    out.append({startWord, startChar, -1});
    // Full middle words.
    for (int w = startWord + 1; w < endWord; ++w)
        out.append({w, 0, -1});
    // The last word is cut at endChar.
    if (endChar < 0)
        out.append({endWord, 0, -1});
    else if (endChar > 0)
        out.append({endWord, 0, endChar});
    return out;
}