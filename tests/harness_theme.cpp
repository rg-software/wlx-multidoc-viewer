// Headless theme-resolution harness (viewer-theme).
//
// Verifies the shared palette model in viewer_settings.h:
//   * [Viewer] Theme = light | dark | auto (case-insensitive, malformed -> auto)
//   * auto follows the recorded host mode (setHostDark)
//   * [Theme:light] / [Theme:dark] sections supply the palette values
//     (falling back to the built-in defaults for missing keys)
//
// Both PluginConfig and activePalette() cache on first use, so each scenario
// runs in its own process: the no-argument run performs the INI-independent
// checks and then spawns itself once per scenario (writing a temporary
// multidocviewer.ini next to the executable for the duration).

#include "viewer_settings.h"

#include <QCoreApplication>
#include <QFile>
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

QString iniPath() {
    return QCoreApplication::applicationDirPath() + QStringLiteral("/multidocviewer.ini");
}

bool paletteEquals(const viewer_settings::Palette& a, const viewer_settings::Palette& b) {
    return a.pageBg == b.pageBg && a.sidebarBg == b.sidebarBg && a.toolbarBg == b.toolbarBg &&
           a.toolbarCheckedTint == b.toolbarCheckedTint &&
           a.toolbarCheckedRing == b.toolbarCheckedRing && a.glyph == b.glyph &&
           a.treeText == b.treeText && a.editBg == b.editBg && a.editText == b.editText &&
           a.selectionFill == b.selectionFill && a.searchActiveFill == b.searchActiveFill &&
           a.searchActivePen == b.searchActivePen;
}

void writeIni(const QString& text) {
    QFile f(iniPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        std::printf("FAIL could not write %s\n", iniPath().toUtf8().constData());
    f.write(text.toUtf8());
}

int runScenario(const QString& s) {
    if (s == QStringLiteral("light")) {
        writeIni(QStringLiteral("[Viewer]\nTheme = light\n"));
        viewer_settings::setHostDark(false);
        check(paletteEquals(viewer_settings::activePalette(), viewer_settings::kLightPalette),
              "Theme=light -> light palette");
    } else if (s == QStringLiteral("dark")) {
        writeIni(QStringLiteral("[Viewer]\nTheme = dark\n"));
        viewer_settings::setHostDark(false);
        check(paletteEquals(viewer_settings::activePalette(), viewer_settings::kDarkPalette),
              "Theme=dark -> dark palette");
    } else if (s == QStringLiteral("auto-dark")) {
        writeIni(QStringLiteral("[Viewer]\n"));
        viewer_settings::setHostDark(true);
        check(paletteEquals(viewer_settings::activePalette(), viewer_settings::kDarkPalette),
              "auto + dark host -> dark palette");
    } else if (s == QStringLiteral("auto-light")) {
        writeIni(QStringLiteral("[Viewer]\n"));
        viewer_settings::setHostDark(false);
        check(paletteEquals(viewer_settings::activePalette(), viewer_settings::kLightPalette),
              "auto + light host -> light palette");
    } else if (s == QStringLiteral("malformed")) {
        writeIni(QStringLiteral("[Viewer]\nTheme = pink\n"));
        viewer_settings::setHostDark(false);
        check(paletteEquals(viewer_settings::activePalette(), viewer_settings::kLightPalette),
              "malformed Theme -> auto (light host)");
    } else if (s == QStringLiteral("theme-section")) {
        writeIni(QStringLiteral("[Viewer]\nTheme = dark\n"
                                "[Theme:dark]\n"
                                "PageBackground = #102030\n"
                                "SidebarBackground = #405060\n"
                                "SelectionFill = #11223344\n"));
        viewer_settings::setHostDark(false);
        const viewer_settings::Palette& p = viewer_settings::activePalette();
        check(p.pageBg == 0x102030u, "Theme:dark PageBackground is applied");
        check(p.sidebarBg == 0x405060u, "Theme:dark SidebarBackground is applied");
        check(p.selectionFill == 0x44112233u, "8-digit #RRGGBBAA repacks to 0xAARRGGBB");
        check(p.toolbarBg == viewer_settings::kDarkPalette.toolbarBg,
              "a key absent from the section falls back to the built-in default");
    } else if (s == QStringLiteral("section-select")) {
        writeIni(QStringLiteral("[Theme:light]\nPageBackground = #ABCDEF\n"
                                "[Theme:dark]\nPageBackground = #123456\n"
                                "[Viewer]\nTheme = light\n"));
        viewer_settings::setHostDark(false);
        check(viewer_settings::activePalette().pageBg == 0xABCDEFu,
              "Theme=light reads [Theme:light], not [Theme:dark]");
    } else if (s == QStringLiteral("precedence")) {
        writeIni(QStringLiteral("[Viewer]\nTheme = dark\n"));
        viewer_settings::setHostDark(false); // host says light; explicit Theme must win
        check(viewer_settings::activeTheme() == viewer_settings::Theme::Dark,
              "explicit Theme wins over the host mode");
    } else {
        std::printf("FAIL unknown scenario: %s\n", s.toUtf8().constData());
        return 2;
    }

    QFile::remove(iniPath());
    return g_fail == 0 ? 0 : 1;
}

int runAll() {
    using viewer_settings::Theme;
    check(viewer_settings::parseTheme("light") == Theme::Light, "parseTheme(light)");
    check(viewer_settings::parseTheme("DARK") == Theme::Dark, "parseTheme(DARK) is case-insensitive");
    check(viewer_settings::parseTheme("auto") == Theme::Auto, "parseTheme(auto)");
    check(viewer_settings::parseTheme("pink") == Theme::Auto, "parseTheme(pink) -> auto");

    // Keep any pre-existing INI out of the way while the child scenarios run.
    const QString bak = iniPath() + QStringLiteral(".harnessbak");
    const bool hadIni = QFile::exists(iniPath());
    if (hadIni)
        QFile::rename(iniPath(), bak);

    int rc = 0;
    const char* scenarios[] = { "light",     "dark",     "auto-dark",    "auto-light",
                                "malformed", "theme-section", "section-select", "precedence" };
    for (const char* s : scenarios) {
        const int child = QProcess::execute(QCoreApplication::applicationFilePath(),
                                            { QString::fromLatin1(s) });
        if (child != 0)
            rc = 1;
    }

    if (hadIni)
        QFile::rename(bak, iniPath());
    return rc;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc > 1)
        return runScenario(QString::fromLocal8Bit(argv[1]));
    return runAll();
}
