// Windows headless smoke harness for the MOBI cover-page interception
// (openspec change add-mobi-cover-page). Loads examples/sample1.mobi through
// the real MuPdfEngine and asserts:
//   6.1 24 pages (body is 23, cover adds page 1) with a cover on page 1
//   6.2 page 1 renders non-blank and is an image page (no selectable text)
//   6.3 a copy with a corrupted cover record falls back to 23 body pages
// Prints PASS/FAIL lines; exits nonzero on any failure.

#include "mupdfengine.h"

#include <QFile>
#include <QImage>
#include <QString>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static int g_failures = 0;

#define CHECK(name, cond)                                                     \
    do {                                                                      \
        std::printf("%s %s (line %d)\n", (cond) ? "PASS" : "FAIL", name,      \
                    __LINE__);                                                \
        std::fflush(stdout);                                                  \
        if (!(cond))                                                          \
            ++g_failures;                                                     \
    } while (0)

// Corrupt the cover record (first image index, MOBI header offset 0x5C) in a
// copy of the source MOBI so EXTH type 201 resolves to an image record whose
// bytes fail fz_recognize_image_format -> interception must bail out.
static bool writeCoverCorruptedCopy(const QString& src, const QString& dst) {
    QFile in(src);
    if (!in.open(QIODevice::ReadOnly))
        return false;
    QByteArray raw = in.readAll();
    in.close();
    if (raw.size() < 96)
        return false;

    const auto u32be = [](const QByteArray& b, size_t o) -> quint32 {
        return (static_cast<quint32>(static_cast<unsigned char>(b[o])) << 24) |
               (static_cast<quint32>(static_cast<unsigned char>(b[o + 1])) << 16) |
               (static_cast<quint32>(static_cast<unsigned char>(b[o + 2])) << 8) |
               static_cast<quint32>(static_cast<unsigned char>(b[o + 3]));
    };
    const auto u16be = [](const QByteArray& b, size_t o) -> quint32 {
        return (static_cast<quint32>(static_cast<unsigned char>(b[o])) << 8) |
               static_cast<quint32>(static_cast<unsigned char>(b[o + 1]));
    };

    const quint32 recCount = u16be(raw, 76);
    if (recCount < 1)
        return false;
    const quint32 rec0 = u32be(raw, 78); // record 0 data offset
    if (rec0 + 16 + 4 + 4 > static_cast<quint32>(raw.size()))
        return false;
    const quint32 hdrLen = u32be(raw, rec0 + 16 + 4);
    if (rec0 + 16 + hdrLen + 4 > static_cast<quint32>(raw.size()))
        return false;

    // Rewrite the "First Image Index" (offset 0x5C within the MOBI header to a
    // huge value so the computed cover record lands out of range / unparsable.
    const size_t firstImage = rec0 + 16 + 0x5C;
    raw[firstImage] = '\xFF';
    raw[firstImage + 1] = '\xFF';
    raw[firstImage + 2] = '\xFF';
    raw[firstImage + 3] = '\xFF';

    QFile out(dst);
    if (!out.open(QIODevice::WriteOnly))
        return false;
    return out.write(raw) == raw.size();
}

int main() {
    const QString sample =
        QStringLiteral("C:/Projects-Git/wlx-multidoc-viewer/examples/sample1.mobi");
    const QString corrupt =
        QStringLiteral("C:/Users/Maxim/AppData/Local/Temp/opencode/sample1_nocover.mobi");

    if (!writeCoverCorruptedCopy(sample, corrupt)) {
        std::printf("FAIL prep: could not write corrupted MOBI copy\n");
        return 2;
    }

    // ---- 6.1/6.2: real sample -> cover injected as page 1
    {
        MuPdfEngine e;
        CHECK("open sample1.mobi", e.open(sample));
        if (!e.isOpen()) {
            std::printf("RESULT: FAIL (%d failure(s))\n", g_failures);
            return g_failures ? 1 : 0;
        }
        const int pages = e.pageCount();
        std::printf("  [dbg] pageCount=%d (plain body is 23; +cover = 24)\n", pages);
        CHECK("body 23 pages + cover = 24 pages", pages == 24);
        for (int pg = 1; pg <= pages; ++pg) {
            const QString t = e.extractText(pg).simplified();
            const PageInfo d = e.pageDimensions(pg);
            std::printf("  [dbg] p%02d [%dx%d] len=%d: %.60s\n", pg, d.width, d.height,
                        t.size(), t.left(60).toUtf8().constData());
            if (pg == 2 || pg == 3)
                std::printf("  [dbg] p%02d FULL: %.900s\n", pg, t.toUtf8().constData());
        }
        const QString allText = [&] {
            QString out;
            for (int pg = 1; pg <= pages; ++pg)
                out += e.extractText(pg);
            return out;
        }();
        std::printf("  [dbg] contains 'for Sharon'=%d contains 'Plato'=%d contains 'Chapter 1 THE'=%d\n",
                    allText.contains(QStringLiteral("for Sharon")) ? 1 : 0,
                    allText.contains(QStringLiteral("Plato")) ? 1 : 0,
                    allText.contains(QStringLiteral("Chapter 1")) ? 1 : 0);

        const QImage p1 = e.renderPage(1, 1.0f, 1.0f, 0);
        CHECK("cover page 1 renders non-blank", !p1.isNull());
        if (!p1.isNull()) {
            std::printf("  [dbg] cover size %dx%d\n", p1.width(), p1.height());
            CHECK("page 1 has no text layer", !e.pageText(1).hasText);

            // The cover is a full-page graphic: rendered as full-bleed contain
            // (aspect preserved), so it should span most of the page — NOT be
            // shrunk to the body-text margins. Measure the ink bbox.
            int minX = p1.width(), minY = p1.height(), maxX = -1, maxY = -1;
            QImage probe = p1;
            if (probe.format() != QImage::Format_RGB888)
                probe = probe.convertToFormat(QImage::Format_RGB888);
            const int bpl = probe.bytesPerLine();
            for (int y = 0; y < probe.height(); ++y) {
                const uchar* line = probe.constScanLine(y);
                for (int x = 0; x < probe.width(); ++x) {
                    const uchar* p = line + x * 3;
                    // Treat (near-)white as empty canvas; anything darker or
                    // more saturated is cover ink.
                    if (p[0] < 250 || p[1] < 250 || p[2] < 250) {
                        minX = qMin(minX, x);
                        minY = qMin(minY, y);
                        maxX = qMax(maxX, x);
                        maxY = qMax(maxY, y);
                    }
                }
            }
            std::printf("  [dbg] cover ink bbox x=%d..%d y=%d..%d (page %dx%d)\n",
                        minX, maxX, minY, maxY, p1.width(), p1.height());
            const bool inkPresent = maxX >= minX && maxY >= minY;
            CHECK("cover ink present", inkPresent);
            if (inkPresent) {
                // Full-bleed contain: the image must span nearly the whole
                // page height (its limiting axis) with only small centering
                // margins. 450x680 in 420x595 -> ~394x595, ~13px sides, ~0px
                // top/bottom; tolerate rounding.
                const int w = maxX - minX + 1;
                const int h = maxY - minY + 1;
                std::printf("  [dbg] cover ink size %dx%d\n", w, h);
                CHECK("cover spans most of the page (full-bleed)",
                      w > p1.width() * 4 / 5 && h > p1.height() * 4 / 5);
            }
        }

        CHECK("page 2 still has copyright text", e.pageText(2).hasText);
        const QImage p2 = e.renderPage(2, 1.0f, 1.0f, 0);
        CHECK("page 2 body renders", !p2.isNull());
        {
            QImage pd = e.renderPage(3, 1.0f, 1.0f, 0);   // Contents page (real body text)
            if (!pd.isNull()) {
                int bminX = pd.width(), bminY = pd.height(), bmaxX = -1, bmaxY = -1;
                QImage bprobe = pd;
                if (bprobe.format() != QImage::Format_RGB888)
                    bprobe = bprobe.convertToFormat(QImage::Format_RGB888);
                for (int y = 0; y < bprobe.height(); ++y) {
                    const uchar* line = bprobe.constScanLine(y);
                    for (int x = 0; x < bprobe.width(); ++x) {
                        const uchar* p = line + x * 3;
                        if (p[0] < 250 || p[1] < 250 || p[2] < 250) {
                            bminX = qMin(bminX, x); bminY = qMin(bminY, y);
                            bmaxX = qMax(bmaxX, x); bmaxY = qMax(bmaxY, y);
                        }
                    }
                }
                if (bmaxX >= bminX && bmaxY >= bminY)
                    std::printf("  [dbg] body p3(Contents) ink x=%d..%d y=%d..%d margins L=%d T=%d R=%d B=%d\n",
                                bminX, bmaxX, bminY, bmaxY,
                                bminX, bminY,
                                pd.width() - 1 - bmaxX, pd.height() - 1 - bmaxY);
            }
        }

        // Whole-document search finds body text, not the cover page.
        const bool supports = e.supportsSearch();
        std::printf("  [dbg] supportsSearch=%d\n", supports ? 1 : 0);
        if (supports) {
            const auto hits = e.searchText(2, QStringLiteral("Copyright"),
                                           /*matchCase=*/false);
            std::printf("  [dbg] page-2 'Copyright' hits=%lld\n",
                        static_cast<long long>(hits.size()));
            CHECK("search hits on body page 2", !hits.isEmpty());
        }
        CHECK("outline targets account for cover page",
              [&] {
                  const auto ol = e.outline();
                  for (const auto& item : ol)
                      if (item.pageNo < 1 || item.pageNo > pages) return false;
                  return true;
              }());
    }

    // ---- 6.3: corrupted cover record -> fallback to plain MOBI open (30 pages)
    {
        MuPdfEngine e;
        CHECK("open corrupted-copy MOBI", e.open(corrupt));
        if (e.isOpen()) {
            const int pages = e.pageCount();
            std::printf("  [dbg] fallback pageCount=%d (expected 23)\n", pages);
            CHECK("fallback stays at 23 body pages", pages == 23);
            for (int pg = 1; pg <= pages; ++pg) {
                const QString t = e.extractText(pg).simplified();
                const PageInfo d = e.pageDimensions(pg);
                std::printf("  [dbg] fb%02d [%dx%d]: %.80s\n", pg, d.width, d.height,
                            t.left(80).toUtf8().constData());
                if (pg == 2 || pg == 3)
                    std::printf("  [dbg] fb%02d FULL: %.600s\n", pg,
                                e.extractText(pg).simplified().toUtf8().constData());
            }
            const QString allText = [&] {
                QString out;
                for (int pg = 1; pg <= pages; ++pg)
                    out += e.extractText(pg);
                return out;
            }();
            std::printf("  [dbg] fb contains 'for Sharon'=%d contains 'Plato'=%d\n",
                        allText.contains(QStringLiteral("for Sharon")) ? 1 : 0,
                        allText.contains(QStringLiteral("Plato")) ? 1 : 0);
            const QImage p1 = e.renderPage(1, 1.0f, 1.0f, 0);
            CHECK("fallback page 1 renders", !p1.isNull());
        }
    }

    QFile::remove(corrupt);

    std::printf("\n%s (%d failure(s))\n",
                g_failures ? "RESULT: FAIL" : "RESULT: ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}