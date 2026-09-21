// Headless favorites-store harness (add-favorites).
//
// Covers FavoritesStore without a viewer: toggle add/remove, page ordering and
// label retention, per-document independence, atomic disk round-trip (via
// resetForTesting() to simulate a restart), no-op flush debouncing, tolerant
// load of corrupt/unknown files, notifier firing, and Windows key folding.

#include "favorites.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cstdio>

namespace {

int g_fail = 0;

void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
        ++g_fail;
}

QVector<int> pagesOf(const QVector<FavoriteEntry>& entries) {
    QVector<int> pages;
    for (const FavoriteEntry& e : entries)
        pages.append(e.page);
    return pages;
}

QJsonObject readJson(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

int run() {
    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        std::printf("FAIL could not create temp dir\n");
        return 2;
    }
    const QString dir = QDir::cleanPath(tmp.path());
    FavoritesStore::setLocationOverrideForTesting(dir);
    FavoritesStore::resetForTesting();

    FavoritesStore& store = FavoritesStore::get();
    const QString file = store.filePath();
    check(file.startsWith(dir), "store file lives under the override dir");
    check(QFileInfo(file).fileName() == QStringLiteral("multidocviewer-favorites.json"),
          "store file uses the expected name");

    const QString docA = dir + QStringLiteral("/alpha.pdf");
    const QString docB = dir + QStringLiteral("/beta.pdf");

    check(store.favoritesFor(docA).isEmpty(), "new document has no favorites");
    check(!store.isFavorite(docA, 1), "new document page is not a favorite");

    store.toggle(docA, 1);
    check(store.isFavorite(docA, 1), "toggle adds the page");
    check(pagesOf(store.favoritesFor(docA)) == QVector<int>{1}, "single favorite recorded");

    store.toggle(docA, 1);
    check(!store.isFavorite(docA, 1), "second toggle removes the page");
    check(store.favoritesFor(docA).isEmpty(), "the document key is dropped when empty");

    store.toggle(docA, 5, QStringLiteral("Chapter"));
    store.toggle(docA, 2);
    store.toggle(docA, 9);
    check(pagesOf(store.favoritesFor(docA)) == (QVector<int>{2, 5, 9}),
          "favorites are kept sorted by page");
    const QVector<FavoriteEntry> a = store.favoritesFor(docA);
    check(a.size() == 3 && a[1].label == QStringLiteral("Chapter"),
          "the label survives insertion ordering");

    store.toggle(docB, 4);
    check(store.favoritesFor(docA).size() == 3 && store.favoritesFor(docB).size() == 1,
          "documents keep independent favorite sets");

    int notified = 0;
    store.setChangeNotifier([&notified] { ++notified; });
    store.toggle(docA, 7);
    check(notified == 1, "the change notifier fires on toggle");
    store.flush();
    check(notified == 1, "flush does not fire the change notifier");

    store.flush();
    check(QFile::exists(file), "flush writes the store file");
    const QJsonObject root = readJson(file);
    check(root.value(QStringLiteral("version")).toInt() == 1, "file records format version 1");
    check(root.value(QStringLiteral("favorites")).toArray().size() == 2,
          "both documents are persisted");

    const QFileInfo beforeInfo(file);
    store.flush();
    check(QFileInfo(file).lastModified() == beforeInfo.lastModified(),
          "a no-op flush leaves the file mtime unchanged");

    FavoritesStore::resetForTesting();
    FavoritesStore& reopened = FavoritesStore::get();
    check(pagesOf(reopened.favoritesFor(docA)) == (QVector<int>{2, 5, 7, 9}),
          "favorites survive a simulated restart");
    check(reopened.favoritesFor(docA)[1].label == QStringLiteral("Chapter"),
          "labels survive a simulated restart");

    reopened.toggle(docB, 4);
    reopened.flush();
    const QJsonObject afterRemove = readJson(file);
    bool betaPresent = false;
    for (const QJsonValue& v : afterRemove.value(QStringLiteral("favorites")).toArray()) {
        if (v.toObject().value(QStringLiteral("path")).toString() == docB)
            betaPresent = true;
    }
    check(!betaPresent, "emptied documents are removed from the persisted file");

#ifdef _WIN32
    reopened.toggle(QStringLiteral("C:/Temp/Doc.PDF"), 3);
    check(reopened.isFavorite(QStringLiteral("c:/temp/doc.pdf"), 3),
          "Windows path keys are case-insensitive");
#endif

    {
        QFile f(file);
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.write("this is not json");
    }
    FavoritesStore::resetForTesting();
    check(FavoritesStore::get().favoritesFor(docA).isEmpty(),
          "corrupt files load as an empty store");

    {
        QFile f(file);
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.write(R"({"version":99,"favorites":[{"path":"x","items":[{"page":1}]}]})");
    }
    FavoritesStore::resetForTesting();
    check(FavoritesStore::get().favoritesFor(docA).isEmpty(),
          "unknown format versions load as an empty store");

    return g_fail == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    return run();
}
