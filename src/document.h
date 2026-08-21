#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <QString>
#include <QImage>
#include <QVector>
#include <memory>

struct OutlineItem {
    QString title;
    int pageNo = 0;
    QVector<OutlineItem> children;
};

struct PageInfo {
    int width = 0;
    int height = 0;
};

class DocumentEngine {
public:
    virtual ~DocumentEngine() = default;

    virtual bool open(const QString& path) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    virtual int pageCount() const = 0;
    virtual QImage renderPage(int page, float zoom, float dpiScale = 1.0f, int rotation = 0) = 0;
    virtual QString extractText(int page) = 0;
    virtual QString metadata(const QString& key) const = 0;
    virtual QVector<OutlineItem> outline() const = 0;
    virtual PageInfo pageDimensions(int page) const = 0;
};

std::unique_ptr<DocumentEngine> createEngine(const QString& path);

#endif // DOCUMENT_H
