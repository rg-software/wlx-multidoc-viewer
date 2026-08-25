#ifndef PRINT_QT_H
#define PRINT_QT_H

class ViewerController;
class QWidget;

// Opens the Qt-native print dialog and spools the chosen range at the printer's
// resolution through the shared render math (design D7; see print_qt.cpp).
// Returns without side effects when the user cancels.
bool printDocumentQt(QWidget* parent, ViewerController* controller);

#endif // PRINT_QT_H