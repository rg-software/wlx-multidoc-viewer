#ifndef IMAGEFOLDER_H
#define IMAGEFOLDER_H

#include <QString>
#include <QVector>

// Sibling discovery for standalone raster images: lists the raster image files
// in a directory, sorts them in deterministic natural order (shared with the
// comic archive engine), and resolves a file's position in that ordering so
// next/prev can cross from one standalone image document to its neighbour.
class ImageFolder {
public:
    // Scans `dir` for raster images (jpg/jpeg/png/gif/tif/tiff/bmp/webp,
    // case-insensitive), sorted naturally. A missing/unreadable directory
    // yields an empty list.
    void scan(const QString& dir);

    bool isEmpty() const { return m_files.isEmpty(); }
    int count() const { return m_files.size(); }

    // Absolute path of the file at natural-order index, or empty when out of
    // range.
    QString fileAt(int index) const {
        return (index >= 0 && index < m_files.size()) ? m_files.at(index) : QString();
    }
    // Index of `filePath` (absolute) in the natural order, or -1.
    int indexOf(const QString& filePath) const;
    bool contains(const QString& filePath) const { return indexOf(filePath) >= 0; }

private:
    QVector<QString> m_files;
};

#endif // IMAGEFOLDER_H