#include "viewercontroller.h"
#include "print_qt.h"
#include "printcoordinator.h"

#include <QImage>
#include <QMessageBox>
#include <QPainter>
#include <QPrinter>
#include <QPrintDialog>
#include <algorithm>

static QImage renderPage(QWidget* parent, ViewerController* c, int page,
                         const QSizeF& printableDots, float rotation) {
    QImage img;
    if (!c || c->pageCount() <= 0)
        return img;
    PageInfo info = c->engine()->pageDimensions(page);
    if (info.width <= 0 || info.height <= 0 || printableDots.width() <= 0)
        return img;
    double pw = info.width;
    double ph = info.height;
    if (rotation == 90 || rotation == 270)
        std::swap(pw, ph);
    const double scale = std::min(static_cast<double>(printableDots.width()) / pw,
                                  static_cast<double>(printableDots.height()) / ph);
    return c->engine()->renderPage(page, static_cast<float>(scale), 1.0f, static_cast<int>(rotation));
}

bool printDocumentQt(QWidget* parent, ViewerController* controller) {
    if (!controller || !controller->hasDocument())
        return false;

    QPrinter printer(QPrinter::HighResolution);
    printer.setFromTo(1, controller->pageCount());

    QPrintDialog dlg(&printer, parent);
    if (dlg.exec() != QDialog::Accepted)
        return false; // cancelled: no side effects

    QPainter painter;
    if (!painter.begin(&printer)) {
        QMessageBox::warning(parent, QObject::tr("Print"),
                             QObject::tr("Printing failed: the printer rejected the document."));
        return false;
    }

    const QRectF printable = printer.pageRect(QPrinter::DevicePixel);
    const float rotation = static_cast<float>(controller->rotation());

    const QPrinter::PrintRange range = printer.printRange();
    const int first = (range == QPrinter::PageRange) ? printer.fromPage() : 1;
    const int last = (range == QPrinter::PageRange)
                         ? printer.toPage()
                         : controller->pageCount();

    for (int page = first; page <= last; ++page) {
        const QImage img = renderPage(parent, controller, page, printable.size(), rotation);
        if (!img.isNull()) {
            QRectF dst = printable;
            const double scale = std::min(printable.width() / img.width(),
                                          printable.height() / img.height());
            const double dw = img.width() * scale;
            const double dh = img.height() * scale;
            dst = QRectF(printable.x() + (printable.width() - dw) / 2.0,
                         printable.y() + (printable.height() - dh) / 2.0,
                         dw, dh);
            painter.drawImage(dst, img);
        }
        if (page < last)
            printer.newPage();
    }

    painter.end();
    return true;
}