#ifndef SEARCHCONTROLLER_H
#define SEARCHCONTROLLER_H

#include "document.h"

#include <QString>
#include <QVector>
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>

// Runs a whole-document text search on a worker thread, walking pages from the
// current page upward (wrapping), and delivering progressive results. The
// engine performs the actual page search; this class owns only the scan
// control (thread, cancellation). All callbacks are invoked from the worker
// thread — the caller is responsible for marshaling them to its UI thread.
class SearchController {
public:
    using PageCallback = std::function<void(const QVector<TextMatch>& matches)>;
    using DoneCallback = std::function<void(bool cancelled)>;

    SearchController() = default;
    ~SearchController();
    SearchController(const SearchController&) = delete;
    SearchController& operator=(const SearchController&) = delete;

    // Starts a scan of every page, beginning at startPage and wrapping. Returns
    // false if a scan is already running (call cancel()/join() first).
    bool start(DocumentEngine* engine, int pageCount, int startPage,
               const QString& needle, bool matchCase,
               PageCallback onPage, DoneCallback onDone);

    // Signals the worker to stop. Thread-safe; the scan checks between pages,
    // so the current page search is allowed to complete.
    void cancel();

    // Blocks until the worker thread exits. Must be called (UI thread) before
    // the engine is torn down or this object is destroyed.
    void join();

    bool running() const;

private:
    void runScan(DocumentEngine* engine, int pageCount, int startPage,
                 QString needle, bool matchCase, PageCallback onPage,
                 DoneCallback onDone);

    std::thread m_thread;
    mutable std::mutex m_mutex;
    std::atomic<bool> m_cancel{false};
};

#endif // SEARCHCONTROLLER_H