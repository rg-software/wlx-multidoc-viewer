#!/usr/bin/env python3
"""Generate .fb2.zip sample fixtures for the harness_fb2zip harness.

Produces (relative to the repo root):
  examples/sample.fb2.zip - the existing examples/sample.fb2 zipped under its
                            own name; the supported case.
  examples/not-fb2.zip    - a zip with no FictionBook entry; must decline.
  examples/fake-fb2.zip   - a zip whose only .fb2-named entry is not actually
                            FictionBook content; must decline.
  examples/multi.fb2.zip  - two valid FictionBook entries written in the wrong
                            order (chapter10 before chapter2); the engine must
                            pick chapter2 by natural order, proving the probe
                            sorts entries instead of trusting zip order.

The generated fixtures are committed, so this script is only needed to
regenerate them.
"""

import os
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXAMPLES = os.path.join(ROOT, "examples")


def write_zip(path, entries):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        for name, payload in entries:
            z.writestr(name, payload)
    print("wrote", path)


def fb2_doc(marker):
    return (
        '<?xml version="1.0" encoding="utf-8"?>\n'
        '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0">\n'
        '  <body><title><p>Fixture</p></title>\n'
        '  <section><title><p>Section</p></title><p>%s</p></section>\n'
        "  </body>\n"
        "</FictionBook>\n"
    ) % marker


def main():
    os.makedirs(EXAMPLES, exist_ok=True)

    with open(os.path.join(EXAMPLES, "sample.fb2"), "rb") as f:
        fb2 = f.read()
    write_zip(os.path.join(EXAMPLES, "sample.fb2.zip"), [("sample.fb2", fb2)])

    write_zip(os.path.join(EXAMPLES, "not-fb2.zip"), [
        ("readme.txt", "This archive contains no FictionBook entry.\n"),
    ])

    write_zip(os.path.join(EXAMPLES, "fake-fb2.zip"), [
        ("book.fb2", '<?xml version="1.0"?>\n'
                     "<html><body>Not a FictionBook.</body></html>\n"),
    ])

    # Written with chapter10 first so central-directory order differs from
    # natural order; the engine must pick chapter2.
    write_zip(os.path.join(EXAMPLES, "multi.fb2.zip"), [
        ("chapter10.fb2", fb2_doc("TENTH CHAPTER MARKER.")),
        ("chapter2.fb2", fb2_doc("SECOND CHAPTER MARKER.")),
    ])


if __name__ == "__main__":
    main()