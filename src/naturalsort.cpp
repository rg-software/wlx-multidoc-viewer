#include "naturalsort.h"

int naturalCompare(const QString& a, const QString& b) {
    int i = 0, j = 0;
    const int na = a.size(), nb = b.size();
    while (i < na && j < nb) {
        const QChar ca = a[i], cb = b[j];
        const bool da = ca.isDigit(), db = cb.isDigit();
        if (da && db) {
            int ia = i, ib = j;
            while (ia < na && a[ia].isDigit()) ++ia;
            while (ib < nb && b[ib].isDigit()) ++ib;
            // strip leading zeros
            while (i < ia - 1 && a[i] == QLatin1Char('0')) ++i;
            while (j < ib - 1 && b[j] == QLatin1Char('0')) ++j;
            const int la = ia - i, lb = ib - j;
            if (la != lb)
                return la < lb ? -1 : 1;
            for (int k = 0; k < la; ++k) {
                if (a[i + k] != b[j + k])
                    return a[i + k] < b[j + k] ? -1 : 1;
            }
            i = ia;
            j = ib;
        } else {
            const QChar la = ca.toLower(), lb = cb.toLower();
            if (la != lb)
                return la < lb ? -1 : 1;
            ++i;
            ++j;
        }
    }
    if (i < na) return 1;
    if (j < nb) return -1;
    return 0;
}