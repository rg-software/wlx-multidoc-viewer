#ifndef FAVORITES_H
#define FAVORITES_H

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVector>
#include <functional>
#include <memory>

struct FavoriteEntry {
    int page = 1;
    QString label;
};

// Per-document page favorites (openers/add-favorites). Singleton, created
// lazily on first use; all methods are UI-thread only. Entries are keyed by the
// canonical absolute path of the document; a toggle inserts or removes the
// page, and persistence happens through an atomic QSaveFile write debounced to
// collapse rapid toggle bursts.
class FavoritesStore {
public:
    using ChangeNotifier = std::function<void()>;

    static FavoritesStore& get();
    // Test hooks (harness-favorites): destroy/recreate the singleton to simulate
    // a process restart, and pin the persistence directory.
    static void resetForTesting();
    static void setLocationOverrideForTesting(const QString& dir);

    QVector<FavoriteEntry> favoritesFor(const QString& path) const;
    bool isFavorite(const QString& path, int page) const;
    void toggle(const QString& path, int page, const QString& label = {});
    void flush();

    // The viewer installs one notifier to react to favorites changes (sidebar
    // section rebuild, availability gates). Only the owning viewer instance
    // may set it; clear it ({}) on viewer destruction.
    void setChangeNotifier(ChangeNotifier fn) { m_notify = std::move(fn); }
    QString filePath() const { return m_path; }

private:
    FavoritesStore();
    static FavoritesStore* createInstance();
    QString canonicalKey(const QString& path) const;
    QString resolveDir();
    QByteArray serialize() const;
    void loadFrom(const QByteArray& json);
    void scheduleSave();
    void saveNow();
    void notifyChanged();

    QHash<QString, QVector<FavoriteEntry>> m_favorites;
    QString m_dir;
    QString m_path;
    QByteArray m_lastSavedJson;
    bool m_dirty = false;
    ChangeNotifier m_notify;
};

#endif // FAVORITES_H