#include "hierarchy_panel.h"
#include "editor_context.h"

#include "components/camera2d.h"
#include "components/camera_follow.h"

#include <QAction>
#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <cmath>
#include <functional>

namespace {
constexpr int kKindRole = Qt::UserRole + 1;     // 0 = entité, 1 = catégorie, 2 = layer
constexpr int kEntityRole = Qt::UserRole + 2;   // entité, ou tilemap du layer
constexpr int kCategoryRole = Qt::UserRole + 3; // nom d'origine de la catégorie (pour le renommage)
constexpr int kLayerRole = Qt::UserRole + 4;
constexpr int kHeightColumn = 1;
constexpr int kModeColumn = 2;
constexpr int kEyeColumn = 3;                   // œil d'un layer, + d'une tilemap
QIcon layerIcon(const char* name) { return QIcon(QString(":/resources/icons/layers/%1.png").arg(name)); }

// un cran de hauteur = une demi-case, assez fin pour caler falaises et toits
float heightStep(const TileMapData& map) { return std::max(1.f, map.tileSize.y * 0.5f); }

// marches empilées, le nombre de crans par-dessus
QIcon heightIcon(int level, const QColor& color) {
    QPixmap pm(32, 32);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QColor c = color;
    if (level == 0) c.setAlphaF(0.35);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawRoundedRect(QRectF(3, 26, 26, 4), 1.5, 1.5);
    if (level != 0) {
        const bool up = level > 0;
        QPolygonF arrow;
        if (up) arrow << QPointF(5, 13) << QPointF(11, 5) << QPointF(17, 13);
        else arrow << QPointF(5, 6) << QPointF(11, 14) << QPointF(17, 6);
        p.drawPolygon(arrow);
        p.drawRect(QRectF(9, up ? 12 : 16, 4, 8));
        QFont f = p.font();
        f.setPixelSize(15);
        f.setBold(true);
        p.setFont(f);
        p.setPen(c);
        p.drawText(QRectF(14, 2, 18, 22), Qt::AlignRight | Qt::AlignVCenter, QString::number(std::abs(level)));
    }
    return QIcon(pm);
}
QIcon addIcon() { return QIcon(":/resources/icons/buttons/add.png"); }
quint64 pack(Entity e) { return (quint64(e.index) << 32) | e.generation; }

// Champ de renommage (double-clic / F2) : le style général des champs (marges, bordure) ne tient pas
// dans la hauteur d'une ligne de l'arbre et cachait le texte. Ici : champ compact, plus haut que la
// ligne si besoin, sur toute la largeur disponible, texte sélectionné.
class RenameDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex& index) const override {
        if (index.column() != 0) return nullptr;   // colonnes d'icônes
        auto* edit = new QLineEdit(parent);
        edit->setObjectName("hierarchyRename");
        edit->setFrame(true);
        // Seules les marges sont imposées : les couleurs restent celles du thème (règle QLineEdit du .qss)
        edit->setStyleSheet("QLineEdit#hierarchyRename { padding: 1px 5px; margin: 0px; border-radius: 4px; }");
        return edit;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        auto* edit = static_cast<QLineEdit*>(editor);
        edit->setText(index.data(Qt::EditRole).toString());
        edit->selectAll();
    }

    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex&) const override {
        QRect r = option.rect;
        // jusqu'au bord droit de la zone visible (les noms longs restent lisibles)
        if (auto* view = qobject_cast<QAbstractItemView*>(editor->parentWidget()->parentWidget()))
            r.setRight(std::max(r.right(), view->viewport()->width() - 2));
        const int h = std::max(r.height(), editor->sizeHint().height());
        r.setTop(r.center().y() - h / 2);
        r.setHeight(h);
        editor->setGeometry(r);
    }
};

enum class DropPos { On, Above, Below };   // où on lâche par rapport à la ligne visée

// Arbre qui gère lui-même le glisser-déposer : il ne déplace pas les lignes,
// il signale "ces entités vont dans cette catégorie" et la liste est reconstruite.
class HierarchyTree : public QTreeWidget {
public:
    std::function<void(QTreeWidgetItem* target, DropPos pos)> onDropped;              // lignes de la Hierarchy
    std::function<void(const QStringList& files, QTreeWidgetItem* target)> onFilesDropped;   // .prefab / .h
    std::function<QByteArray(const QList<QTreeWidgetItem*>&)> entitiesMime;          // pour Hierarchy -> Project

protected:
    static QStringList files(const QMimeData* mime) {
        QStringList out;
        if (mime && mime->hasUrls())
            for (const QUrl& url : mime->urls()) if (url.isLocalFile()) out << url.toLocalFile();
        return out;
    }
    QMimeData* mimeData(const QList<QTreeWidgetItem*>& items) const override {
        QMimeData* mime = QTreeWidget::mimeData(items);
        if (mime && entitiesMime) mime->setData(EditorContext::kEntityMimeType, entitiesMime(items));
        return mime;
    }
    void dragEnterEvent(QDragEnterEvent* e) override {
        if (e->source() == this) { QTreeWidget::dragEnterEvent(e); e->accept(); }
        else if (!files(e->mimeData()).isEmpty()) e->acceptProposedAction();   // fichier du panneau Project
        else e->ignore();
    }
    void dragMoveEvent(QDragMoveEvent* e) override {
        QTreeWidget::dragMoveEvent(e);   // défilement auto + indicateur
        if (e->source() == this || !files(e->mimeData()).isEmpty()) e->accept();
    }
    void dropEvent(QDropEvent* e) override {
        QTreeWidgetItem* target = itemAt(e->position().toPoint());
        if (e->source() != this) {
            const QStringList dropped = files(e->mimeData());
            if (dropped.isEmpty() || !onFilesDropped) { e->ignore(); return; }
            e->acceptProposedAction();
            onFilesDropped(dropped, target);
            return;
        }
        const DropIndicatorPosition p = dropIndicatorPosition();
        if (onDropped) onDropped(target, p == AboveItem ? DropPos::Above : p == BelowItem ? DropPos::Below : DropPos::On);
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
    add->setIcon(QIcon(":/resources/icons/buttons/add.png"));
    add->setToolTip(tr("Créer une entité ou une catégorie"));
    add->setPopupMode(QToolButton::InstantPopup);
    auto* addMenu = new QMenu(add);
    connect(addMenu->addAction(tr("Entité vide")), &QAction::triggered, this, &HierarchyPanel::createEntityHere);
    connect(addMenu->addAction(tr("Catégorie")), &QAction::triggered, this, [this] { m_ctx.addCategory(); });
    addMenu->addSeparator();
    // Tilemap : facultative, une entité avec un composant Tilemap (fichier maps/*.map)
    auto newTilemap = [this](TileMapOrientation o) {
        QString err;
        if (!m_ctx.isValid(m_ctx.createTilemap(o, 32, 32, &err))) QMessageBox::warning(this, tr("Tilemap"), err);
    };
    connect(addMenu->addAction(tr("Tilemap orthogonale")), &QAction::triggered, this,
            [newTilemap] { newTilemap(TileMapOrientation::Orthogonal); });
    connect(addMenu->addAction(tr("Tilemap isométrique")), &QAction::triggered, this,
            [newTilemap] { newTilemap(TileMapOrientation::Isometric); });
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
    m_tree->setColumnCount(4);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c : { kHeightColumn, kModeColumn, kEyeColumn }) {
        m_tree->header()->setSectionResizeMode(c, QHeaderView::Fixed);
        m_tree->header()->resizeSection(c, 22);
    }
    m_tree->setUniformRowHeights(true);
    m_tree->setIndentation(14);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);   // plusieurs entités à ranger d'un coup
    m_tree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_tree->setItemDelegate(new RenameDelegate(m_tree));   // champ de renommage lisible
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setDragEnabled(true);
    m_tree->setAcceptDrops(true);
    m_tree->setDropIndicatorShown(true);
    m_tree->setDragDropMode(QAbstractItemView::DragDrop);
    m_tree->setDefaultDropAction(Qt::CopyAction);
    layout->addWidget(m_tree, 1);

    tree->onDropped = [this](QTreeWidgetItem* target, DropPos pos) {
        for (QTreeWidgetItem* item : m_tree->selectedItems())
            if (isLayer(item)) { moveLayers(target, int(pos)); return; }
        const QString category = targetCategory(target);

        // catégories glissées : sur une catégorie = dedans, entre deux = à côté, dans le vide = racine
        QStringList selected;
        for (QTreeWidgetItem* item : m_tree->selectedItems())
            if (isCategory(item)) selected << item->data(0, kCategoryRole).toString();
        QStringList cats;   // dans l'ordre de la liste ; une sous-catégorie suit sa parente si les deux sont glissées
        for (const QString& c : m_ctx.categories()) {
            if (!selected.contains(c)) continue;
            bool nested = false;
            for (const QString& other : selected) nested |= other != c && m_ctx.isInsideCategory(c, other);
            if (!nested) cats << c;
        }
        if (!cats.isEmpty()) {
            QString parent = category;   // entité visée : dans sa catégorie
            int index = int(m_ctx.categories().size());
            if (isCategory(target) && pos != DropPos::On) {
                parent = m_ctx.categoryParent(category);
                index = int(m_ctx.categories().indexOf(category)) + (pos == DropPos::Below ? 1 : 0);
            }
            for (const QString& c : cats) {
                if (!m_ctx.setCategoryParent(c, parent)) continue;   // pas dans elle-même
                m_ctx.moveCategory(c, index);
                index = int(m_ctx.categories().indexOf(c)) + 1;
            }
            if (!parent.isEmpty()) m_collapsed.remove(parent);
        }

        QList<Entity> moved;
        for (QTreeWidgetItem* item : m_tree->selectedItems())
            if (isEntity(item)) moved.push_back(entityOf(item));
        Registry& reg = m_ctx.scene();
        for (Entity e : moved) {
            const bool isCamera = reg.hasComponent<Camera2d>(e);
            // Caméra déposée sur une entité : elle la suit (CameraFollow)
            if (isCamera && isEntity(target) && entityOf(target) != e) {
                setFollowTarget(e, entityOf(target));
                continue;
            }
            // Ailleurs : une caméra redevient libre, et l'entité va dans la catégorie visée
            if (isCamera) setFollowTarget(e, NULL_ENTITY);
            m_ctx.setCategory(e, category);
        }
        if (!category.isEmpty()) m_collapsed.remove(category);   // on montre où elles sont allées
    };

    // Fichiers glissés depuis le panneau Project :
    //   comportement (.h) sur une entité -> ajouté ; prefab (.prefab) -> instancié (dans la catégorie visée)
    tree->onFilesDropped = [this](const QStringList& files, QTreeWidgetItem* target) {
        const Entity onEntity = (target && !isCategory(target)) ? entityOf(target) : NULL_ENTITY;   // layer : sa tilemap
        const std::vector<Entity> created = m_ctx.dropFiles(files, onEntity);
        const QString category = targetCategory(target);
        if (!category.isEmpty())
            for (Entity e : created) m_ctx.setCategory(e, category);
    };
    // Glisser des entités vers le panneau Project = créer un prefab
    tree->entitiesMime = [this](const QList<QTreeWidgetItem*>& items) {
        std::vector<Entity> list;
        for (QTreeWidgetItem* item : items) if (isEntity(item)) list.push_back(entityOf(item));
        return EditorContext::entitiesToMime(list);
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
        if (current && current->isSelected() && isLayer(current)) {
            m_layerFocus = true;
            m_ctx.setActiveLayer(entityOf(current), layerOf(current));
            return;
        }
        m_layerFocus = false;
        if (current && current->isSelected() && isEntity(current)) { m_ctx.select(entityOf(current)); return; }
        for (QTreeWidgetItem* item : m_tree->selectedItems())
            if (isEntity(item)) { m_ctx.select(entityOf(item)); return; }
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
        if (isLayer(item)) {
            const TileMapData* map = m_ctx.scene().getComponent<Tilemap>(e).data.get();
            m_updating = true;
            if (name.isEmpty() && map && map->layer(layerOf(item))) item->setText(0, QString::fromStdString(map->layer(layerOf(item))->name));
            m_updating = false;
            m_ctx.renameLayer(e, layerOf(item), name);
            return;
        }
        if (name.isEmpty()) { m_updating = true; item->setText(0, m_ctx.displayName(e)); m_updating = false; return; }
        if (name != m_ctx.displayName(e)) m_ctx.renameEntity(e, name);
    });
    connect(m_tree, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem* item) {
        if (m_updating) return;
        if (isCategory(item)) m_collapsed.remove(item->data(0, kCategoryRole).toString());
        else if (isEntity(item)) m_collapsedMaps.remove(item->data(0, kEntityRole).toULongLong());
    });
    connect(m_tree, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem* item) {
        if (m_updating) return;
        if (isCategory(item)) m_collapsed.insert(item->data(0, kCategoryRole).toString());
        else if (isEntity(item)) m_collapsedMaps.insert(item->data(0, kEntityRole).toULongLong());
    });
    // icônes des layers : œil, mode, et + sur la tilemap
    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int column) {
        if (!item || isCategory(item)) return;
        const Entity e = entityOf(item);
        if (!isLayer(item)) {
            if (column == kEyeColumn && m_ctx.scene().hasComponent<Tilemap>(e) && m_ctx.scene().getComponent<Tilemap>(e).data) {
                m_layerFocus = true;
                m_collapsedMaps.remove(pack(e));
                m_ctx.addLayer(e);
            }
            return;
        }
        const TileMapData* map = m_ctx.scene().getComponent<Tilemap>(e).data.get();
        const TileLayer* layer = map ? map->layer(layerOf(item)) : nullptr;
        if (!layer) return;
        if (column == kEyeColumn) m_ctx.setLayerHidden(e, layerOf(item), !layer->hidden);
        else if (column == kModeColumn) m_ctx.setLayerMode(e, layerOf(item), TileLayerMode((int(layer->mode) + 1) % 3));
        else if (column == kHeightColumn) {
            // clic : un cran plus haut, maj + clic : un cran plus bas
            const float step = heightStep(*map);
            const int level = int(std::lround(layer->height / step)) + ((QApplication::keyboardModifiers() & Qt::ShiftModifier) ? -1 : 1);
            m_ctx.setLayerHeight(e, layerOf(item), float(level) * step);
        }
    });

    // Différé : un renommage ou une suppression déclenché depuis la liste ne la détruit pas en plein milieu
    connect(&m_ctx, &EditorContext::sceneChanged, this, &HierarchyPanel::rebuild, Qt::QueuedConnection);
    connect(&m_ctx, &EditorContext::selectionChanged, this, &HierarchyPanel::syncSelection);
    connect(&m_ctx, &EditorContext::activeLayerChanged, this, [this] {
        refreshLayerItems();
        syncSelection();
    });
    connect(&m_ctx, &EditorContext::layerVisibilityChanged, this, &HierarchyPanel::refreshLayerItems);
    connect(&m_ctx, &EditorContext::tilemapChanged, this, &HierarchyPanel::refreshLayerItems);
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
        auto* item = new QTreeWidgetItem({ name });   // rangée plus bas, une fois toutes les catégories créées
        item->setIcon(0, folder);
        item->setData(0, kKindRole, 1);
        item->setData(0, kCategoryRole, name);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled |
                       Qt::ItemIsDropEnabled);
        QFont f = item->font(0);
        f.setBold(true);
        item->setFont(0, f);
        categoryItems.insert(name, item);
    }
    for (const QString& name : m_ctx.categories()) {
        if (QTreeWidgetItem* parent = categoryItems.value(m_ctx.categoryParent(name), nullptr))
            parent->addChild(categoryItems.value(name));
        else
            m_tree->addTopLevelItem(categoryItems.value(name));
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

        // tilemap : la grille et ses layers, le plus haut devant
        const TileMapData* map = reg.hasComponent<Tilemap>(e) ? reg.getComponent<Tilemap>(e).data.get() : nullptr;
        if (!map) continue;
        item->setIcon(0, layerIcon("grid"));
        item->setIcon(kEyeColumn, addIcon());
        item->setToolTip(kEyeColumn, tr("layer"));
        for (int l = map->layerCount() - 1; l >= 0; --l) {
            auto* row = new QTreeWidgetItem(item, { QString::fromStdString(map->layer(l)->name) });
            row->setData(0, kKindRole, 2);
            row->setData(0, kEntityRole, pack(e));
            row->setData(0, kLayerRole, l);
            row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled |
                          Qt::ItemIsDropEnabled);
        }
        item->setExpanded(!m_collapsedMaps.contains(pack(e)));
    }
    // Une caméra qui suit une entité (CameraFollow::target) s'affiche sous cette entité.
    // Rien n'est ajouté aux entités : c'est seulement la donnée du composant, montrée en arbre.
    for (Entity e : m_ctx.entities()) {
        if (!reg.hasComponent<Camera2d>(e) || !reg.hasComponent<CameraFollow>(e)) continue;
        const Entity target = reg.getComponent<CameraFollow>(e).target;
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
        it.value()->setToolTip(0, tr("%n élément(s)", nullptr, it.value()->childCount()));
        it.value()->setExpanded(!m_collapsed.contains(it.key()));
    }
    m_updating = false;
    refreshLayerItems();
    applyFilter();
    syncSelection();
}

void HierarchyPanel::refreshLayerItems() {
    const bool wasUpdating = m_updating;
    m_updating = true;
    const QColor text = m_tree->palette().color(QPalette::Text);
    QColor dim = text;
    dim.setAlphaF(0.4);
    std::function<void(QTreeWidgetItem*)> visit = [&](QTreeWidgetItem* item) {
        if (isLayer(item)) {
            const Entity e = entityOf(item);
            const TileMapData* map = (m_ctx.isValid(e) && m_ctx.scene().hasComponent<Tilemap>(e)) ? m_ctx.scene().getComponent<Tilemap>(e).data.get() : nullptr;
            const TileLayer* layer = map ? map->layer(layerOf(item)) : nullptr;
            if (!layer) return;
            const bool active = e == m_ctx.activeTilemap() && m_ctx.activeLayer(e) == layerOf(item);
            QFont f = item->font(0);
            f.setBold(active);
            item->setFont(0, f);
            item->setForeground(0, layer->hidden ? dim : text);
            item->setIcon(0, layerIcon(active ? "layer_active" : "layer"));
            item->setIcon(kModeColumn, layerIcon(layer->mode == TileLayerMode::Below ? "mode_below" : layer->mode == TileLayerMode::YSort ? "mode_ysort" : "mode_above"));
            item->setToolTip(kModeColumn, layer->mode == TileLayerMode::Below ? tr("sous") : layer->mode == TileLayerMode::YSort ? tr("trié") : tr("dessus"));
            item->setIcon(kHeightColumn, heightIcon(int(std::lround(layer->height / heightStep(*map))), text));
            item->setToolTip(kHeightColumn, tr("hauteur"));
            item->setIcon(kEyeColumn, layerIcon(layer->hidden ? "eye_closed" : "eye_open"));
            item->setToolTip(kEyeColumn, tr("visible"));
            return;
        }
        for (int i = 0; i < item->childCount(); ++i) visit(item->child(i));
    };
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) visit(m_tree->topLevelItem(i));
    m_updating = wasUpdating;
}

QTreeWidgetItem* HierarchyPanel::layerItem(Entity tilemap, int layer) const {
    QTreeWidgetItem* item = itemFor(tilemap);
    if (!item) return nullptr;
    for (int i = 0; i < item->childCount(); ++i)
        if (isLayer(item->child(i)) && layerOf(item->child(i)) == layer) return item->child(i);
    return nullptr;
}

// layer glissé : au-dessus d'une ligne = devant elle (la liste montre le plus haut en premier)
void HierarchyPanel::moveLayers(QTreeWidgetItem* target, int pos) {
    QTreeWidgetItem* moved = nullptr;
    for (QTreeWidgetItem* item : m_tree->selectedItems()) if (isLayer(item)) { moved = item; break; }
    if (!moved || !target) return;
    const Entity e = entityOf(moved);
    if (entityOf(target) != e || isCategory(target)) return;   // pas d'une tilemap à l'autre
    const TileMapData* map = m_ctx.scene().getComponent<Tilemap>(e).data.get();
    if (!map) return;
    const int from = layerOf(moved);
    int insert = map->layerCount();   // sur la tilemap : tout devant
    if (isLayer(target)) insert = layerOf(target) + (DropPos(pos) == DropPos::Below ? 0 : 1);
    const int to = std::clamp(from < insert ? insert - 1 : insert, 0, map->layerCount() - 1);
    m_layerFocus = true;
    m_ctx.moveLayer(e, from, to);
}

void HierarchyPanel::syncSelection() {
    m_updating = true;
    m_tree->clearSelection();
    QTreeWidgetItem* item = itemFor(m_ctx.selected());
    if (m_layerFocus && item)
        if (QTreeWidgetItem* layer = layerItem(m_ctx.selected(), m_ctx.activeLayer(m_ctx.selected()))) item = layer;
    if (item) {
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
    m_ctx.setFollowTarget(camera, target);   // CameraFollow::target + caméra placée sur la cible
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
    if (current && isLayer(current) && current->isSelected()) {
        m_ctx.removeLayer(entityOf(current), layerOf(current));
        return;
    }
    QList<Entity> doomed;
    for (QTreeWidgetItem* item : m_tree->selectedItems())
        if (isEntity(item)) doomed.push_back(entityOf(item));
    for (Entity e : doomed) m_ctx.destroyEntity(e);
}

void HierarchyPanel::showContextMenu(const QPoint& pos) {
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    QMenu menu(this);
    if (item) m_tree->setCurrentItem(item);

    if (item && isLayer(item)) {
        const Entity e = entityOf(item);
        connect(menu.addAction(addIcon(), tr("Nouveau layer")), &QAction::triggered, this, [this, e] {
            m_layerFocus = true;
            m_ctx.addLayer(e);
        });
        connect(menu.addAction(tr("Renommer")), &QAction::triggered, this, [this, item] { m_tree->editItem(item); });
        QAction* remove = menu.addAction(tr("Supprimer"));
        const TileMapData* map = m_ctx.scene().getComponent<Tilemap>(e).data.get();
        remove->setEnabled(map && map->layerCount() > 1);
        connect(remove, &QAction::triggered, this, [this, e, layer = layerOf(item)] { m_ctx.removeLayer(e, layer); });
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
        return;
    }
    if (item && isCategory(item)) {
        const QString name = item->data(0, kCategoryRole).toString();
        connect(menu.addAction(tr("Créer une entité dans « %1 »").arg(name)), &QAction::triggered, this,
                &HierarchyPanel::createEntityHere);
        connect(menu.addAction(tr("Créer une sous-catégorie")), &QAction::triggered, this, [this, name] {
            m_collapsed.remove(name);
            m_ctx.addCategory(tr("Nouvelle catégorie"), name);
        });
        menu.addSeparator();
        connect(menu.addAction(tr("Renommer")), &QAction::triggered, this, [this, item] { m_tree->editItem(item); });
        connect(menu.addAction(tr("Supprimer la catégorie (garder les entités)")), &QAction::triggered, this,
                [this, name] { m_ctx.removeCategory(name); });
    } else if (item) {
        const Entity e = entityOf(item);
        if (!item->isSelected()) m_ctx.select(e);
        if (m_ctx.scene().hasComponent<Tilemap>(e) && m_ctx.scene().getComponent<Tilemap>(e).data)
            connect(menu.addAction(addIcon(), tr("Nouveau layer")), &QAction::triggered, this, [this, e] {
                m_layerFocus = true;
                m_collapsedMaps.remove(pack(e));
                m_ctx.addLayer(e);
            });
        connect(menu.addAction(tr("Renommer")), &QAction::triggered, this, [this, item] { m_tree->editItem(item); });

        QMenu* move = menu.addMenu(tr("Déplacer vers"));
        const QString current = m_ctx.categoryOf(e);
        QList<Entity> selection;
        for (QTreeWidgetItem* it : m_tree->selectedItems())
            if (isEntity(it)) selection.push_back(entityOf(it));
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
            const Entity currentTarget = m_ctx.followTarget(e);
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

        connect(menu.addAction(tr("Créer un prefab")), &QAction::triggered, this, [this, e] {
            QString err;
            if (!m_ctx.createPrefab(e, m_ctx.defaultPrefabPath(e), &err))
                QMessageBox::warning(this, tr("Prefab"), err);
        });
        menu.addSeparator();
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
        if (isEntity(item) && item->data(0, kEntityRole).toULongLong() == key) return item;
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

bool HierarchyPanel::isLayer(const QTreeWidgetItem* item) {
    return item && item->data(0, kKindRole).toInt() == 2;
}

bool HierarchyPanel::isEntity(const QTreeWidgetItem* item) {
    return item && item->data(0, kKindRole).toInt() == 0;
}

int HierarchyPanel::layerOf(const QTreeWidgetItem* item) {
    return item->data(0, kLayerRole).toInt();
}

Entity HierarchyPanel::entityOf(const QTreeWidgetItem* item) {
    const quint64 v = item->data(0, kEntityRole).toULongLong();
    return Entity{ uint32_t(v >> 32), uint32_t(v & 0xFFFFFFFFu) };
}
