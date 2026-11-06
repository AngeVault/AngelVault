#include "customWindow.h"
#include "DockAreaWidget.h"
#include "DockManager.h"
#include "DockWidget.h"
#include "IconProvider.h"
#include "editor_context.h"
#include "game_view.h"
#include "game_loop.h"
#include "game_systems.h"
#include "systems_dialog.h"
#include "input_settings_dialog.h"
#include "scene_toolbar.h"
#include "panels/console_panel.h"
#include "panels/hierarchy_panel.h"
#include "panels/inspector_panel.h"
#include "panels/project_panel.h"
#include "panels/tile_palette_panel.h"
#include "rpg_scene.h"
#include "theme_manager.h"
#include "tilemap_view.h"
#include "titleBar.h"

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QStyleFactory>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>
#include <iostream>

namespace {
QAction* addAction(QMenu* menu, const QString& text, const QKeySequence& shortcut = {}) {
    QAction* a = menu->addAction(text);
    a->setShortcut(shortcut);
    return a;
    }
}

// Chemin de rpg.exe : il est construit dans le même dossier bin/ que l'éditeur
QString gameExecutablePath() {
#ifdef Q_OS_WIN
    const QString name = "rpg.exe";
#else
    const QString name = "rpg";
#endif
    return QDir(QCoreApplication::applicationDirPath()).filePath(name);
}

// Dossier affiché par le panneau Project (défini par CMake : le projet RPG, assets + scripts)
QString projectDir() {
#ifdef AV_PROJECT_DIR
    return QStringLiteral(AV_PROJECT_DIR);
#else
    return QCoreApplication::applicationDirPath() + "/assets";
#endif
}

// Panneau détachable (Qt Advanced Docking System) : on le glisse par son onglet, on le dépose sur un
// repère (bord, centre = en onglet), on le fait flotter, on l'épingle sur le côté ou on le ferme.
ads::CDockWidget* makeDock(ads::CDockManager* manager, const QString& title, const QString& id, QWidget* content) {
    auto* dock = new ads::CDockWidget(manager, title);
    dock->setObjectName(id);   // indispensable pour mémoriser la disposition
    content->setAttribute(Qt::WA_StyledBackground);   // sinon le QSS ne dessine pas le fond
    dock->setWidget(content);
    dock->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromDockWidget);   // se réduit sans bloquer
    dock->setMinimumSize(120, 80);
    return dock;
}

// Icônes des petits boutons des panneaux (fermer, punaise, menu des onglets, réduire, détacher) :
// PNG de assets/icons/dock/ (gris moyen, lisible sur les thèmes clairs comme sombres).
// La version @2x de chaque fichier est chargée toute seule pour les écrans haute densité.
QIcon dockIcon(ads::eIcon id) {
    const char* name = nullptr;
    switch (id) {
    case ads::TabCloseIcon:
    case ads::DockAreaCloseIcon:   name = "close"; break;
    case ads::AutoHideIcon:        name = "pin"; break;
    case ads::DockAreaMenuIcon:    name = "menu"; break;
    case ads::DockAreaMinimizeIcon: name = "minimize"; break;
    case ads::DockAreaUndockIcon:  name = "undock"; break;
    default: return {};
    }
    return QIcon(QStringLiteral(":/resources/icons/dock/%1.png").arg(QLatin1String(name)));
}

// Projets récents (Fichier > Récents), du plus récent au plus ancien
QStringList recentProjects() {
    QStringList list = QSettings().value("recentProjects").toStringList();
    list.erase(std::remove_if(list.begin(), list.end(), [](const QString& p) { return !QFileInfo::exists(p); }), list.end());
    return list;
}
void addRecentProject(const QString& path) {
    QStringList list = recentProjects();
    const QString clean = QFileInfo(path).absoluteFilePath();
    list.removeAll(clean);
    list.prepend(clean);
    while (list.size() > 8) list.removeLast();
    QSettings().setValue("recentProjects", list);
}

// Laisse fermer la fenêtre seulement si canClose() le permet (modifications non enregistrées)
class CloseGuard : public QObject {
public:
    CloseGuard(QObject* parent, std::function<bool()> canClose) : QObject(parent), m_canClose(std::move(canClose)) {}
protected:
    bool eventFilter(QObject* watched, QEvent* e) override {
        if (e->type() == QEvent::Close && !m_canClose()) { e->ignore(); return true; }
        return QObject::eventFilter(watched, e);
    }
private:
    std::function<bool()> m_canClose;
};

auto addLink = [](QMenu* menu, const QString& text, const QString& url) {
    QObject::connect(menu->addAction(text), &QAction::triggered, [url] { QDesktopServices::openUrl(QUrl(url)); });
};

int main(int argc, char** argv) {
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    LogCapture::instance().install();                   // std::cout / std::cerr / sf::err() -> panneau Console
    QApplication::setOrganizationName("AngeVault");   // pour QSettings (thème, etc.)
    QApplication::setApplicationName("AngeVault Editor");
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setFont(QFont("Segoe UI", 9));

    // Scène éditée : état partagé par tous les panneaux (déclaré avant la fenêtre : il lui survit)
    EditorContext ctx;

    // Rangement de départ dans la Hierarchy (catégories : dossiers de l'éditeur) pour des entités importées.
    // Une caméra qui suit une entité s'affiche déjà sous elle : seules les caméras libres vont dans « Caméras ».
    auto defaultCategories = [&ctx] {
        QString cameras, characters;
        for (Entity e : ctx.entities()) {
            Registry& reg = ctx.scene();
            if (reg.hasComponent<Camera2d>(e)) {
                if (reg.isValid(reg.getComponent<Camera2d>(e).targetLock)) continue;
                if (cameras.isEmpty()) cameras = ctx.addCategory("Caméras");
                ctx.setCategory(e, cameras);
            } else if (reg.hasComponent<Script>(e)) {
                if (characters.isEmpty()) characters = ctx.addCategory("Personnages");
                ctx.setCategory(e, characters);
            }
        }
    };

    // Ouvre un .angproj et sa scène de départ.
    // Projet RPG sans fichier de scène : on la crée avec les entités de rpg_scene.h (une seule fois).
    auto openProjectFile = [&](const QString& path, QString* error) -> bool {
        if (!ctx.openProject(path, error)) return false;
        if (!QFileInfo::exists(ctx.scenePath()) && QFileInfo(path).fileName() == "rpg.angproj") {
            ctx.importEntities([](Registry& registry) { loadRpgEntities(registry); });
            defaultCategories();
            ctx.saveScene();
        }
        addRecentProject(path);
        return true;
    };

    {   // Projet ouvert au démarrage : le dernier ouvert (Fichier > Récents), sinon le RPG
        const QString rpgProject = projectDir() + "/rpg.angproj";
        if (!QFileInfo::exists(rpgProject)) {   // premier lancement : on crée le fichier projet du RPG
            Project rpg;
            rpg.name = "RPG";
            rpg.window.title = "RPG";
            rpg.scenes = { rpg.startScene };
            std::string err;
            if (!rpg.save(rpgProject.toUtf8().toStdString(), &err)) std::cerr << "[Projet] " << err << std::endl;
        }
        QStringList candidates = recentProjects();
        candidates.append(rpgProject);
        for (const QString& candidate : candidates) {
            QString err;
            if (openProjectFile(candidate, &err)) break;
            std::cerr << "[Projet] " << err.toStdString() << std::endl;
        }
    }

    CustomWindow window;
    ThemeManager themes(&window);
    themes.restoreSaved();                              // dernier thème choisi
    // Titre : projet — scène (* = modifications non enregistrées)
    auto updateTitle = [&] {
        const QString project = ctx.hasProject() ? QString::fromStdString(ctx.project().name) + "  —  " : QString();
        window.setWindowTitle(QString("%1%2%3  —  AngeVault Editor").arg(project, ctx.sceneName(), ctx.isDirty() ? " *" : ""));
    };
    updateTitle();
    QObject::connect(&ctx, &EditorContext::dirtyChanged, &window, updateTitle);
    QObject::connect(&ctx, &EditorContext::documentChanged, &window, updateTitle);
    window.setWindowIcon(QIcon(":/resources/icons/icon.png"));
    window.setMinimumSize(640, 400);

    // --- Menus de la barre de titre :
    QMenu* file = window.addMenu("&Fichier");
    QAction* newProjectAction = addAction(file, "Nouveau projet...");
    QAction* openAction = addAction(file, "Ouvrir...", QKeySequence::Open);   // .angproj ou .scene
    QMenu* recent = file->addMenu("Récents");
    file->addSeparator();
    QAction* newSceneAction = addAction(file, "Nouvelle scène", QKeySequence::New);
    QAction* saveAction = addAction(file, "Enregistrer", QKeySequence::Save);
    QAction* saveAsAction = addAction(file, "Enregistrer sous...", QKeySequence::SaveAs);
    file->addSeparator();
    QObject::connect(addAction(file, "Quitter", QKeySequence(Qt::ALT | Qt::Key_F4)),
                     &QAction::triggered, &window, &QWidget::close);

    QMenu* edit = window.addMenu("&Édition");
    QAction* undoAction = addAction(edit, "Annuler", QKeySequence::Undo);
    QAction* redoAction = addAction(edit, "Rétablir", QKeySequence::Redo);
    {   // Ctrl+Y et Ctrl+Maj+Z partout (sans doublon : Windows a déjà Ctrl+Y)
        QList<QKeySequence> keys = QKeySequence::keyBindings(QKeySequence::Redo);
        for (const QKeySequence& k : { QKeySequence(Qt::CTRL | Qt::Key_Y), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z) })
            if (!keys.contains(k)) keys << k;
        redoAction->setShortcuts(keys);
    }
    QObject::connect(undoAction, &QAction::triggered, &ctx, &EditorContext::undo);
    QObject::connect(redoAction, &QAction::triggered, &ctx, &EditorContext::redo);
    auto syncUndo = [&ctx, undoAction, redoAction] {
        undoAction->setEnabled(ctx.canUndo());
        redoAction->setEnabled(ctx.canRedo());
    };
    QObject::connect(&ctx, &EditorContext::undoStateChanged, undoAction, syncUndo);
    syncUndo();
    edit->addSeparator();
    addAction(edit, "Préférences...", QKeySequence(Qt::CTRL | Qt::Key_Comma));
    QAction* inputSettings = addAction(edit, "Input Manager...");   // actions d'entrée du projet (assets/input.cfg)
    edit->addMenu(themes.createMenu(edit));             // Sombre / Néon rose / fichier .qss

    QMenu* view = window.addMenu("&Affichage");
    QAction* grid = addAction(view, "Grille", QKeySequence(Qt::CTRL | Qt::Key_G));
    grid->setCheckable(true);
    grid->setChecked(true);
    QAction* colliders = addAction(view, "Collisions", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    colliders->setCheckable(true);
    colliders->setChecked(QSettings().value("view/showColliders", true).toBool());
    QAction* fit = addAction(view, "Recadrer la carte", QKeySequence(Qt::CTRL | Qt::Key_0));
    QAction* fullScreen = addAction(view, "Plein écran", QKeySequence(Qt::Key_F11));
    QObject::connect(fullScreen, &QAction::triggered, &window, [&window] {
        window.isFullScreen() ? window.showNormal() : window.showFullScreen();
    });

    view->addSeparator();
    QMenu* windowsMenu = view->addMenu("Fenêtres");   // rempli plus bas, une fois les panneaux créés

    QMenu* help = window.addMenu("&Aide");
    addLink(help, "À propos d'AngeVault", "https://angevault.netlify.app/accueil");
    addLink(help, "Documentation", "https://angevault.netlify.app/docs");
    addLink(help, "Signaler un bug", "https://github.com/AngeVault/angelVault/issues");

    // --- Vue centrale : tilemap + entités ---
    // La vue de jeu s'empile par-dessus pendant que l'on joue.
    // Au-dessus : la barre des outils d'entités (Sélection, Taille), cachée pendant que l'on joue.
    auto* central = new QWidget;
    central->setObjectName("sceneCentral");
    central->setAttribute(Qt::WA_StyledBackground);
    auto* centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    auto* sceneToolbar = new SceneToolbar(ctx);
    window.addActions(sceneToolbar->toolActions());   // R partout dans la fenêtre
    centralLayout->addWidget(sceneToolbar);
    auto* viewports = new QStackedWidget;
    auto* tilemapView = new TilemapView(ctx);
    viewports->addWidget(tilemapView);
    centralLayout->addWidget(viewports, 1);
    QObject::connect(viewports, &QStackedWidget::currentChanged, sceneToolbar,
                     [viewports, tilemapView, sceneToolbar] { sceneToolbar->setVisible(viewports->currentWidget() == tilemapView); });

    QObject::connect(grid, &QAction::toggled, tilemapView, &TilemapView::setGridVisible);
    tilemapView->setCollidersVisible(colliders->isChecked());
    QObject::connect(colliders, &QAction::toggled, tilemapView, [tilemapView](bool on) {
        tilemapView->setCollidersVisible(on);
        QSettings().setValue("view/showColliders", on);
    });
    QObject::connect(fit, &QAction::triggered, tilemapView, &TilemapView::fitToMap);

    auto* tileInfo = new QLabel;
    window.statusBar()->addPermanentWidget(tileInfo);
    QObject::connect(tilemapView, &TilemapView::hoveredTileChanged, tileInfo,
                     [tileInfo](int x, int y, const QString& type) {
        tileInfo->setText(x < 0 ? QString() : QString("Case %1, %2  —  %3").arg(x).arg(y).arg(type));
    });
    window.statusBar()->showMessage("Molette : zoom  —  clic droit / molette + glisser : déplacer  —  F5 : jouer");

    // --- Panneaux : Hierarchy / Inspector / Project / Console / Tile Palette ---
    // Qt Advanced Docking System : glisser un onglet affiche des repères (comme Visual Studio) ;
    // centre = empiler en onglet, flèches = coller sur ce bord. Double-clic sur l'onglet = flottant.
    // Les options globales se règlent AVANT de créer le gestionnaire.
    using ads::CDockManager;
    CDockManager::setConfigFlags(CDockManager::DefaultNonOpaqueConfig);   // aperçu pendant le glisser (la vue SFML ne bouge pas)
    CDockManager::setConfigFlag(CDockManager::FocusHighlighting, true);              // onglet du panneau actif souligné
    CDockManager::setConfigFlag(CDockManager::EqualSplitOnInsertion, true);
    CDockManager::setConfigFlag(CDockManager::DockAreaHasUndockButton, false);       // double-clic ou glisser suffit
    CDockManager::setConfigFlag(CDockManager::DockAreaDynamicTabsMenuButtonVisibility, true);
    CDockManager::setConfigFlag(CDockManager::MiddleMouseButtonClosesTab, true);
    CDockManager::setAutoHideConfigFlags(CDockManager::DefaultAutoHideConfig);   // épingler sur le côté (comme VS)
    for (ads::eIcon id : { ads::TabCloseIcon, ads::AutoHideIcon, ads::DockAreaMenuIcon, ads::DockAreaUndockIcon,
                           ads::DockAreaCloseIcon, ads::DockAreaMinimizeIcon })
        CDockManager::iconProvider().registerCustomIcon(id, dockIcon(id));
    auto* docks = new CDockManager(&window);   // devient la zone centrale de la fenêtre
    docks->setStyleSheet(QString());           // le style vient des thèmes (QSS de l'application)

    // La scène au centre : fixe, les panneaux se rangent autour
    auto* sceneDock = new ads::CDockWidget(docks, "Scène");
    sceneDock->setObjectName("dockScene");
    sceneDock->setWidget(central);
    sceneDock->setFeature(ads::CDockWidget::NoTab, true);
    ads::CDockAreaWidget* sceneArea = docks->setCentralWidget(sceneDock);

    auto* palettePanel = new TilePalettePanel(ctx);
    auto* projectPanel = new ProjectPanel(ctx.hasProject() ? ctx.projectDir() : projectDir());
    ads::CDockWidget* hierarchyDock = makeDock(docks, "Hierarchy", "dockHierarchy", new HierarchyPanel(ctx));
    ads::CDockWidget* inspectorDock = makeDock(docks, "Inspector", "dockInspector", new InspectorPanel(ctx));
    ads::CDockWidget* projectDock   = makeDock(docks, "Project", "dockProject", projectPanel);
    ads::CDockWidget* consoleDock   = makeDock(docks, "Console", "dockConsole", new ConsolePanel);
    ads::CDockWidget* paletteDock   = makeDock(docks, "Tile Palette", "dockTilePalette", palettePanel);
    window.addActions(palettePanel->toolActions());   // V / B / E / G / I et taille du pinceau partout dans la fenêtre

    // Disposition par défaut : Hierarchy + Tile Palette à gauche, Inspector à droite, Project + Console en bas
    ads::CDockAreaWidget* leftArea   = docks->addDockWidget(ads::LeftDockWidgetArea, hierarchyDock);
    docks->addDockWidget(ads::BottomDockWidgetArea, paletteDock, leftArea);
    docks->addDockWidget(ads::RightDockWidgetArea, inspectorDock);
    ads::CDockAreaWidget* bottomArea = docks->addDockWidget(ads::BottomDockWidgetArea, projectDock, sceneArea);
    docks->addDockWidgetTabToArea(consoleDock, bottomArea);
    projectDock->setAsCurrentTab();

    // Affichage > Fenêtres : une case à cocher par panneau
    const QList<ads::CDockWidget*> panels = { hierarchyDock, inspectorDock, projectDock, consoleDock, paletteDock };
    for (int i = 0; i < panels.size(); ++i) {
        QAction* toggle = panels[i]->toggleViewAction();
        toggle->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key(Qt::Key_1 + i)));
        windowsMenu->addAction(toggle);
    }
    windowsMenu->addSeparator();
    QAction* lockLayout = windowsMenu->addAction("Verrouiller la disposition");
    lockLayout->setCheckable(true);
    lockLayout->setToolTip("Empêche de déplacer, détacher ou fermer les panneaux par erreur");
    QObject::connect(lockLayout, &QAction::toggled, docks, [docks](bool locked) {
        docks->lockDockWidgetFeaturesGlobally(locked ? ads::CDockWidget::GloballyLockableFeatures
                                                     : ads::CDockWidget::NoDockWidgetFeatures);
    });
    QAction* resetLayout = windowsMenu->addAction("Réinitialiser la disposition");

    // Mémorise la disposition (après l'affichage, quand les tailles sont connues)
    constexpr int kLayoutVersion = 4;   // 4 : panneaux ADS (l'ancienne disposition QDockWidget est ignorée)
    QByteArray defaultLayout;
    QTimer::singleShot(0, &window, [&] {
        // Tailles de départ : le séparateur qui contient `area` dans ce sens (si le nombre de parts correspond)
        const auto sizeSplitter = [](QWidget* area, Qt::Orientation orientation, QList<int> sizes) {
            for (QWidget* w = area; w; w = w->parentWidget()) {
                auto* splitter = qobject_cast<QSplitter*>(w->parentWidget());
                if (splitter && splitter->orientation() == orientation) {
                    if (splitter->count() != sizes.size()) return;
                    int total = 0;
                    for (int i = 0; i < splitter->count(); ++i) total += splitter->sizes().at(i);
                    int fixed = 0;
                    for (int v : sizes) fixed += std::max(0, v);
                    for (int& v : sizes) if (v < 0) v = std::max(100, total - fixed);   // -1 = le reste
                    splitter->setSizes(sizes);
                    return;
                }
            }
        };
        sizeSplitter(leftArea, Qt::Horizontal, { 380, -1, 320 });     // gauche | scène | droite
        sizeSplitter(leftArea, Qt::Vertical, { 230, 520 });           // Hierarchy / Tile Palette
        sizeSplitter(bottomArea, Qt::Vertical, { -1, 220 });          // scène / Project + Console
        defaultLayout = docks->saveState(kLayoutVersion);
        const QByteArray saved = QSettings().value("ui/adsLayout").toByteArray();
        if (!saved.isEmpty()) docks->restoreState(saved, kLayoutVersion);
        lockLayout->setChecked(QSettings().value("ui/layoutLocked", false).toBool());
    });
    QObject::connect(resetLayout, &QAction::triggered, &window, [&] {
        if (defaultLayout.isEmpty()) return;
        docks->restoreState(defaultLayout, kLayoutVersion);
        for (ads::CDockWidget* d : panels) d->toggleView(true);
        projectDock->setAsCurrentTab();
    });
    QObject::connect(&app, &QApplication::aboutToQuit, [&] {
        QSettings().setValue("ui/adsLayout", docks->saveState(kLayoutVersion));
        QSettings().setValue("ui/layoutLocked", lockLayout->isChecked());
    });

    std::cout << "[Editeur] Scene chargee : " << ctx.entities().size() << " entites" << std::endl;

    // --- Exécuter ---
    //  F5       : jouer dans l'éditeur (la scène du RPG remplace la vue tilemap)
    //  F6       : pause / reprise
    //  Maj+F5   : arrêter et revenir à la tilemap
    //  Ctrl+F5  : lancer rpg.exe dans sa propre fenêtre
    QMenu* run = window.addMenu("E&xécuter");
    window.menuBar()->insertMenu(help->menuAction(), run);   // placé avant « Aide »
    QAction* play  = addAction(run, "Jouer", QKeySequence(Qt::Key_F5));
    QAction* pause = addAction(run, "Pause", QKeySequence(Qt::Key_F6));
    QAction* stop  = addAction(run, "Arrêter", QKeySequence(Qt::SHIFT | Qt::Key_F5));
    run->addSeparator();
    QAction* playExternal = addAction(run, "Lancer rpg.exe (fenêtre séparée)", QKeySequence(Qt::CTRL | Qt::Key_F5));
    run->addSeparator();
    QAction* systemsAction = addAction(run, "Systèmes...", QKeySequence());
    pause->setCheckable(true);
    pause->setEnabled(false);
    stop->setEnabled(false);

    GameView* gameView = nullptr;
    QProcess gameProcess;
    gameProcess.setWorkingDirectory(QCoreApplication::applicationDirPath());   // pour trouver assets/

    auto inputPath = [&]() -> QString {
        if (!ctx.hasProject()) return projectDir() + "/assets/input.cfg";
        return QString::fromStdString(ctx.project().resolve(ctx.project().inputPath));
    };

    // Édition > Input Manager... : fenêtre non modale, une seule à la fois
    QPointer<InputSettingsDialog> inputDialog;
    QObject::connect(inputSettings, &QAction::triggered, [&] {
        if (!inputDialog) {
            inputDialog = new InputSettingsDialog(inputPath(),
                                                  [](InputManager& input) { defaultRpgInput(input); }, &window);
            inputDialog->setAttribute(Qt::WA_DeleteOnClose);
            // jeu lancé dans l'éditeur : il prend les nouvelles touches tout de suite
            QObject::connect(inputDialog, &InputSettingsDialog::saved, [&] { if (gameView) gameView->reloadInput(); });
        }
        inputDialog->show();
        inputDialog->raise();
        inputDialog->activateWindow();
    });

    // Exécuter > Systèmes... : liste des systèmes (moteur + jeu), cocher / décocher
    QObject::connect(systemsAction, &QAction::triggered, [&] {
        std::vector<SystemsDialog::Row> rows;
        auto collect = [&](Game& game) {
            for (ISystem* s : game.systems().list()) {
                SystemsDialog::Row r;
                r.name = QString::fromStdString(s->name);
                r.priority = s->priority;
                for (const GameSystemInfo& info : GameSystemRegistry::get().all())
                    if (info.name == s->name) r.fromGame = true;
                const auto it = ctx.project().systems.find(s->name);
                r.enabled = it == ctx.project().systems.end() || it->second;
                rows.push_back(r);
            }
        };
        if (gameView && gameView->game()) collect(*gameView->game());
        else if (sf::RenderWindow* target = tilemapView->window()) {
            Game probe(*target);               // un Game vide, juste pour lister ses systèmes
            registerRpgSystems(probe);
            collect(probe);
        }
        SystemsDialog dialog(rows, &window);
        QObject::connect(&dialog, &SystemsDialog::systemToggled, [&](const QString& name, bool enabled) {
            QString error;
            if (!ctx.setSystemEnabled(name, enabled, &error))
                QMessageBox::warning(&window, "Systèmes", "Impossible d'enregistrer le projet :\n" + error);
            if (gameView && gameView->game()) gameView->game()->systems().setEnabled(name.toStdString(), enabled);
        });
        dialog.exec();
    });

    auto updatePlayState = [&] {
        const bool inEditor = gameView != nullptr;
        const bool external = gameProcess.state() != QProcess::NotRunning;
        play->setEnabled(!inEditor);
        pause->setEnabled(inEditor);
        stop->setEnabled(inEditor || external);
        playExternal->setEnabled(!external);
        grid->setEnabled(!inEditor);
        fit->setEnabled(!inEditor);
        if (inEditor)
            window.statusBar()->showMessage(gameView->isPaused()
                ? "Jeu en pause  —  F6 : reprendre, Maj+F5 : arrêter"
                : "En jeu  —  F6 : pause, Maj+F5 : arrêter");
        else if (external)
            window.statusBar()->showMessage("rpg.exe en cours d'exécution  —  Maj+F5 : arrêter");
    };

    QObject::connect(play, &QAction::triggered, [&] {
        if (gameView) return;
        GameView::MapCopies maps;   // tilemaps telles qu'elles sont dans l'éditeur (même non enregistrées)
        ctx.scene().view<Tilemap>([&](Entity, Tilemap& tm) {
            if (tm.data) maps[tm.file] = std::make_shared<TileMapData>(*tm.data);
        });
        gameView = new GameView(ctx.sceneSnapshot(), inputPath(), ctx.projectDir(), std::move(maps), ctx.project().systems);   // une COPIE de la scène
        viewports->addWidget(gameView);
        viewports->setCurrentWidget(gameView);
        gameView->setFocus();
        pause->setChecked(false);
        updatePlayState();
    });
    QObject::connect(pause, &QAction::toggled, [&](bool paused) {
        if (!gameView) return;
        gameView->setPaused(paused);
        updatePlayState();
    });
    QObject::connect(stop, &QAction::triggered, [&] {
        if (gameView) {
            viewports->setCurrentWidget(tilemapView);
            viewports->removeWidget(gameView);
            delete gameView;                      // arrête le jeu (onStop des systèmes)
            gameView = nullptr;
            pause->setChecked(false);
            window.statusBar()->showMessage("Jeu arrêté.", 3000);
        }
        if (gameProcess.state() != QProcess::NotRunning) {
            gameProcess.terminate();                  // demande la fermeture de la fenêtre
            if (!gameProcess.waitForFinished(1500)) gameProcess.kill();
        }
        updatePlayState();
    });

    QObject::connect(playExternal, &QAction::triggered, [&] {
        if (gameProcess.state() != QProcess::NotRunning) return;
        const QString exe = gameExecutablePath();
        if (!QFileInfo::exists(exe)) {
            QMessageBox::warning(&window, "rpg.exe introuvable",
                                 "Impossible de trouver :\n" + QDir::toNativeSeparators(exe) +
                                 "\n\nCompilez la cible rpg, puis réessayez.");
            return;
        }
        // rpg.exe joue les fichiers : on enregistre d'abord la scène (comme « Enregistrer et lancer »)
        if (ctx.isDirty() && !ctx.scenePath().isEmpty()) {
            QString err;
            if (!ctx.saveScene(&err)) { QMessageBox::warning(&window, "Enregistrement impossible", err); return; }
        }
        QStringList args;   // rpg.exe <projet.angproj> <scène>
        if (ctx.hasProject()) args << ctx.projectFile();
        if (ctx.hasProject() && !ctx.scenePath().isEmpty()) args << ctx.scenePath();
        gameProcess.start(exe, args);
    });
    QObject::connect(&gameProcess, &QProcess::started, [&] { updatePlayState(); });
    QObject::connect(&gameProcess, &QProcess::finished, [&](int code, QProcess::ExitStatus status) {
        updatePlayState();
        window.statusBar()->showMessage(status == QProcess::CrashExit
            ? "rpg.exe s'est arrêté brutalement."
            : QString("rpg.exe terminé (code %1).").arg(code), 5000);
    });
    QObject::connect(&gameProcess, &QProcess::errorOccurred, [&](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            window.statusBar()->showMessage("Impossible de lancer rpg.exe : " + gameProcess.errorString(), 5000);
    });

    // --- Fichier : ouvrir / enregistrer ---------------------------------------------------------
    std::function<bool()> saveSceneAs = [&]() -> bool {
        const QString dir = ctx.hasProject() ? ctx.projectDir() + "/scenes" : QDir::homePath();
        const QString suggested = ctx.scenePath().isEmpty() ? dir + "/nouvelle.scene" : ctx.scenePath();
        QString path = QFileDialog::getSaveFileName(&window, "Enregistrer la scène", suggested, "Scènes AngeVault (*.scene)");
        if (path.isEmpty()) return false;
        if (!path.endsWith(".scene", Qt::CaseInsensitive)) path += ".scene";
        QString err;
        if (!ctx.saveSceneAs(path, &err)) { QMessageBox::warning(&window, "Enregistrement impossible", err); return false; }
        window.statusBar()->showMessage("Scène enregistrée : " + QDir::toNativeSeparators(path), 4000);
        return true;
    };
    auto saveScene = [&]() -> bool {
        if (ctx.scenePath().isEmpty()) return saveSceneAs();
        QString err;
        if (!ctx.saveScene(&err)) { QMessageBox::warning(&window, "Enregistrement impossible", err); return false; }
        window.statusBar()->showMessage("Scène enregistrée : " + QDir::toNativeSeparators(ctx.scenePath()), 4000);
        return true;
    };
    // Avant de quitter / ouvrir autre chose : proposer d'enregistrer. false = annuler.
    auto maybeSave = [&]() -> bool {
        if (!ctx.isDirty()) return true;
        const auto answer = QMessageBox::question(&window, "Modifications non enregistrées",
            QString("Enregistrer les modifications de « %1 » ?").arg(ctx.sceneName()),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
        if (answer == QMessageBox::Cancel) return false;
        if (answer == QMessageBox::Discard) return true;
        return saveScene();
    };
    auto openFile = [&](const QString& path) {
        if (!maybeSave()) return;
        QString err;
        const bool isProject = path.endsWith(kProjectExtension, Qt::CaseInsensitive);
        const bool ok = isProject ? openProjectFile(path, &err) : ctx.openScene(path, &err);
        if (!ok) QMessageBox::warning(&window, "Ouverture impossible", err);
        else window.statusBar()->showMessage("Ouvert : " + QDir::toNativeSeparators(path), 4000);
    };

    QObject::connect(saveAction, &QAction::triggered, [&] { saveScene(); });
    QObject::connect(saveAsAction, &QAction::triggered, [&] { saveSceneAs(); });
    QObject::connect(newSceneAction, &QAction::triggered, [&] { if (maybeSave()) ctx.newScene(); });
    QObject::connect(openAction, &QAction::triggered, [&] {
        const QString start = ctx.hasProject() ? ctx.projectDir() : QDir::homePath();
        const QString path = QFileDialog::getOpenFileName(&window, "Ouvrir un projet ou une scène", start,
            "Projets et scènes AngeVault (*.angproj *.scene);;Projets (*.angproj);;Scènes (*.scene)");
        if (!path.isEmpty()) openFile(path);
    });
    QObject::connect(newProjectAction, &QAction::triggered, [&] {
        if (!maybeSave()) return;
        const QString dir = QFileDialog::getExistingDirectory(&window, "Dossier du nouveau projet", QDir::homePath());
        if (dir.isEmpty()) return;
        bool ok = false;
        const QString name = QInputDialog::getText(&window, "Nouveau projet", "Nom du projet :",
                                                   QLineEdit::Normal, QFileInfo(dir).fileName(), &ok).trimmed();
        if (!ok || name.isEmpty()) return;
        Project created;
        std::string err;
        if (!Project::create(dir.toUtf8().toStdString(), name.toUtf8().toStdString(), created, &err)) {
            QMessageBox::warning(&window, "Création impossible", QString::fromUtf8(err.c_str()));
            return;
        }
        openFile(QString::fromStdString(created.filePath));
    });
    QObject::connect(recent, &QMenu::aboutToShow, [&] {   // liste à jour à chaque ouverture du menu
        recent->clear();
        const QStringList list = recentProjects();
        if (list.isEmpty()) recent->addAction("(aucun)")->setEnabled(false);
        for (const QString& path : list) {
            QAction* a = recent->addAction(QFileInfo(path).completeBaseName() + "   " + QDir::toNativeSeparators(path));
            QObject::connect(a, &QAction::triggered, [&, path] { openFile(path); });
        }
    });
    QObject::connect(projectPanel, &ProjectPanel::sceneOpenRequested, [&](const QString& path) { openFile(path); });
    // Entités glissées de la Hierarchy vers le panneau Project : un prefab par entité, dans ce dossier
    QObject::connect(projectPanel, &ProjectPanel::entitiesDropped, [&](const QByteArray& data, const QString& dir) {
        for (Entity e : ctx.entitiesFromMime(data)) {
            QString err;
            if (!ctx.createPrefab(e, ctx.defaultPrefabPath(e, dir), &err)) QMessageBox::warning(&window, "Prefab", err);
        }
    });
    QObject::connect(tilemapView, &TilemapView::statusText, &window, [&](const QString& text) {
        window.statusBar()->showMessage(text, 5000);
    });
    QObject::connect(&ctx, &EditorContext::statusMessage, &window, [&](const QString& text) {
        window.statusBar()->showMessage(text, 6000);
    });
    QObject::connect(&ctx, &EditorContext::documentChanged, projectPanel, [&] {
        if (ctx.hasProject()) projectPanel->setRootDir(ctx.projectDir());
    });
    window.installEventFilter(new CloseGuard(&window, maybeSave));   // quitter : enregistrer ?

    window.resize(1280, 780);
    window.show();
    const int result = app.exec();
    delete gameView;                                // arrête proprement le jeu en cours
    if (gameProcess.state() != QProcess::NotRunning) {   // on ferme aussi le jeu lancé
        gameProcess.terminate();
        if (!gameProcess.waitForFinished(1500)) gameProcess.kill();
    }
    LogCapture::instance().uninstall();
    return result;
}
