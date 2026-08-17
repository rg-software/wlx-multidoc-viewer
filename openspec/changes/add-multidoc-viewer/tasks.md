## 1. Project scaffold and build system

- [ ] 1.1 Create CMakeLists.txt with Qt6::Widgets, MuPDF (pkg-config), and DjVuLibre (pkg-config) discovery
- [ ] 1.2 Create src/ directory structure: plugin.cpp, document.h/cpp, mupdfengine.h/cpp, djvuengine.h/cpp, viewer.h/cpp
- [ ] 1.3 Add wlxplugin.h SDK header to the project (WLX type definitions: HWND, HANDLE, DCPCALL, LISTPLUGIN_OK, etc.)
- [ ] 1.4 Verify the project compiles and links on Linux with all three dependencies present

## 2. Document engine abstraction

- [ ] 2.1 Define DocumentEngine abstract interface in document.h with: open(path), close(), pageCount(), renderPage(page, zoom) → QImage, extractText(page) → QString, metadata(key) → QString, outline() → OutlineItems
- [ ] 2.2 Define supporting types: OutlineItem (title, pageNo, children), PageInfo (width, height)

## 3. MuPDF engine

- [ ] 3.1 Implement MuPdfEngine::open() — create fz_context via fz_new_context(), register document plugins, call fz_open_document()
- [ ] 3.2 Implement MuPdfEngine::pageCount() — call fz_count_pages()
- [ ] 3.3 Implement MuPdfEngine::renderPage() — load page, create transform matrix from zoom, render to fz_pixmap via fz_new_pixmap_from_page(), convert to QImage (Format_RGB888)
- [ ] 3.4 Implement MuPdfEngine::extractText() — load page, create structured text via fz_new_stext_page_from_page(), walk blocks/lines/chars to build UTF-8 string
- [ ] 3.5 Implement MuPdfEngine::metadata() — call fz_lookup_metadata for title, author, subject, creator, producer, creationDate, modDate
- [ ] 3.6 Implement MuPdfEngine::outline() — call fz_load_outline(), walk fz_outline tree to build OutlineItems
- [ ] 3.7 Implement MuPdfEngine::close() — drop all fz_ handles in reverse order (outline, page, document, context)
- [ ] 3.8 Add error handling: fz_try/fz_catch around all MuPDF calls, return empty results on failure, never crash

## 4. DjVu engine

- [ ] 4.1 Implement DjVuEngine::open() — create ddjvu_context_t, create ddjvu_document_t via ddjvu_document_create_by_filename(), wait for document info
- [ ] 4.2 Implement DjVuEngine::pageCount() — call ddjvu_document_get_page_num()
- [ ] 4.3 Implement DjVuEngine::renderPage() — create ddjvu_page_t, call ddjvu_page_get_rendered_size() for dimensions, allocate buffer, call ddjvu_page_render() with DDJVU_RENDER_COLOR, convert buffer to QImage
- [ ] 4.4 Implement DjVuEngine::extractText() — call ddjvu_page_text() for UTF-8 text extraction
- [ ] 4.5 Implement DjVuEngine::pageDimensions() — use ddjvu_page_get_rendered_size() at native resolution
- [ ] 4.6 Implement DjVuEngine::close() — destroy ddjvu_page_t and ddjvu_document_t, destroy ddjvu_context_t
- [ ] 4.7 Add error handling: check ddjvu_status after operations, handle DDJVU_FAILED, never crash

## 5. Format dispatcher

- [ ] 5.1 Implement createEngine(path) factory function: map file extension to engine type (DjVu → DjVuEngine, everything else → MuPdfEngine)
- [ ] 5.2 Handle case-insensitive extension matching
- [ ] 5.3 Handle edge cases: no extension, unknown extension (try MuPDF which sniffs content)

## 6. Viewer widget

- [ ] 6.1 Implement ViewerWidget class: QFrame containing QToolBar + QScrollArea + QLabel
- [ ] 6.2 Implement toolbar: Previous, Next, First, Last page buttons; page counter QLabel; Zoom In, Zoom Out, Original Size buttons; Fit toggle button; Info button
- [ ] 6.3 Implement page navigation: wire buttons to engine->renderPage(), update QLabel pixmap and page counter
- [ ] 6.4 Implement zoom: zoom factor state variable, re-render page at new zoom on zoom in/out, clamp to 10%-500% range
- [ ] 6.5 Implement fit modes: Fit to Width (scale to viewport width), Fit to Page (scale to fit both dimensions), toggle between them
- [ ] 6.6 Implement go-to-page dialog: Ctrl+G shortcut, QInputDialog for page number, navigate to entered page
- [ ] 6.7 Implement document info dialog: metadata fields displayed in QMessageBox, skip empty fields
- [ ] 6.8 Wire keyboard shortcuts: Right/Left arrows for page nav, Home/End for first/last, Ctrl+=/- for zoom, Ctrl+M for fit toggle

## 7. WLX plugin entry points

- [ ] 7.1 Implement ListLoad(): detect extension via createEngine(), create ViewerWidget as child of ParentWin, load document, return widget handle
- [ ] 7.2 Implement ListLoadNext(): get existing ViewerWidget from PluginWin, close old document, load new file, return LISTPLUGIN_OK or LISTPLUGIN_ERROR
- [ ] 7.3 Implement ListCloseWindow(): call ViewerWidget cleanup, release engine, delete widget
- [ ] 7.4 Implement ListGetDetectString(): build extension list string from supported format table
- [ ] 7.5 Ensure no global mutable state — all state lives in the ViewerWidget instance

## 8. Testing and validation

- [ ] 8.1 Test with a sample PDF (multi-page, with bookmarks and metadata)
- [ ] 8.2 Test with a DjVu document
- [ ] 8.3 Test with an EPUB and a MOBI file
- [ ] 8.4 Test with CBZ/CBR comic archives
- [ ] 8.5 Test with various image formats (JPEG, PNG, TIFF, WebP)
- [ ] 8.6 Test error handling: corrupted file, empty file, zero-byte file
- [ ] 8.7 Test multiple simultaneous instances (two preview panes)
- [ ] 8.8 Test all keyboard shortcuts and toolbar actions
