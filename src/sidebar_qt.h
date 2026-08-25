#ifndef SIDEBAR_QT_H
#define SIDEBAR_QT_H

#include "sidebar.h"
#include "viewer_settings.h"

#include <QHash>
#include <QSet>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QWidget>

// Qt outline sidebar backend: a fixed-width QTreeWidget dock area populated
// lazily per expanded node (design D10), mirroring the Win32 tree behavior.
class SidebarQt : public QWidget, public SidebarBackend {
public:
    explicit SidebarQt(QWidget* parent = nullptr);
    ~SidebarQt() override;

    void setWidth(int widthPx);

    // --- SidebarBackend ---
    void clearEntries() override;
    void addEntry(int id, int parentId, const QString& title) override;
    void selectEntry(int id) override;
    void setVisible(bool on) override;

private:
    void materialize(QTreeWidgetItem* item, int id);
    void ensurePath(int id);
    QTreeWidgetItem* insertItem(QTreeWidgetItem* parent, int id, const QString& title);
    int idOf(QTreeWidgetItem* item) const;

    QTreeWidget* m_tree = nullptr;
    QHash<int, QTreeWidgetItem*> m_items;
    QSet<int> m_materialized;
};

#endif // SIDEBAR_QT_H