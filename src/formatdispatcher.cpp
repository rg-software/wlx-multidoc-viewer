#include "document.h"
#include "mupdfengine.h"
#include "djvuengine.h"
#include "chmengine.h"
#include "comicengine.h"
#include "imageengine.h"

#include <QFileInfo>
#include <QDebug>

// Raster image suffixes handled by ImageEngine (the proposal's list). When
// ImageEngine cannot decode a file, createEngine returns a MuPDF engine so no
// currently-openable raster regresses.
bool isRasterSuffix(const QString& suffix) {
    return suffix == "jpg" || suffix == "jpeg" || suffix == "png" ||
           suffix == "gif" || suffix == "tif" || suffix == "tiff" ||
           suffix == "bmp" || suffix == "webp";
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
