#include "sidebar_qt.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QApplication>
#include <QSet>

SidebarQt::SidebarQt(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setExpandsOnDoubleClick(true);
    m_tree->installEventFilter(this);
    layout->addWidget(m_tree);

    setFixedWidth(viewer_settings::kSidebarBaseWidth);

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