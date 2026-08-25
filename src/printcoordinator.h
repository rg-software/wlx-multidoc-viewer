#ifndef PRINTCoORDINATOR_H
#define PRINTCoORDINATOR_H

#include "document.h"

#include <QImage>
#include <QVector>
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>

// Shared print pipeline (design D7 / task 10.1): resolves a concrete page ×
// copy job, renders each page at printer resolution on a worker thread using
// the current rotation, and hands the rendered bitmap to a platform sink that
// spools it. Cancellation is atomic (checked per page) and must be joined
// before the engine is torn down, mirroring the search controller.
class PrintCoordinator {
public:
    using SinkFn = std::function<bool(int passIndex, const QImage& pageImage)>;
    using ProgressFn = std::function<void(int done, int total)>;
    using FinishedFn = std::function<void(bool cancelled)>;

    PrintCoordinator() = default;
    ~PrintCoordinator();
    PrintCoordinator(const PrintCoordinator&) = delete;
    PrintCoordinator& operator=(const PrintCoordinator&) = delete;

    // pages: 1-based page numbers in print order (already expanded per copy).
    // printablePx is the target printer's printable area in device pixels;
    // each page is rendered so its content fits entirely without cropping.
    bool start(DocumentEngine* engine, int rotation, const QSize& printablePx,
               const QVector<int>& pages, SinkFn sink, ProgressFn progress,
               FinishedFn finished);

    void cancel();
    void join();
    bool running() const;

private:
    void runWorker(DocumentEngine* engine, int rotation, const QSize& printablePx,
                   const QVector<int>& pages, SinkFn sink, ProgressFn progress,
                   FinishedFn finished);

    std::thread m_thread;
    mutable std::mutex m_mutex;
    std::atomic<bool> m_cancel{false};
};

#endif // PRINTCoORDINATOR_H