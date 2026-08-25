#include "searchcontroller.h"

SearchController::~SearchController() {
    cancel();
    join();
}

bool SearchController::start(DocumentEngine* engine, int pageCount, int startPage,
                             const QString& needle, bool matchCase,
                             PageCallback onPage, DoneCallback onDone) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_thread.joinable())
        return false;

    m_cancel.store(false, std::memory_order_relaxed);
    PageCallback pageCb = std::move(onPage);
    DoneCallback doneCb = std::move(onDone);
    m_thread = std::thread(&SearchController::runScan, this,
                           engine, pageCount, startPage,
                           needle, matchCase,
                           std::move(pageCb), std::move(doneCb));
    return true;
}

void SearchController::cancel() {
    m_cancel.store(true, std::memory_order_relaxed);
}

void SearchController::join() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_thread.joinable())
        m_thread.join();
}

bool SearchController::running() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_thread.joinable();
}

void SearchController::runScan(DocumentEngine* engine, int pageCount, int startPage,
                               QString needle, bool matchCase,
                               PageCallback onPage, DoneCallback onDone) {
    if (pageCount <= 0 || !engine) {
        if (onDone)
            onDone(m_cancel.load(std::memory_order_relaxed));
        return;
    }

    int page = startPage;
    int scanned = 0;
    bool cancelled = false;
    while (scanned < pageCount) {
        if (m_cancel.load(std::memory_order_relaxed)) {
            cancelled = true;
            break;
        }
        const QVector<TextMatch> matches = engine->searchText(page, needle, matchCase);
        if (onPage)
            onPage(matches);
        ++scanned;
        ++page;
        if (page > pageCount)
            page = 1;
    }

    if (onDone)
        onDone(cancelled);
}