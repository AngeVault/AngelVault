#include "hierarchy_panel.h"
#include "editor_context.h"

#include <QAction>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QSignalBlocker>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
constexpr int kEntityRole = Qt::UserRole + 1;
quint64 pack(Entity e) { return (quint64(e.index) << 32) | e.generation; }
}

HierarchyPanel::HierarchyPanel(EditorContext& context, QWidget* parent) : QWidget(parent), m_ctx(context) {
    setObjectName("hierarchyPanel");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    auto* bar = new QHBoxLayout;
    bar->setSpacing(4);
    auto* add = new QToolButton;
    add->setObjectName("panelToolButton");
    add->setText("+");
    add->setToolTip(tr("Créer une entité"));
    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Rechercher..."));
    m_search->setClearButtonEnabled(true);
    bar->addWidget(add);
    bar->addWidget(m_search, 1);
    layout->addLayout(bar);

    m_tree = new QTreeWidget;
    m_tree->setObjectName("hierarchyTree");
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_tree, 1);

    // Suppr / F2 quand la liste a le focus
    auto* del = new QAction(tr("Supprimer"), this);
    del->setShortcut(QKeySequence::Delete);
    del->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_tree->addAction(del);
    connect(del, &QAction::triggered, this, [this] { if (m_ctx.hasSelection()) m_ctx.destroyEntity(m_ctx.selected()); });

    connect(add, &QToolButton::clicked, this, [this] { m_ctx.createEntity(); });
    connect(m_search, &QLineEdit::textChanged, this, &HierarchyPanel::applyFilter);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &HierarchyPanel::showContextMenu);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, [this] {
        if (m_updating) return;
        const auto items = m_tree->selectedItems();
        if (items.isEmpty()) m_ctx.clearSelection();
        else m_ctx.select(entityOf(items.first()));
    });
    connect(m_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int) {
        if (m_updating) return;
        const QString name = item->text(0).trimmed();
        const Entity e = entityOf(item);
        if (name.isEmpty()) { m_updating = true; item->setText(0, m_ctx.displayName(e)); m_updating = false; return; }
        if (name != m_ctx.displayName(e)) m_ctx.renameEntity(e, name);
    });

    // Différé : un renommage ou une suppression déclenché depuis la liste ne la détruit pas en plein milieu
    connect(&m_ctx, &EditorContext::sceneChanged, this, &HierarchyPanel::rebuild, Qt::QueuedConnection);
    connect(&m_ctx, &EditorContext::selectionChanged, this, &HierarchyPanel::syncSelection);
    connect(&m_ctx, &EditorContext::entityModified, this, [this](Entity e, QObject*) {
        if (QTreeWidgetItem* item = itemFor(e)) {
            m_updating = true;
            item->setText(0, m_ctx.displayName(e));
            m_updating = false;
        }
    });

    rebuild();
}

void HierarchyPanel::rebuild() {
    m_updating = true;
    m_tree->clear();
    for (Entity e : m_ctx.entities()) {
        auto* item = new QTreeWidgetItem(m_tree, { m_ctx.displayName(e) });
        item->setData(0, kEntityRole, pack(e));
        item->setFlags(item->flags() | Qt::ItemIsEditable);
    }
    m_updating = false;
    applyFilter();
    syncSelection();
}

void HierarchyPanel::syncSelection() {
    m_updating = true;
    m_tree->clearSelection();
    if (QTreeWidgetItem* item = itemFor(m_ctx.selected())) {
        item->setSelected(true);
        m_tree->scrollToItem(item);
    }
    m_updating = false;
}

void HierarchyPanel::applyFilter() {
    const QString text = m_search->text().trimmed();
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_tree->topLevelItem(i);
        item->setHidden(!text.isEmpty() && !item->text(0).contains(text, Qt::CaseInsensitive));
    }
}

void HierarchyPanel::showContextMenu(const QPoint& pos) {
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    QMenu menu(this);
    connect(menu.addAction(tr("Créer une entité vide")), &QAction::triggered, this, [this] { m_ctx.createEntity(); });
    if (item) {
        const Entity e = entityOf(item);
        m_ctx.select(e);
        menu.addSeparator();
        connect(menu.addAction(tr("Renommer")), &QAction::triggered, this, [this, item] { m_tree->editItem(item); });
        connect(menu.addAction(tr("Supprimer")), &QAction::triggered, this, [this, e] { m_ctx.destroyEntity(e); });
    }
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

QTreeWidgetItem* HierarchyPanel::itemFor(Entity e) const {
    const quint64 key = pack(e);
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        if (m_tree->topLevelItem(i)->data(0, kEntityRole).toULongLong() == key) return m_tree->topLevelItem(i);
    return nullptr;
}

Entity HierarchyPanel::entityOf(const QTreeWidgetItem* item) {
    const quint64 v = item->data(0, kEntityRole).toULongLong();
    return Entity{ uint32_t(v >> 32), uint32_t(v & 0xFFFFFFFFu) };
}
