// Headless plugin-config layering harness (preserve-ini-on-update follow-up).
//
// Verifies PluginConfig::get() resolves settings in layers:
//   1. multidocviewer.ini.sample (shipped template) as the base layer;
//   2. multidocviewer.ini (user file) overlaid key by key on top;
//   3. hardcoded read-site defaults when a key is in neither file.
//
// PluginConfig caches for the process lifetime, so each scenario runs in its
// own child process. The parent stages scratch sample/user files next to the
// executable (backing up and restoring whatever was there), spawns the child
// with the scenario name, and cleans up afterwards.

#include "pluginconfig.h"

#include <QCoreApplication>
#include <QDir>
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

QString modDir() {
    return QDir::cleanPath(QString::fromStdString(PluginConfig::modulePath()));
}

QString samplePath() {
    return modDir() + QStringLiteral("/multidocviewer.ini.sample");
}

QString iniPath() {
    return modDir() + QStringLiteral("/multidocviewer.ini");
}

bool writeFile(const QString& path, const char* text) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        std::printf("FAIL could not write %s\n", path.toUtf8().constData());
        return false;
    }
    f.write(text);
    return true;
}

// Scratch template with values distinct from every built-in default, so a
// fallback to the sample layer is observable (and a leak of the real staged
// template would fail loudly).
constexpr const char* kSampleText =
    "[Viewer]\n"
    "SidebarWidth = 111\n"
    "Theme = dark\n"
    "PagedMode = true\n"
    "[Theme:dark]\n"
    "PageBackground = #111111\n";

// Partial user file: overrides one key, leaves the rest to the sample.
constexpr const char* kUserText =
    "[Viewer]\n"
    "SidebarWidth = 222\n";

std::string getStr(const char* section, const char* key) {
    return PluginConfig::get().get(section).get(key);
}

int runChild(const QString& scenario) {
    if (scenario == QStringLiteral("sample-only")) {
        check(getStr("Viewer", "SidebarWidth") == "111", "sample supplies SidebarWidth");
        check(getStr("Viewer", "Theme") == "dark", "sample supplies Theme");
        check(getStr("Viewer", "PagedMode") == "true", "sample supplies PagedMode");
        check(getStr("Theme:dark", "PageBackground") == "#111111",
              "sample supplies theme slot");
        check(getStr("Viewer", "FontSize").empty(), "key in neither file stays empty");
    } else if (scenario == QStringLiteral("overlay")) {
        check(getStr("Viewer", "SidebarWidth") == "222", "user key wins over sample");
        check(getStr("Viewer", "Theme") == "dark", "non-overridden key falls to sample");
        check(getStr("Theme:dark", "PageBackground") == "#111111",
              "untouched section falls to sample");
        check(getStr("Viewer", "FontSize").empty(), "key in neither file stays empty");
    } else if (scenario == QStringLiteral("neither")) {
        check(getStr("Viewer", "SidebarWidth").empty(), "no files: SidebarWidth empty");
        check(getStr("Viewer", "Theme").empty(), "no files: Theme empty");
        check(getStr("Theme:dark", "PageBackground").empty(), "no files: slot empty");
    } else if (scenario == QStringLiteral("user-only")) {
        check(getStr("Viewer", "SidebarWidth") == "222", "user-only: key honored");
        check(getStr("Viewer", "Theme").empty(), "user-only: missing key stays empty");
    } else {
        std::printf("FAIL unknown scenario %s\n", scenario.toUtf8().constData());
        return 2;
    }
    return g_fail == 0 ? 0 : 1;
}

int runScenario(QCoreApplication& app, const QString& name, bool wantSample, bool wantUser) {
    // Back up whatever the build tree staged next to the exe, then stage the
    // scratch files for this scenario.
    const QString sampleBak = samplePath() + QStringLiteral(".harness-bak");
    const QString iniBak = iniPath() + QStringLiteral(".harness-bak");
    QFile::remove(sampleBak);
    QFile::remove(iniBak);
    if (QFile::exists(samplePath()) && !QFile::rename(samplePath(), sampleBak))
        return 2;
    if (QFile::exists(iniPath()) && !QFile::rename(iniPath(), iniBak))
        return 2;
    if (wantSample && !writeFile(samplePath(), kSampleText))
        return 2;
    if (wantUser && !writeFile(iniPath(), kUserText))
        return 2;

    QProcess child;
    child.setProgram(QCoreApplication::applicationFilePath());
    child.setArguments({name});
    child.start();
    int rc = 2;
    if (child.waitForFinished(30000)) {
        std::printf("%s", child.readAllStandardOutput().constData());
        rc = child.exitCode();
    } else {
        std::printf("FAIL scenario %s timed out\n", name.toUtf8().constData());
        child.kill();
    }

    QFile::remove(samplePath());
    QFile::remove(iniPath());
    if (QFile::exists(sampleBak) && !QFile::rename(sampleBak, samplePath()))
        rc = 2;
    if (QFile::exists(iniBak) && !QFile::rename(iniBak, iniPath()))
        rc = 2;
    if (rc != 0)
        ++g_fail;
    return rc;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() == 2)
        return runChild(args.at(1));

    int rc = 0;
    rc |= runScenario(app, QStringLiteral("sample-only"), true, false);
    rc |= runScenario(app, QStringLiteral("overlay"), true, true);
    rc |= runScenario(app, QStringLiteral("neither"), false, false);
    rc |= runScenario(app, QStringLiteral("user-only"), false, true);
    if (g_fail == 0)
        std::printf("RESULT: PASS\n");
    else
        std::printf("RESULT: FAIL (%d failure(s))\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
