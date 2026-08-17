#ifndef DJVUENGINE_H
#define DJVUENGINE_H

#include "document.h"

#include <djvulibre/ddjvuapi.h>
#include <QString>
#include <QVector>

class DjVuEngine : public DocumentEngine {
public:
    DjVuEngine();
    ~DjVuEngine() override;

    bool open(const QString& path) override;
    void close() override;
    bool isOpen() const override;

    int pageCount() const override;
    QImage renderPage(int page, float zoom) override;
    QString extractText(int page) override;
    QString metadata(const QString& key) const override;
    QVector<OutlineItem> outline() const override;
    PageInfo pageDimensions(int page) const override;

private:
    ddjvu_context_t* m_ctx = nullptr;
    ddjvu_document_t* m_doc = nullptr;
    int m_pageCount = 0;
    QString m_path;
};

#endif // DJVUENGINE_H
