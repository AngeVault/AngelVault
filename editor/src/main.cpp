#include "customWindow.h"
#include "editor_context.h"
#include "game_view.h"
#include "input_settings_dialog.h"
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
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QSettings>
#include <QStackedWidget>
#include <QTimer>
#include <QUrl>

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

// Panneau détachable : on peut le déplacer, l'empiler en onglets, le faire flotter ou le fermer
QDockWidget* makeDock(QMainWindow& window, const QString& title, const QString& id, QWidget* content) {
    auto* dock = new QDockWidget(title, &window);
    dock->setObjectName(id);   // indispensable pour mémoriser la disposition
    content->setAttribute(Qt::WA_StyledBackground);   // sinon le QSS ne dessine ni fond ni contour
    dock->setWidget(content);
    dock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    return dock;
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
    addAction(edit, "Annuler", QKeySequence::Undo);
    addAction(edit, "Rétablir", QKeySequence::Redo);
    edit->addSeparator();
    addAction(edit, "Préférences...", QKeySequence(Qt::CTRL | Qt::Key_Comma));
    QAction* inputSettings = addAction(edit, "Input Manager...");   // actions d'entrée du projet (assets/input.cfg)
    edit->addMenu(themes.createMenu(edit));             // Sombre / Néon rose / fichier .qss

    QMenu* view = window.addMenu("&Affichage");
    QAction* grid = addAction(view, "Grille", QKeySequence(Qt::CTRL | Qt::Key_G));
    grid->setCheckable(true);
    grid->setChecked(true);
    QAction* fit = addAction(view, "Recadrer la carte", QKeySequence(Qt::CTRL | Qt::Key_0));
    QAction* fullScreen = addAction(view, "Plein écran", QKeySequence(Qt::Key_F11));
    QObject::connect(fullScreen, &QAction::triggered, &window, [&window] {
        window.isFullScreen() ? window.showNormal() : window.showFullScreen();
    });

    view->addSeparator();
    QMenu* windowsMenu = view->addMenu("Fenêtres");   // rempli plus bas, une fois les panneaux créés

    QMenu* help = window.addMenu("&Aide");
    addLink(help, "À propos d'AngeVault", "https://sites.google.com/view/angelvault/accueil");
    addLink(help, "Documentation", "https://sites.google.com/view/angelvault/docs");
    addLink(help, "Signaler un bug", "https://github.com/AngeVault/angelVault/issues");

    // --- Vue centrale : tilemap + entités ---
    // La vue de jeu s'empile par-dessus pendant que l'on joue.
    auto* viewports = new QStackedWidget;
    auto* tilemapView = new TilemapView(ctx);
    viewports->addWidget(tilemapView);
    window.setCentralWidget(viewports);

    QObject::connect(grid, &QAction::toggled, tilemapView, &TilemapView::setGridVisible);
    QObject::connect(fit, &QAction::triggered, tilemapView, &TilemapView::fitToMap);

    auto* tileInfo = new QLabel;
    window.statusBar()->addPermanentWidget(tileInfo);
    QObject::connect(tilemapView, &TilemapView::hoveredTileChanged, tileInfo,
                     [tileInfo](int x, int y, const QString& type) {
        tileInfo->setText(x < 0 ? QString() : QString("Case %1, %2  —  %3").arg(x).arg(y).arg(type));
    });
    window.statusBar()->showMessage("Molette : zoom  —  clic droit / molette + glisser : déplacer  —  F5 : jouer");

    // --- Panneaux : Hierarchy / Inspector / Project / Console / Tile Palette ---
    window.setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks | QMainWindow::AllowNestedDocks);
    window.setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);
    window.setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);      // gauche et droite sur toute la hauteur
    window.setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    auto* palettePanel = new TilePalettePanel(ctx);
    auto* projectPanel = new ProjectPanel(ctx.hasProject() ? ctx.projectDir() : projectDir());
    QDockWidget* hierarchyDock = makeDock(window, "Hierarchy", "dockHierarchy", new HierarchyPanel(ctx));
    QDockWidget* inspectorDock = makeDock(window, "Inspector", "dockInspector", new InspectorPanel(ctx));
    QDockWidget* projectDock   = makeDock(window, "Project", "dockProject", projectPanel);
    QDockWidget* consoleDock   = makeDock(window, "Console", "dockConsole", new ConsolePanel);
    QDockWidget* paletteDock   = makeDock(window, "Tile Palette", "dockTilePalette", palettePanel);
    window.addActions(palettePanel->toolActions());   // V / B / E / I partout dans la fenêtre

    // Disposition par défaut
    window.addDockWidget(Qt::LeftDockWidgetArea, hierarchyDock);
    window.splitDockWidget(hierarchyDock, paletteDock, Qt::Vertical);
    window.addDockWidget(Qt::RightDockWidgetArea, inspectorDock);
    window.addDockWidget(Qt::BottomDockWidgetArea, projectDock);
    window.tabifyDockWidget(projectDock, consoleDock);
    projectDock->raise();

    // Affichage > Fenêtres : une case à cocher par panneau
    const QList<QDockWidget*> docks = { hierarchyDock, inspectorDock, projectDock, consoleDock, paletteDock };
    for (int i = 0; i < docks.size(); ++i) {
        QAction* toggle = docks[i]->toggleViewAction();
        toggle->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key(Qt::Key_1 + i)));
        windowsMenu->addAction(toggle);
    }
    windowsMenu->addSeparator();
    QAction* resetLayout = windowsMenu->addAction("Réinitialiser la disposition");

    // Mémorise la disposition (après l'affichage, quand les tailles sont connues)
    constexpr int kLayoutVersion = 2;   // 2 : Hierarchy et Inspector de retour (l'ancienne disposition est ignorée)
    QByteArray defaultLayout;
    QTimer::singleShot(0, &window, [&] {
        window.resizeDocks({ hierarchyDock, inspectorDock }, { 250, 320 }, Qt::Horizontal);
        window.resizeDocks({ hierarchyDock, paletteDock }, { 320, 280 }, Qt::Vertical);
        window.resizeDocks({ projectDock }, { 220 }, Qt::Vertical);
        defaultLayout = window.saveState(kLayoutVersion);
        const QByteArray saved = QSettings().value("ui/dockLayout").toByteArray();
        if (!saved.isEmpty()) window.restoreState(saved, kLayoutVersion);
    });
    QObject::connect(resetLayout, &QAction::triggered, &window, [&] {
        if (defaultLayout.isEmpty()) return;
        window.restoreState(defaultLayout, kLayoutVersion);
        for (QDockWidget* d : docks) d->show();
        projectDock->raise();
    });
    QObject::connect(&app, &QApplication::aboutToQuit, [&] {
        QSettings().setValue("ui/dockLayout", window.saveState(kLayoutVersion));
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
        gameView = new GameView(ctx.sceneSnapshot(), inputPath());   // une COPIE de la scène éditée
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
