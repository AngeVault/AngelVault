#pragma once
#include <QHash>
#include <QIcon>
#include <QString>
#include <QWidget>

class QFileSystemModel;
class QFileSystemWatcher;
class QLabel;
class QLineEdit;
class QListWidget;
class QTreeView;

// Navigateur des assets du projet : dossiers à gauche, contenu (avec aperçus d'images) à droite.
// Double-clic sur un dossier : l'ouvrir ; sur un fichier : l'ouvrir avec l'application par défaut.
class ProjectPanel : public QWidget {
    Q_OBJECT
public:
    explicit ProjectPanel(const QString& rootDir, QWidget* parent = nullptr);

    void setCurrentDir(const QString& dir);
    void setRootDir(const QString& dir);   // autre projet ouvert

signals:
    void sceneOpenRequested(const QString& path);   // double-clic sur un fichier .scene
    void entitiesDropped(const QByteArray& entities, const QString& dir);   // depuis la Hierarchy : créer des prefabs

private:
    void refresh();
    void openPath(const QString& path);
    void showContextMenu(const QString& path, const QPoint& globalPos);
    QIcon iconFor(const QString& path, bool isDir);
    void createCode(int kind);   // clic droit > Créer (C++) : 0 behavior, 1 composant, 2 système

    QString m_root;
    QString m_current;
    QFileSystemModel* m_dirs = nullptr;
    QTreeView* m_tree = nullptr;
    QListWidget* m_list = nullptr;
    QLabel* m_path = nullptr;
    QLineEdit* m_search = nullptr;
    QFileSystemWatcher* m_watcher = nullptr;
    QHash<QString, QIcon> m_thumbnails;   // chemin + date -> aperçu
};
