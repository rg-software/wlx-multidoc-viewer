#include "document.h"
#include "mupdfengine.h"
#include "djvuengine.h"
#include "chmengine.h"
#include "comicengine.h"
#include "imageengine.h"

#include <QFileInfo>
#include <QDebug>

// Raster image suffixes handled by ImageEngine. Only formats this Qt build
// decodes natively via QImageReader are listed (png/bmp built-in; jpg/gif/ico
// via installed image plugins); TIFF is a document handled by the default MuPDF
// engine, so it intentionally falls through below. WEBP currently has no decoder
// (neither Qt nor MuPDF) and is not routed anywhere.
bool isRasterSuffix(const QString& suffix) {
    return suffix == "jpg" || suffix == "jpeg" || suffix == "png" ||
           suffix == "gif" || suffix == "bmp" || suffix == "ico";
}

std::unique_ptr<DocumentEngine> createEngine(const QString& path) {
    QString suffix = QFileInfo(path).suffix().toLower();

    if (suffix == "djvu" || suffix == "djv") {
        qDebug() << "createEngine: DjVu engine for" << path;
        return std::make_unique<DjVuEngine>();
    }

    if (suffix == "chm") {
        qDebug() << "createEngine: CHM engine for" << path;
        return std::make_unique<ChmEngine>();
    }

    if (suffix == "cbr" || suffix == "cb7") {
        qDebug() << "createEngine: Comic engine for" << path;
        return std::make_unique<ComicEngine>();
    }

    if (isRasterSuffix(suffix)) {
        qDebug() << "createEngine: Image engine for" << path;
        return std::make_unique<ImageEngine>();
    }

    qDebug() << "createEngine: MuPDF engine for" << path;
    return std::make_unique<MuPdfEngine>();
}
