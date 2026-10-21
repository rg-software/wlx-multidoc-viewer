// Find-box/search-sync regression harness (fix-find-box-search-sync,
// issue #12). Drives a REAL ToolbarPresenter with a fake ToolbarBackend and a
// real ViewerController over a fake searchable engine, and asserts:
//   4.1) FindPrev/FindNext enable on the first non-empty box character and
//        disable when the box is emptied (no commit needed);
//        Prev/Next with no active query for the box text START a search,
//        while with an active query they NAVIGATE without re-scanning.
//   4.2) A box edit clears highlights/match count/active match immediately;
//        a zero-match term does not re-trigger a scan on repeated Prev/Next
//        (trigger keyed on the committed query, not the hit list);
//        the match-case toggle re-runs only an active query;
//        onDocumentLoaded resets box + search state.

#include "document.h"
#include "toolbar.h"
#include "viewercontroller.h"

#include <QCoreApplication>
#include <QHash>
#include <QString>
#include <atomic>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

static int g_fail = 0;

#define CHECK(name, cond)                                                     \
    do {                                                                      \
        std::printf("%s %s (line %d)\n", (cond) ? "PASS" : "FAIL", name,      \
                    __LINE__);                                                \
        std::fflush(stdout);                                                  \
        if (!(cond))                                                          \
            ++g_fail;                                                         \
    } while (0)

namespace {

// --------------------------------------------------------------- fake engine
// Searchable stub: "bar" matches on every page, "foo" only on page 2, any
// other non-empty term matches nowhere. Counts scans so the harness can prove
// which activations initiated a search and which only navigated.
class FakeEngine : public DocumentEngine {
public:
    bool open(const QString&) override { m_open = true; return true; }
    void close() override { m_open = false; }
    bool isOpen() const override { return m_open; }
    int pageCount() const override { return 3; }
    QImage renderPage(int, float, float, int) override { return {}; }
    QString extractText(int) override { return {}; }
    QString metadata(const QString&) const override { return {}; }
    QVector<OutlineItem> outline() const override { return {}; }
    PageInfo pageDimensions(int) const override { return PageInfo{200, 300}; }

    bool supportsSearch() const override { return true; }
    QVector<TextMatch> searchText(int page, const QString& needle, bool matchCase) override
    {
        ++scanCount;
        lastMatchCase = matchCase;
        QVector<TextMatch> out;
        const bool hit = (needle == QLatin1String("bar")) ||
                         (needle == QLatin1String("foo") && page == 2);
        if (hit) {
            TextMatch m;
            m.page = page;
            m.rects.append(QRectF(0.1, 0.1, 0.2, 0.02));
            out.append(m);
        }
        return out;
    }

    std::atomic<int> scanCount{0};
    std::atomic<bool> lastMatchCase{false};

private:
    bool m_open = false;
};

// -------------------------------------------------------------- fake backend
// Stores control state in plain maps. setEditText mimics a worst-case real
// backend: a programmatic find-box write still raises the change notification
// (Win32 EN_CHANGE fires before the backend guard could help), so the
// presenter-side re-entrancy guard is what must keep it inert.
class FakeBackend : public toolbar::ToolbarBackend {
public:
    void setEnabled(toolbar::Control c, bool on) override { enabled[c] = on; }
    void setChecked(toolbar::Control c, bool on) override { checked[c] = on; }
    void setIcon(toolbar::Control, toolbar::Icon) override {}
    void setText(toolbar::Control c, const QString& t) override { texts[c] = t; }
    void setEditText(toolbar::Control c, const QString& t) override
    {
        edits[c] = t;
        if (c == toolbar::Control::FindBox)
            notifyFindTextChanged(); // worst case: notification raised anyway
    }
    QString editText(toolbar::Control c) const override { return edits.value(c); }
    bool isEditFocused() const override { return false; }
    void focusFind() override {}

    // Simulates the user typing into the find box: the box text changes and
    // the backend raises the change notification (EN_CHANGE / textChanged).
    void userEdit(const QString& text)
    {
        edits[toolbar::Control::FindBox] = text;
        notifyFindTextChanged();
    }

    bool isEnabled(toolbar::Control c) const { return enabled.value(c, false); }

    QHash<toolbar::Control, bool> enabled;
    QHash<toolbar::Control, bool> checked;
    QHash<toolbar::Control, QString> texts;
    QHash<toolbar::Control, QString> edits;
};

// UI-marshalling queue: search callbacks are posted here from the worker
// thread and drained on the "UI" thread, exactly like the platform viewers.
class UiQueue {
public:
    std::function<void(std::function<void()>)> marshal()
    {
        return [this](std::function<void()> task) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_tasks.push_back(std::move(task));
        };
    }
    void drain()
    {
        std::vector<std::function<void()>> local;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            local.swap(m_tasks);
        }
        for (auto& t : local)
            t();
    }

private:
    std::mutex m_mutex;
    std::vector<std::function<void()>> m_tasks;
};

// Drains the UI queue until the controller reports no scan in progress (the
// done callback is itself a marshaled task, so searchInProgress() only flips
// after a drain).
void pumpSearchIdle(ViewerController& c, UiQueue& q)
{
    for (int i = 0; i < 2000; ++i) {
        q.drain();
        if (!c.searchInProgress()) {
            q.drain();
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

struct Rig {
    FakeEngine* engine = nullptr; // owned by the controller

    explicit Rig()
    {
        auto eng = std::make_unique<FakeEngine>();
        engine = eng.get();
        controller.setEngine(std::move(eng));
        controller.setUiMarshal(queue.marshal());
        controller.openDocument(QStringLiteral("stub.pdf"));
        controller.setViewportSize(QSize(600, 800));
        backend.userEdit(QString()); // start with an empty box
        presenter.attach(&controller, &backend);
        presenter.refreshState();
    }

    ViewerController controller;
    FakeBackend backend;
    toolbar::ToolbarPresenter presenter;
    UiQueue queue;
};

} // namespace

// 4.1a: enablement follows the box contents, not the result set.
static void enablementFollowsBox() {
    Rig rig;

    CHECK("initially disabled with an empty box",
          !rig.backend.isEnabled(toolbar::Control::FindPrev) &&
              !rig.backend.isEnabled(toolbar::Control::FindNext));

    rig.backend.userEdit(QStringLiteral("b"));
    CHECK("enabled on first character (before any commit)",
          rig.backend.isEnabled(toolbar::Control::FindPrev) &&
              rig.backend.isEnabled(toolbar::Control::FindNext));

    rig.backend.userEdit(QString());
    CHECK("disabled again when the box is emptied",
          !rig.backend.isEnabled(toolbar::Control::FindPrev) &&
              !rig.backend.isEnabled(toolbar::Control::FindNext));
}

// 4.1b: Prev/Next initiate a search when no query is active for the box text,
// and navigate (without re-scanning) once it is.
static void nextWithoutQueryStartsSearch() {
    Rig rig;
    rig.backend.userEdit(QStringLiteral("bar"));
    CHECK("edit before any search leaves no active query",
          rig.controller.searchQuery().isEmpty() &&
              rig.controller.searchMatchCount() == 0);

    rig.presenter.onFindNext();
    pumpSearchIdle(rig.controller, rig.queue);
    const int scansAfterFirst = rig.engine->scanCount.load();
    CHECK("Next with no active query started a search", scansAfterFirst > 0);
    CHECK("search committed the box term",
          rig.controller.searchQuery() == QLatin1String("bar"));
    CHECK("first match selected", rig.controller.activeMatchIndex() == 0);
    CHECK("matches found", rig.controller.searchMatchCount() > 0);

    const int activeBefore = rig.controller.activeMatchIndex();
    rig.presenter.onFindNext();
    pumpSearchIdle(rig.controller, rig.queue);
    CHECK("Next with an active query navigates",
          rig.controller.activeMatchIndex() != activeBefore);
    CHECK("navigation did not re-scan",
          rig.engine->scanCount.load() == scansAfterFirst);

    const int activeAt = rig.controller.activeMatchIndex();
    rig.presenter.onFindPrev();
    pumpSearchIdle(rig.controller, rig.queue);
    CHECK("Prev with an active query navigates back",
          rig.controller.activeMatchIndex() != activeAt);
    CHECK("Prev did not re-scan",
          rig.engine->scanCount.load() == scansAfterFirst);
}

// 4.2a: an edit invalidates highlights/count/active match immediately.
static void editClearsSearchState() {
    Rig rig;
    rig.backend.userEdit(QStringLiteral("bar"));
    rig.presenter.onFindCommitted(QStringLiteral("bar")); // Enter commits
    pumpSearchIdle(rig.controller, rig.queue);
    CHECK("setup: search active with highlights",
          rig.controller.searchMatchCount() > 0 &&
              rig.controller.hasSearchHighlights());

    rig.backend.userEdit(QStringLiteral("baz")); // plain edit, no commit
    CHECK("edit clears the committed query",
          rig.controller.searchQuery().isEmpty());
    CHECK("edit clears the match count", rig.controller.searchMatchCount() == 0);
    CHECK("edit clears the highlights", !rig.controller.hasSearchHighlights());
    CHECK("edit clears the active match", rig.controller.activeMatchIndex() < 0);
    CHECK("buttons stay enabled while the box is non-empty",
          rig.backend.isEnabled(toolbar::Control::FindNext));
}

// 4.2b: trigger keyed on the committed query — a zero-match term is not
// re-scanned on repeated Prev/Next activations (regression for the
// hits-list-keyed trigger).
static void zeroMatchTermDoesNotRescan() {
    Rig rig;
    rig.backend.userEdit(QStringLiteral("zzz")); // matches nothing
    rig.presenter.onFindNext();
    pumpSearchIdle(rig.controller, rig.queue);
    const int scansAfterFirst = rig.engine->scanCount.load();
    CHECK("first Next for the term scanned", scansAfterFirst > 0);
    CHECK("term reports no match", rig.controller.searchNoMatch());
    CHECK("no highlights", rig.controller.searchMatchCount() == 0);

    rig.presenter.onFindNext();
    rig.presenter.onFindPrev();
    rig.presenter.onFindNext();
    pumpSearchIdle(rig.controller, rig.queue);
    CHECK("repeated Prev/Next on a zero-match term does not re-scan",
          rig.engine->scanCount.load() == scansAfterFirst);
}

// Match-case re-runs an active query immediately; with no active query it
// only affects the next search.
static void matchCaseToggleSemantics() {
    Rig rig;
    rig.backend.userEdit(QStringLiteral("bar"));
    rig.presenter.onFindNext();
    pumpSearchIdle(rig.controller, rig.queue);
    const int scansAfterSearch = rig.engine->scanCount.load();
    CHECK("setup: query active", !rig.controller.searchQuery().isEmpty());

    rig.presenter.onMatchCaseToggled(true);
    pumpSearchIdle(rig.controller, rig.queue);
    CHECK("toggle re-ran the active search",
          rig.engine->scanCount.load() > scansAfterSearch);
    CHECK("toggle re-ran with the new case flag",
          rig.engine->lastMatchCase.load() == true);

    // Clear the query via an edit, then toggle: no scan may run.
    rig.backend.userEdit(QStringLiteral("bar")); // edit invalidates
    const int scansBeforeToggle = rig.engine->scanCount.load();
    rig.presenter.onMatchCaseToggled(false);
    CHECK("toggle with no active query does not scan",
          rig.engine->scanCount.load() == scansBeforeToggle);
}

// Document load resets find state: box cleared, no query, no highlights.
static void documentLoadResetsFindState() {
    Rig rig;
    rig.backend.userEdit(QStringLiteral("bar"));
    rig.presenter.onFindNext();
    pumpSearchIdle(rig.controller, rig.queue);
    CHECK("setup: search active", rig.controller.searchMatchCount() > 0);

    rig.presenter.onDocumentLoaded();
    CHECK("document load clears the find box",
          rig.backend.editText(toolbar::Control::FindBox).isEmpty());
    CHECK("document load clears the query", rig.controller.searchQuery().isEmpty());
    CHECK("document load clears highlights", !rig.controller.hasSearchHighlights());
    CHECK("document load disables the match buttons",
          !rig.backend.isEnabled(toolbar::Control::FindPrev) &&
              !rig.backend.isEnabled(toolbar::Control::FindNext));

    // The programmatic clear must not have been treated as a user edit that
    // re-invalidated anything or crashed; a fresh search must still work.
    rig.backend.userEdit(QStringLiteral("bar"));
    rig.presenter.onFindNext();
    pumpSearchIdle(rig.controller, rig.queue);
    CHECK("search works again after a document-load reset",
          rig.controller.searchQuery() == QLatin1String("bar") &&
              rig.controller.searchMatchCount() > 0);
}

// Enter semantics are preserved: empty term clears; re-commit cycles matches.
static void enterCommitSemanticsUnchanged() {
    Rig rig;
    rig.backend.userEdit(QStringLiteral("bar"));
    rig.presenter.onFindCommitted(QStringLiteral("bar"));
    pumpSearchIdle(rig.controller, rig.queue);
    const int scansAfterCommit = rig.engine->scanCount.load();
    CHECK("Enter committed the search", rig.controller.searchMatchCount() > 0);

    rig.presenter.onFindCommitted(QStringLiteral("bar")); // Enter again
    pumpSearchIdle(rig.controller, rig.queue);
    CHECK("Enter on the active term cycles without re-scanning",
          rig.engine->scanCount.load() == scansAfterCommit);

    rig.presenter.onFindCommitted(QStringLiteral("   ")); // Enter on blanks
    pumpSearchIdle(rig.controller, rig.queue);
    CHECK("Enter on an empty term clears the search",
          rig.controller.searchQuery().isEmpty() &&
              !rig.controller.hasSearchHighlights());
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    enablementFollowsBox();
    nextWithoutQueryStartsSearch();
    editClearsSearchState();
    zeroMatchTermDoesNotRescan();
    matchCaseToggleSemantics();
    documentLoadResetsFindState();
    enterCommitSemanticsUnchanged();

    std::printf("RESULT: %s\n", g_fail == 0 ? "PASS" : "FAIL");
    return g_fail == 0 ? 0 : 1;
}
