#include "document.h"
#include "mupdfengine.h"
#include "djvuengine.h"
#include "chmengine.h"

#include <QFileInfo>
#include <QDebug>

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

    qDebug() << "createEngine: MuPDF engine for" << path;
    return std::make_unique<MuPdfEngine>();
}
