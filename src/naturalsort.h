#ifndef NATURALSORT_H
#define NATURALSORT_H

#include <QString>

// Natural comparison for filenames: digit runs compare numerically
// (case-insensitive), so "page2" sorts before "page10". Everything else
// compares by lower-cased code point. Returns -1/0/1. Shared by the comic
// archive engine (page enumeration) and the image folder browser (sibling
// ordering) so both order deterministically and identically.
int naturalCompare(const QString& a, const QString& b);

#endif // NATURALSORT_H