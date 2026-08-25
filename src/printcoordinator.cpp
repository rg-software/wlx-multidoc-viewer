#include "printcoordinator.h"

PrintCoordinator::~PrintCoordinator() {
    cancel();
    join();
}

bool PrintCoordinator::start(DocumentEngine* engine, int rotation, const QSize& printablePx,
                             const QVector<int>& pages, SinkFn sink,
                             ProgressFn progress, FinishedFn finished) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_thread.joinable())
        return false;
    m_cancel.store(false, std::memory_order_relaxed);

    SinkFn sinkFn = std::move(sink);
    ProgressFn progressFn = std::move(progress);
    FinishedFn finishedFn = std::move(finished);
    m_thread = std::thread(&PrintCoordinator::runWorker, this, engine, rotation,
                           printablePx, pages, std::move(sinkFn), std::move(progressFn),
                           std::move(finishedFn));
    return true;
}

void PrintCoordinator::cancel() {
    m_cancel.store(true, std::memory_order_relaxed);
}

void PrintCoordinator::join() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_thread.joinable())
        m_thread.join();
}

bool PrintCoordinator::running() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_thread.joinable();
}

void PrintCoordinator::runWorker(DocumentEngine* engine, int rotation, const QSize& printablePx,
                                 const QVector<int>& pages, SinkFn sink,
                                 ProgressFn progress, FinishedFn finished) {
    const int total = pages.size();
    bool cancelled = false;
    for (int i = 0; i < total; ++i) {
        if (m_cancel.load(std::memory_order_relaxed)) {
            cancelled = true;
            break;
        }
        const int page = pages[i];
        const PageInfo info = engine ? engine->pageDimensions(page) : PageInfo();
        if (engine && info.width > 0 && info.height > 0 && !printablePx.isEmpty()) {
            double pw = info.width;
            double ph = info.height;
            if (rotation == 90 || rotation == 270)
                std::swap(pw, ph);
            // Fit the whole rotated page inside the printable area, same math
            // on both platforms (share this fit; margins chosen by the sink).
            const double scale = std::min(
                static_cast<double>(printablePx.width()) / pw,
                static_cast<double>(printablePx.height()) / ph);
            if (scale > 0.0) {
                const QImage img = engine->renderPage(page, static_cast<float>(scale), 1.0f, rotation);
                if (!img.isNull()) {
                    if (sink && !sink(i, img))
                        break; // sink reports a hard spool failure
                }
            }
        }
        if (progress)
            progress(i + 1, total);
    }
    if (finished)
        finished(cancelled);
}