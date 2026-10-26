#pragma once
#include <QList>
#include <QObject>
#include <QString>

class QAction;
class QActionGroup;
class QMenu;
class QWidget;

// Thèmes de l'éditeur = feuilles de style Qt (.qss).
//  - thèmes intégrés (dans les ressources) : Sombre, Néon rose
//  - n'importe quel fichier .qss choisi sur le disque
// Le choix est mémorisé (QSettings) et réappliqué au démarrage.
//
// Ajouter un thème intégré : mettre le .qss dans editor/assets/style/, l'ajouter à
// resources.qrc, puis une ligne dans ThemeManager::ThemeManager (theme_manager.cpp).
class ThemeManager : public QObject {
    Q_OBJECT
public:
    explicit ThemeManager(QWidget* dialogParent);

    // Sous-menu « Thème » prêt à être ajouté à un menu (ex. Édition)
    QMenu* createMenu(QWidget* parent);

    bool apply(const QString& path, bool showErrors = true);
    void restoreSaved();                 // thème mémorisé, sinon le thème par défaut
    void reload();                       // relit le fichier courant (pratique pendant qu'on l'édite)
    void chooseFile();                   // boîte de dialogue « Charger un fichier QSS »

    QString currentPath() const { return m_current; }

private:
    struct Builtin { QString name; QString path; QAction* action = nullptr; };

    void updateChecks();

    QWidget* m_parent;
    QList<Builtin> m_builtins;
    QActionGroup* m_group = nullptr;
    QAction* m_customAction = nullptr;   // « Personnalisé : fichier.qss »
    QString m_current;
};
