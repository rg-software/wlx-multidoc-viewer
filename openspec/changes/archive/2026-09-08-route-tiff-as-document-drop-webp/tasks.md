## 1. Narrow the native-image rosters

- [x] 1.1 In `src/formatdispatcher.cpp`, reduce `isRasterSuffix` to `jpg/jpeg/png/gif/bmp/ico`; TIFF falls through to the default MuPDF engine; WEBP is unrouted
- [x] 1.2 In `src/imagefolder.cpp`, reduce `isRasterName` to `jpg/jpeg/png/gif/bmp/ico` so TIFF/WEBP are not counted or browsed as siblings

## 2. Remove the dead controller fallback

- [x] 2.1 In `src/viewercontroller.cpp` / `.h`, remove `isRasterPath` and the raster→MuPDF retry branch in `openDocument` and `openSibling` (unreachable now that ImageEngine only sees natively-decodable rasters)

## 3. Bring docs in line

- [x] 3.1 Update `src/imageengine.cpp` comments referencing TIFF/WEBP to the narrowed roster
- [x] 3.2 Update AGENTS.md Engines note + gaps table: image engine/folder covers `jpg/jpeg/png/gif/bmp/ico`; TIFF is a document via MuPDF (one page per IFD); WEBP has no decoder and is unreserved

## 4. Verify + samples

- [x] 4.1 Add `sample-multipage.tiff` (3 IFDs, verified 3 pages) to the generator and `examples/`; remove `sample4.webp`
- [x] 4.2 Confirm a folder of `sample1.jpg`, `sample2.png`, `sample3.bmp`, `sample-animated.gif`, `sample5.tiff`, `sample-multipage.tiff` presents the 4 natively-decodable images as siblings (count 4), opening `sample5.tiff`/`sample-multipage.tiff` directly gives the 1-page / 3-page MuPDF document, and WEBP is not an advertised format
