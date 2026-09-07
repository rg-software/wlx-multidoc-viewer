#include "imagefolder.h"
#include "naturalsort.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace {

bool isRasterName(const QString& name) {
    const QString suffix = QFileInfo(name).suffix().toLower();
    return suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg") ||
           suffix == QLatin1String("png") || suffix == QLatin1String("gif") ||
           suffix == QLatin1String("tif") || suffix == QLatin1String("tiff") ||
           suffix == QLatin1String("bmp") || suffix == QLatin1String("webp");
}

bool samePath(const QString& a, const QString& b) {
    return a.toLower() == b.toLower();
}

} // namespace

void ImageFolder::scan(const QString& dir) {
    m_files.clear();

    QDir directory(dir);
    if (!directory.exists())
        return;

    QVector<QString> names = directory.entryList(QDir::Files);
    for (const QString& name : names) {
        if (!isRasterName(name))
            continue;
        m_files.append(QDir::cleanPath(directory.absoluteFilePath(name)));
    }

    if (m_files.size() > 1) {
        std::sort(m_files.begin(), m_files.end(),
                  [](const QString& a, const QString& b) {
                      return naturalCompare(a, b) < 0;
                  });
    }
}

int ImageFolder::indexOf(const QString& filePath) const {
    for (int i = 0; i < m_files.size(); ++i) {
        if (samePath(m_files.at(i), filePath))
            return i;
    }
    return -1;
}