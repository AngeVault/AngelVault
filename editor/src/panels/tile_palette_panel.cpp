#include "tile_palette_panel.h"
#include "tileset_dialog.h"
#include "tileset_import.h"
#include "tileset_view.h"

#include <QAction>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {
constexpr int kIcon = 24;
QString qs(const std::string& s) { return QString::fromUtf8(s.data(), qsizetype(s.size())); }
std::string utf8(const QString& s) { return s.toUtf8().toStdString(); }

// Aperçu d'un tileset : sa tile « représentative » (case pleine pour un autotile)
QIcon tilesetIcon(const TileMapData& map, const Tileset& ts) {
    QPixmap pix(kIcon, kIcon);
    pix.fill(Qt::transparent);
    if (ts.missing || ts.count <= 0) return QIcon(pix);
    QImageReader reader(qs(map.resolve(ts.image)));
    const sf::IntRect r = ts.rect(ts.previewTile());
    reader.setClipRect(QRect(r.position.x, r.position.y, r.size.x, r.size.y));
    const QImage part = reader.read();
    if (part.isNull()) return QIcon(pix);
    // on enlève le vide autour (les tiles iso en ont beaucoup)
    int x0 = part.width(), y0 = part.height(), x1 = -1, y1 = -1;
    const QImage a = part.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x)
            if (qAlpha(a.pixel(x, y)) > 20) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y); }
    const QImage cropped = x1 >= x0 ? part.copy(x0, y0, x1 - x0 + 1, y1 - y0 + 1) : part;
    const bool pixelArt = cropped.width() <= 64;
    const QImage scaled = cropped.scaled(kIcon, kIcon, Qt::KeepAspectRatio, pixelArt ? Qt::FastTransformation : Qt::SmoothTransformation);
    QPainter p(&pix);
    p.drawImage((kIcon - scaled.width()) / 2, (kIcon - scaled.height()) / 2, scaled);
    return QIcon(pix);
}

QString uniquePath(const QString& wanted) {
    if (!QFileInfo::exists(wanted)) return wanted;
    const QFileInfo fi(wanted);
    for (int i = 2;; ++i) {
        const QString p = fi.absolutePath() + "/" + fi.completeBaseName() + QString("_%1.").arg(i) + fi.suffix();
        if (!QFileInfo::exists(p)) return p;
    }
}
} // namespace

TilePalettePanel::TilePalettePanel(EditorContext& context, QWidget* parent) : QWidget(parent), m_ctx(context) {
    setObjectName("tilePalettePanel");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // ---------------- Outils ----------------
    auto* toolsRow = new QHBoxLayout;
    toolsRow->setSpacing(3);
    m_tools = new QButtonGroup(this);
    m_tools->setExclusive(true);
    struct ToolDef { EditorTool tool; QString text; QString tip; QKeySequence key; };
    const ToolDef defs[] = {
        { EditorTool::Select, tr("Sélection"), tr("Sélectionner et déplacer des entités (V)"),               QKeySequence(Qt::Key_V) },
        { EditorTool::Paint,  tr("Pinceau"),   tr("Peindre (B)\nMaj + clic : ligne droite   Alt + clic : pipette"), QKeySequence(Qt::Key_B) },
        { EditorTool::Erase,  tr("Gomme"),     tr("Effacer (E)"),                                             QKeySequence(Qt::Key_E) },
        { EditorTool::Fill,   tr("Remplir"),   tr("Remplir une zone de cases identiques (G)"),               QKeySequence(Qt::Key_G) },
        { EditorTool::Pick,   tr("Pipette"),   tr("Prendre la tile d'une case (I, ou Alt + clic)"),          QKeySequence(Qt::Key_I) },
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

    // ---------------- Pinceau ----------------
    m_brushBox = new QWidget;
    auto* bv = new QVBoxLayout(m_brushBox);
    bv->setContentsMargins(0, 0, 0, 0);
    bv->setSpacing(4);
    auto* quickRow = new QHBoxLayout;
    quickRow->setSpacing(3);
    auto* sizeTitle = new QLabel(tr("Pinceau"));
    sizeTitle->setObjectName("paletteSectionTitle");
    quickRow->addWidget(sizeTitle);
    m_quickSizes = new QButtonGroup(this);
    m_quickSizes->setExclusive(false);   // aucune coché si la taille n'est pas une des tailles rapides
    for (int s : { 1, 2, 3, 5, 9 }) {
        auto* b = new QToolButton;
        b->setObjectName("paletteTool");
        b->setText(QString::number(s));
        b->setToolTip(tr("Pinceau de %1 x %1 case(s)").arg(s));
        b->setCheckable(true);
        b->setFixedWidth(30);
        m_quickSizes->addButton(b, s);
        quickRow->addWidget(b);
    }
    quickRow->addStretch(1);
    m_square = new QToolButton;
    m_circle = new QToolButton;
    m_square->setText(QString::fromUtf8("■"));
    m_circle->setText(QString::fromUtf8("●"));
    m_square->setToolTip(tr("Pinceau carré"));
    m_circle->setToolTip(tr("Pinceau rond"));
    auto* shapes = new QButtonGroup(this);
    for (QToolButton* b : { m_square, m_circle }) {
        b->setObjectName("paletteTool");
        b->setCheckable(true);
        b->setFixedWidth(30);
        shapes->addButton(b);
        quickRow->addWidget(b);
    }
    bv->addLayout(quickRow);

    auto* sliderRow = new QHBoxLayout;
    m_brushSlider = new QSlider(Qt::Horizontal);
    m_brushSlider->setRange(1, EditorContext::kMaxBrushSize);
    m_brushSlider->setPageStep(2);
    m_brushSpin = new QSpinBox;
    m_brushSpin->setRange(1, EditorContext::kMaxBrushSize);
    m_brushSpin->setSuffix(tr(" case(s)"));
    m_brushSpin->setToolTip(tr("Côté du pinceau, en cases"));
    sliderRow->addWidget(m_brushSlider, 1);
    sliderRow->addWidget(m_brushSpin);
    bv->addLayout(sliderRow);
    auto* brushHelp = new QLabel(tr("Ctrl+molette ou [ ] : taille · Maj+clic : ligne · Alt+clic : pipette"));
    brushHelp->setObjectName("paletteHelp");
    brushHelp->setWordWrap(true);
    bv->addWidget(brushHelp);
    layout->addWidget(m_brushBox);

    connect(m_quickSizes, &QButtonGroup::idClicked, &m_ctx, &EditorContext::setBrushSize);
    connect(m_brushSlider, &QSlider::valueChanged, &m_ctx, &EditorContext::setBrushSize);
    connect(m_brushSpin, &QSpinBox::valueChanged, &m_ctx, &EditorContext::setBrushSize);
    connect(m_square, &QToolButton::clicked, this, [this] { m_ctx.setBrushShape(BrushShape::Square); });
    connect(m_circle, &QToolButton::clicked, this, [this] { m_ctx.setBrushShape(BrushShape::Circle); });

    // Raccourcis de taille : [ ] (QWERTY) et - + (AZERTY : touches sans AltGr)
    struct SizeKey { QKeySequence key; int delta; };
    const SizeKey sizeKeys[] = { { QKeySequence(Qt::Key_BracketLeft), -1 }, { QKeySequence(Qt::Key_BracketRight), +1 },
                                 { QKeySequence(Qt::Key_Minus), -1 },       { QKeySequence(Qt::Key_Plus), +1 },
                                 { QKeySequence(Qt::Key_Equal), +1 } };
    for (const SizeKey& k : sizeKeys) {
        auto* a = new QAction(k.delta < 0 ? tr("Pinceau plus petit") : tr("Pinceau plus grand"), this);
        a->setShortcut(k.key);
        a->setShortcutContext(Qt::WindowShortcut);
        const int delta = k.delta;
        connect(a, &QAction::triggered, this, [this, delta] { m_ctx.setBrushSize(m_ctx.brushSize() + delta); });
        m_toolActions.push_back(a);
    }

    // ---------------- Tilesets ----------------
    auto* title = new QLabel(tr("Tilesets"));
    title->setObjectName("paletteSectionTitle");
    layout->addWidget(title);

    m_tilesetBar = new QWidget;
    auto* bar = new QHBoxLayout(m_tilesetBar);
    bar->setContentsMargins(0, 0, 0, 0);
    bar->setSpacing(3);
    m_tilesets = new QComboBox;
    m_tilesets->setObjectName("paletteTilesets");
    m_tilesets->setIconSize(QSize(kIcon, kIcon));
    m_tilesets->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_tilesets->setMinimumContentsLength(8);
    m_tilesets->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    bar->addWidget(m_tilesets, 1);
    auto* add = new QToolButton;
    add->setText("+");
    add->setToolTip(tr("Ajouter un tileset"));
    add->setPopupMode(QToolButton::InstantPopup);
    auto* addMenu = new QMenu(add);
    addMenu->addAction(tr("Nouveau tileset depuis une feuille (PNG)..."), this, &TilePalettePanel::newTilesetFromSheet);
    addMenu->addAction(tr("Nouveau tileset depuis des images séparées..."), this, &TilePalettePanel::newTilesetFromImages);
    addMenu->addSeparator();
    addMenu->addAction(tr("Ajouter un tileset existant (.tileset)..."), this, &TilePalettePanel::addExistingTileset);
    add->setMenu(addMenu);
    m_editTs = new QToolButton;
    m_editTs->setText(QString::fromUtf8("✎"));
    m_editTs->setToolTip(tr("Modifier le tileset (image, grille, autotile, placement)"));
    m_removeTs = new QToolButton;
    m_removeTs->setText(QString::fromUtf8("✕"));
    m_removeTs->setToolTip(tr("Retirer le tileset de cette carte (le fichier n'est pas supprimé)"));
    m_upTs = new QToolButton;
    m_upTs->setText(QString::fromUtf8("▲"));
    m_upTs->setToolTip(tr("Monter : passe SOUS les tilesets suivants"));
    m_downTs = new QToolButton;
    m_downTs->setText(QString::fromUtf8("▼"));
    m_downTs->setToolTip(tr("Descendre : déborde par-dessus les tilesets précédents (autotile)"));
    for (QToolButton* b : { add, m_editTs, m_removeTs, m_upTs, m_downTs }) {
        b->setObjectName("panelToolButton");
        bar->addWidget(b);
    }
    layout->addWidget(m_tilesetBar);
    connect(m_editTs, &QToolButton::clicked, this, &TilePalettePanel::editTileset);
    connect(m_removeTs, &QToolButton::clicked, this, &TilePalettePanel::removeTileset);
    connect(m_upTs, &QToolButton::clicked, this, [this] { moveTileset(-1); });
    connect(m_downTs, &QToolButton::clicked, this, [this] { moveTileset(+1); });
    connect(m_tilesets, &QComboBox::activated, this, [this](int index) {
        currentSlotChanged(m_tilesets->itemData(index).toInt());
    });

    auto* modeRow = new QHBoxLayout;
    m_autotile = new QToolButton;
    m_autotile->setObjectName("paletteTool");
    m_autotile->setCheckable(true);
    m_autotile->setText(tr("Autotile : peindre le terrain"));
    m_autotile->setToolTip(tr("Coché : les transitions sont choisies seules selon les cases voisines.\n"
                              "Décoché : on pose la tile choisie dans la grille, telle quelle."));
    m_autotile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    modeRow->addWidget(m_autotile, 1);
    layout->addLayout(modeRow);
    connect(m_autotile, &QToolButton::toggled, this, [this](bool on) {
        TileBrush b = m_ctx.brush();
        if (!b.valid()) return;
        b.autotile = on;
        if (!on) {   // retour à la tile choisie dans la grille
            const QRect sel = m_view->selection();
            if (const Tileset* ts = m_ctx.activeMap() ? m_ctx.activeMap()->tileset(b.slot) : nullptr; ts && sel.isValid()) {
                b.tile = std::max(0, ts->tileIndex(sel.left(), sel.top()));
                b.width = sel.width();
                b.height = sel.height();
            }
        } else {
            b.width = b.height = 1;
        }
        m_ctx.setBrush(b);
        if (m_ctx.tool() == EditorTool::Select || m_ctx.tool() == EditorTool::Pick || m_ctx.tool() == EditorTool::Erase)
            m_ctx.setTool(EditorTool::Paint);
    });

    m_view = new TilesetView;
    m_scroll = new QScrollArea;
    m_scroll->setObjectName("paletteTilesetScroll");
    m_scroll->setWidgetResizable(false);
    m_scroll->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_scroll->setWidget(m_view);
    m_scroll->setMinimumHeight(140);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->viewport()->setAutoFillBackground(false);
    m_scroll->viewport()->installEventFilter(this);
    layout->addWidget(m_scroll, 1);
    connect(m_view, &TilesetView::selectionChanged, this, &TilePalettePanel::gridSelectionChanged);

    m_selectionInfo = new QLabel;
    m_selectionInfo->setObjectName("paletteHelp");
    layout->addWidget(m_selectionInfo);
    m_mapInfo = new QLabel;
    m_mapInfo->setObjectName("paletteHelp");
    m_mapInfo->setWordWrap(true);
    layout->addWidget(m_mapInfo);

    connect(&m_ctx, &EditorContext::tilemapChanged, this, &TilePalettePanel::buildTilesets);
    connect(&m_ctx, &EditorContext::toolChanged, this, &TilePalettePanel::syncTool);
    connect(&m_ctx, &EditorContext::brushChanged, this, &TilePalettePanel::syncBrush);
    connect(&m_ctx, &EditorContext::brushSizeChanged, this, &TilePalettePanel::syncBrushSize);
    connect(&m_ctx, &EditorContext::brushShapeChanged, this, [this](BrushShape s) {
        (s == BrushShape::Circle ? m_circle : m_square)->setChecked(true);
    });
    (m_ctx.brushShape() == BrushShape::Circle ? m_circle : m_square)->setChecked(true);
    syncBrushSize(m_ctx.brushSize());
    buildTilesets();
    syncTool(m_ctx.tool());
}

QString TilePalettePanel::projectDir() const { return m_ctx.projectDir(); }

int TilePalettePanel::currentSlot() const {
    const int i = m_tilesets->currentIndex();
    return i >= 0 ? m_tilesets->itemData(i).toInt() : -1;
}

bool TilePalettePanel::eventFilter(QObject* watched, QEvent* event) {
    // La grille tient en largeur tant que l'utilisateur n'a pas zoomé lui-même (Ctrl + molette)
    if (watched == m_scroll->viewport() && event->type() == QEvent::Resize && !m_view->userZoomed())
        m_view->fitWidth(m_scroll->viewport()->width());
    return QWidget::eventFilter(watched, event);
}

// ---- Liste des tilesets ----------------------------------------------------------------------------

void TilePalettePanel::buildTilesets() {
    TileMapData* map = m_ctx.activeMap();
    {
        QSignalBlocker block(m_tilesets);
        m_tilesets->clear();
        if (map)
            for (std::size_t i = 0; i < map->tilesets.size(); ++i) {
                const Tileset& ts = *map->tilesets[i];
                QString label = qs(ts.name);
                if (ts.missing) label += tr("  (introuvable)");
                else if (ts.autotile == AutotileMode::Edges16) label += tr("  · autotile 4 bords");
                else if (ts.autotile == AutotileMode::Corners16) label += tr("  · autotile 16 coins");
                if (ts.ySort) label += tr("  · tri en Y");
                m_tilesets->addItem(tilesetIcon(*map, ts), label, int(i));
                m_tilesets->setItemData(int(i), qs(ts.file), Qt::ToolTipRole);
            }
    }
    const bool hasMap = map != nullptr;
    const bool any = hasMap && !map->tilesets.empty();
    m_tilesetBar->setEnabled(hasMap);
    for (QWidget* w : { static_cast<QWidget*>(m_editTs), static_cast<QWidget*>(m_removeTs),
                        static_cast<QWidget*>(m_upTs), static_cast<QWidget*>(m_downTs) })
        w->setEnabled(any);
    m_tilesets->setEnabled(any);

    if (!hasMap) {
        m_mapInfo->setText(tr("Pas de tilemap dans cette scène (elle est facultative).\n"
                              "Hierarchy > + > Tilemap orthogonale / isométrique, ou glissez un fichier .map."));
    } else {
        const bool iso = map->orientation == TileMapOrientation::Isometric;
        m_mapInfo->setText(tr("%1 %2 x %3, cases de %4 x %5").arg(iso ? tr("Isométrique") : tr("Orthogonale"))
                               .arg(map->width()).arg(map->height()).arg(map->tileSize.x).arg(map->tileSize.y)
                           + (any ? QString() : tr("\nAucun tileset : « + » pour en ajouter un (feuille PNG, "
                                                   "images séparées ou fichier .tileset).")));
    }

    // Le pinceau garde son tileset s'il existe encore, sinon le premier
    TileBrush b = m_ctx.brush();
    if (!any) {
        if (b.valid()) m_ctx.setBrush(TileBrush{});
        m_shownSlot = -2;
        showTileset(-1);
        return;
    }
    const Tileset* ts = map->tileset(b.slot);
    if (!ts) {
        const Tileset& first = *map->tilesets.front();
        m_ctx.setBrush(TileBrush{ 0, first.isAutotile(), first.previewTile() });
    } else if (!b.autotile && !ts->validTile(b.tile)) {
        b.tile = ts->previewTile();
        b.width = b.height = 1;
        m_ctx.setBrush(b);
    }
    m_shownSlot = -2;   // la grille est redessinée (le tileset a pu changer)
    syncBrush(m_ctx.brush());
}

void TilePalettePanel::showTileset(int slot) {
    TileMapData* map = m_ctx.activeMap();
    const Tileset* ts = map ? map->tileset(slot) : nullptr;
    const uint32_t version = map ? map->version : 0;
    if (slot == m_shownSlot && version == m_shownVersion) return;
    m_shownSlot = slot;
    m_shownVersion = version;
    m_view->setTileset(ts, ts ? qs(map->resolve(ts->image)) : QString());
    m_autotile->setVisible(ts && ts->isAutotile());
    if (!m_view->userZoomed()) m_view->fitWidth(m_scroll->viewport()->width());
}

void TilePalettePanel::currentSlotChanged(int slot) {
    const TileMapData* map = m_ctx.activeMap();
    const Tileset* ts = map ? map->tileset(slot) : nullptr;
    if (!ts) return;
    TileBrush b{ slot, ts->isAutotile(), ts->previewTile() };
    m_ctx.setBrush(b);
    if (m_ctx.tool() == EditorTool::Select || m_ctx.tool() == EditorTool::Pick || m_ctx.tool() == EditorTool::Erase)
        m_ctx.setTool(EditorTool::Paint);
}

void TilePalettePanel::gridSelectionChanged(const QRect& tiles) {
    const TileMapData* map = m_ctx.activeMap();
    const int slot = currentSlot();
    const Tileset* ts = map ? map->tileset(slot) : nullptr;
    if (!ts || !tiles.isValid()) return;
    TileBrush b;
    b.slot = slot;
    b.autotile = false;
    b.tile = std::max(0, ts->tileIndex(tiles.left(), tiles.top()));
    b.width = tiles.width();
    b.height = tiles.height();
    m_ctx.setBrush(b);
    if (m_ctx.tool() != EditorTool::Paint && m_ctx.tool() != EditorTool::Fill) m_ctx.setTool(EditorTool::Paint);
}

void TilePalettePanel::syncBrush(const TileBrush& b) {
    const TileMapData* map = m_ctx.activeMap();
    const Tileset* ts = map ? map->tileset(b.slot) : nullptr;
    {
        QSignalBlocker block(m_tilesets);
        for (int i = 0; i < m_tilesets->count(); ++i)
            if (m_tilesets->itemData(i).toInt() == b.slot) m_tilesets->setCurrentIndex(i);
    }
    showTileset(ts ? b.slot : -1);
    {
        QSignalBlocker block(m_autotile);
        m_autotile->setChecked(ts && b.autotile);
    }
    if (ts) {
        if (!b.autotile) {
            const sf::Vector2i c = ts->tileCoords(b.tile);
            m_view->setSelection(QRect(c.x, c.y, b.width, b.height));
        } else if (!m_view->selection().isValid()) {
            const sf::Vector2i c = ts->tileCoords(ts->previewTile());
            m_view->setSelection(QRect(c.x, c.y, 1, 1));
        }
        m_view->setDimmed(b.autotile);
    }
    if (!ts) m_selectionInfo->setText(QString());
    else if (b.autotile) m_selectionInfo->setText(tr("%1 : terrain (autotile)").arg(qs(ts->name)));
    else if (b.isStamp()) m_selectionInfo->setText(tr("%1 : bloc de %2 x %3 tiles").arg(qs(ts->name)).arg(b.width).arg(b.height));
    else m_selectionInfo->setText(tr("%1 : tile %2").arg(qs(ts->name)).arg(b.tile));
}

void TilePalettePanel::syncTool(EditorTool tool) {
    if (QAbstractButton* b = m_tools->button(int(tool))) b->setChecked(true);
}

void TilePalettePanel::syncBrushSize(int size) {
    QSignalBlocker a(m_brushSlider), b(m_brushSpin);
    m_brushSlider->setValue(size);
    m_brushSpin->setValue(size);
    for (QAbstractButton* q : m_quickSizes->buttons()) q->setChecked(m_quickSizes->id(q) == size);
}

// ---- Création / modification -------------------------------------------------------------------

bool TilePalettePanel::runTilesetDialog(const Tileset& start, bool isNew, int slot) {
    TileMapData* map = m_ctx.activeMap();
    if (!map) return false;
    TilesetDialog dialog(projectDir(), start, isNew, map->orientation == TileMapOrientation::Isometric, this);
    if (dialog.exec() != QDialog::Accepted) return false;
    Tileset result = dialog.result();
    std::string err;
    if (!saveTileset(result, utf8(dialog.absoluteFile()), &err)) {
        QMessageBox::warning(this, tr("Tileset"), qs(err));
        return false;
    }
    if (isNew) {
        auto ts = std::make_shared<Tileset>(result);
        int s = map->tilesetSlot(result.file);
        if (s >= 0) *map->tilesets[std::size_t(s)] = result;   // ce fichier était déjà dans la carte : remplacé
        else s = map->addTileset(ts);
        m_ctx.notifyTilesChanged();
        m_ctx.notifyTilesetSaved(map->tilesets[std::size_t(s)]);
        m_ctx.setBrush(TileBrush{ s, result.isAutotile(), result.previewTile() });
        if (m_ctx.tool() == EditorTool::Select || m_ctx.tool() == EditorTool::Pick) m_ctx.setTool(EditorTool::Paint);
    } else if (Tileset* ts = map->tileset(slot)) {
        *ts = result;
        m_view->reloadImage();
        m_ctx.notifyTilesChanged();
        m_ctx.notifyTilesetSaved(map->tilesets[std::size_t(slot)]);
    }
    return true;
}

void TilePalettePanel::newTilesetFromSheet() {
    TileMapData* map = m_ctx.activeMap();
    if (!map) return;
    const QString file = QFileDialog::getOpenFileName(this, tr("Feuille du tileset"), projectDir() + "/assets",
                                                      tr("Images (*.png *.jpg *.jpeg *.bmp *.tga)"));
    if (file.isEmpty()) return;
    Tileset start;
    const QString rel = QDir(projectDir()).relativeFilePath(file);
    start.image = utf8(rel.startsWith("..") ? file : rel);
    start.name = utf8(QFileInfo(file).completeBaseName());
    // Taille de départ : la case de la carte si elle tombe juste, sinon 32 (ajustable dans la fenêtre)
    const QSize size = QImageReader(file).size();
    const int w = int(map->tileSize.x), h = map->orientation == TileMapOrientation::Isometric ? int(map->tileSize.x) : int(map->tileSize.y);
    if (size.isValid() && w > 0 && h > 0 && size.width() % w == 0 && size.height() % h == 0) start.tileSize = { w, h };
    else start.tileSize = { 32, 32 };
    runTilesetDialog(start, true, -1);
}

void TilePalettePanel::newTilesetFromImages() {
    TileMapData* map = m_ctx.activeMap();
    if (!map) return;
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Images à rassembler en un tileset"), projectDir() + "/assets",
                                                            tr("Images (*.png *.jpg *.jpeg *.bmp *.tga)"));
    if (files.isEmpty()) return;
    const QSize first = QImageReader(files.front()).size();

    // Options d'assemblage
    QDialog options(this);
    options.setWindowTitle(tr("Images → tileset"));
    options.setObjectName("tileDefDialog");
    auto* form = new QFormLayout(&options);
    auto* info = new QLabel(tr("%1 image(s) vont être rassemblées dans une seule feuille PNG.").arg(files.size()));
    info->setWordWrap(true);
    form->addRow(info);
    auto* name = new QLineEdit(QFileInfo(QFileInfo(files.front()).absolutePath()).fileName());
    form->addRow(tr("Nom"), name);
    auto* group = new QCheckBox(tr("Ranger les rotations _N _E _S _W sur une même ligne"));
    group->setChecked(true);
    form->addRow(QString(), group);
    auto* trim = new QCheckBox(tr("Rogner le vide commun à toutes les images"));
    trim->setChecked(true);
    form->addRow(QString(), trim);
    auto* autoAnchor = new QCheckBox(tr("Centre de l'image"));
    autoAnchor->setChecked(true);
    form->addRow(tr("Ancre (images d'origine)"), autoAnchor);
    auto* anchorRow = new QWidget;
    auto* ah = new QHBoxLayout(anchorRow);
    ah->setContentsMargins(0, 0, 0, 0);
    auto* ax = new QDoubleSpinBox;
    auto* ay = new QDoubleSpinBox;
    for (QDoubleSpinBox* s : { ax, ay }) { s->setRange(-4096, 4096); s->setDecimals(1); s->setSuffix(" px"); }
    ax->setValue(first.width() * 0.5);
    ay->setValue(first.height() * 0.5);
    ah->addWidget(new QLabel("X"));
    ah->addWidget(ax, 1);
    ah->addWidget(new QLabel("Y"));
    ah->addWidget(ay, 1);
    anchorRow->setToolTip(tr("Point des images d'ORIGINE posé au centre de la case (il est recalculé après le rognage).\n"
                             "Ex. tiles iso 256 x 256 dont le losange est en bas : X 128, Y 208."));
    anchorRow->setEnabled(false);
    connect(autoAnchor, &QCheckBox::toggled, anchorRow, [anchorRow](bool on) { anchorRow->setEnabled(!on); });
    form->addRow(QString(), anchorRow);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &options, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &options, &QDialog::reject);
    form->addRow(buttons);
    if (options.exec() != QDialog::Accepted) return;

    QString base = name->text().trimmed();
    base.replace(QRegularExpression(R"([\\/:*?"<>|])"), "_");
    if (base.isEmpty()) base = "tileset";
    const QString png = uniquePath(projectDir() + "/assets/tilesets/" + base + ".png");
    const PackedSheet sheet = packTilesetImages(files, png, group->isChecked(), trim->isChecked());
    if (!sheet.ok) { QMessageBox::warning(this, tr("Images → tileset"), sheet.error); return; }

    Tileset start;
    start.name = utf8(name->text().trimmed().isEmpty() ? base : name->text().trimmed());
    start.image = utf8(QDir(projectDir()).relativeFilePath(png));
    start.tileSize = { sheet.tileSize.width(), sheet.tileSize.height() };
    start.columns = sheet.columns;
    start.count = sheet.count;
    // L'ancre suit le rognage. « Centre » des images d'origine = centre de l'ancienne image, pas de la tile rognée.
    const QPointF anchor = autoAnchor->isChecked() ? QPointF(sheet.sourceSize.width() * 0.5, sheet.sourceSize.height() * 0.5)
                                                   : QPointF(ax->value(), ay->value());
    if (trim->isChecked() || !autoAnchor->isChecked())
        start.anchor = { float(anchor.x() - sheet.cropOffset.x()), float(anchor.y() - sheet.cropOffset.y()) };
    if (map->orientation == TileMapOrientation::Orthogonal && autoAnchor->isChecked()) start.anchor = { -1.f, -1.f };
    if (!runTilesetDialog(start, true, -1)) QFile::remove(png);   // annulé : la feuille créée est retirée
}

void TilePalettePanel::addExistingTileset() {
    TileMapData* map = m_ctx.activeMap();
    if (!map) return;
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Ajouter des tilesets"), projectDir() + "/assets",
                                                            tr("Tileset (*.tileset)"));
    if (files.isEmpty()) return;
    m_ctx.dropFiles(files, NULL_ENTITY);   // même chemin que le glisser-déposer
}

void TilePalettePanel::editTileset() {
    TileMapData* map = m_ctx.activeMap();
    const int slot = currentSlot();
    if (!map || !map->tileset(slot)) return;
    runTilesetDialog(*map->tileset(slot), false, slot);
}

void TilePalettePanel::removeTileset() {
    TileMapData* map = m_ctx.activeMap();
    const int slot = currentSlot();
    if (!map || !map->tileset(slot)) return;
    int used = 0;
    for (int y = 0; y < map->height(); ++y)
        for (int x = 0; x < map->width(); ++x) used += cellSlot(map->get(x, y)) == slot;
    const QString name = qs(map->tileset(slot)->name);
    if (QMessageBox::question(this, tr("Retirer le tileset"),
            tr("Retirer « %1 » de cette carte ?\n%2 case(s) peinte(s) avec deviennent vides.\n"
               "Le fichier .tileset et son image restent sur le disque.").arg(name).arg(used)) != QMessageBox::Yes)
        return;
    map->removeTileset(slot);
    TileBrush b = m_ctx.brush();
    if (b.slot == slot) b = TileBrush{};
    else if (b.slot > slot) --b.slot;
    m_ctx.setBrush(b);
    m_ctx.notifyTilesChanged();
}

void TilePalettePanel::moveTileset(int delta) {
    TileMapData* map = m_ctx.activeMap();
    const int slot = currentSlot();
    const int to = slot + delta;
    if (!map || !map->tileset(slot) || !map->tileset(to)) return;
    map->moveTileset(slot, to);
    TileBrush b = m_ctx.brush();
    if (b.slot == slot) b.slot = to;
    else if (b.slot == to) b.slot = slot;
    m_ctx.setBrush(b);
    m_ctx.notifyTilesChanged();
}
