#include "hierarchy_panel.h"
#include "editor_context.h"

#include "components/camera2d.h"

#include <QAction>
#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QStyle>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <functional>

namespace {
constexpr int kKindRole = Qt::UserRole + 1;     // 0 = entité, 1 = catégorie
constexpr int kEntityRole = Qt::UserRole + 2;
constexpr int kCategoryRole = Qt::UserRole + 3; // nom d'origine de la catégorie (pour le renommage)
quint64 pack(Entity e) { return (quint64(e.index) << 32) | e.generation; }

// Arbre qui gère lui-même le glisser-déposer : il ne déplace pas les lignes,
// il signale "ces entités vont dans cette catégorie" et la liste est reconstruite.
class HierarchyTree : public QTreeWidget {
public:
    std::function<void(QTreeWidgetItem* target)> onDropped;

protected:
    void dragEnterEvent(QDragEnterEvent* e) override {
        if (e->source() == this) { QTreeWidget::dragEnterEvent(e); e->accept(); }
        else e->ignore();
    }
    void dragMoveEvent(QDragMoveEvent* e) override {
        QTreeWidget::dragMoveEvent(e);   // défilement auto + indicateur
        if (e->source() == this) e->accept();
    }
    void dropEvent(QDropEvent* e) override {
        if (e->source() != this) { e->ignore(); return; }
        if (onDropped) onDropped(itemAt(e->position().toPoint()));
        e->setDropAction(Qt::CopyAction);   // "copie" : Qt ne supprime pas les lignes d'origine
        e->accept();
    }
};
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
    add->setToolTip(tr("Créer une entité ou une catégorie"));
    add->setPopupMode(QToolButton::InstantPopup);
    auto* addMenu = new QMenu(add);
    connect(addMenu->addAction(tr("Entité vide")), &QAction::triggered, this, &HierarchyPanel::createEntityHere);
    connect(addMenu->addAction(tr("Catégorie")), &QAction::triggered, this, [this] { m_ctx.addCategory(); });
    add->setMenu(addMenu);
    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Rechercher..."));
    m_search->setClearButtonEnabled(true);
    bar->addWidget(add);
    bar->addWidget(m_search, 1);
    layout->addLayout(bar);

    auto* tree = new HierarchyTree;
    m_tree = tree;
    m_tree->setObjectName("hierarchyTree");
    m_tree->setHeaderHidden(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setIndentation(14);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);   // plusieurs entités à ranger d'un coup
    m_tree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setDragEnabled(true);
    m_tree->setAcceptDrops(true);
    m_tree->setDropIndicatorShown(true);
    m_tree->setDragDropMode(QAbstractItemView::DragDrop);
    m_tree->setDefaultDropAction(Qt::CopyAction);
    layout->addWidget(m_tree, 1);

    tree->onDropped = [this](QTreeWidgetItem* target) {
        const QString category = targetCategory(target);
        QList<Entity> moved;
        for (QTreeWidgetItem* item : m_tree->selectedItems())
            if (!isCategory(item)) moved.push_back(entityOf(item));
        Registry& reg = m_ctx.scene();
        for (Entity e : moved) {
            const bool isCamera = reg.hasComponent<Camera2d>(e);
            // Caméra déposée sur une entité : elle la suit (targetLock)
            if (isCamera && target && !isCategory(target) && entityOf(target) != e) {
                setFollowTarget(e, entityOf(target));
                continue;
            }
            // Ailleurs : une caméra redevient libre, et l'entité va dans la catégorie visée
            if (isCamera) setFollowTarget(e, NULL_ENTITY);
            m_ctx.setCategory(e, category);
        }
        if (!category.isEmpty()) m_collapsed.remove(category);   // on montre où elles sont allées
    };

    // Suppr quand la liste a le focus
    auto* del = new QAction(tr("Supprimer"), this);
    del->setShortcut(QKeySequence::Delete);
    del->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_tree->addAction(del);
    connect(del, &QAction::triggered, this, &HierarchyPanel::deleteCurrent);

    connect(m_search, &QLineEdit::textChanged, this, &HierarchyPanel::applyFilter);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &HierarchyPanel::showContextMenu);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, [this] {
        if (m_updating) return;
        QTreeWidgetItem* current = m_tree->currentItem();
        if (current && current->isSelected() && !isCategory(current)) { m_ctx.select(entityOf(current)); return; }
        for (QTreeWidgetItem* item : m_tree->selectedItems())
            if (!isCategory(item)) { m_ctx.select(entityOf(item)); return; }
        m_ctx.clearSelection();
    });
    connect(m_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int) {
        if (m_updating) return;
        const QString name = item->text(0).trimmed();
        if (isCategory(item)) {
            const QString old = item->data(0, kCategoryRole).toString();
            if (m_collapsed.remove(old)) m_collapsed.insert(name);
            m_ctx.renameCategory(old, name);   // refusé si vide ou déjà pris : la liste est reconstruite
            return;
        }
        const Entity e = entityOf(item);
        if (name.isEmpty()) { m_updating = true; item->setText(0, m_ctx.displayName(e)); m_updating = false; return; }
        if (name != m_ctx.displayName(e)) m_ctx.renameEntity(e, name);
    });
    connect(m_tree, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem* item) {
        if (!m_updating && isCategory(item)) m_collapsed.remove(item->data(0, kCategoryRole).toString());
    });
    connect(m_tree, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem* item) {
        if (!m_updating && isCategory(item)) m_collapsed.insert(item->data(0, kCategoryRole).toString());
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

    const QIcon folder = style()->standardIcon(QStyle::SP_DirIcon);
    QHash<QString, QTreeWidgetItem*> categoryItems;
    for (const QString& name : m_ctx.categories()) {
        auto* item = new QTreeWidgetItem(m_tree, { name });
        item->setIcon(0, folder);
        item->setData(0, kKindRole, 1);
        item->setData(0, kCategoryRole, name);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDropEnabled);
        QFont f = item->font(0);
        f.setBold(true);
        item->setFont(0, f);
        categoryItems.insert(name, item);
    }
    Registry& reg = m_ctx.scene();
    QHash<quint64, QTreeWidgetItem*> entityItems;
    for (Entity e : m_ctx.entities()) {
        QTreeWidgetItem* parent = categoryItems.value(m_ctx.categoryOf(e), nullptr);
        auto* item = parent ? new QTreeWidgetItem(parent, { m_ctx.displayName(e) })
                            : new QTreeWidgetItem(m_tree, { m_ctx.displayName(e) });
        item->setData(0, kKindRole, 0);
        item->setData(0, kEntityRole, pack(e));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled |
                       Qt::ItemIsDropEnabled);
        entityItems.insert(pack(e), item);
    }
    // Une caméra qui suit une entité (Camera2d::targetLock) s'affiche sous cette entité.
    // Rien n'est ajouté aux entités : c'est seulement la donnée du composant, montrée en arbre.
    for (Entity e : m_ctx.entities()) {
        if (!reg.hasComponent<Camera2d>(e)) continue;
        const Entity target = reg.getComponent<Camera2d>(e).targetLock;
        if (target == NULL_ENTITY || target == e || !reg.isValid(target)) continue;
        QTreeWidgetItem* item = entityItems.value(pack(e));
        QTreeWidgetItem* parent = entityItems.value(pack(target));
        if (!item || !parent) continue;
        bool cycle = false;   // A suit B qui suit A : on laisse à sa place
        for (QTreeWidgetItem* p = parent; p; p = p->parent()) cycle |= (p == item);
        if (cycle) continue;
        QTreeWidgetItem* oldParent = item->parent();
        if (oldParent) oldParent->removeChild(item);
        else m_tree->takeTopLevelItem(m_tree->indexOfTopLevelItem(item));
        parent->addChild(item);
        item->setToolTip(0, tr("Caméra qui suit « %1 »").arg(m_ctx.displayName(target)));
        QFont f = item->font(0);
        f.setItalic(true);
        item->setFont(0, f);
        parent->setExpanded(true);
    }
    for (auto it = categoryItems.begin(); it != categoryItems.end(); ++it) {
        it.value()->setToolTip(0, tr("%n entité(s)", nullptr, it.value()->childCount()));
        it.value()->setExpanded(!m_collapsed.contains(it.key()));
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
        m_tree->setCurrentItem(item, 0, QItemSelectionModel::NoUpdate);
        if (item->parent()) item->parent()->setExpanded(true);
        m_tree->scrollToItem(item);
    }
    m_updating = false;
}

void HierarchyPanel::applyFilter() {
    const QString text = m_search->text().trimmed();
    // Visible si le nom correspond, ou si un enfant correspond (on garde alors le chemin jusqu'à lui)
    std::function<bool(QTreeWidgetItem*, bool)> filter = [&](QTreeWidgetItem* item, bool parentMatches) -> bool {
        const bool matches = text.isEmpty() || item->text(0).contains(text, Qt::CaseInsensitive);
        bool anyChild = false;
        for (int i = 0; i < item->childCount(); ++i)
            anyChild |= filter(item->child(i), parentMatches || (matches && isCategory(item)));
        const bool visible = matches || anyChild || parentMatches;
        item->setHidden(!visible);
        if (!text.isEmpty() && anyChild) item->setExpanded(true);
        return visible;
    };
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) filter(m_tree->topLevelItem(i), false);
}

QString HierarchyPanel::targetCategory(QTreeWidgetItem* item) const {
    for (; item; item = item->parent())
        if (isCategory(item)) return item->data(0, kCategoryRole).toString();
    return {};
}

void HierarchyPanel::setFollowTarget(Entity camera, Entity target) {
    m_ctx.setFollowTarget(camera, target);   // Camera2d::targetLock + caméra placée sur la cible
}

void HierarchyPanel::createEntityHere() {
    const QString category = targetCategory(m_tree->currentItem());
    const Entity e = m_ctx.createEntity();
    if (!category.isEmpty()) {
        m_collapsed.remove(category);
        m_ctx.setCategory(e, category);
    }
}

void HierarchyPanel::deleteCurrent() {
    QTreeWidgetItem* current = m_tree->currentItem();
    if (current && isCategory(current) && current->isSelected()) {
        m_ctx.removeCategory(current->data(0, kCategoryRole).toString());
        return;
    }
    QList<Entity> doomed;
    for (QTreeWidgetItem* item : m_tree->selectedItems())
        if (!isCategory(item)) doomed.push_back(entityOf(item));
    for (Entity e : doomed) m_ctx.destroyEntity(e);
}

void HierarchyPanel::showContextMenu(const QPoint& pos) {
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    QMenu menu(this);
    if (item) m_tree->setCurrentItem(item);

    if (item && isCategory(item)) {
        const QString name = item->data(0, kCategoryRole).toString();
        connect(menu.addAction(tr("Créer une entité dans « %1 »").arg(name)), &QAction::triggered, this,
                &HierarchyPanel::createEntityHere);
        menu.addSeparator();
        connect(menu.addAction(tr("Renommer")), &QAction::triggered, this, [this, item] { m_tree->editItem(item); });
        connect(menu.addAction(tr("Supprimer la catégorie (garder les entités)")), &QAction::triggered, this,
                [this, name] { m_ctx.removeCategory(name); });
    } else if (item) {
        const Entity e = entityOf(item);
        if (!item->isSelected()) m_ctx.select(e);
        connect(menu.addAction(tr("Renommer")), &QAction::triggered, this, [this, item] { m_tree->editItem(item); });

        QMenu* move = menu.addMenu(tr("Déplacer vers"));
        const QString current = m_ctx.categoryOf(e);
        QList<Entity> selection;
        for (QTreeWidgetItem* it : m_tree->selectedItems())
            if (!isCategory(it)) selection.push_back(entityOf(it));
        if (selection.isEmpty()) selection.push_back(e);
        auto addTarget = [&](const QString& label, const QString& category) {
            QAction* a = move->addAction(label);
            a->setEnabled(category != current || selection.size() > 1);
            connect(a, &QAction::triggered, this, [this, selection, category] {
                for (Entity s : selection) m_ctx.setCategory(s, category);
            });
        };
        addTarget(tr("(Aucune catégorie)"), QString());
        if (!m_ctx.categories().isEmpty()) move->addSeparator();
        for (const QString& c : m_ctx.categories()) addTarget(c, c);
        move->addSeparator();
        connect(move->addAction(tr("Nouvelle catégorie...")), &QAction::triggered, this, [this, selection] {
            const QString c = m_ctx.addCategory();
            for (Entity s : selection) m_ctx.setCategory(s, c);
        });

        if (m_ctx.scene().hasComponent<Camera2d>(e)) {
            QMenu* follow = menu.addMenu(tr("Suivre"));
            const Entity currentTarget = m_ctx.scene().getComponent<Camera2d>(e).targetLock;
            auto addFollow = [&](const QString& label, Entity target) {
                QAction* a = follow->addAction(label);
                a->setCheckable(true);
                a->setChecked(currentTarget == target || (target == NULL_ENTITY && !m_ctx.isValid(currentTarget)));
                connect(a, &QAction::triggered, this, [this, e, target] { setFollowTarget(e, target); });
            };
            addFollow(tr("(Personne : caméra libre)"), NULL_ENTITY);
            follow->addSeparator();
            for (Entity other : m_ctx.entities())
                if (other != e) addFollow(m_ctx.displayName(other), other);
        }

        connect(menu.addAction(tr("Supprimer")), &QAction::triggered, this, &HierarchyPanel::deleteCurrent);
    }

    if (item) menu.addSeparator();
    connect(menu.addAction(tr("Créer une entité vide")), &QAction::triggered, this, [this, item] {
        if (!item) m_tree->setCurrentItem(nullptr);
        createEntityHere();
    });
    connect(menu.addAction(tr("Créer une catégorie")), &QAction::triggered, this, [this] { m_ctx.addCategory(); });
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

QTreeWidgetItem* HierarchyPanel::itemFor(Entity e) const {
    const quint64 key = pack(e);
    std::function<QTreeWidgetItem*(QTreeWidgetItem*)> search = [&](QTreeWidgetItem* item) -> QTreeWidgetItem* {
        if (!isCategory(item) && item->data(0, kEntityRole).toULongLong() == key) return item;
        for (int i = 0; i < item->childCount(); ++i)
            if (QTreeWidgetItem* found = search(item->child(i))) return found;
        return nullptr;
    };
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        if (QTreeWidgetItem* found = search(m_tree->topLevelItem(i))) return found;
    return nullptr;
}

bool HierarchyPanel::isCategory(const QTreeWidgetItem* item) {
    return item && item->data(0, kKindRole).toInt() == 1;
}

Entity HierarchyPanel::entityOf(const QTreeWidgetItem* item) {
    const quint64 v = item->data(0, kEntityRole).toULongLong();
    return Entity{ uint32_t(v >> 32), uint32_t(v & 0xFFFFFFFFu) };
}
