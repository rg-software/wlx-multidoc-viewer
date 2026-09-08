#include "sidebar_qt.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QApplication>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSet>

// Right-edge drag handle. Mouse drags mirror the Win32 grip: candidate widths are
// pushed through the shared notifyWidthChanged path so the viewer clamps and
// re-runs its chrome chain, keeping keyboard and mouse state in lockstep.
class SidebarQt::ResizeGrip : public QWidget {
public:
    explicit ResizeGrip(SidebarQt* owner)
        : QWidget(owner)
        , m_owner(owner)
    {
        setFixedWidth(viewer_settings::kSidebarGripWidthPx);
        setCursor(Qt::SizeHorCursor);
    }

protected:
    void mousePressEvent(QMouseEvent* ev) override {
        if (ev->button() == Qt::LeftButton) {
            m_owner->m_resizing = true;
            m_owner->m_tree->setCursor(Qt::SizeHorCursor);
            m_owner->notifyWidthChanged(m_owner->dragCandidateLogical(ev->globalPosition().toPoint()));
            ev->accept();
            return;
        }
        QWidget::mousePressEvent(ev);
    }

    void mouseMoveEvent(QMouseEvent* ev) override {
        if (m_owner->m_resizing) {
            m_owner->notifyWidthChanged(m_owner->dragCandidateLogical(ev->globalPosition().toPoint()));
            ev->accept();
            return;
        }
        QWidget::mouseMoveEvent(ev);
    }

    void mouseReleaseEvent(QMouseEvent* ev) override {
        if (ev->button() == Qt::LeftButton) {
            m_owner->m_resizing = false;
            m_owner->m_tree->unsetCursor();
            ev->accept();
            return;
        }
        QWidget::mouseReleaseEvent(ev);
    }

private:
    SidebarQt* m_owner = nullptr;
};

SidebarQt::SidebarQt(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setExpandsOnDoubleClick(true);
    m_tree->installEventFilter(this);
    layout->addWidget(m_tree, 1);

    m_grip = new ResizeGrip(this);
    layout->addWidget(m_grip, 0);

    const uint32_t sbg = viewer_settings::kSidebarBackground;
    const QColor sbgColor(static_cast<int>((sbg >> 16) & 0xFF),
                          static_cast<int>((sbg >> 8) & 0xFF),
                          static_cast<int>(sbg & 0xFF));

    // The tree viewport has its own palette copy at construction, so set both
    // explicitly; Base covers the tree rows, Window the surrounding strip.
    QPalette treePal = m_tree->palette();
    treePal.setColor(QPalette::Window, sbgColor);
    treePal.setColor(QPalette::Base, sbgColor);
    m_tree->setPalette(treePal);
    m_tree->viewport()->setPalette(treePal);
    m_tree->setAutoFillBackground(true);
    m_tree->viewport()->setAutoFillBackground(true);

    // Container background; the grip inherits this palette as a child widget.
    QPalette panelPal = palette();
    panelPal.setColor(QPalette::Window, sbgColor);
    setPalette(panelPal);
    setAutoFillBackground(true);

    setFixedWidth(viewer_settings::kSidebarInitialWidth);

    connect(m_tree, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem* item) {
        const int id = idOf(item);
        if (id >= 0 && presenter() && !m_materialized.contains(id))
            materialize(item, id);
    });
    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
        const int id = idOf(item);
        if (id >= 0 && presenter())
            presenter()->onEntryActivated(id);
    });
}

SidebarQt::~SidebarQt() = default;

// Candidate sidebar width in logical px from the pointer's global position,
// measured from the panel's left edge (Qt coordinates are already DPI-independent).
int SidebarQt::dragCandidateLogical(const QPoint& globalPos) const {
    return (std::max)(0, globalPos.x() - mapToGlobal(QPoint(0, 0)).x());
}

void SidebarQt::setWidth(int widthPx) {
    setFixedWidth(widthPx);
}

int SidebarQt::idOf(QTreeWidgetItem* item) const {
    return item ? item->data(0, Qt::UserRole).toInt() : -1;
}

QTreeWidgetItem* SidebarQt::insertItem(QTreeWidgetItem* parent, int id, const QString& title) {
    auto* it = new QTreeWidgetItem(parent);
    it->setText(0, title);
    it->setData(0, Qt::UserRole, id);
    m_items.insert(id, it);
    return it;
}

void SidebarQt::clearEntries() {
    m_tree->clear();
    m_items.clear();
    m_materialized.clear();
    m_tree->horizontalScrollBar()->setValue(0);
}

void SidebarQt::addEntry(int id, int parentId, const QString& title) {
    if (!presenter())
        return;
    if (parentId < 0) {
        QTreeWidgetItem* it = insertItem(nullptr, id, title);
        if (!presenter()->childrenOf(id).isEmpty()) {
            auto* ph = new QTreeWidgetItem(it);
            ph->setData(0, Qt::UserRole, -1);
        }
        m_tree->addTopLevelItem(it);
        return;
    }
    // Non-root entries are inserted lazily when their parent expands.
    Q_UNUSED(parentId)
}

void SidebarQt::materialize(QTreeWidgetItem* item, int id) {
    if (m_materialized.contains(id))
        return;
    m_materialized.insert(id);

    for (int i = 0; i < item->childCount();) {
        QTreeWidgetItem* child = item->child(i);
        if (idOf(child) < 0) {
            item->removeChild(child);
            delete child;
        } else {
            ++i;
        }
    }

    const QVector<int> kids = presenter()->childrenOf(id);
    for (int kid : kids) {
        const SidebarEntry* e = presenter()->entry(kid);
        if (!e)
            continue;
        QTreeWidgetItem* it = insertItem(item, kid, e->title);
        it->setExpanded(false);
        if (!presenter()->childrenOf(kid).isEmpty()) {
            auto* ph = new QTreeWidgetItem(it);
            ph->setData(0, Qt::UserRole, -1);
        }
    }
}

// Ensures the whole ancestor path to id is materialized (recursive) so the tree
// actually contains the item chain.
void SidebarQt::ensurePath(int id) {
    const SidebarEntry* e = presenter()->entry(id);
    if (!e)
        return;
    if (e->parentId >= 0) {
        ensurePath(e->parentId);
        if (QTreeWidgetItem* parent = m_items.value(e->parentId)) {
            if (!m_materialized.contains(e->parentId))
                materialize(parent, e->parentId);
        }
    }
}

void SidebarQt::selectEntry(int id) {
    if (!presenter())
        return;
    ensurePath(id);
    if (!m_items.contains(id))
        return;

    // Expand the ancestor path so the highlighted entry is visible.
    const SidebarEntry* e = presenter()->entry(id);
    while (e) {
        if (QTreeWidgetItem* it = m_items.value(e->id)) {
            it->setExpanded(true);
            m_tree->setCurrentItem(it);
        }
        e = (e->parentId >= 0) ? presenter()->entry(e->parentId) : nullptr;
    }

    if (QTreeWidgetItem* t = m_items.value(id)) {
        m_tree->setCurrentItem(t);
        m_tree->scrollToItem(t);
        m_tree->horizontalScrollBar()->setValue(0);
    }
}

void SidebarQt::setVisible(bool on) {
    QWidget::setVisible(on);
}

// Forward Escape from the tree to the viewer so it behaves exactly as when
// the reading area holds keyboard focus.
bool SidebarQt::eventFilter(QObject* watched, QEvent* ev) {
    if (watched == m_tree && ev->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(ev);
        if (ke->key() == Qt::Key_Escape) {
            QWidget* viewer = parentWidget() ? parentWidget() : window();
            QKeyEvent fwd(QEvent::KeyPress, ke->key(), ke->modifiers(), ke->text(),
                          ke->isAutoRepeat(), ke->count());
            QApplication::sendEvent(viewer, &fwd);
            return true;
        }
    }
    return QWidget::eventFilter(watched, ev);
}