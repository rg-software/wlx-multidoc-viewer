#!/usr/bin/env python3
"""Generate EPUB table-of-contents fixtures.

Two fixtures that exercise both EPUB table-of-contents parsers MuPDF
supports, each with a nested entry and a fragment target:

  examples/epub-toc-nav3.epub  - EPUB3, navigation document (nav.xhtml with
                                 epub:type="toc"). MuPDF's EPUB3 branch
                                 (source/html/epub-doc.c, epub_parse_nav).
  examples/epub-toc-ncx2.epub  - EPUB2, toc.ncx with nested <navPoint>s.
                                 MuPDF's EPUB2 branch (epub_parse_ncx).

Both books share the same body: two chapters. Chapter one carries enough
filler text to span several A5 slices, so the "Section 1.2" fragment lands on
a later page of the chapter (and therefore not on the chapter's first page),
and chapter two starts after it. Chapter two also links back to the
"Section 1.2" fragment, which lets the harness cross-check the table-of-
contents destination against the in-page link destination.

The exact page numbers depend on the reflow layout (font size, publisher
CSS), so the harness asserts relationships (chapter one starts on page 1,
the fragment is on a later page of chapter one, chapter two comes after it)
rather than hard-coded numbers.

Regenerate with: python tools/generate_sample_epub_toc.py
"""

import os
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXAMPLES = os.path.join(ROOT, "examples")


def write_epub(path, entries):
    # mimetype must be first and uncompressed per the EPUB spec.
    with zipfile.ZipFile(path, "w") as z:
        z.writestr("mimetype", "application/epub+zip",
                   compress_type=zipfile.ZIP_STORED)
        for name, payload in entries:
            if name == "mimetype":
                continue
            z.writestr(name, payload, compress_type=zipfile.ZIP_DEFLATED)
    print("wrote", path)


CONTAINER = """<?xml version="1.0"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles><rootfile full-path="OEBPS/content.opf"
    media-type="application/oebps-package+xml"/></rootfiles>
</container>
"""

# Filler paragraphs. Enough of them to push "Section 1.2" past the first
# A5 slice no matter how the reflow layout turns out, so the fragment target
# is always on a later page of chapter one.
FILLER_BEFORE = "\n".join(
    "<p>filler paragraph %d</p>" % i for i in range(1, 36))
FILLER_AFTER = "\n".join(
    "<p>trailing filler paragraph %d</p>" % i for i in range(1, 36))

# Chapter one spans several A5 slices: the filler paragraphs push "Section
# 1.2" onto a later page of the chapter, where it becomes a mid-page anchor.
CH1_XHTML = """<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml">
<head><title>Chapter One</title></head>
<body>
<h2 id="sec-1-1">Section 1.1</h2>
<p>Body of section 1.1.</p>
%s
<h2 id="sec-1-2">Section 1.2</h2>
<p>Body of section 1.2.</p>
%s
</body></html>
""" % (FILLER_BEFORE, FILLER_AFTER)

CH2_XHTML = """<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml">
<head><title>Chapter Two</title></head>
<body>
<h2 id="sec-2-1">Section 2.1</h2>
<p>Body of section 2.1.</p>
<p>Back to <a href="ch1.xhtml#sec-1-2">Section 1.2</a>.</p>
</body></html>
"""

# --- EPUB3: navigation document -------------------------------------------

OPF_NAV3 = """<?xml version="1.0"?>
<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="uid">
 <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
  <dc:identifier id="uid">urn:uuid:epub-toc-nav3</dc:identifier>
  <dc:title>EPUB TOC nav3</dc:title>
  <dc:creator>wlx-multidoc-viewer</dc:creator>
  <meta property="dcterms:modified">2026-01-01T00:00:00Z</meta>
 </metadata>
 <manifest>
  <item id="ch1" href="ch1.xhtml" media-type="application/xhtml+xml"/>
  <item id="ch2" href="ch2.xhtml" media-type="application/xhtml+xml"/>
  <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>
 </manifest>
 <spine><itemref idref="ch1"/><itemref idref="ch2"/></spine>
</package>
"""

NAV3_XHTML = """<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">
<head><title>Contents</title></head>
<body><nav epub:type="toc"><h2>Content</h2>
<ol>
<li><a href="ch1.xhtml">Chapter One</a>
<ol><li><a href="ch1.xhtml#sec-1-2">Section 1.2</a></li></ol>
</li>
<li><a href="ch2.xhtml">Chapter Two</a></li>
</ol>
</nav></body></html>
"""

# --- EPUB2: NCX ----------------------------------------------------------

OPF_NCX2 = """<?xml version="1.0"?>
<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="uid">
 <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
  <dc:identifier id="uid">urn:uuid:epub-toc-ncx2</dc:identifier>
  <dc:title>EPUB TOC ncx2</dc:title>
  <dc:creator>wlx-multidoc-viewer</dc:creator>
 </metadata>
 <manifest>
  <item id="ch1" href="ch1.xhtml" media-type="application/xhtml+xml"/>
  <item id="ch2" href="ch2.xhtml" media-type="application/xhtml+xml"/>
  <item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>
 </manifest>
 <spine toc="ncx"><itemref idref="ch1"/><itemref idref="ch2"/></spine>
</package>
"""

NCX2 = """<?xml version="1.0"?>
<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">
<head><meta name="dtb:uid" content="urn:uuid:epub-toc-ncx2"/></head>
<docTitle><text>EPUB TOC ncx2</text></docTitle>
<navMap>
<navPoint id="np-1" playOrder="1"><navLabel><text>Chapter One</text></navLabel>
<content src="ch1.xhtml"/>
<navPoint id="np-1-2" playOrder="2"><navLabel><text>Section 1.2</text></navLabel>
<content src="ch1.xhtml#sec-1-2"/></navPoint>
</navPoint>
<navPoint id="np-2" playOrder="3"><navLabel><text>Chapter Two</text></navLabel>
<content src="ch2.xhtml"/></navPoint>
</navMap>
</ncx>
"""


def main():
    os.makedirs(EXAMPLES, exist_ok=True)

    write_epub(os.path.join(EXAMPLES, "epub-toc-nav3.epub"), [
        ("META-INF/container.xml", CONTAINER),
        ("OEBPS/content.opf", OPF_NAV3),
        ("OEBPS/nav.xhtml", NAV3_XHTML),
        ("OEBPS/ch1.xhtml", CH1_XHTML),
        ("OEBPS/ch2.xhtml", CH2_XHTML),
    ])

    write_epub(os.path.join(EXAMPLES, "epub-toc-ncx2.epub"), [
        ("META-INF/container.xml", CONTAINER),
        ("OEBPS/content.opf", OPF_NCX2),
        ("OEBPS/toc.ncx", NCX2),
        ("OEBPS/ch1.xhtml", CH1_XHTML),
        ("OEBPS/ch2.xhtml", CH2_XHTML),
    ])


if __name__ == "__main__":
    main()
