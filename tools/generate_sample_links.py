#!/usr/bin/env python3
"""Generate hyperlink sample fixtures for the add-hyperlink-navigation harness.

Produces (relative to the repo root):
  examples/sample-links.pdf  - a 2-page PDF with an internal page link, an
                               external URI link, and an internal link carrying
                               an XYZ anchor coordinate.
  examples/sample-links.chm  - a 2-topic CHM whose home topic links to the
                               second topic, to a #fragment anchor in it, and to
                               an external URI (Windows only; requires hhc.exe
                               from HTML Help Workshop).

The generated fixtures are committed, so this script is only needed to
regenerate them.
"""

import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXAMPLES = os.path.join(ROOT, "examples")

PDF_PATH = os.path.join(EXAMPLES, "sample-links.pdf")
CHM_PATH = os.path.join(EXAMPLES, "sample-links.chm")

# Page-2 anchor y (PDF user space, y-down from the top in PyMuPDF), chosen well
# below the top so the extracted anchor is clearly non-zero.
ANCHOR_Y = 500.0


def generate_pdf():
    import pymupdf

    doc = pymupdf.open()
    doc.new_page()
    doc.new_page()
    # Fetch after creating both: holding a Page across new_page() invalidates it.
    page1 = doc[0]
    page2 = doc[1]

    page1.insert_text((72, 72), "Go to page 2", fontsize=14)
    page1.insert_link({
        "kind": pymupdf.LINK_GOTO,
        "from": pymupdf.Rect(60, 58, 220, 88),
        "page": 1,                      # zero-based -> page 2
    })

    page1.insert_text((72, 140), "Example site", fontsize=14)
    page1.insert_link({
        "kind": pymupdf.LINK_URI,
        "from": pymupdf.Rect(60, 126, 220, 156),
        "uri": "https://example.com",
    })

    page1.insert_text((72, 220), "Jump to the anchor", fontsize=14)
    page1.insert_link({
        "kind": pymupdf.LINK_GOTO,
        "from": pymupdf.Rect(60, 206, 260, 236),
        "page": 1,                      # zero-based -> page 2
        "to": pymupdf.Point(72, ANCHOR_Y),
    })

    page2.insert_text((72, 72), "Second page", fontsize=14)
    page2.insert_text((72, ANCHOR_Y), "Anchor target", fontsize=14)

    doc.save(PDF_PATH)
    doc.close()
    print("wrote", PDF_PATH)


HHP_TEMPLATE = """[OPTIONS]
Compatibility=1.1 or later
Compiled file={compiled}
Contents file=index.hhc
Default topic=index.htm
Title=Sample Links
[FILES]
index.htm
page2.htm
"""

INDEX_HTML = """<!DOCTYPE html>
<html><head><title>Sample Links Home</title></head>
<body>
<h1>Sample Links Home</h1>
<p><a href="page2.htm">Go to page 2</a></p>
<p><a href="page2.htm#anchor">Jump to the anchor</a></p>
<p><a href="https://example.com">Example site</a></p>
</body></html>
"""

PAGE2_HTML = """<!DOCTYPE html>
<html><head><title>Page 2</title></head>
<body>
<h1>Second page</h1>
<p><a name="anchor"></a>Anchor target</p>
<p style="margin-top: 500px;">Bottom of page 2</p>
</body></html>
"""

HHC = """<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
<HTML><BODY>
<OBJECT type="text/sitemap">
<param name="Name" value="Home">
<param name="Local" value="index.htm">
</OBJECT>
<OBJECT type="text/sitemap">
<param name="Name" value="Page 2">
<param name="Local" value="page2.htm">
</OBJECT>
</BODY></HTML>
"""


def find_hhc():
    found = shutil.which("hhc.exe") or shutil.which("hhc")
    if found:
        return found
    for candidate in (
        r"C:\Program Files (x86)\HTML Help Workshop\hhc.exe",
        r"C:\Program Files\HTML Help Workshop\hhc.exe",
    ):
        if os.path.exists(candidate):
            return candidate
    return None


def generate_chm():
    hhc = find_hhc()
    if not hhc:
        print("skip CHM: hhc.exe (HTML Help Workshop) not found", file=sys.stderr)
        return

    with tempfile.TemporaryDirectory(prefix="wlx-chm-fixture-") as tmp:
        with open(os.path.join(tmp, "index.htm"), "w", encoding="ascii") as f:
            f.write(INDEX_HTML)
        with open(os.path.join(tmp, "page2.htm"), "w", encoding="ascii") as f:
            f.write(PAGE2_HTML)
        with open(os.path.join(tmp, "index.hhc"), "w", encoding="ascii") as f:
            f.write(HHC)
        with open(os.path.join(tmp, "sample-links.hhp"), "w", encoding="ascii") as f:
            f.write(HHP_TEMPLATE.format(compiled="sample-links.chm"))

        result = subprocess.run([hhc, "sample-links.hhp"], cwd=tmp,
                                capture_output=True, text=True)
        built = os.path.join(tmp, "sample-links.chm")
        if result.returncode != 0 or not os.path.exists(built):
            print("FAILED to compile CHM:", result.stdout, result.stderr, file=sys.stderr)
            sys.exit(1)
        shutil.copyfile(built, CHM_PATH)
    print("wrote", CHM_PATH)


def main():
    os.makedirs(EXAMPLES, exist_ok=True)
    generate_pdf()
    generate_chm()


if __name__ == "__main__":
    main()
