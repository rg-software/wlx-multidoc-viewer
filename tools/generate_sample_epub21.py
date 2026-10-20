#!/usr/bin/env python3
"""Generate issue #21 (epub issues) repro fixtures.

Produces (relative to the repo root, committed):
  examples/epub21-links.epub      - EPUB3 with TOC nav + endnotes
                                    (text.xhtml <-> notes.xhtml#note-1)
  examples/epub21-fb2notes.fb2    - FB2 with l:href endnote
  examples/epub21-bad-ncx.epub    - EPUB2 whose toc.ncx has the reporter's
                                    <contentsrc="..."/> typo (well-formed XML,
                                    wrong tag name)
  examples/epub21-unclosed.epub   - EPUB2 whose toc.ncx has a truly unclosed
                                    tag (malformed XML)
  examples/epub21-txt.epub        - valid EPUB + extra .txt entries
                                    (OEBPS/about.txt, OEBPS/text/list.txt,
                                     META-INF/calibre_bookmarks.txt)

Regenerate with: python tools/generate_sample_epub21.py
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

OPF_LINKS = """<?xml version="1.0"?>
<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="uid">
 <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
  <dc:identifier id="uid">urn:uuid:epub21-links</dc:identifier>
  <dc:title>EPUB21 links</dc:title>
  <dc:creator>wlx-multidoc-viewer</dc:creator>
  <meta property="dcterms:modified">2026-01-01T00:00:00Z</meta>
 </metadata>
 <manifest>
  <item id="text" href="text.xhtml" media-type="application/xhtml+xml"/>
  <item id="notes" href="notes.xhtml" media-type="application/xhtml+xml"/>
  <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>
 </manifest>
 <spine><itemref idref="text"/><itemref idref="notes"/></spine>
</package>
"""

NAV_XHTML = """<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">
<head><title>Contents</title></head>
<body><nav epub:type="toc"><h2>Content</h2>
<ol>
<li><a href="text.xhtml">TEXT</a>
<ol><li><a href="text.xhtml#text-1">CHAPTER ONE</a></li></ol>
</li>
<li><a href="notes.xhtml">NOTES</a></li>
</ol>
</nav></body></html>
"""

TEXT_XHTML = """<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">
<head><title>Text</title></head>
<body>
<h2 id="text-1">Chapter one</h2>
<p>text<sup><a epub:type="endnote" id="note-1" href="notes.xhtml#note-1">[1]</a></sup></p>
<p style="margin-top:600px">padding to force pagination</p>
</body></html>
"""

NOTES_XHTML = """<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml">
<head><title>Notes</title></head>
<body>
<h2>Notes</h2>
<p><a id="note-1" href="text.xhtml#note-1">[1]</a> return back to text</p>
</body></html>
"""

FB2_NOTES = """<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"
             xmlns:l="http://www.w3.org/1999/xlink">
 <body>
  <section><title><p>Text</p></title>
  <p>text<sup><a type="note" l:href="#note1">[1]</a></sup></p>
  </section>
 </body>
 <body name="notes">
  <section><title><p>Notes</p></title></section>
  <section id="note1"><p>[1] Note text.</p></section>
 </body>
</FictionBook>
"""

OPF_NCX = """<?xml version="1.0"?>
<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="uid">
 <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
  <dc:identifier id="uid">urn:uuid:epub21-ncx</dc:identifier>
  <dc:title>EPUB21 ncx</dc:title>
  <dc:creator>wlx-multidoc-viewer</dc:creator>
 </metadata>
 <manifest>
  <item id="ch1" href="chapter1.xhtml" media-type="application/xhtml+xml"/>
  <item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>
 </manifest>
 <spine toc="ncx"><itemref idref="ch1"/></spine>
</package>
"""

CHAPTER1 = """<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml">
<head><title>Ch1</title></head>
<body><h2>Chapter 1</h2><p>Body text.</p></body></html>
"""

NCX_GOOD = """<?xml version="1.0" encoding="utf-8"?>
<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">
<head><meta name="dtb:uid" content="urn:uuid:epub21-ncx"/></head>
<docTitle><text>EPUB21 ncx</text></docTitle>
<navMap>
<navPoint id="ch1"><navLabel><text>Chapter 1</text></navLabel>
<content src="chapter1.xhtml"/></navPoint>
</navMap>
</ncx>
"""

# Reporter's typo: well-formed XML, wrong tag name (no src attribute).
NCX_BAD_TAG = NCX_GOOD.replace("<content src=", "<contentsrc=")

# Truly malformed XML: unclosed navLabel element.
NCX_UNCLOSED = NCX_GOOD.replace("</navLabel>", "")


def main():
    os.makedirs(EXAMPLES, exist_ok=True)

    write_epub(os.path.join(EXAMPLES, "epub21-links.epub"), [
        ("META-INF/container.xml", CONTAINER),
        ("OEBPS/content.opf", OPF_LINKS),
        ("OEBPS/nav.xhtml", NAV_XHTML),
        ("OEBPS/text.xhtml", TEXT_XHTML),
        ("OEBPS/notes.xhtml", NOTES_XHTML),
    ])

    with open(os.path.join(EXAMPLES, "epub21-fb2notes.fb2"), "w",
              encoding="utf-8", newline="\n") as f:
        f.write(FB2_NOTES)
    print("wrote", os.path.join(EXAMPLES, "epub21-fb2notes.fb2"))

    base_ncx = [("META-INF/container.xml", CONTAINER),
                ("OEBPS/content.opf", OPF_NCX),
                ("OEBPS/chapter1.xhtml", CHAPTER1)]
    write_epub(os.path.join(EXAMPLES, "epub21-bad-ncx.epub"),
               base_ncx + [("OEBPS/toc.ncx", NCX_BAD_TAG)])
    write_epub(os.path.join(EXAMPLES, "epub21-unclosed.epub"),
               base_ncx + [("OEBPS/toc.ncx", NCX_UNCLOSED)])

    write_epub(os.path.join(EXAMPLES, "epub21-txt.epub"), [
        ("META-INF/container.xml", CONTAINER),
        ("OEBPS/content.opf", OPF_LINKS),
        ("OEBPS/nav.xhtml", NAV_XHTML),
        ("OEBPS/text.xhtml", TEXT_XHTML),
        ("OEBPS/notes.xhtml", NOTES_XHTML),
        ("OEBPS/about.txt", "about file, not in manifest\n"),
        ("OEBPS/text/list.txt", "list file, not in manifest\n"),
        ("META-INF/calibre_bookmarks.txt",
         "eyJib29rbWFya3MiOiBbXX0=  /* base64 json, calibre progress */\n"),
    ])

    # Reporter's EXACT malformed TOC shape as body content: a nested <ol>
    # directly inside <ol> (must be inside <li>) plus an unclosed <a>.
    OPF_MALFORMED = OPF_LINKS.replace(
        '  <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>\n',
        '')
    TEXT_MALFORMED = (
        '<?xml version="1.0" encoding="utf-8"?>\n'
        '<!DOCTYPE html>\n'
        '<html xmlns="http://www.w3.org/1999/xhtml">\n'
        '<head><title>Text</title></head>\n'
        '<body>\n'
        '<section id="content">\n'
        '<h2>Content</h2>\n'
        '<ol class="nest">\n'
        '<li class="nest"><a href="notes.xhtml">NOTES</a></li>\n'
        '<li class="nest"><a href="text.xhtml">TEXT</a></li>\n'
        '<ol class="nest">\n'
        '<li class="nest"><a href="text.xhtml#self">CHAPTER ONE</a></li>\n'
        '</ol>\n'
        '<li class="nest"><a href="notes.xhtml">NOTES2</a></li>\n'
        '<li class="nest"><a href="notes.xhtml">REFERENCES</a</li>\n'
        '</ol>\n'
        '</section>\n'
        '<h2 id="self">Chapter one</h2>\n'
        '<p>body</p>\n'
        '</body></html>\n'
    )
    write_epub(os.path.join(EXAMPLES, "epub21-malformed-links.epub"), [
        ("META-INF/container.xml", CONTAINER),
        ("OEBPS/content.opf", OPF_MALFORMED),
        ("OEBPS/text.xhtml", TEXT_MALFORMED),
        ("OEBPS/notes.xhtml", NOTES_XHTML),
    ])

    # Bisect variants for the Ctrl+Q inner-.txt hijack (issue #21.3):
    # which inner file makes TC bypass lister plugins for the .epub itself?
    links_base = [
        ("META-INF/container.xml", CONTAINER),
        ("OEBPS/content.opf", OPF_LINKS),
        ("OEBPS/nav.xhtml", NAV_XHTML),
        ("OEBPS/text.xhtml", TEXT_XHTML),
        ("OEBPS/notes.xhtml", NOTES_XHTML),
    ]
    write_epub(os.path.join(EXAMPLES, "epub21-txtonly-meta.epub"), links_base + [
        ("META-INF/calibre_bookmarks.txt",
         "eyJib29rbWFya3MiOiBbXX0=\n"),
    ])
    write_epub(os.path.join(EXAMPLES, "epub21-txtonly-oebps.epub"), links_base + [
        ("OEBPS/about.txt", "about file, not in manifest\n"),
    ])


if __name__ == "__main__":
    main()
