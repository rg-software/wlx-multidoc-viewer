#include "toolbar_qt.h"
#include "toolbar_icons.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QHash>
#include <QIcon>

namespace {

toolbar::Icon defaultIconFor(toolbar::Control c) {
    using namespace toolbar;
    switch (c) {
    case Control::PrevPage:      return Icon::Prev;
    case Control::NextPage:      return Icon::Next;
    case Control::ModeToggle:    return Icon::ModePaged;
    case Control::FitButton:     return Icon::FitPage;
    case Control::RotateLeft:    return Icon::RotateLeft;
    case Control::RotateRight:   return Icon::RotateRight;
    case Control::ZoomOut:       return Icon::ZoomOut;
    case Control::ZoomIn:        return Icon::ZoomIn;
    case Control::FindPrev:      return Icon::FindPrev;
    case Control::FindNext:      return Icon::FindNext;
    case Control::MatchCase:     return Icon::MatchCase;
    case Control::Print:         return Icon::Print;
    case Control::Copy:          return Icon::Copy;
    case Control::SidebarToggle: return Icon::SidebarToggle;
    default: return Icon::Prev;
    }
}

QIcon makeIcon(toolbar::Icon icon) {
    return QIcon(QPixmap::fromImage(toolbar::makeIcon(icon, viewer_settings::kIconBaseSize)));
}

template <typename T>
T* as(QWidget* w) {
    return qobject_cast<T*>(w);
}

} // namespace

ToolbarQt::ToolbarQt(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 1, 6, 1);
    layout->setSpacing(4);

    toolbar::ToolbarPresenter* p = presenter();

    auto addButton = [&](toolbar::Control c, bool checkable, const QString& tooltip,
                         const std::function<void()>& click) {
        auto* b = new QToolButton(this);
        b->setIcon(makeIcon(defaultIconFor(c)));
        b->setIconSize(QSize(viewer_settings::kIconBaseSize, viewer_settings::kIconBaseSize));
        b->setAutoRaise(true);
        b->setCheckable(checkable);
        b->setToolTip(tooltip);
        b->setFixedHeight(viewer_settings::kToolbarBaseHeight - 6);
        connect(b, &QToolButton::clicked, this, [click](bool) { if (click) click(); });
        layout->addWidget(b);
        m_ctl.insert(c, b);
    };
    auto addEdit = [&](toolbar::Control c, int width, Qt::Alignment align = Qt::AlignCenter) {
        auto* e = new QLineEdit(this);
        e->setFixedWidth(width);
        e->setAlignment(align);
        layout->addWidget(e);
        m_ctl.insert(c, e);
    };
    auto addLabel = [&](toolbar::Control c) {
        auto* l = new QLabel(this);
        l->setAlignment(Qt::AlignCenter);
        l->setMinimumWidth(28);
        layout->addWidget(l);
        m_ctl.insert(c, l);
    };
    auto addSeparator = [&]() { layout->addSpacing(8); };

    addButton(toolbar::Control::SidebarToggle, true, QObject::tr("Toggle outline sidebar"), [p] { p->onSidebarToggled(); });
    addSeparator();

    addButton(toolbar::Control::Print, false, QObject::tr("Print"), [p] { p->onPrint(); });
    addSeparator();

    addButton(toolbar::Control::PrevPage, false, QObject::tr("Previous page"), [p] { p->onPrevPage(); });
    addEdit(toolbar::Control::PageBox, 40, Qt::AlignRight|Qt::AlignVCenter);
    addLabel(toolbar::Control::PageCount);
    addButton(toolbar::Control::NextPage, false, QObject::tr("Next page"), [p] { p->onNextPage(); });
    addSeparator();

    addButton(toolbar::Control::ModeToggle, true, QObject::tr("Toggle paged / continuous"), [p] { p->onModeToggled(); });
    addButton(toolbar::Control::FitButton, false, QObject::tr("Fit mode (manual / page / width)"), [p] { p->onFitCycled(); });
    addSeparator();

    addButton(toolbar::Control::RotateLeft, false, QObject::tr("Rotate left"), [p] { p->onRotateLeft(); });
    addButton(toolbar::Control::RotateRight, false, QObject::tr("Rotate right"), [p] { p->onRotateRight(); });
    addButton(toolbar::Control::ZoomOut, false, QObject::tr("Zoom out"), [p] { p->onZoomOut(); });
    addButton(toolbar::Control::ZoomIn, false, QObject::tr("Zoom in"), [p] { p->onZoomIn(); });
    addSeparator();

addEdit(toolbar::Control::FindBox, 110);
    addButton(toolbar::Control::FindPrev, false, QObject::tr("Previous match"), [p] { p->onFindPrev(); });
    addButton(toolbar::Control::FindNext, false, QObject::tr("Next match"), [p] { p->onFindNext(); });
    addButton(toolbar::Control::MatchCase, true, QObject::tr("Match case"), [p] { (void)p; });
    // Copy directly after Match case - no spacer.
    addButton(toolbar::Control::Copy, false, QObject::tr("Copy (text selection arrives in a future change)"), [p] { p->onCopy(); });
    addLabel(toolbar::Control::FindStatus);   // "n / m" at the rightmost end
    layout->addStretch(1);

    setFixedHeight(viewer_settings::kToolbarBaseHeight);

    connect(as<QLineEdit>(m_ctl.value(toolbar::Control::PageBox)), &QLineEdit::editingFinished,
            this, [this] { presenter()->onGoToPageCommitted(editText(toolbar::Control::PageBox)); });
    connect(as<QLineEdit>(m_ctl.value(toolbar::Control::FindBox)), &QLineEdit::editingFinished,
            this, [this] { presenter()->onFindCommitted(editText(toolbar::Control::FindBox)); });
    connect(as<QToolButton>(m_ctl.value(toolbar::Control::MatchCase)), &QToolButton::toggled,
            this, [this](bool on) { presenter()->onMatchCaseToggled(on); });
}

ToolbarQt::~ToolbarQt() = default;

void ToolbarQt::setEnabled(toolbar::Control c, bool on) {
    if (QWidget* w = m_ctl.value(c, nullptr))
        w->setEnabled(on);
}

void ToolbarQt::setChecked(toolbar::Control c, bool on) {
    if (auto* b = button(c))
        b->setChecked(on);
}

void ToolbarQt::setIcon(toolbar::Control c, toolbar::Icon icon) {
    if (auto* b = button(c))
        b->setIcon(makeIcon(icon));
}

void ToolbarQt::setText(toolbar::Control c, const QString& text) {
    if (auto* l = as<QLabel>(m_ctl.value(c)))
        l->setText(text);
}

void ToolbarQt::setEditText(toolbar::Control c, const QString& text) {
    if (auto* e = as<QLineEdit>(m_ctl.value(c)))
        e->setText(text);
}

QString ToolbarQt::editText(toolbar::Control c) const {
    if (auto* e = qobject_cast<QLineEdit*>(m_ctl.value(c)))
        return e->text();
    return QString();
}

bool ToolbarQt::isEditFocused() const {
    const QWidget* w = QApplication::focusWidget();
    if (!w)
        return false;
    return w == m_ctl.value(toolbar::Control::PageBox) ||
           w == m_ctl.value(toolbar::Control::FindBox);
}