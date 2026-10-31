#include "tile_palette_panel.h"
#include "tile_def_dialog.h"

#include <QAction>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QPainter>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kTypeRole = Qt::UserRole + 1;
constexpr int kIcon = 32;

// Aperçu d'une tile : son image pleine (feuille dual-grid : colonne 15), recadrée sur ce qui est dessiné
QIcon tileIcon(const TileMapData& map, const TileDef& def) {
    QPixmap pix(kIcon, kIcon);
    pix.fill(Qt::transparent);
    if (def.images.empty()) return QIcon(pix);
    const QImage image(QString::fromStdString(map.resolve(def.images.front())));
    if (image.isNull()) return QIcon(pix);
    QImage part = image;
    if (def.autotile == AutotileMode::Corners16) {
        const int cell = std::max(1, def.cellPixels);
        part = image.copy(15 * cell, 0, cell, cell);
    } else {   // on enlève les bords transparents (les tiles iso ont beaucoup de vide)
        int x0 = image.width(), y0 = image.height(), x1 = -1, y1 = -1;
        const QImage a = image.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < a.height(); y += 2)
            for (int x = 0; x < a.width(); x += 2)
                if (qAlpha(a.pixel(x, y)) > 20) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y); }
        if (x1 >= x0) part = image.copy(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
    }
    const bool pixelArt = part.width() <= 64;
    const QImage scaled = part.scaled(kIcon, kIcon, Qt::KeepAspectRatio, pixelArt ? Qt::FastTransformation : Qt::SmoothTransformation);
    QPainter p(&pix);
    p.drawImage((kIcon - scaled.width()) / 2, (kIcon - scaled.height()) / 2, scaled);
    return QIcon(pix);
}
} // namespace

TilePalettePanel::TilePalettePanel(EditorContext& context, QWidget* parent) : QWidget(parent), m_ctx(context) {
    setObjectName("tilePalettePanel");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(8);

    // --- Outils ---
    auto* toolsRow = new QHBoxLayout;
    toolsRow->setSpacing(4);
    m_tools = new QButtonGroup(this);
    m_tools->setExclusive(true);
    struct ToolDef { EditorTool tool; QString text; QString tip; QKeySequence key; };
    const ToolDef defs[] = {
        { EditorTool::Select, tr("Sélection"), tr("Sélectionner et déplacer des entités (V)"), QKeySequence(Qt::Key_V) },
        { EditorTool::Paint,  tr("Pinceau"),   tr("Peindre le terrain choisi (B)"),             QKeySequence(Qt::Key_B) },
        { EditorTool::Erase,  tr("Gomme"),     tr("Effacer des tiles (E)"),                      QKeySequence(Qt::Key_E) },
        { EditorTool::Pick,   tr("Pipette"),   tr("Choisir le terrain d'une case (I)"),          QKeySequence(Qt::Key_I) },
    };
    for (const ToolDef& d : defs) {
        auto* b = new QToolButton;
        b->setObjectName("paletteTool");
        b->setText(d.text);
        b->setToolTip(d.tip);
        b->setCheckable(true);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_tools->addButton(b, int(d.tool));
        toolsRow->addWidget(b);

        // Raccourci (un champ texte qui a le focus garde ses lettres)
        auto* shortcut = new QAction(d.text, this);
        shortcut->setShortcut(d.key);
        shortcut->setShortcutContext(Qt::WindowShortcut);
        const EditorTool tool = d.tool;
        connect(shortcut, &QAction::triggered, this, [this, tool] { m_ctx.setTool(tool); });
        m_toolActions.push_back(shortcut);
    }
    layout->addLayout(toolsRow);
    connect(m_tools, &QButtonGroup::idClicked, this, [this](int id) { m_ctx.setTool(EditorTool(id)); });

    // --- Taille du pinceau ---
    auto* brushRow = new QHBoxLayout;
    brushRow->addWidget(new QLabel(tr("Taille du pinceau")));
    m_brush = new QSpinBox;
    m_brush->setRange(1, 9);
    m_brush->setButtonSymbols(QAbstractSpinBox::NoButtons);   // molette ou clavier
    m_brush->setValue(m_ctx.brushSize());
    m_brush->setSuffix(tr(" case(s)"));
    brushRow->addWidget(m_brush, 1);
    layout->addLayout(brushRow);
    connect(m_brush, &QSpinBox::valueChanged, &m_ctx, &EditorContext::setBrushSize);

    // --- Terrains ---
    auto* title = new QLabel(tr("Tiles"));
    title->setObjectName("paletteSectionTitle");
    layout->addWidget(title);
    m_terrains = new QListWidget;
    m_terrains->setObjectName("paletteTerrains");
    // Liste : aperçu à gauche, nom à droite (le texte ne peut pas déborder sur les autres cases)
    m_terrains->setViewMode(QListView::ListMode);
    m_terrains->setIconSize(QSize(kIcon, kIcon));
    m_terrains->setUniformItemSizes(true);
    m_terrains->setSpacing(1);
    layout->addWidget(m_terrains, 1);
    m_mapInfo = new QLabel;
    m_mapInfo->setObjectName("paletteHelp");
    m_mapInfo->setWordWrap(true);
    layout->addWidget(m_mapInfo);

    // + Tile / Modifier / Supprimer / ordre
    m_tileButtons = new QWidget;
    auto* tb = new QHBoxLayout(m_tileButtons);
    tb->setContentsMargins(0, 0, 0, 0);
    tb->setSpacing(4);
    auto* addTile = new QPushButton(tr("+ Tile"));
    auto* editT = new QPushButton(tr("Modifier"));
    auto* delT = new QPushButton(tr("Supprimer"));
    auto* up = new QToolButton;
    up->setText(QString::fromUtf8("▲"));
    up->setToolTip(tr("Monter : passe SOUS les tiles suivantes"));
    auto* down = new QToolButton;
    down->setText(QString::fromUtf8("▼"));
    down->setToolTip(tr("Descendre : déborde par-dessus les tiles précédentes (autotile)"));
    tb->addWidget(addTile);
    tb->addWidget(editT);
    tb->addWidget(delT);
    tb->addStretch();
    tb->addWidget(up);
    tb->addWidget(down);
    layout->addWidget(m_tileButtons);
    connect(addTile, &QPushButton::clicked, this, [this] { editTile(-1); });
    connect(editT, &QPushButton::clicked, this, [this] { editTile(m_terrains->currentRow()); });
    connect(delT, &QPushButton::clicked, this, &TilePalettePanel::removeTile);
    connect(up, &QToolButton::clicked, this, [this] { moveTile(-1); });
    connect(down, &QToolButton::clicked, this, [this] { moveTile(+1); });
    connect(m_terrains, &QListWidget::itemDoubleClicked, this, [this] { editTile(m_terrains->currentRow()); });
    buildTerrains();
    connect(&m_ctx, &EditorContext::tilemapChanged, this, &TilePalettePanel::buildTerrains);
    connect(m_terrains, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        if (!item) return;
        m_ctx.setPaintTile(uint16_t(item->data(kTypeRole).toInt()));
        if (m_ctx.tool() == EditorTool::Select || m_ctx.tool() == EditorTool::Pick) m_ctx.setTool(EditorTool::Paint);
    });

    auto* help = new QLabel(tr("Clic gauche : peindre  ·  clic droit / molette : déplacer la vue"));
    help->setObjectName("paletteHelp");
    help->setWordWrap(true);
    layout->addWidget(help);

    connect(&m_ctx, &EditorContext::toolChanged, this, &TilePalettePanel::syncTool);
    connect(&m_ctx, &EditorContext::paintTileChanged, this, &TilePalettePanel::syncTerrain);
    connect(&m_ctx, &EditorContext::brushSizeChanged, this, [this](int s) { QSignalBlocker b(m_brush); m_brush->setValue(s); });
    syncTool(m_ctx.tool());
    syncTerrain(m_ctx.paintTile());
}

void TilePalettePanel::buildTerrains() {
    QSignalBlocker block(m_terrains);
    m_terrains->clear();
    const TileMapData* map = m_ctx.activeMap();
    m_tileButtons->setEnabled(map != nullptr);
    if (!map) {
        m_mapInfo->setText(tr("Pas de tilemap dans cette scène (elle est facultative).\n"
                              "Hierarchy > + > Tilemap orthogonale / isométrique, ou glissez un fichier .map."));
        return;
    }
    const bool iso = map->orientation == TileMapOrientation::Isometric;
    m_mapInfo->setText(tr("%1 %2 x %3, cases de %4 x %5").arg(iso ? tr("Isométrique") : tr("Orthogonale"))
                           .arg(map->width()).arg(map->height()).arg(map->tileSize.x).arg(map->tileSize.y)
                       + (map->tiles.empty() ? tr("\nAucune tile : cliquez sur « + Tile ».") : QString()));
    for (std::size_t i = 0; i < map->tiles.size(); ++i) {
        const TileDef& def = map->tiles[i];
        QString label = QString::fromStdString(def.name);
        if (def.autotile == AutotileMode::Corners16) label += tr("   (autotile 16 coins)");
        if (def.autotile == AutotileMode::Edges16) label += tr("   (autotile 4 bords)");
        auto* item = new QListWidgetItem(tileIcon(*map, def), label);
        item->setData(kTypeRole, int(i + 1));
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        item->setSizeHint(QSize(0, kIcon + 10));
        m_terrains->addItem(item);
    }
    if (!map->tile(m_ctx.paintTile()) && !map->tiles.empty()) m_ctx.setPaintTile(1);
    syncTerrain(m_ctx.paintTile());
}

void TilePalettePanel::editTile(int index) {
    TileMapData* map = m_ctx.activeMap();
    if (!map) return;
    const bool isNew = index < 0 || index >= int(map->tiles.size());
    TileDef start;
    if (!isNew) start = map->tiles[std::size_t(index)];
    else {
        start.name = tr("Tile %1").arg(map->tiles.size() + 1).toStdString();
        if (map->orientation == TileMapOrientation::Isometric) start.anchor = { -1.f, -1.f };
    }
    TileDefDialog dialog(*map, start, isNew ? -1 : index, this);
    if (dialog.exec() != QDialog::Accepted) return;
    TileDef def = dialog.result();
    if (isNew) {
        map->tiles.push_back(def);
        m_ctx.setPaintTile(uint16_t(map->tiles.size()));
    } else {
        const std::string oldName = map->tiles[std::size_t(index)].name;
        map->tiles[std::size_t(index)] = def;
        if (oldName != def.name)   // renommée : les « dessous » qui la citaient suivent
            for (TileDef& t : map->tiles) if (t.base == oldName) t.base = def.name;
    }
    m_ctx.notifyTilesChanged();
}

void TilePalettePanel::removeTile() {
    TileMapData* map = m_ctx.activeMap();
    const int row = m_terrains->currentRow();
    if (!map || row < 0 || row >= int(map->tiles.size())) return;
    const QString name = QString::fromStdString(map->tiles[std::size_t(row)].name);
    if (QMessageBox::question(this, tr("Supprimer la tile"),
            tr("Supprimer « %1 » ? Les cases peintes avec deviennent vides.").arg(name)) != QMessageBox::Yes)
        return;
    map->removeTile(uint16_t(row + 1));
    m_ctx.notifyTilesChanged();
}

void TilePalettePanel::moveTile(int delta) {
    TileMapData* map = m_ctx.activeMap();
    const int row = m_terrains->currentRow();
    const int to = row + delta;
    if (!map || row < 0 || to < 0 || to >= int(map->tiles.size())) return;
    // Échange de deux tiles : on échange aussi les ids dans les cases
    std::swap(map->tiles[std::size_t(row)], map->tiles[std::size_t(to)]);
    const uint16_t a = uint16_t(row + 1), b = uint16_t(to + 1);
    for (int y = 0; y < map->height(); ++y)
        for (int x = 0; x < map->width(); ++x) {
            const uint16_t id = map->get(x, y);
            if (id == a) map->set(x, y, b);
            else if (id == b) map->set(x, y, a);
        }
    m_ctx.setPaintTile(b);
    m_ctx.notifyTilesChanged();
}

void TilePalettePanel::syncTool(EditorTool tool) {
    if (QAbstractButton* b = m_tools->button(int(tool))) b->setChecked(true);
}

void TilePalettePanel::syncTerrain(uint16_t id) {
    QSignalBlocker block(m_terrains);
    for (int i = 0; i < m_terrains->count(); ++i)
        if (m_terrains->item(i)->data(kTypeRole).toInt() == int(id)) m_terrains->setCurrentRow(i);
}
