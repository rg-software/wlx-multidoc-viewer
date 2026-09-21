#include "favorites.h"

#include "pluginconfig.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <QTimer>
#endif

namespace {

constexpr int kFormatVersion = 1;
constexpr const char* kFileName = "multidocviewer-favorites.json";
constexpr int kDebounceMs = 300;

QString& locationOverride() {
    static QString dir;
    return dir;
}

std::unique_ptr<FavoritesStore>& storeHolder() {
    static std::unique_ptr<FavoritesStore> store;
    return store;
}

#ifdef _WIN32
constexpr UINT_PTR kDebounceTimerId = 1;
HWND g_timerWnd = nullptr;

LRESULT CALLBACK favoritesTimerProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_TIMER && wp == kDebounceTimerId) {
        FavoritesStore::get().flush();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void scheduleDebouncedSave() {
    if (!g_timerWnd) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = favoritesTimerProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"WLXDocFavoritesTimer";
        RegisterClassExW(&wc);
        g_timerWnd = CreateWindowExW(0, L"WLXDocFavoritesTimer", L"", WS_POPUP,
                                     0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                     wc.hInstance, nullptr);
    }
    if (g_timerWnd)
        SetTimer(g_timerWnd, kDebounceTimerId, kDebounceMs, nullptr);
}
#else
QTimer* debounceTimer() {
    static QTimer* timer = [] {
        auto* t = new QTimer();
        t->setSingleShot(true);
        QObject::connect(t, &QTimer::timeout,
                         [] { FavoritesStore::get().flush(); });
        return t;
    }();
    return timer;
}

void scheduleDebouncedSave() {
    debounceTimer()->start(kDebounceMs);
}
#endif

} // namespace

FavoritesStore* FavoritesStore::createInstance() {
    return new FavoritesStore();
}

FavoritesStore& FavoritesStore::get() {
    if (!storeHolder())
        storeHolder().reset(createInstance());
    return *storeHolder();
}

void FavoritesStore::resetForTesting() {
    storeHolder().reset();
}

void FavoritesStore::setLocationOverrideForTesting(const QString& dir) {
    locationOverride() = QDir::cleanPath(dir);
}

FavoritesStore::FavoritesStore() {
    m_dir = resolveDir();
    m_path = QDir(m_dir).filePath(QString::fromLatin1(kFileName));
    QFile f(m_path);
    if (f.open(QIODevice::ReadOnly))
        loadFrom(f.readAll());
    m_lastSavedJson = serialize();
}

QVector<FavoriteEntry> FavoritesStore::favoritesFor(const QString& path) const {
    const QString key = canonicalKey(path);
    if (key.isEmpty())
        return {};
    return m_favorites.value(key);
}

bool FavoritesStore::isFavorite(const QString& path, int page) const {
    const QString key = canonicalKey(path);
    if (key.isEmpty() || page < 1)
        return false;
    const QVector<FavoriteEntry> entries = m_favorites.value(key);
    for (const FavoriteEntry& e : entries)
        if (e.page == page)
            return true;
    return false;
}

void FavoritesStore::toggle(const QString& path, int page, const QString& label) {
    const QString key = canonicalKey(path);
    if (key.isEmpty() || page < 1)
        return;
    QVector<FavoriteEntry> entries = m_favorites.value(key);
    auto it = std::find_if(entries.begin(), entries.end(),
                           [page](const FavoriteEntry& e) { return e.page == page; });
    if (it != entries.end()) {
        entries.erase(it);
    } else {
        FavoriteEntry e;
        e.page = page;
        e.label = label;
        entries.append(e);
        std::sort(entries.begin(), entries.end(),
                  [](const FavoriteEntry& a, const FavoriteEntry& b) {
                      return a.page < b.page;
                  });
    }
    if (entries.isEmpty())
        m_favorites.remove(key);
    else
        m_favorites.insert(key, entries);
    m_dirty = true;
    scheduleSave();
    notifyChanged();
}

void FavoritesStore::flush() {
    saveNow();
}

QString FavoritesStore::canonicalKey(const QString& path) const {
    if (path.isEmpty())
        return {};
    QString key = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
#ifdef _WIN32
    key = key.toLower();
#endif
    return key;
}

QString FavoritesStore::resolveDir() {
    if (!locationOverride().isEmpty())
        return locationOverride();

    QStringList candidates;
    const std::string modulePath = PluginConfig::modulePath();
    if (!modulePath.empty())
        candidates.append(QString::fromUtf8(modulePath.c_str()));
    const QString configDir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!configDir.isEmpty())
        candidates.append(configDir);
    const QString tempDir =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (!tempDir.isEmpty())
        candidates.append(tempDir);

    for (const QString& dir : candidates) {
        if (dir.isEmpty())
            continue;
        QDir d(dir);
        if (!d.exists())
            d.mkpath(QString());
        // Probe writability with an actual create/write so read-only locations
        // (Program Files, plugin dirs) fall through to the next candidate.
        const QString probePath = d.filePath(QStringLiteral(".multidocviewer-write-probe"));
        QSaveFile probe(probePath);
        if (probe.open(QIODevice::WriteOnly)) {
            probe.cancelWriting();
            return dir;
        }
    }
    return {};
}

QByteArray FavoritesStore::serialize() const {
    QJsonObject root;
    root.insert(QStringLiteral("version"), kFormatVersion);
    QJsonArray favs;
    QStringList keys = m_favorites.keys();
    std::sort(keys.begin(), keys.end());
    for (const QString& key : keys) {
        const QVector<FavoriteEntry> entries = m_favorites.value(key);
        if (entries.isEmpty())
            continue;
        QJsonObject fav;
        fav.insert(QStringLiteral("path"), key);
        QJsonArray items;
        for (const FavoriteEntry& e : entries) {
            QJsonObject it;
            it.insert(QStringLiteral("page"), e.page);
            if (!e.label.isEmpty())
                it.insert(QStringLiteral("label"), e.label);
            items.append(it);
        }
        fav.insert(QStringLiteral("items"), items);
        favs.append(fav);
    }
    root.insert(QStringLiteral("favorites"), favs);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

void FavoritesStore::loadFrom(const QByteArray& json) {
    m_favorites.clear();
    if (json.isEmpty())
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject())
        return;
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("version")).toInt() != kFormatVersion)
        return;
    const QJsonArray favs = root.value(QStringLiteral("favorites")).toArray();
    for (const QJsonValue& v : favs) {
        if (!v.isObject())
            continue;
        const QJsonObject fav = v.toObject();
        const QString path = fav.value(QStringLiteral("path")).toString();
        if (path.isEmpty())
            continue;
        const QJsonArray items = fav.value(QStringLiteral("items")).toArray();
        QVector<FavoriteEntry> entries;
        for (const QJsonValue& iv : items) {
            if (!iv.isObject())
                continue;
            const QJsonObject it = iv.toObject();
            const int page = it.value(QStringLiteral("page")).toInt(0);
            if (page < 1)
                continue;
            FavoriteEntry e;
            e.page = page;
            e.label = it.value(QStringLiteral("label")).toString();
            entries.append(e);
        }
        std::sort(entries.begin(), entries.end(),
                  [](const FavoriteEntry& a, const FavoriteEntry& b) {
                      return a.page < b.page;
                  });
        if (!entries.isEmpty())
            m_favorites.insert(path, entries);
    }
}

void FavoritesStore::saveNow() {
    if (!m_dirty || m_path.isEmpty())
        return;
    const QByteArray json = serialize();
    if (json == m_lastSavedJson) {
        m_dirty = false;
        return;
    }
    QSaveFile f(m_path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(json);
        if (f.commit()) {
            m_lastSavedJson = json;
            m_dirty = false;
        }
    }
}

void FavoritesStore::scheduleSave() {
    scheduleDebouncedSave();
}

void FavoritesStore::notifyChanged() {
    if (m_notify)
        m_notify();
}