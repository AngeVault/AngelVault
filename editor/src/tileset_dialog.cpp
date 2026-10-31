#include "tileset_dialog.h"
#include "tileset_view.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {
QString qs(const std::string& s) { return QString::fromUtf8(s.data(), qsizetype(s.size())); }
std::string utf8(const QString& s) { return s.toUtf8().toStdString(); }

void toggle(std::vector<int>& list, int index) {
    const auto it = std::find(list.begin(), list.end(), index);
    if (it != list.end()) list.erase(it);
    else list.push_back(index);
}
} // namespace

TilesetDialog::TilesetDialog(const QString& projectDir, const Tileset& tileset, bool isNew, bool isometric, QWidget* parent)
    : QDialog(parent), m_projectDir(projectDir), m_ts(tileset), m_isNew(isNew), m_iso(isometric), m_file(qs(tileset.file)) {
    setWindowTitle(isNew ? tr("Nouveau tileset") : tr("Tileset : %1").arg(qs(tileset.name)));
    setObjectName("tileDefDialog");   // style des thèmes (QSS) partagé avec les autres fenêtres de tiles
    resize(980, 640);
    m_fileChosen = !isNew && !m_file.isEmpty();

    auto* root = new QVBoxLayout(this);
    auto* columns = new QHBoxLayout;
    root->addLayout(columns, 1);

    // ---------------- Colonne de gauche : réglages ----------------
    auto* left = new QVBoxLayout;
    columns->addLayout(left, 0);

    auto* general = new QGroupBox(tr("Feuille"));
    auto* gf = new QFormLayout(general);
    m_name = new QLineEdit(qs(tileset.name));
    gf->addRow(tr("Nom"), m_name);
    auto* imageRow = new QWidget;
    auto* ih = new QHBoxLayout(imageRow);
    ih->setContentsMargins(0, 0, 0, 0);
    m_image = new QLineEdit(qs(tileset.image));
    m_image->setPlaceholderText(tr("assets/tilesets/....png"));
    auto* browse = new QToolButton;
    browse->setText("...");
    ih->addWidget(m_image, 1);
    ih->addWidget(browse);
    gf->addRow(tr("Image"), imageRow);

    auto* sizeRow = new QWidget;
    auto* sh = new QHBoxLayout(sizeRow);
    sh->setContentsMargins(0, 0, 0, 0);
    m_tileW = new QSpinBox;
    m_tileH = new QSpinBox;
    for (QSpinBox* s : { m_tileW, m_tileH }) { s->setRange(1, 4096); s->setSuffix(" px"); }
    m_tileW->setValue(tileset.tileSize.x);
    m_tileH->setValue(tileset.tileSize.y);
    sh->addWidget(m_tileW, 1);
    sh->addWidget(new QLabel("x"));
    sh->addWidget(m_tileH, 1);
    gf->addRow(tr("Taille d'une tile"), sizeRow);
    m_margin = new QSpinBox;
    m_spacing = new QSpinBox;
    for (QSpinBox* s : { m_margin, m_spacing }) { s->setRange(0, 512); s->setSuffix(" px"); }
    m_margin->setValue(tileset.margin);
    m_spacing->setValue(tileset.spacing);
    m_margin->setToolTip(tr("Bord vide autour de la grille"));
    m_spacing->setToolTip(tr("Espace vide entre deux tiles"));
    gf->addRow(tr("Marge"), m_margin);
    gf->addRow(tr("Espacement"), m_spacing);
    m_gridInfo = new QLabel;
    m_gridInfo->setObjectName("hintLabel");
    gf->addRow(QString(), m_gridInfo);
    left->addWidget(general);

    auto* typeBox = new QGroupBox(tr("Type"));
    auto* tv = new QVBoxLayout(typeBox);
    m_mode = new QComboBox;
    m_mode->addItem(tr("Simple : on choisit la tile dans la grille"));
    m_mode->addItem(tr("Autotile 16 coins (feuille dual-grid, 16 colonnes)"));
    m_mode->addItem(tr("Autotile 4 bords (pièces 1 bord / 2 bords / 4 bords)"));
    m_mode->setCurrentIndex(int(tileset.autotile));
    tv->addWidget(m_mode);

    // Autotile 4 bords : rôles
    m_edgesBox = new QGroupBox(tr("Rôles des tiles (cliquez dans la feuille)"));
    auto* ef = new QVBoxLayout(m_edgesBox);
    m_role = new QComboBox;
    const QStringList dirs = m_iso ? QStringList{ tr("NE"), tr("SE"), tr("SO"), tr("NO") }
                                   : QStringList{ tr("haut"), tr("droite"), tr("bas"), tr("gauche") };
    m_role->addItem(tr("P : case pleine (plusieurs = variantes)"));
    for (int i = 0; i < 4; ++i) m_role->addItem(tr("1 bord : %1").arg(dirs[i]));
    for (int i = 0; i < 4; ++i) m_role->addItem(tr("2 bords : %1 + %2").arg(dirs[i], dirs[(i + 1) % 4]));
    m_role->addItem(tr("4 bords : anneau (facultatif, variantes)"));
    m_role->addItem(tr("D : dessous (sous la case pleine, facultatif)"));
    ef->addWidget(m_role);
    auto* roleButtons = new QHBoxLayout;
    auto* standard = new QPushButton(tr("Disposition standard"));
    standard->setToolTip(tr("4 colonnes : ligne 1 = pleine, 2 = 1 bord, 3 = 2 bords, 4 = 4 bords, 5 = dessous"));
    auto* clearRoles = new QPushButton(tr("Tout effacer"));
    roleButtons->addWidget(standard);
    roleButtons->addWidget(clearRoles);
    roleButtons->addStretch();
    ef->addLayout(roleButtons);
    auto* edgesHint = new QLabel(tr("Les pièces de bord sont posées sur les cases VOISINES : ce terrain « déborde » "
                                    "sur les tilesets placés avant lui dans la palette."));
    edgesHint->setObjectName("hintLabel");
    edgesHint->setWordWrap(true);
    ef->addWidget(edgesHint);
    tv->addWidget(m_edgesBox);

    // Autotile 16 coins
    m_cornersBox = new QGroupBox(tr("Feuille dual-grid"));
    auto* cf = new QFormLayout(m_cornersBox);
    m_cornersInfo = new QLabel;
    m_cornersInfo->setObjectName("hintLabel");
    m_cornersInfo->setWordWrap(true);
    cf->addRow(m_cornersInfo);
    m_frameTime = new QDoubleSpinBox;
    m_frameTime->setRange(0.0, 10.0);
    m_frameTime->setSingleStep(0.05);
    m_frameTime->setSuffix(" s");
    m_frameTime->setValue(tileset.frameTime);
    m_frameTime->setToolTip(tr("0 : chaque ligne de la feuille est une variante.\n> 0 : les lignes sont les images d'une animation."));
    cf->addRow(tr("Animation (par image)"), m_frameTime);
    tv->addWidget(m_cornersBox);
    left->addWidget(typeBox);

    auto* placeBox = new QGroupBox(tr("Placement"));
    auto* pf = new QFormLayout(placeBox);
    m_autoAnchor = new QCheckBox(m_iso ? tr("Centre de la tile") : tr("Remplir la case (étirer la tile)"));
    m_autoAnchor->setChecked(tileset.anchor.x < 0.f);
    pf->addRow(tr("Ancre"), m_autoAnchor);
    auto* anchorRow = new QWidget;
    auto* ah = new QHBoxLayout(anchorRow);
    ah->setContentsMargins(0, 0, 0, 0);
    m_anchorX = new QDoubleSpinBox;
    m_anchorY = new QDoubleSpinBox;
    for (QDoubleSpinBox* s : { m_anchorX, m_anchorY }) { s->setRange(-4096, 4096); s->setDecimals(1); s->setSuffix(" px"); }
    m_anchorX->setValue(tileset.anchor.x >= 0.f ? tileset.anchor.x : 0.0);
    m_anchorY->setValue(tileset.anchor.y >= 0.f ? tileset.anchor.y : 0.0);
    ah->addWidget(new QLabel("X"));
    ah->addWidget(m_anchorX, 1);
    ah->addWidget(new QLabel("Y"));
    ah->addWidget(m_anchorY, 1);
    anchorRow->setToolTip(tr("Point de la tile (en pixels) posé au centre de la case.\n"
                             "Ex. tiles iso dont le losange est en bas : le centre du losange."));
    pf->addRow(QString(), anchorRow);
    m_scale = new QDoubleSpinBox;
    m_scale->setRange(0.01, 100.0);
    m_scale->setSingleStep(0.1);
    m_scale->setValue(tileset.scale);
    pf->addRow(tr("Échelle"), m_scale);
    m_solid = new QCheckBox(tr("Bloque le passage"));
    m_solid->setChecked(tileset.solid);
    pf->addRow(tr("Solide"), m_solid);
    left->addWidget(placeBox);
    connect(m_autoAnchor, &QCheckBox::toggled, anchorRow, [anchorRow](bool on) { anchorRow->setEnabled(!on); });
    anchorRow->setEnabled(!m_autoAnchor->isChecked());

    auto* fileRow = new QHBoxLayout;
    m_fileLabel = new QLabel;
    m_fileLabel->setObjectName("hintLabel");
    m_fileLabel->setWordWrap(true);
    fileRow->addWidget(m_fileLabel, 1);
    if (isNew) {
        auto* chooseFile = new QToolButton;
        chooseFile->setText(tr("Changer..."));
        fileRow->addWidget(chooseFile);
        connect(chooseFile, &QToolButton::clicked, this, [this] {
            const QString start = absolute(m_file.isEmpty() ? QString("assets/tilesets/%1.tileset").arg(m_name->text()) : m_file);
            const QString f = QFileDialog::getSaveFileName(this, tr("Enregistrer le tileset"), start, tr("Tileset (*.tileset)"));
            if (f.isEmpty()) return;
            m_file = relative(f.endsWith(".tileset") ? f : f + ".tileset");
            m_fileChosen = true;
            updateFileLabel();
        });
    }
    left->addLayout(fileRow);
    left->addStretch(1);

    // ---------------- Colonne de droite : la feuille ----------------
    auto* right = new QVBoxLayout;
    columns->addLayout(right, 1);
    m_viewHint = new QLabel;
    m_viewHint->setObjectName("hintLabel");
    m_viewHint->setWordWrap(true);
    right->addWidget(m_viewHint);
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(false);
    scroll->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_view = new TilesetView;
    m_view->setMode(TilesetView::Mode::Click);
    scroll->setWidget(m_view);
    right->addWidget(scroll, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &TilesetDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    // ---------------- Liens ----------------
    connect(browse, &QToolButton::clicked, this, [this] {
        const QString p = pickImage();
        if (p.isEmpty()) return;
        m_image->setText(p);
        if (m_name->text().trimmed().isEmpty() || m_name->text() == tr("Tileset"))
            m_name->setText(QFileInfo(p).completeBaseName());
    });
    connect(m_image, &QLineEdit::editingFinished, this, &TilesetDialog::updateGrid);
    connect(m_image, &QLineEdit::textChanged, this, [this] { updateGrid(); updateFileLabel(); });
    for (QSpinBox* s : { m_tileW, m_tileH, m_margin, m_spacing })
        connect(s, &QSpinBox::valueChanged, this, &TilesetDialog::updateGrid);
    connect(m_name, &QLineEdit::textChanged, this, &TilesetDialog::updateFileLabel);
    connect(m_mode, &QComboBox::currentIndexChanged, this, &TilesetDialog::updateMode);
    connect(m_view, &TilesetView::tileClicked, this, &TilesetDialog::onTileClicked);
    connect(standard, &QPushButton::clicked, this, [this] { m_ts.applyStandardLayout(); updateLabels(); });
    connect(clearRoles, &QPushButton::clicked, this, [this] {
        m_ts.full.clear(); m_ts.ring.clear(); m_ts.under.clear();
        m_ts.edge = { -1, -1, -1, -1 };
        m_ts.corner = { -1, -1, -1, -1 };
        updateLabels();
    });

    updateGrid();
    updateMode();
    updateFileLabel();
    // la feuille tient en largeur au départ
    m_view->fitWidth(std::max(300, scroll->viewport()->width()));
}

QString TilesetDialog::absolute(const QString& path) const {
    if (path.isEmpty() || QDir::isAbsolutePath(path)) return path;
    return QDir(m_projectDir).absoluteFilePath(path);
}

QString TilesetDialog::relative(const QString& abs) const {
    if (m_projectDir.isEmpty()) return abs;
    const QString rel = QDir(m_projectDir).relativeFilePath(abs);
    return rel.startsWith("..") ? abs : rel;   // hors du projet : chemin complet
}

QString TilesetDialog::pickImage() {
    static QString lastDir;
    if (lastDir.isEmpty()) lastDir = m_projectDir + "/assets";
    const QString file = QFileDialog::getOpenFileName(this, tr("Feuille du tileset"), lastDir, tr("Images (*.png *.jpg *.jpeg *.bmp *.tga)"));
    if (file.isEmpty()) return {};
    lastDir = QFileInfo(file).absolutePath();
    const QString rel = relative(file);
    if (QDir::isAbsolutePath(rel))
        QMessageBox::information(this, tr("Image hors du projet"),
            tr("Cette image n'est pas dans le dossier du projet : copiez-la dans assets/ pour que le jeu la retrouve partout."));
    return rel;
}

void TilesetDialog::updateGrid() {
    m_ts.image = utf8(m_image->text().trimmed());
    m_ts.tileSize = { m_tileW->value(), m_tileH->value() };
    m_ts.margin = m_margin->value();
    m_ts.spacing = m_spacing->value();
    const QString path = absolute(m_image->text().trimmed());
    m_imageSize = QImageReader(path).size();
    if (m_imageSize.isValid()) {
        m_ts.computeGrid(sf::Vector2u(unsigned(m_imageSize.width()), unsigned(m_imageSize.height())));
        m_gridInfo->setText(tr("Image %1 x %2 px  →  %3 colonnes x %4 lignes = %5 tiles")
                                .arg(m_imageSize.width()).arg(m_imageSize.height())
                                .arg(m_ts.columns).arg(m_ts.rows()).arg(m_ts.count));
    } else {
        m_ts.columns = 1;
        m_ts.count = 0;
        m_gridInfo->setText(m_image->text().isEmpty() ? tr("Choisissez une image.") : tr("Image introuvable."));
    }
    // les rôles qui sortent de la feuille sont oubliés
    auto keep = [this](std::vector<int>& v) { v.erase(std::remove_if(v.begin(), v.end(), [this](int i) { return !m_ts.validTile(i); }), v.end()); };
    keep(m_ts.full); keep(m_ts.ring); keep(m_ts.under);
    for (int& i : m_ts.edge) if (!m_ts.validTile(i)) i = -1;
    for (int& i : m_ts.corner) if (!m_ts.validTile(i)) i = -1;

    m_view->setTileset(&m_ts, path);
    updateMode();
}

void TilesetDialog::updateMode() {
    const auto mode = AutotileMode(m_mode->currentIndex());
    m_edgesBox->setVisible(mode == AutotileMode::Edges16);
    m_cornersBox->setVisible(mode == AutotileMode::Corners16);
    if (mode == AutotileMode::Edges16 && m_ts.full.empty() && m_ts.count > 0 && m_isNew) m_ts.applyStandardLayout();
    if (mode == AutotileMode::Corners16)
        m_cornersInfo->setText(m_ts.columns == 16
            ? tr("16 colonnes, une par combinaison des 4 coins (colonne 15 = case pleine). %1 ligne(s) : variantes ou animation.").arg(m_ts.rows())
            : tr("⚠ La feuille a %1 colonnes : il en faut 16 (une par combinaison des 4 coins). Vérifiez la taille d'une tile.").arg(m_ts.columns));
    m_viewHint->setText(mode == AutotileMode::Edges16
        ? tr("Choisissez un rôle à gauche puis cliquez sur les tiles. P = pleine, 1 / 2 = bords, 4 = anneau, D = dessous.")
        : mode == AutotileMode::Corners16 ? tr("Aperçu de la feuille dual-grid.")
                                          : tr("Aperçu : dans la palette, on choisit une tile (ou un bloc) dans cette grille."));
    updateLabels();
}

void TilesetDialog::updateLabels() {
    QHash<int, QString> labels;
    if (AutotileMode(m_mode->currentIndex()) == AutotileMode::Edges16) {
        const QStringList dirs = m_iso ? QStringList{ "NE", "SE", "SO", "NO" } : QStringList{ "H", "D", "B", "G" };
        auto add = [&](int index, const QString& text) {
            if (index < 0) return;
            labels[index] = labels.contains(index) ? labels[index] + " " + text : text;
        };
        for (int i : m_ts.full) add(i, "P");
        for (int i = 0; i < 4; ++i) add(m_ts.edge[i], "1·" + dirs[i]);
        for (int i = 0; i < 4; ++i) add(m_ts.corner[i], "2·" + dirs[i] + "+" + dirs[(i + 1) % 4]);
        for (int i : m_ts.ring) add(i, "4");
        for (int i : m_ts.under) add(i, "D");
    } else if (AutotileMode(m_mode->currentIndex()) == AutotileMode::Corners16 && m_ts.columns == 16) {
        labels[15] = tr("pleine");
    }
    m_view->setLabels(labels);
}

void TilesetDialog::onTileClicked(int index) {
    if (AutotileMode(m_mode->currentIndex()) != AutotileMode::Edges16 || index < 0) return;
    const int role = m_role->currentIndex();
    auto setSingle = [&](int& slot) { slot = (slot == index) ? -1 : index; };
    switch (role) {
    case Full:  toggle(m_ts.full, index); break;
    case Ring:  toggle(m_ts.ring, index); break;
    case Under: toggle(m_ts.under, index); break;
    default:
        if (role >= Edge0 && role <= Edge3) setSingle(m_ts.edge[std::size_t(role - Edge0)]);
        else if (role >= Corner0 && role <= Corner3) setSingle(m_ts.corner[std::size_t(role - Corner0)]);
        // rôle à une seule tile : on passe au suivant (on enchaîne les clics)
        if (role < Ring) m_role->setCurrentIndex(role + 1);
        break;
    }
    updateLabels();
}

void TilesetDialog::updateFileLabel() {
    if (m_isNew && !m_fileChosen) {
        // à côté de l'image, avec le nom du tileset
        const QString image = m_image->text().trimmed();
        const QString dir = image.isEmpty() ? QString("assets/tilesets") : QFileInfo(image).path();
        QString base = m_name->text().trimmed();
        base.replace(QRegularExpression(R"([\\/:*?"<>|])"), "_");
        if (base.isEmpty()) base = "tileset";
        m_file = dir + "/" + base + ".tileset";
    }
    m_fileLabel->setText(tr("Fichier : %1").arg(m_file));
}

Tileset TilesetDialog::result() const {
    Tileset t = m_ts;
    t.name = utf8(m_name->text().trimmed());
    t.image = utf8(m_image->text().trimmed());
    t.autotile = AutotileMode(m_mode->currentIndex());
    t.frameTime = float(m_frameTime->value());
    t.anchor = m_autoAnchor->isChecked() ? sf::Vector2f{ -1.f, -1.f }
                                         : sf::Vector2f{ float(m_anchorX->value()), float(m_anchorY->value()) };
    t.scale = float(m_scale->value());
    t.solid = m_solid->isChecked();
    t.file = utf8(m_file);
    t.missing = false;
    if (t.autotile != AutotileMode::Edges16) {   // rôles inutiles : pas enregistrés
        t.full.clear(); t.ring.clear(); t.under.clear();
        t.edge = { -1, -1, -1, -1 };
        t.corner = { -1, -1, -1, -1 };
    }
    return t;
}

QString TilesetDialog::absoluteFile() const { return absolute(m_file); }

void TilesetDialog::accept() {
    if (m_name->text().trimmed().isEmpty()) { QMessageBox::warning(this, windowTitle(), tr("Donnez un nom au tileset.")); return; }
    if (!m_imageSize.isValid() || m_ts.count <= 0) {
        QMessageBox::warning(this, windowTitle(), tr("Choisissez une image et une taille de tile qui y tient au moins une fois."));
        return;
    }
    const auto mode = AutotileMode(m_mode->currentIndex());
    if (mode == AutotileMode::Edges16 && m_ts.full.empty()) {
        QMessageBox::warning(this, windowTitle(), tr("Autotile 4 bords : choisissez au moins une case pleine (rôle P)."));
        return;
    }
    if (mode == AutotileMode::Corners16 && m_ts.columns != 16) {
        QMessageBox::warning(this, windowTitle(), tr("Autotile 16 coins : la feuille doit avoir 16 colonnes."));
        return;
    }
    if (m_isNew && QFileInfo::exists(absoluteFile()) &&
        QMessageBox::question(this, windowTitle(), tr("%1 existe déjà. Le remplacer ?").arg(m_file)) != QMessageBox::Yes)
        return;
    QDialog::accept();
}
