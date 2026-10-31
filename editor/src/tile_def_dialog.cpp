#include "tile_def_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>

namespace {
QString qs(const std::string& s) { return QString::fromUtf8(s.data(), qsizetype(s.size())); }
std::string utf8(const QString& s) { return s.toUtf8().toStdString(); }

// Liste d'images + boutons Ajouter / Retirer
QWidget* imageList(QListWidget*& list, const std::function<QString()>& pick) {
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    list = new QListWidget;
    list->setMaximumHeight(90);
    v->addWidget(list);
    auto* row = new QHBoxLayout;
    auto* add = new QPushButton(QObject::tr("Ajouter..."));
    auto* remove = new QPushButton(QObject::tr("Retirer"));
    row->addWidget(add);
    row->addWidget(remove);
    row->addStretch();
    v->addLayout(row);
    QListWidget* l = list;
    QObject::connect(add, &QPushButton::clicked, w, [l, pick] { const QString p = pick(); if (!p.isEmpty()) l->addItem(p); });
    QObject::connect(remove, &QPushButton::clicked, w, [l] { delete l->currentItem(); });
    return w;
}
} // namespace

TileDefDialog::TileDefDialog(const TileMapData& map, const TileDef& tile, int editedIndex, QWidget* parent)
    : QDialog(parent), m_map(map), m_tile(tile) {
    setWindowTitle(editedIndex < 0 ? tr("Nouvelle tile") : tr("Modifier la tile"));
    setObjectName("tileDefDialog");
    resize(620, 560);
    const bool iso = map.orientation == TileMapOrientation::Isometric;

    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    m_name = new QLineEdit(qs(tile.name));
    form->addRow(tr("Nom"), m_name);
    m_mode = new QComboBox;
    m_mode->addItem(tr("Simple (une image, ou des variantes)"));
    m_mode->addItem(tr("Autotile : 16 coins (feuille dual-grid, 16 colonnes)"));
    m_mode->addItem(tr("Autotile : 4 bords (pièces 1 bord / 2 bords / 4 bords)"));
    m_mode->setCurrentIndex(int(tile.autotile));
    form->addRow(tr("Type"), m_mode);
    root->addLayout(form);

    // Images principales
    m_imagesLabel = new QLabel;
    m_imagesLabel->setObjectName("hintLabel");
    m_imagesLabel->setWordWrap(true);
    root->addWidget(m_imagesLabel);
    root->addWidget(imageList(m_images, [this] { return pickImage(tr("Image de la tile")); }));
    for (const std::string& s : tile.images) m_images->addItem(qs(s));

    // Autotile 16 coins
    m_cornersBox = new QGroupBox(tr("Feuille dual-grid"));
    auto* cf = new QFormLayout(m_cornersBox);
    m_cellPixels = new QSpinBox;
    m_cellPixels->setRange(1, 1024);
    m_cellPixels->setSuffix(" px");
    m_cellPixels->setValue(tile.cellPixels);
    cf->addRow(tr("Taille d'une cellule"), m_cellPixels);
    m_frameTime = new QDoubleSpinBox;
    m_frameTime->setRange(0.0, 10.0);
    m_frameTime->setSingleStep(0.05);
    m_frameTime->setSuffix(" s");
    m_frameTime->setValue(tile.frameTime);
    m_frameTime->setToolTip(tr("0 : chaque ligne de la feuille est une variante.\n> 0 : les lignes sont les images d'une animation."));
    cf->addRow(tr("Animation (par image)"), m_frameTime);
    root->addWidget(m_cornersBox);

    // Autotile 4 bords
    m_edgesBox = new QGroupBox(tr("Pièces de transition (posées sur les cases voisines)"));
    auto* grid = new QGridLayout(m_edgesBox);
    const QStringList dirs = iso ? QStringList{ tr("NE"), tr("SE"), tr("SO"), tr("NO") }
                                 : QStringList{ tr("Haut"), tr("Droite"), tr("Bas"), tr("Gauche") };
    grid->addWidget(new QLabel(tr("Bord")), 0, 0);
    grid->addWidget(new QLabel(tr("1 bord")), 0, 1);
    grid->addWidget(new QLabel(tr("2 bords (ce bord + le suivant)")), 0, 2);
    for (int i = 0; i < 4; ++i) {
        grid->addWidget(new QLabel(dirs[i]), i + 1, 0);
        for (int kind = 0; kind < 2; ++kind) {
            auto* cell = new QWidget;
            auto* h = new QHBoxLayout(cell);
            h->setContentsMargins(0, 0, 0, 0);
            auto* field = new QLineEdit(qs(kind == 0 ? tile.edge[i] : tile.corner[i]));
            field->setPlaceholderText(tr("(aucune)"));
            auto* browse = new QToolButton;
            browse->setText("...");
            h->addWidget(field, 1);
            h->addWidget(browse);
            grid->addWidget(cell, i + 1, kind + 1);
            (kind == 0 ? m_edge : m_corner)[i] = field;
            connect(browse, &QToolButton::clicked, this, [this, i, kind] {
                const QString p = pickImage(kind == 0 ? tr("Pièce « 1 bord »") : tr("Pièce « 2 bords »"));
                if (p.isEmpty()) return;
                auto& fields = kind == 0 ? m_edge : m_corner;
                fields[i]->setText(p);
                fillRotations(fields, i, p);   // _N / _E / _S / _W : les 3 autres rotations sont retrouvées
            });
        }
    }
    auto* ringHint = new QLabel(tr("4 bords (anneau, facultatif)"));
    grid->addWidget(ringHint, 5, 0, 1, 3);
    grid->addWidget(imageList(m_ring, [this] { return pickImage(tr("Pièce « 4 bords »")); }), 6, 0, 1, 3);
    for (const std::string& s : tile.ring) m_ring->addItem(qs(s));
    auto* edgesHint = new QLabel(tr("Astuce : choisissez un fichier qui finit par _N, _E, _S ou _W, "
                                    "les autres rotations sont remplies seules."));
    edgesHint->setObjectName("hintLabel");
    edgesHint->setWordWrap(true);
    grid->addWidget(edgesHint, 7, 0, 1, 3);
    root->addWidget(m_edgesBox);

    // Placement / options
    auto* opts = new QGroupBox(tr("Options"));
    auto* of = new QFormLayout(opts);
    m_base = new QComboBox;
    m_base->addItem(tr("(aucune)"));
    for (std::size_t i = 0; i < map.tiles.size(); ++i)
        if (int(i) != editedIndex) m_base->addItem(qs(map.tiles[i].name));
    m_base->setCurrentIndex(std::max(0, m_base->findText(qs(tile.base))));
    m_base->setToolTip(tr("Tile dessinée sous celle-ci (ex. le bloc de terre sous l'herbe)"));
    of->addRow(tr("Dessous"), m_base);

    m_autoAnchor = new QCheckBox(iso ? tr("Centre de l'image") : tr("Remplir la case (étirer l'image)"));
    m_autoAnchor->setChecked(tile.anchor.x < 0.f);
    of->addRow(tr("Placement"), m_autoAnchor);
    auto* anchorRow = new QWidget;
    auto* ah = new QHBoxLayout(anchorRow);
    ah->setContentsMargins(0, 0, 0, 0);
    m_anchorX = new QDoubleSpinBox;
    m_anchorY = new QDoubleSpinBox;
    for (QDoubleSpinBox* s : { m_anchorX, m_anchorY }) { s->setRange(-4096, 4096); s->setDecimals(1); s->setSuffix(" px"); }
    m_anchorX->setValue(tile.anchor.x >= 0.f ? tile.anchor.x : 0.0);
    m_anchorY->setValue(tile.anchor.y >= 0.f ? tile.anchor.y : 0.0);
    ah->addWidget(new QLabel("X"));
    ah->addWidget(m_anchorX, 1);
    ah->addWidget(new QLabel("Y"));
    ah->addWidget(m_anchorY, 1);
    anchorRow->setToolTip(tr("Point de l'image (en pixels) posé au centre de la case.\n"
                             "Ex. tiles iso 256x256 dont le losange est en bas : X 128, Y 208."));
    of->addRow(tr("Ancre"), anchorRow);
    m_scale = new QDoubleSpinBox;
    m_scale->setRange(0.01, 100.0);
    m_scale->setSingleStep(0.1);
    m_scale->setValue(tile.scale);
    of->addRow(tr("Échelle"), m_scale);
    m_solid = new QCheckBox(tr("Bloque le passage"));
    m_solid->setChecked(tile.solid);
    of->addRow(tr("Solide"), m_solid);
    root->addWidget(opts);
    connect(m_autoAnchor, &QCheckBox::toggled, anchorRow, [anchorRow](bool on) { anchorRow->setEnabled(!on); });
    anchorRow->setEnabled(!m_autoAnchor->isChecked());

    root->addStretch(1);   // les blocs masqués (selon le type) ne laissent pas de trous
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &TileDefDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    connect(m_mode, &QComboBox::currentIndexChanged, this, &TileDefDialog::updateMode);
    updateMode();
}

void TileDefDialog::updateMode() {
    const auto mode = AutotileMode(m_mode->currentIndex());
    m_cornersBox->setVisible(mode == AutotileMode::Corners16);
    m_edgesBox->setVisible(mode == AutotileMode::Edges16);
    if (mode == AutotileMode::None)
        m_imagesLabel->setText(tr("Image(s) : plusieurs = variantes choisies au hasard (toujours les mêmes pour une case)."));
    else if (mode == AutotileMode::Corners16)
        m_imagesLabel->setText(tr("Feuille(s) dual-grid : 16 colonnes, une par combinaison des 4 coins "
                                  "(colonne 15 = case pleine). Plusieurs feuilles = variantes."));
    else
        m_imagesLabel->setText(tr("Case pleine (variantes). Les cases voisines d'une autre tile reçoivent les pièces ci-dessous : "
                                  "cette tile « déborde » sur les tiles placées AVANT elle dans la palette."));
}

QString TileDefDialog::absolute(const QString& path) const {
    return QString::fromStdString(m_map.resolve(utf8(path)));
}

QString TileDefDialog::relative(const QString& abs) const {
    if (m_map.baseDir.empty()) return abs;
    const QString rel = QDir(qs(m_map.baseDir)).relativeFilePath(abs);
    return rel.startsWith("..") ? abs : rel;   // hors du projet : chemin complet
}

QString TileDefDialog::pickImage(const QString& title) {
    static QString lastDir;
    if (lastDir.isEmpty()) lastDir = qs(m_map.baseDir) + "/assets";
    const QString file = QFileDialog::getOpenFileName(this, title, lastDir, tr("Images (*.png *.jpg *.jpeg *.bmp *.tga)"));
    if (file.isEmpty()) return {};
    lastDir = QFileInfo(file).absolutePath();
    const QString rel = relative(file);
    if (QDir::isAbsolutePath(rel))
        QMessageBox::information(this, tr("Image hors du projet"),
            tr("Cette image n'est pas dans le dossier du projet : copiez-la dans assets/ pour que le jeu la retrouve partout."));
    return rel;
}

void TileDefDialog::fillRotations(std::array<QLineEdit*, 4>& fields, int index, const QString& path) {
    static const QRegularExpression re(R"(^(.*[_\- ])([NESW])(\.[^.]+)$)");
    const QRegularExpressionMatch m = re.match(path);
    if (!m.hasMatch()) return;
    const QString letters = "NESW";
    const int start = letters.indexOf(m.captured(2));
    for (int j = 0; j < 4; ++j) {
        if (j == index || !fields[j]->text().isEmpty()) continue;
        const QString candidate = m.captured(1) + letters[(start + (j - index) + 4) % 4] + m.captured(3);
        if (QFileInfo::exists(absolute(candidate))) fields[j]->setText(candidate);
    }
}

TileDef TileDefDialog::result() const {
    TileDef t = m_tile;
    t.name = utf8(m_name->text().trimmed());
    t.autotile = AutotileMode(m_mode->currentIndex());
    t.images.clear();
    for (int i = 0; i < m_images->count(); ++i) t.images.push_back(utf8(m_images->item(i)->text()));
    t.cellPixels = m_cellPixels->value();
    t.frameTime = float(m_frameTime->value());
    for (int i = 0; i < 4; ++i) {
        t.edge[i] = utf8(m_edge[i]->text().trimmed());
        t.corner[i] = utf8(m_corner[i]->text().trimmed());
    }
    t.ring.clear();
    for (int i = 0; i < m_ring->count(); ++i) t.ring.push_back(utf8(m_ring->item(i)->text()));
    t.base = m_base->currentIndex() > 0 ? utf8(m_base->currentText()) : std::string();
    t.anchor = m_autoAnchor->isChecked() ? sf::Vector2f{ -1.f, -1.f }
                                         : sf::Vector2f{ float(m_anchorX->value()), float(m_anchorY->value()) };
    t.scale = float(m_scale->value());
    t.solid = m_solid->isChecked();
    return t;
}

void TileDefDialog::accept() {
    const QString name = m_name->text().trimmed();
    if (name.isEmpty()) { QMessageBox::warning(this, windowTitle(), tr("Donnez un nom à la tile.")); return; }
    for (const TileDef& other : m_map.tiles)
        if (qs(other.name) == name && other.name != m_tile.name) {
            QMessageBox::warning(this, windowTitle(), tr("Une tile s'appelle déjà « %1 ».").arg(name));
            return;
        }
    if (m_images->count() == 0) {
        QMessageBox::warning(this, windowTitle(), tr("Ajoutez au moins une image."));
        return;
    }
    QDialog::accept();
}
