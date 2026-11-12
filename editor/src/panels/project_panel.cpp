#include "project_panel.h"
#include "external_tools.h"

#include <QAction>
#include <QActionGroup>
#include <QClipboard>
#include <QGuiApplication>
#include <QMenu>

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFileSystemWatcher>
#include <QInputDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QRegularExpression>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QSplitter>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>

namespace {
constexpr int kPathRole = Qt::UserRole + 1;
constexpr const char* kEntityMime = "application/x-angevault-entities";   // = EditorContext::kEntityMimeType

// Liste des fichiers : on peut les glisser (vers la Hierarchy, l'Inspector, la vue scène)
// et y déposer des entités venant de la Hierarchy (= créer des prefabs dans ce dossier).
class ProjectFileList : public QListWidget {
public:
    std::function<void(const QByteArray& entities)> onEntitiesDropped;

protected:
    QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override {
        auto* mime = new QMimeData;
        QList<QUrl> urls;
        for (QListWidgetItem* item : items) urls << QUrl::fromLocalFile(item->data(kPathRole).toString());
        mime->setUrls(urls);
        return mime;
    }
    void dragEnterEvent(QDragEnterEvent* e) override {
        if (e->mimeData()->hasFormat(kEntityMime)) e->acceptProposedAction();
        else if (e->source() == this) e->ignore();   // pas de déplacement d'icônes dans la liste
        else e->ignore();
    }
    void dragMoveEvent(QDragMoveEvent* e) override {
        if (e->mimeData()->hasFormat(kEntityMime)) e->acceptProposedAction();
        else e->ignore();
    }
    void dropEvent(QDropEvent* e) override {
        if (!e->mimeData()->hasFormat(kEntityMime) || !onEntitiesDropped) { e->ignore(); return; }
        e->acceptProposedAction();
        onEntitiesDropped(e->mimeData()->data(kEntityMime));
    }
};

// "EnemyAI" -> "enemy_ai" (nom de fichier d'un nouveau behavior)
QString snakeCase(const QString& name) {
    QString out;
    for (int i = 0; i < name.size(); ++i) {
        const QChar ch = name[i];
        if (ch.isUpper() && i > 0 && (name[i - 1].isLower() || (i + 1 < name.size() && name[i + 1].isLower()))) out += '_';
        out += ch.toLower();
    }
    return out;
}

// Modèle d'un nouveau comportement
// Modèle d'un nouveau composant de jeu
QString componentTemplate(const QString& className) {
    return QString(R"(#pragma once
#include "ecs/game_components.h"
#include <SFML/System/Vector2.hpp>
#include <string>

// Composant de jeu « %1 » (données seulement : la logique va dans un système ou un behavior).
// Après compilation : Inspector > Ajouter un composant > %1, ou glissez ce fichier sur une entité.
struct %1 {
    float value = 1.f;
    // int count = 0;   bool active = true;   sf::Vector2f direction{ 1.f, 0.f };   std::string tag;
};

// Champs visibles dans l'Inspector et enregistrés dans la scène : FIELD(nom) pour chacun
REGISTER_COMPONENT(%1, FIELD(value))
)").arg(className);
}

// Modèle d'un nouveau système de jeu
QString systemTemplate(const QString& className) {
    return QString(R"(#pragma once
#include "game_systems.h"
#include "components/transform2d.h"
// #include "../components/mon_composant.h"

// Système de jeu « %1 » : ajouté automatiquement au jeu après compilation.
// Activer / désactiver : éditeur > Exécuter > Systèmes...
class %1 : public ISystem {
public:
    %1() { priority = 3; }   // ordre : physique 0, collisions 1, scripts 2, ... rendu 10
    // Pour dessiner : un constructeur %1(sf::RenderWindow& window) est appelé à la place.

    void onStart(Registry& registry) override {
    }

    void onUpdate(Registry& registry, float dt) override {
        // registry.view<MonComposant, Transform2D>([&](Entity e, MonComposant& c, Transform2D& t) {
        // });
    }
};
REGISTER_SYSTEM(%1)
)").arg(className);
}

QString behaviorTemplate(const QString& className) {
    return QString(R"(#pragma once
#include "ecs/behavior.h"
#include "ecs/behavior_factory.h"
#include "ecs/registry.h"
#include "components/transform2d.h"
#include "input/input.h"

// Comportement « %1 ».
// Après compilation, glissez ce fichier sur une entité (Hierarchy, Inspector ou vue scène),
// ou Inspector > Ajouter un composant > Comportement > %1.
class %1 : public IBehavior {
public:
    // Réglages visibles dans l'Inspector et enregistrés dans la scène (float, int, bool, sf::Vector2f, std::string)
    PROPERTY(float, speed, 100.f)

    // Appelé une fois, avant le premier onUpdate
    void onStart(Entity self, Registry& registry) override {
    }

    // Appelé à chaque frame (dt en secondes)
    void onUpdate(Entity self, Registry& registry, float dt) override {
        // auto& transform = registry.getComponent<Transform2D>(self);
        // transform.move({ Input::getAxis("Horizontal") * speed * dt, 0.f });
    }
};

// Nom utilisé dans les fichiers .scene / .prefab
REGISTER_BEHAVIOR(%1)
)").arg(className);
}
constexpr int kThumb = 56;

bool isImage(const QFileInfo& fi) {
    static const QStringList ext = { "png", "jpg", "jpeg", "bmp", "gif", "tga" };
    return ext.contains(fi.suffix().toLower());
}

// Aperçu centré sur fond transparent, sans flou (pixel art)
QIcon thumbnail(const QString& path) {
    QImageReader reader(path);
    QImage img = reader.read();
    if (img.isNull()) return {};
    img = img.scaled(kThumb, kThumb, Qt::KeepAspectRatio, Qt::FastTransformation);
    QPixmap canvas(kThumb, kThumb);
    canvas.fill(Qt::transparent);
    QPainter p(&canvas);
    p.drawImage((kThumb - img.width()) / 2, (kThumb - img.height()) / 2, img);
    return QIcon(canvas);
}
}

ProjectPanel::ProjectPanel(const QString& rootDir, QWidget* parent)
    : QWidget(parent), m_root(QDir::cleanPath(rootDir)) {
    setObjectName("projectPanel");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // Barre : dossier parent, chemin, recherche, ouvrir dans l'explorateur
    auto* bar = new QHBoxLayout;
    bar->setSpacing(4);
    auto* up = new QToolButton;
    up->setObjectName("panelToolButton");
    up->setText(QString::fromUtf8("↑"));
    up->setToolTip(tr("Dossier parent"));
    m_path = new QLabel;
    m_path->setObjectName("projectPath");
    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Rechercher..."));
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(200);
    auto* reveal = new QToolButton;
    reveal->setObjectName("panelToolButton");
    reveal->setText(tr("Ouvrir le dossier"));
    reveal->setToolTip(tr("Afficher dans l'explorateur de fichiers"));
    bar->addWidget(up);
    bar->addWidget(m_path, 1);
    bar->addWidget(m_search);
    bar->addWidget(reveal);
    layout->addLayout(bar);

    auto* split = new QSplitter(Qt::Horizontal);
    split->setChildrenCollapsible(false);

    m_dirs = new QFileSystemModel(this);
    m_dirs->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
    m_dirs->setRootPath(m_root);
    m_tree = new QTreeView;
    m_tree->setObjectName("projectTree");
    m_tree->setModel(m_dirs);
    m_tree->setRootIndex(m_dirs->index(QFileInfo(m_root).absolutePath()));
    for (int c = 1; c < m_dirs->columnCount(); ++c) m_tree->hideColumn(c);
    m_tree->setHeaderHidden(true);
    // On n'affiche que le dossier racine du projet (et ses sous-dossiers)
    const QModelIndex rootIdx = m_dirs->index(m_root);
    for (int r = 0; r < m_dirs->rowCount(rootIdx.parent()); ++r) {
        const QModelIndex sib = m_dirs->index(r, 0, rootIdx.parent());
        if (sib != rootIdx) m_tree->setRowHidden(r, rootIdx.parent(), true);
    }
    connect(m_dirs, &QFileSystemModel::directoryLoaded, this, [this] {
        const QModelIndex rootIdx = m_dirs->index(m_root);
        for (int r = 0; r < m_dirs->rowCount(rootIdx.parent()); ++r)
            m_tree->setRowHidden(r, rootIdx.parent(), m_dirs->index(r, 0, rootIdx.parent()) != rootIdx);
        m_tree->expand(rootIdx);
    });

    auto* fileList = new ProjectFileList;
    fileList->onEntitiesDropped = [this](const QByteArray& data) { emit entitiesDropped(data, m_current); };
    m_list = fileList;
    m_list->setObjectName("projectFiles");
    m_list->setViewMode(QListView::IconMode);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setIconSize(QSize(kThumb, kThumb));
    m_list->setGridSize(QSize(kThumb + 36, kThumb + 36));
    m_list->setWordWrap(true);
    m_list->setUniformItemSizes(true);
    m_list->setSpacing(4);
    m_list->setDragEnabled(true);                          // glisser un fichier vers la scène / une entité
    m_list->setAcceptDrops(true);                          // déposer une entité = créer un prefab
    m_list->viewport()->setAcceptDrops(true);              // (setMovement(Static) l'avait désactivé)
    m_list->setDragDropMode(QAbstractItemView::DragDrop);
    m_list->setDefaultDropAction(Qt::CopyAction);

    split->addWidget(m_tree);
    split->addWidget(m_list);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 4);
    split->setSizes({ 180, 600 });
    layout->addWidget(split, 1);

    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &ProjectPanel::refresh);

    connect(m_tree->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& idx) {
        if (idx.isValid()) setCurrentDir(m_dirs->filePath(idx));
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        openPath(item->data(kPathRole).toString());
    });

    // Clic droit : sur un fichier / dossier, ou dans le vide (= dossier courant)
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QListWidgetItem* item = m_list->itemAt(pos);
        if (item && !(item->flags() & Qt::ItemIsEnabled)) item = nullptr;
        showContextMenu(item ? item->data(kPathRole).toString() : m_current, m_list->viewport()->mapToGlobal(pos));
    });
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeView::customContextMenuRequested, this, [this](const QPoint& pos) {
        const QModelIndex idx = m_tree->indexAt(pos);
        if (idx.isValid()) showContextMenu(m_dirs->filePath(idx), m_tree->viewport()->mapToGlobal(pos));
    });
    connect(up, &QToolButton::clicked, this, [this] {
        if (m_current != m_root) setCurrentDir(QFileInfo(m_current).absolutePath());
    });
    connect(reveal, &QToolButton::clicked, this, [this] { ExternalTools::revealInFileManager(m_current); });
    connect(m_search, &QLineEdit::textChanged, this, &ProjectPanel::refresh);

    setCurrentDir(m_root);
}

void ProjectPanel::setCurrentDir(const QString& dir) {
    QString clean = QDir::cleanPath(dir);
    if (!clean.startsWith(m_root)) clean = m_root;   // on ne sort pas du projet
    if (!m_watcher->directories().isEmpty()) m_watcher->removePaths(m_watcher->directories());
    m_current = clean;
    if (QFileInfo::exists(m_current)) m_watcher->addPath(m_current);

    const QModelIndex idx = m_dirs->index(m_current);
    if (idx.isValid() && m_tree->currentIndex() != idx) {
        m_tree->setCurrentIndex(idx);
        m_tree->scrollTo(idx);
    }
    refresh();
}

void ProjectPanel::refresh() {
    const QString rel = QDir(QFileInfo(m_root).absolutePath()).relativeFilePath(m_current);
    m_path->setText(QDir::toNativeSeparators(rel));
    m_path->setToolTip(QDir::toNativeSeparators(m_current));
    m_list->clear();

    QDir dir(m_current);
    if (!dir.exists()) {
        auto* item = new QListWidgetItem(tr("Dossier introuvable :\n%1").arg(QDir::toNativeSeparators(m_current)));
        item->setFlags(Qt::NoItemFlags);
        m_list->addItem(item);
        return;
    }
    const QString filter = m_search->text().trimmed();
    const auto entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot,
                                           QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& fi : entries) {
        if (!filter.isEmpty() && !fi.fileName().contains(filter, Qt::CaseInsensitive)) continue;
        auto* item = new QListWidgetItem(iconFor(fi.absoluteFilePath(), fi.isDir()), fi.fileName());
        item->setData(kPathRole, fi.absoluteFilePath());
        item->setToolTip(fi.isDir() ? fi.fileName()
                                    : QString("%1\n%2 Ko").arg(fi.fileName()).arg((fi.size() + 1023) / 1024));
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        m_list->addItem(item);
    }
}

void ProjectPanel::setRootDir(const QString& dir) {
    const QString clean = QDir::cleanPath(dir);
    if (clean.isEmpty() || clean == m_root) return;
    m_root = clean;
    m_dirs->setRootPath(m_root);
    m_tree->setRootIndex(m_dirs->index(QFileInfo(m_root).absolutePath()));
    const QModelIndex rootIdx = m_dirs->index(m_root);   // on ne montre que le dossier du projet
    for (int r = 0; r < m_dirs->rowCount(rootIdx.parent()); ++r)
        m_tree->setRowHidden(r, rootIdx.parent(), m_dirs->index(r, 0, rootIdx.parent()) != rootIdx);
    m_tree->expand(rootIdx);
    setCurrentDir(m_root);
}

void ProjectPanel::openPath(const QString& path) {
    const QFileInfo fi(path);
    if (fi.isDir()) setCurrentDir(path);
    else if (fi.suffix().compare("scene", Qt::CaseInsensitive) == 0) emit sceneOpenRequested(path);   // ouvrir dans l'éditeur
    else if (ExternalTools::isScript(fi)) ExternalTools::openScript(path, this);   // CLion / VS Code
    else QDesktopServices::openUrl(QUrl::fromLocalFile(path));                     // application par défaut
}

void ProjectPanel::showContextMenu(const QString& path, const QPoint& globalPos) {
    using namespace ExternalTools;
    const QFileInfo fi(path);
    QMenu menu(this);
    menu.setToolTipsVisible(true);

    QAction* open = menu.addAction(fi.isDir() ? tr("Ouvrir") : isScript(fi)
                                       ? tr("Ouvrir dans %1").arg(displayName(preferredEditor()))
                                       : tr("Ouvrir"));
    QFont bold = open->font();
    bold.setBold(true);
    open->setFont(bold);
    connect(open, &QAction::triggered, this, [this, path] { openPath(path); });
    connect(menu.addAction(tr("Afficher dans l'explorateur")), &QAction::triggered, this,
            [path] { revealInFileManager(path); });
    menu.addSeparator();

    for (CodeEditor e : { CodeEditor::VSCode, CodeEditor::CLion }) {
        QAction* a = menu.addAction(tr("Ouvrir avec %1").arg(displayName(e)));
        if (executable(e).isEmpty()) a->setToolTip(tr("Introuvable : vous pourrez indiquer l'exécutable"));
        connect(a, &QAction::triggered, this, [this, e, path] { openWith(e, { path }, this); });
    }
    QMenu* preferred = menu.addMenu(tr("Éditeur des scripts (double-clic)"));
    auto* group = new QActionGroup(preferred);
    for (CodeEditor e : { CodeEditor::VSCode, CodeEditor::CLion }) {
        QAction* a = preferred->addAction(displayName(e));
        a->setCheckable(true);
        a->setChecked(preferredEditor() == e);
        group->addAction(a);
        connect(a, &QAction::triggered, this, [e] { setPreferredEditor(e); });
    }
    menu.addSeparator();
    QMenu* create = menu.addMenu(tr("Créer (C++)"));
    connect(create->addAction(tr("Behavior...")), &QAction::triggered, this, [this] { createCode(0); });
    connect(create->addAction(tr("Composant de jeu...")), &QAction::triggered, this, [this] { createCode(1); });
    connect(create->addAction(tr("Système de jeu...")), &QAction::triggered, this, [this] { createCode(2); });
    menu.addSeparator();
    connect(menu.addAction(tr("Copier le chemin")), &QAction::triggered, this,
            [path] { QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(path)); });

    menu.exec(globalPos);
}

// Icônes personnalisées : editor/assets/icons/files/<nom>.png (incluses automatiquement par CMake).
// On cherche d'abord <extension>.png (ex. "cpp.png"), puis le nom du groupe (ex. "h.png" pour .hpp),
// puis "folder.png" / "file.png". Sinon : icône du système.
QIcon customIcon(const QFileInfo& fi, bool isDir) {
    static QHash<QString, QIcon> cache;
    QStringList names;
    if (isDir) {
        names << "folder";
    } else {
        const QString ext = fi.suffix().toLower();
        static const QHash<QString, QString> groups = {
            { "hpp", "h" }, { "hh", "h" }, { "hxx", "h" }, { "inl", "h" },
            { "cc", "cpp" }, { "cxx", "cpp" }, { "c", "cpp" },
            { "jpeg", "jpg" }, { "qss", "css" },
        };
        if (fi.fileName().compare("CMakeLists.txt", Qt::CaseInsensitive) == 0) names << "cmake";
        if (!ext.isEmpty()) names << ext;
        if (groups.contains(ext)) names << groups.value(ext);
        names << "file";
    }
    for (const QString& n : names) {
        auto it = cache.find(n);
        if (it == cache.end()) {
            const QString res = ":/resources/icons/files/" + n + ".png";
            it = cache.insert(n, QFile::exists(res) ? QIcon(res) : QIcon());
        }
        if (!it->isNull()) return *it;
    }
    return {};
}

QIcon ProjectPanel::iconFor(const QString& path, bool isDir) {
    static QFileIconProvider provider;
    const QFileInfo fi(path);
    if (isDir || !isImage(fi)) {          // les images gardent leur aperçu
        const QIcon custom = customIcon(fi, isDir);
        return custom.isNull() ? provider.icon(fi) : custom;
    }
    const QString key = path + QString::number(fi.lastModified().toMSecsSinceEpoch());
    auto it = m_thumbnails.find(key);
    if (it == m_thumbnails.end()) {
        QIcon icon = thumbnail(path);
        it = m_thumbnails.insert(key, icon.isNull() ? provider.icon(fi) : icon);
    }
    return *it;
}

void ProjectPanel::createCode(int kind) {
    // 0 = behavior, 1 = composant, 2 = système : chacun dans le dossier que CMake inclut automatiquement
    const QString titles[] = { tr("Nouveau behavior"), tr("Nouveau composant"), tr("Nouveau système") };
    const QString examples[] = { "EnemyAI", "Health", "DamageSystem" };
    const QString folders[] = { "behaviors", "components", "systems" };
    const QString title = titles[kind];
    bool ok = false;
    const QString name = QInputDialog::getText(this, title,
        tr("Nom de la classe (ex. %1) :").arg(examples[kind]), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    if (!QRegularExpression("^[A-Za-z_][A-Za-z0-9_]*$").match(name).hasMatch()) {
        QMessageBox::warning(this, title, tr("« %1 » n'est pas un nom de classe C++ valide.").arg(name));
        return;
    }
    const QString dir = m_root + "/" + folders[kind];
    QDir().mkpath(dir);
    const QString path = dir + "/" + snakeCase(name) + ".h";
    if (QFileInfo::exists(path)) {
        QMessageBox::warning(this, title, tr("%1 existe déjà.").arg(QDir::toNativeSeparators(path)));
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, title, tr("Impossible d'écrire %1").arg(QDir::toNativeSeparators(path)));
        return;
    }
    const QString code = kind == 0 ? behaviorTemplate(name) : kind == 1 ? componentTemplate(name) : systemTemplate(name);
    file.write(code.toUtf8());
    file.close();
    setCurrentDir(dir);
    ExternalTools::openScript(path, this);   // ouvert dans CLion / VS Code
    QMessageBox::information(this, title,
        tr("%1 a été créé.\n\nC'est du C++ : recompilez l'éditeur (et rpg) pour pouvoir l'utiliser.")
            .arg(QDir::toNativeSeparators(path)));
}
