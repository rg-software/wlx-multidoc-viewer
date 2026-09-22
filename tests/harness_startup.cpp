// Headless startup-view-state harness (issue #11: FR ini settings).
//
// Verifies the [Viewer] PagedMode / FitMode keys in viewer_settings.h:
//   * PagedMode: false = continuous (built-in default), true = paged;
//     accepts the full parseBool vocabulary (true/1/yes/on, false/0/no/off).
//   * FitMode: page | width | manual (case-insensitive); malformed -> page.
//
// The kStart* values are namespace-scope statics that capture the INI at
// process startup — exactly like the real plugin (PluginConfig caches for the
// process lifetime) — so each scenario runs in its own process. The parent
// writes a temporary multidocviewer.ini next to the executable BEFORE spawning
// the child; the child only asserts what its startup statics captured and the
// parent cleans up afterwards.

#include "viewer_settings.h"
#include "viewercontroller.h"

#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QString>

#include <cstdio>
#include <memory>

namespace {

int g_fail = 0;

void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        ++g_fail;
}

// Minimal fixed-size document so the real ViewerController can open and lay
// out headlessly; the reported regression is purely about mode state, so the
// engine itself only needs honest page geometry.
class StubEngine : public DocumentEngine {
public:
    bool open(const QString&) override
    {
        m_open = true;
        return true;
    }
    void close() override { m_open = false; }
    bool isOpen() const override { return m_open; }
    int pageCount() const override { return 3; }
    QImage renderPage(int, float, float, int) override { return {}; }
    QString extractText(int) override { return {}; }
    QString metadata(const QString&) const override { return {}; }
    QVector<OutlineItem> outline() const override { return {}; }
    PageInfo pageDimensions(int) const override
    {
        PageInfo p;
        p.width = 200;
        p.height = 300;
        return p;
    }

private:
    bool m_open = false;
};

// Regression for the reported bug: "[Viewer] PagedMode=true actually does not
// start the viewer in paged mode." Both viewers (Win32/Qt) call closeDocument()
// before the first openDocument(), and closeDocument() used to reset the raw
// ViewerState, silently dropping the mode the constructor seeded from the INI.
// This runs the real controller through that exact call sequence and asserts
// the configured mode survives open, close/reopen (ListLoadNext), and a user
// toggle across a document swap.
void controllerStartupCheck() {
    const QString doc =
        QCoreApplication::applicationDirPath() + QStringLiteral("/stub.pdf");

    // Mirror loadDocument: close (wiping a closed lister's leftovers) -> set
    // engine -> open. The controller's constructor already seeded the INI mode.
    ViewerController controller;
    controller.closeDocument();
    controller.setEngine(std::make_unique<StubEngine>());
    if (controller.openDocument(doc)) {
        check(controller.isPagedMode() == viewer_settings::kStartPagedMode,
              "fresh open honors [Viewer] PagedMode");
        // ListLoadNext reuses the controller: close + reopen must keep the
        // configured mode instead of resetting to the built-in continuous.
        controller.closeDocument();
        controller.openDocument(doc);
        check(controller.isPagedMode() == viewer_settings::kStartPagedMode,
              "close/reopen preserves seeded mode");
        // A live toggle also survives the swap (the constructor comment's
        // promise that a reused controller keeps the user's current choice).
        controller.toggleMode();
        controller.closeDocument();
        controller.openDocument(doc);
        check(controller.isPagedMode() == !viewer_settings::kStartPagedMode,
              "user toggle survives close/reopen");
    } else {
        check(false, "fresh open succeeds");
    }
}

QString iniPath() {
    return QCoreApplication::applicationDirPath() + QStringLiteral("/multidocviewer.ini");
}

bool writeIni(const QString& text) {
    QFile f(iniPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        std::printf("FAIL could not write %s\n", iniPath().toUtf8().constData());
        return false;
    }
    f.write(text.toUtf8());
    return true;
}

QString scenarioIniText(const QString& s) {
    if (s == QStringLiteral("default"))
        return QStringLiteral("[Viewer]\n");
    if (s == QStringLiteral("paged-manual"))
        return QStringLiteral("[Viewer]\nPagedMode = true\nFitMode = manual\n");
    if (s == QStringLiteral("continuous-width"))
        return QStringLiteral("[Viewer]\nPagedMode = false\nFitMode = width\n");
    if (s == QStringLiteral("case-insensitive"))
        return QStringLiteral("[Viewer]\nPagedMode = ON\nFitMode = WIDTH\n");
    if (s == QStringLiteral("malformed"))
        return QStringLiteral("[Viewer]\nPagedMode = maybe\nFitMode = banana\n");
    return {};
}

int runScenario(const QString& s) {
    if (s == QStringLiteral("default")) {
        check(!viewer_settings::kStartPagedMode, "absent PagedMode -> continuous");
        check(viewer_settings::kStartFitMode == viewer_settings::StartFitMode::FitToPage,
              "absent FitMode -> page");
    } else if (s == QStringLiteral("paged-manual")) {
        check(viewer_settings::kStartPagedMode, "PagedMode=true -> paged");
        check(viewer_settings::kStartFitMode == viewer_settings::StartFitMode::Manual,
              "FitMode=manual -> manual");
    } else if (s == QStringLiteral("continuous-width")) {
        check(!viewer_settings::kStartPagedMode, "PagedMode=false -> continuous");
        check(viewer_settings::kStartFitMode == viewer_settings::StartFitMode::FitToWidth,
              "FitMode=width -> width");
    } else if (s == QStringLiteral("case-insensitive")) {
        check(viewer_settings::kStartPagedMode, "PagedMode=ON -> paged");
        check(viewer_settings::kStartFitMode == viewer_settings::StartFitMode::FitToWidth,
              "FitMode=WIDTH is case-insensitive");
    } else if (s == QStringLiteral("malformed")) {
        check(!viewer_settings::kStartPagedMode, "malformed PagedMode -> continuous");
        check(viewer_settings::kStartFitMode == viewer_settings::StartFitMode::FitToPage,
              "malformed FitMode -> page");
    } else {
        std::printf("FAIL unknown scenario: %s\n", s.toUtf8().constData());
        return 2;
    }

    // The INI statics above are the settings-layer half of issue #11; the
    // controller half (does a fresh window actually OPEN in that mode?) must
    // be checked against the exact same INI, so it runs inside each child.
    controllerStartupCheck();

    return g_fail == 0 ? 0 : 1;
}

int runAll() {
    // Keep any pre-existing INI out of the way while the child scenarios run.
    const QString bak = iniPath() + QStringLiteral(".harnessbak");
    const bool hadIni = QFile::exists(iniPath());
    if (hadIni)
        QFile::rename(iniPath(), bak);

    int rc = 0;
    const char* scenarios[] = { "default", "paged-manual", "continuous-width",
                                "case-insensitive", "malformed" };
    for (const char* s : scenarios) {
        if (!writeIni(scenarioIniText(QString::fromLatin1(s))))
            continue;
        const int child = QProcess::execute(QCoreApplication::applicationFilePath(),
                                            { QString::fromLatin1(s) });
        QFile::remove(iniPath());
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