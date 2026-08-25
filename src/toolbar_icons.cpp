#include "toolbar_icons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QVector>

#include <QFontDatabase>
#include <QGuiApplication>
#include <QStringList>

namespace toolbar {
namespace {

constexpr QRgb kGlyphColor = 0xFF4A4A4A; // softer dark gray: visible but lighter

// Material Symbols Outlined codepoints for the embedded (subset, weight-300)
// font. All values verified against the font's cmap.
char32_t laCodepoint(toolbar::Icon icon) {
    using toolbar::Icon;
    switch (icon) {
    case Icon::Prev:
    case Icon::FindPrev:      return 0xe5e0; // arrow_back_ios
    case Icon::Next:
    case Icon::FindNext:      return 0xe5e1; // arrow_forward_ios
    case Icon::SidebarToggle: return 0xe5d2; // menu
    case Icon::Print:         return 0xe8ad; // print
    case Icon::ModePaged:     return 0xe7f9; // pages
    case Icon::ModeContinuous: return 0xe8e9; // view_agenda
    case Icon::FitManual:     return 0xf4c2; // view_real_size
    case Icon::FitPage:       return 0xea10; // fit_screen
    case Icon::FitWidth:      return 0xf8f5; // width_full
    case Icon::RotateLeft:    return 0xe419; // rotate_left
    case Icon::RotateRight:   return 0xe41a; // rotate_right
    case Icon::ZoomOut:       return 0xe900; // zoom_out
    case Icon::ZoomIn:        return 0xe8ff; // zoom_in
    case Icon::Find:          return 0xe8b6; // search
    case Icon::MatchCase:     return 0xf6f1; // match_case
    case Icon::MatchCaseOff:  return 0xf36f; // match_case_off
    case Icon::Copy:          return 0xe14d; // content_copy
    default: return 0;
    }
}

bool hasInk(const QImage& img) {
    if (img.isNull())
        return false;
    for (int y = 0; y < img.height(); ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if (qAlpha(line[x]) > 0)
                return true;
        }
    }
    return false;
}

// Ensure a QGuiApplication exists so QFontDatabase works. On Windows the plugin
// DLL has no app; QGuiApplication is Qt-Gui only (no widgets) and is required
// for font loading. Calling it once from a DLL is safe.
void ensureGuiApp() {
    static bool once = false;
    if (once)
        return;
    once = true;
    if (!QGuiApplication::instance()) {
        static int argc = 1;
        static char arg0[] = "wlx-multidoc-viewer";
        static char* argv[] = { arg0, nullptr };
        new QGuiApplication(argc, argv);
    }
}

// Loads the embedded font once and returns its font ID.
int laFontId() {
    static int fontId = -2; // -2 = not attempted
    if (fontId == -2) {
        ensureGuiApp();
        fontId = QFontDatabase::addApplicationFontFromData(
            QByteArray::fromRawData(reinterpret_cast<const char*>(kMaterialSymbolsTtf),
                                    static_cast<int>(kMaterialSymbolsTtfSize)));
    }
    return fontId;
}

// Render the glyph with QPainter/FreeType (antialiased filled outlines — GDI
// could not rasterize the compound rotate glyphs from this subset). One code
// path for both platforms.
QImage rasterizeGlyph(char32_t cp, int px) {
    if (cp == 0)
        return {};
    const int fontId = laFontId();
    if (fontId < 0)
        return {};
    const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
    if (families.isEmpty())
        return {};

    QImage img(px, px, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    QFont font(families.first());
    font.setPixelSize(px);
    font.setWeight(QFont::Light); // light stroke matches the outline set
    p.setFont(font);
    p.setPen(QColor::fromRgba(kGlyphColor));
    p.drawText(QRect(0, 0, px, px), Qt::AlignCenter,
               QString::fromUcs4(reinterpret_cast<const char32_t*>(&cp), 1));
    p.end();
    return img;
}

// ---------------------------------------------------------------------------
// Programmatic vector fallback (unchanged shapes) used when the glyph is not
// available in the loaded font.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Programmatic vector fallback (unchanged shapes) used when the glyph is not
// available in the loaded font.
// ---------------------------------------------------------------------------

class Glyph {
public:
    Glyph(QPainter& p, int size, float inset)
        : m_p(p), m_s(static_cast<float>(size) - 2.0f * inset), m_off(inset)
    {
        QPen pen(QColor::fromRgba(kGlyphColor));
        pen.setWidthF(qMax(1.0f, m_s / 9.0f));
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        m_p.setPen(pen);
        m_p.setBrush(Qt::NoBrush);
        m_p.setRenderHint(QPainter::Antialiasing, true);
    }

    QPointF pt(float u, float v) const {
        return QPointF(m_off + u * m_s, m_off + (1.0f - v) * m_s);
    }

    void line(float u0, float v0, float u1, float v1) { m_p.drawLine(pt(u0, v0), pt(u1, v1)); }
    void poly(QVector<QPointF> pts) {
        if (pts.isEmpty())
            return;
        QPainterPath path(pts.first());
        for (int i = 1; i < pts.size(); ++i)
            path.lineTo(pts[i]);
        m_p.drawPath(path);
    }
    void rect(float u0, float v0, float u1, float v1) {
        m_p.drawRect(QRectF(pt(u0, v0), pt(u1, v1)).normalized());
    }
    void ellipse(float cu, float cv, float ru, float rv) {
        m_p.drawEllipse(pt(cu, cv), ru * m_s, rv * m_s);
    }
    void bar(float u0, float v0, float u1, float v1) {
        QPen old = m_p.pen();
        m_p.setPen(Qt::NoPen);
        m_p.setBrush(QColor::fromRgba(kGlyphColor));
        m_p.drawRect(QRectF(pt(u0, v0), pt(u1, v1)).normalized());
        m_p.setPen(old);
        m_p.setBrush(Qt::NoBrush);
    }

private:
    QPainter& m_p;
    float m_s;
    float m_off;
};

void drawMagnifier(Glyph& g, QChar op) {
    g.ellipse(0.45f, 0.48f, 0.27f, 0.27f);
    g.line(0.64f, 0.68f, 0.84f, 0.86f);
    if (op == QLatin1Char('+'))
        g.line(0.45f, 0.38f, 0.45f, 0.58f);
    else if (op == QLatin1Char('-'))
        g.line(0.35f, 0.48f, 0.55f, 0.48f);
}

void drawRotate(Glyph& g, bool clockwise) {
    QPainterPath arc;
    const float mid = clockwise ? 0.72f : 0.28f;
    arc.moveTo(g.pt(mid, 0.78f));
    arc.quadTo(g.pt(0.5f, 1.08f), g.pt(0.5f, 0.70f));
    arc.quadTo(g.pt(0.5f, 0.22f), g.pt(mid, 0.22f));
    g.poly({QPointF(g.pt(mid, 0.22f).x() + (clockwise ? -0.06f : 0.06f), g.pt(mid, 0.22f).y()),
            QPointF(g.pt(mid, 0.22f).x(), g.pt(mid, 0.22f).y() + (clockwise ? -0.12f : 0.12f)),
            QPointF(g.pt(mid, 0.22f).x() - (clockwise ? 0.06f : 0.06f), g.pt(mid, 0.22f).y())});
}

QImage drawVectorIcon(toolbar::Icon icon, int px) {
    QImage img(px, px, QImage::Format_ARGB32);
    img.fill(Qt::transparent);

    QPainter p(&img);
    Glyph g(p, px, 1.4f);

    switch (icon) {
    case Icon::Prev:
    case Icon::FindPrev:
        g.poly({QPointF(g.pt(0.74f, 0.26f)), QPointF(g.pt(0.30f, 0.50f)), QPointF(g.pt(0.74f, 0.74f))});
        break;
    case Icon::Next:
    case Icon::FindNext:
        g.poly({QPointF(g.pt(0.26f, 0.26f)), QPointF(g.pt(0.70f, 0.50f)), QPointF(g.pt(0.26f, 0.74f))});
        break;
    case Icon::ModePaged:
        g.rect(0.30f, 0.22f, 0.70f, 0.78f);
        break;
    case Icon::ModeContinuous:
        g.rect(0.28f, 0.64f, 0.72f, 0.88f);
        g.rect(0.20f, 0.40f, 0.64f, 0.74f);
        g.rect(0.34f, 0.14f, 0.80f, 0.50f);
        break;
    case Icon::FitManual:
        g.rect(0.30f, 0.30f, 0.70f, 0.70f);
        g.line(0.14f, 0.54f, 0.24f, 0.54f);
        g.line(0.76f, 0.54f, 0.86f, 0.54f);
        g.line(0.50f, 0.14f, 0.50f, 0.24f);
        g.line(0.50f, 0.76f, 0.50f, 0.86f);
        break;
    case Icon::FitPage:
        g.rect(0.24f, 0.18f, 0.76f, 0.82f);
        g.poly({QPointF(g.pt(0.30f, 0.24f)), QPointF(g.pt(0.10f, 0.24f)), QPointF(g.pt(0.10f, 0.44f))});
        g.poly({QPointF(g.pt(0.70f, 0.76f)), QPointF(g.pt(0.90f, 0.76f)), QPointF(g.pt(0.90f, 0.56f))});
        break;
    case Icon::FitWidth:
        g.rect(0.22f, 0.32f, 0.78f, 0.68f);
        g.poly({QPointF(g.pt(0.10f, 0.32f)), QPointF(g.pt(0.06f, 0.50f)), QPointF(g.pt(0.10f, 0.68f))});
        g.poly({QPointF(g.pt(0.90f, 0.32f)), QPointF(g.pt(0.94f, 0.50f)), QPointF(g.pt(0.90f, 0.68f))});
        break;
    case Icon::RotateLeft:
        drawRotate(g, false);
        break;
    case Icon::RotateRight:
        drawRotate(g, true);
        break;
    case Icon::ZoomIn:
        drawMagnifier(g, QLatin1Char('+'));
        break;
    case Icon::ZoomOut:
        drawMagnifier(g, QLatin1Char('-'));
        break;
    case Icon::Find:
        drawMagnifier(g, QChar());
        break;
case Icon::MatchCase:
    case Icon::MatchCaseOff:
        g.line(0.18f, 0.28f, 0.32f, 0.28f);
        g.line(0.20f, 0.30f, 0.30f, 0.78f);
        g.line(0.45f, 0.30f, 0.45f, 0.78f);
        g.line(0.56f, 0.30f, 0.80f, 0.78f);
        g.line(0.56f, 0.56f, 0.76f, 0.56f);
        break;
    case Icon::Print: {
        g.bar(0.72f, 0.30f, 0.28f, 0.38f);
        g.bar(0.28f, 0.56f, 0.72f, 0.80f);
        g.bar(0.28f, 0.84f, 0.72f, 0.90f);
        g.rect(0.28f, 0.70f, 0.72f, 0.12f);
        g.rect(0.22f, 0.44f, 0.78f, 0.16f);
        break;
    }
    case Icon::Copy: {
        g.rect(0.34f, 0.20f, 0.80f, 0.66f);
        g.rect(0.20f, 0.34f, 0.66f, 0.80f);
        g.line(0.20f, 0.44f, 0.34f, 0.44f);
        g.line(0.20f, 0.56f, 0.34f, 0.56f);
        break;
    }
    case Icon::SidebarToggle:
        g.line(0.18f, 0.24f, 0.18f, 0.76f);
        g.bar(0.22f, 0.30f, 0.66f, 0.38f);
        g.bar(0.22f, 0.50f, 0.78f, 0.58f);
        g.bar(0.22f, 0.70f, 0.58f, 0.78f);
        break;
    }

    p.end();
    return img;
}

} // namespace

QImage makeIcon(Icon icon, int pixelSize) {
    const int px = qMax(1, pixelSize);
    const char32_t cp = laCodepoint(icon);

    QImage glyph = (cp != 0) ? rasterizeGlyph(cp, px) : QImage();

    // A glyph must actually rasterize ink; otherwise keep the drawn shapes.
    if (hasInk(glyph))
        return glyph;
    return drawVectorIcon(icon, px);
}

} // namespace toolbar

