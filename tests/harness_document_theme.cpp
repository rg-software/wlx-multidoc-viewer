// Headless unit checks for the shared document-body transform (theme-document-body).
// Pure helper coverage (monochrome detection, luminance duotone, light identity).
// The palette-driven path runs in a child process: the parent writes a dark INI,
// re-execs itself, and the child reads it at startup (the plugin config is
// resolved once, and a stale INI could otherwise be cached before main).

#include "documenttheme.h"

#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QString>

#include <cstdio>

namespace {

int g_fail = 0;

void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        ++g_fail;
}

// White page with a black "text" band -> monochrome.
QImage monochromePage() {
    QImage img(64, 64, QImage::Format_RGB888);
    img.fill(Qt::white);
    for (int y = 20; y < 30; ++y)
        for (int x = 10; x < 54; ++x)
            img.setPixel(x, y, qRgb(0, 0, 0));
    return img;
}

// Same page plus a saturated red patch covering > 0.5% of pixels -> colored.
QImage coloredPage() {
    QImage img = monochromePage();
    for (int y = 40; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            img.setPixel(x, y, qRgb(220, 20, 20));
    return img;
}

uint32_t pixelAt(const QImage& img, int x, int y) {
    const QColor c = img.pixelColor(x, y);
    return (static_cast<uint32_t>(c.red()) << 16) | (static_cast<uint32_t>(c.green()) << 8) |
           static_cast<uint32_t>(c.blue());
}

QString iniPath() {
    return QCoreApplication::applicationDirPath() + QStringLiteral("/multidocviewer.ini");
}

// Runs in the child: the dark INI is already on disk.
int runDarkScenario() {
    viewer_settings::setHostDark(false); // explicit dark wins

    QImage mono = monochromePage();
    documenttheme::applyToRenderedPage(mono);
    check(pixelAt(mono, 0, 0) == 0x262626u, "dark theme recolors a monochrome page background");
    check(pixelAt(mono, 30, 25) == 0xE0E0E0u, "dark theme recolors a monochrome page text");

    QImage colored = coloredPage();
    documenttheme::applyToRenderedPage(colored);
    check(pixelAt(colored, 32, 50) == 0xDC1414u, "colored page is left unchanged");

    const std::string css = documenttheme::reflowCss();
    check(css.find("#262626") != std::string::npos, "reflow CSS carries the dark document background");
    check(css.find("#E0E0E0") != std::string::npos, "reflow CSS carries the dark document text");
    return g_fail == 0 ? 0 : 1;
}

int runAll() {
    // Pure helpers (no palette / no INI).
    check(documenttheme::isMonochrome(monochromePage()), "monochrome page detected");
    check(!documenttheme::isMonochrome(coloredPage()), "colored page detected");
    check(!documenttheme::isMonochrome(QImage()), "null image is not monochrome");

    {
        QImage img = monochromePage();
        documenttheme::applyDuotone(img, 0xE0E0E0u, 0x262626u);
        check(pixelAt(img, 0, 0) == 0x262626u, "white maps to the dark background");
        check(pixelAt(img, 30, 25) == 0xE0E0E0u, "black maps to the dark text color");
    }
    {
        QImage img = monochromePage();
        documenttheme::applyDuotone(img, 0x000000u, 0xFFFFFFu);
        check(pixelAt(img, 0, 0) == 0xFFFFFFu, "light theme keeps white");
        check(pixelAt(img, 30, 25) == 0x000000u, "light theme keeps black");
    }
    {
        QImage img(4, 4, QImage::Format_RGB888);
        img.fill(QColor(128, 128, 128));
        documenttheme::applyDuotone(img, 0xE0E0E0u, 0x262626u);
        const int r = static_cast<int>((pixelAt(img, 1, 1) >> 16) & 0xFF);
        check(r > 0x26 && r < 0xE0, "mid-gray is interpolated between text and bg");
    }

    // Palette-driven path in a child so the dark INI is present at startup.
    const QString ini = iniPath();
    const QString bak = ini + QStringLiteral(".doctheme.bak");
    const bool hadIni = QFile::exists(ini);
    if (hadIni)
        QFile::rename(ini, bak);
    {
        QFile f(ini);
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.write(QByteArray("[Viewer]\nTheme = dark\n"));
    }
    const int child = QProcess::execute(QCoreApplication::applicationFilePath(),
                                        { QStringLiteral("dark") });
    if (child != 0)
        g_fail += 1;
    QFile::remove(ini);
    if (hadIni)
        QFile::rename(bak, ini);

    std::printf("\nRESULT: %s (%d failure(s))\n", g_fail == 0 ? "ALL PASS" : "FAIL", g_fail);
    return g_fail == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc > 1 && QByteArray(argv[1]) == "dark")
        return runDarkScenario();
    return runAll();
}
