#include "document.h"
#include "mupdfengine.h"
#include "djvuengine.h"

#include <QFileInfo>

std::unique_ptr<DocumentEngine> createEngine(const QString& path) {
    QString suffix = QFileInfo(path).suffix().toLower();

    if (suffix == "djvu" || suffix == "djv")
        return std::make_unique<DjVuEngine>();

    return std::make_unique<MuPdfEngine>();
}
