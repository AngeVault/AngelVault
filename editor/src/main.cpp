#include "customWindow.h"
#include "editor_context.h"
#include "game_view.h"
#include "panels/console_panel.h"
#include "panels/hierarchy_panel.h"
#include "panels/inspector_panel.h"
#include "panels/project_panel.h"
#include "panels/tile_palette_panel.h"
#include "sandbox_scene.h"
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
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QStackedWidget>
#include <QTimer>
#include <QUrl>

#include <iostream>

namespace {
QAction* addAction(QMenu* menu, const QString& text, const QKeySequence& shortcut = {}) {
    QAction* a = menu->addAction(text);
    a->setShortcut(shortcut);
    return a;
    }
}

// Chemin de Sandbox.exe : il est construit dans le même dossier bin/ que l'éditeur
QString sandboxPath() {
#ifdef Q_OS_WIN
    const QString name = "Sandbox.exe";
#else
    const QString name = "Sandbox";
#endif
    return QDir(QCoreApplication::applicationDirPath()).filePath(name);
}

// Dossier affiché par le panneau Project (défini par CMake : le projet Sandbox, assets + scripts)
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
    loadSandboxEntities(ctx.scene());   // en attendant les projets : la scène du Sandbox

    CustomWindow window;
    ThemeManager themes(&window);
    themes.restoreSaved();                              // dernier thème choisi
    window.setWindowTitle("AngeVault Editor");
    window.setWindowIcon(QIcon(":/resources/icons/icon.png"));
    window.setMinimumSize(640, 400);

    // --- Menus de la barre de titre :
    QMenu* file = window.addMenu("&Fichier");
    addAction(file, "Nouveau projet", QKeySequence::New);
    addAction(file, "Ouvrir...", QKeySequence::Open);
    QMenu* recent = file->addMenu("Récents");
    recent->addAction("sandbox");
    recent->addAction("rpg");
    file->addSeparator();
    addAction(file, "Enregistrer", QKeySequence::Save);
    addAction(file, "Enregistrer sous...", QKeySequence::SaveAs);
    file->addSeparator();
    QObject::connect(addAction(file, "Quitter", QKeySequence(Qt::ALT | Qt::Key_F4)),
                     &QAction::triggered, &window, &QWidget::close);

    QMenu* edit = window.addMenu("&Édition");
    addAction(edit, "Annuler", QKeySequence::Undo);
    addAction(edit, "Rétablir", QKeySequence::Redo);
    edit->addSeparator();
    addAction(edit, "Préférences...", QKeySequence(Qt::CTRL | Qt::Key_Comma));
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
    QDockWidget* hierarchyDock = makeDock(window, "Hierarchy", "dockHierarchy", new HierarchyPanel(ctx));
    QDockWidget* inspectorDock = makeDock(window, "Inspector", "dockInspector", new InspectorPanel(ctx));
    QDockWidget* projectDock   = makeDock(window, "Project", "dockProject", new ProjectPanel(projectDir()));
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
    constexpr int kLayoutVersion = 1;
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
    //  F5       : jouer dans l'éditeur (la scène du Sandbox remplace la vue tilemap)
    //  F6       : pause / reprise
    //  Maj+F5   : arrêter et revenir à la tilemap
    //  Ctrl+F5  : lancer Sandbox.exe dans sa propre fenêtre
    QMenu* run = window.addMenu("E&xécuter");
    window.menuBar()->insertMenu(help->menuAction(), run);   // placé avant « Aide »
    QAction* play  = addAction(run, "Jouer", QKeySequence(Qt::Key_F5));
    QAction* pause = addAction(run, "Pause", QKeySequence(Qt::Key_F6));
    QAction* stop  = addAction(run, "Arrêter", QKeySequence(Qt::SHIFT | Qt::Key_F5));
    run->addSeparator();
    QAction* playExternal = addAction(run, "Lancer Sandbox.exe (fenêtre séparée)", QKeySequence(Qt::CTRL | Qt::Key_F5));
    pause->setCheckable(true);
    pause->setEnabled(false);
    stop->setEnabled(false);

    GameView* gameView = nullptr;
    QProcess sandbox;
    sandbox.setWorkingDirectory(QCoreApplication::applicationDirPath());   // pour trouver assets/

    auto updatePlayState = [&] {
        const bool inEditor = gameView != nullptr;
        const bool external = sandbox.state() != QProcess::NotRunning;
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
            window.statusBar()->showMessage("Sandbox.exe en cours d'exécution  —  Maj+F5 : arrêter");
    };

    QObject::connect(play, &QAction::triggered, [&] {
        if (gameView) return;
        gameView = new GameView;                 // scène neuve à chaque lancement
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
        if (sandbox.state() != QProcess::NotRunning) {
            sandbox.terminate();                  // demande la fermeture de la fenêtre
            if (!sandbox.waitForFinished(1500)) sandbox.kill();
        }
        updatePlayState();
    });

    QObject::connect(playExternal, &QAction::triggered, [&] {
        if (sandbox.state() != QProcess::NotRunning) return;
        const QString exe = sandboxPath();
        if (!QFileInfo::exists(exe)) {
            QMessageBox::warning(&window, "Sandbox introuvable",
                                 "Impossible de trouver :\n" + QDir::toNativeSeparators(exe) +
                                 "\n\nCompilez la cible Sandbox, puis réessayez.");
            return;
        }
        sandbox.start(exe, {});
    });
    QObject::connect(&sandbox, &QProcess::started, [&] { updatePlayState(); });
    QObject::connect(&sandbox, &QProcess::finished, [&](int code, QProcess::ExitStatus status) {
        updatePlayState();
        window.statusBar()->showMessage(status == QProcess::CrashExit
            ? "Sandbox.exe s'est arrêté brutalement."
            : QString("Sandbox.exe terminé (code %1).").arg(code), 5000);
    });
    QObject::connect(&sandbox, &QProcess::errorOccurred, [&](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            window.statusBar()->showMessage("Impossible de lancer Sandbox.exe : " + sandbox.errorString(), 5000);
    });

    window.resize(1280, 780);
    window.show();
    const int result = app.exec();
    delete gameView;                                // arrête proprement le jeu en cours
    if (sandbox.state() != QProcess::NotRunning) {   // on ferme aussi le jeu lancé
        sandbox.terminate();
        if (!sandbox.waitForFinished(1500)) sandbox.kill();
    }
    LogCapture::instance().uninstall();
    return result;
}
