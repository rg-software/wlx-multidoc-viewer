#ifndef TOOLBAR_QT_H
#define TOOLBAR_QT_H

#include "toolbar.h"
#include "viewer_settings.h"

#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QWidget>
#include <QMap>

// Qt toolbar backend: a widget row of QToolButtons / QLineEdits / QLabels bound
// through the same ToolbarPresenter as the Win32 backend (design D1). Built
// only on Linux targets where Qt Widgets is linked.
class ToolbarQt : public QWidget, public toolbar::ToolbarBackend {
public:
    explicit ToolbarQt(QWidget* parent = nullptr);
    ~ToolbarQt() override;

    // --- ToolbarBackend ---
    void setEnabled(toolbar::Control c, bool on) override;
    void setChecked(toolbar::Control c, bool on) override;
    void setIcon(toolbar::Control c, toolbar::Icon icon) override;
    void setText(toolbar::Control c, const QString& text) override;
    void setEditText(toolbar::Control c, const QString& text) override;
    QString editText(toolbar::Control c) const override;
    bool isEditFocused() const override;

private:
    QToolButton* button(toolbar::Control c) const { return qobject_cast<QToolButton*>(m_ctl.value(c)); }
    void setButtonIcons();

    QHash<toolbar::Control, QWidget*> m_ctl;
};

#endif // TOOLBAR_QT_H