#ifndef TOOLBAR_ICONS_H
#define TOOLBAR_ICONS_H

#include "toolbar.h"
#include "la_fontdata.h"

#include <QImage>

// Toolbar icon glyphs, single source for both backends.
//
// Preferred: Line Awesome Free (MIT/OFL) glyphs rasterized into a QImage from
// the embedded font. Fallback: the built-in programmatic vector shapes, used
// when the glyph is absent from the loaded font or no ink is produced. Both
// render at exactly `pixelSize` device pixels (so they are sharp at any DPI);
// the Win32 backend converts to an HBITMAP and the Qt backend to a QPixmap.
namespace toolbar {

QImage makeIcon(Icon icon, int pixelSize);

} // namespace toolbar

#endif // TOOLBAR_ICONS_H