#ifndef MUPDFCONTEXT_H
#define MUPDFCONTEXT_H

#include <mupdf/fitz.h>
#include <mutex>

// Process-wide MuPDF context shared by every engine instance (MuPdfEngine,
// ChmEngine, and Fb2ZipEngine through its MuPdfEngine member).
//
// Stock lcms2 — the ICC backend linked by overlay-ports/libmupdf — refuses a
// second live fz_context in the same process ("Stock LCMS2 library cannot be
// used in multiple contexts!"), so each engine owning its context broke the
// second concurrent ListLoad of any MuPDF-backed file (the host then fell
// through to the next plugin). One shared context serves any number of open
// documents and fixes concurrent windows.
//
// The context is created once and never destroyed: close() drops only the
// engine's own documents/archives, so shutdown order can never strand a live
// document on a dead context.
//
// Every fz_* call in every engine MUST hold sharedMuPdfMutex(): a MuPDF
// context is not thread-safe, and UI renders now share it with search/print
// worker threads across engine instances. Helpers documented as "caller must
// hold the shared MuPDF lock" never lock it themselves (in particular the
// ChmResourceArchive callbacks, which run inside fz_* calls).
inline std::mutex& sharedMuPdfMutex() {
    static std::mutex m;
    return m;
}

inline fz_context* sharedMuPdfContext() {
    static fz_context* ctx = [] {
        fz_context* c = fz_new_context(nullptr, nullptr, FZ_STORE_UNLIMITED);
        if (c)
            fz_register_document_handlers(c);
        return c;
    }();
    return ctx;
}

#endif // MUPDFCONTEXT_H
