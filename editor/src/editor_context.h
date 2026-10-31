#pragma once
#include "tilemap/tilemap.h"
#include "ecs/registry.h"
#include "scene/project.h"
#include "scene/scene_io.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>
#include <optional>
#include <string>
#include <vector>

// Outils de la vue scène (choisis dans la Tile Palette)
enum class EditorTool { Select, Paint, Erase, Pick };

// État partagé de l'éditeur : la scène ouverte, la tilemap, la sélection et l'outil actif.
// Tous les panneaux (Hierarchy, Inspector, vue scène...) lisent et modifient CET objet
// et se tiennent à jour grâce à ses signaux : aucun panneau ne parle directement à un autre.
class EditorContext : public QObject {
    Q_OBJECT
public:
    explicit EditorContext(QObject* parent = nullptr);

    Registry& scene() { return m_scene; }

    // --- Tilemaps (composant Tilemap : facultatif, il peut y en avoir 0, 1 ou plusieurs) ---
    // La tilemap « active » est celle que l'on peint : l'entité sélectionnée si c'est une tilemap,
    // sinon la première de la scène.
    Entity activeTilemap();
    TileMapData* activeMap();                 // nullptr : pas de tilemap dans la scène
    sf::Vector2f activeMapOffset();           // position de la carte (Transform2D)
    Entity createTilemap(TileMapOrientation orientation, int width, int height, QString* error = nullptr);
    Entity addTilemapFromFile(const QString& mapPath, QString* error = nullptr);   // .map existant
    void notifyMapEdited();                   // cases peintes : scène « modifiée »
    void notifyTilesChanged();                // liste des tiles / réglages changés : palette à refaire

    // --- Entités ---
    std::vector<Entity> entities() const;
    bool isValid(Entity e) const { return m_scene.isValid(e); }
    QString displayName(Entity e);
    Entity createEntity(const QString& name = tr("Nouvelle entité"));
    void destroyEntity(Entity e);
    void renameEntity(Entity e, const QString& name);

    // --- Catégories (Hierarchy) ---
    // Dossiers de l'éditeur pour ranger les entités, comme les objets vides "dossier" d'Unity.
    // Ils n'existent que dans l'éditeur : rien n'est ajouté aux entités du jeu.
    QStringList categories() const { return m_categories; }
    QString addCategory(const QString& name = tr("Nouvelle catégorie"));   // renvoie le nom (rendu unique)
    void renameCategory(const QString& oldName, const QString& newName);
    void removeCategory(const QString& name);                              // ses entités reviennent à la racine
    QString categoryOf(Entity e) const;                                    // "" = aucune catégorie
    void setCategory(Entity e, const QString& category);                   // "" = racine

    // --- Caméras liées (Camera2d::targetLock) ---
    // Une caméra qui suit une entité est placée sur elle et ne peut plus être déplacée à la main :
    // sa position est dictée par la cible (c'est ce que fait le CameraSystem en jeu).
    Entity followTarget(Entity camera);                       // NULL_ENTITY si libre (ou pas une caméra)
    bool isPositionLocked(Entity e) { return isValid(followTarget(e)); }
    void setFollowTarget(Entity camera, Entity target);       // NULL_ENTITY = libérer la caméra
    void snapFollowers(Entity target, int depth = 0);         // replace les caméras qui suivent `target`
    void snapAllFollowers();

    // --- Projet et scène sur le disque (.angproj / .scene) ---
    // La scène est enregistrée en JSON avec ses entités, leurs composants et les catégories.
    // Les liens entre entités (caméra qui suit le Player) passent par PersistentId.
    const Project& project() const { return m_project; }
    bool hasProject() const { return !m_project.filePath.empty(); }
    QString projectFile() const { return QString::fromStdString(m_project.filePath); }
    QString projectDir() const;
    QString scenePath() const { return m_scenePath; }   // "" = scène jamais enregistrée
    QString sceneName() const;                          // "main.scene" (ou "Sans titre")
    bool openProject(const QString& angprojPath, QString* error = nullptr);   // + sa scène de départ
    bool openScene(const QString& path, QString* error = nullptr);
    bool saveScene(QString* error = nullptr);                                // dans scenePath()
    bool saveSceneAs(const QString& path, QString* error = nullptr);         // et l'ajoute au projet
    // Exécuter > Systèmes... : active / désactive un système et enregistre le .angproj
    bool setSystemEnabled(const QString& name, bool enabled, QString* error = nullptr);
    void newScene();                                                         // vide, sans chemin
    std::string sceneSnapshot();   // la scène en JSON : le mode Jouer charge une copie
    // Remplit une scène neuve avec des entités créées en code (ex. loadRpgEntities de rpg_scene.h)
    void importEntities(const std::function<void(Registry&)>& fill);

    // Modifications non enregistrées (étoile dans le titre, question en quittant)
    bool isDirty() const { return m_dirty; }
    void setDirty(bool dirty);

    // --- Comportements (Script) et prefabs : glisser-déposer ---
    bool addBehavior(Entity e, const std::string& name);    // ajoute au Script de l'entité (créé si besoin)
    void removeBehavior(Entity e, std::size_t index);       // retire le Script s'il devient vide
    // Fichiers déposés (panneau Project ou explorateur) :
    //   .prefab   -> une instance (à `position` si donnée, sinon là où le prefab a été créé)
    //   .h / .hpp -> les comportements déclarés dedans (REGISTER_BEHAVIOR) sont ajoutés à `target`
    // Renvoie les entités créées (prefabs). Un message part dans statusMessage().
    std::vector<Entity> dropFiles(const QStringList& paths, Entity target,
                                  std::optional<sf::Vector2f> position = std::nullopt);
    std::vector<Entity> instantiatePrefab(const QString& path, std::optional<sf::Vector2f> position, QString* error = nullptr);
    bool createPrefab(Entity root, const QString& path, QString* error = nullptr);   // root + caméras qui la suivent
    QString defaultPrefabPath(Entity root, const QString& dir = {});   // <dir ou projet/prefabs>/<Nom>.prefab, unique

    // Glisser des entités d'un panneau à l'autre (Hierarchy -> Project = créer un prefab)
    static constexpr const char* kEntityMimeType = "application/x-angevault-entities";
    static QByteArray entitiesToMime(const std::vector<Entity>& entities);
    std::vector<Entity> entitiesFromMime(const QByteArray& data) const;   // seulement celles encore valides

    // --- Sélection ---
    bool hasSelection() const { return isValid(m_selected); }
    Entity selected() const { return m_selected; }
    void select(Entity e);
    void clearSelection();

    // À appeler après avoir modifié une entité ; `source` = l'émetteur (pour ne pas se rafraîchir soi-même)
    void notifyEntityModified(Entity e, QObject* source = nullptr);
    void notifySceneChanged() { emit sceneChanged(); }

    // --- Outils de la Tile Palette ---
    EditorTool tool() const { return m_tool; }
    void setTool(EditorTool tool);
    uint16_t paintTile() const { return m_paintTile; }   // id de la tile à peindre (index + 1 dans la carte)
    void setPaintTile(uint16_t id);
    int brushSize() const { return m_brushSize; }
    void setBrushSize(int size);

signals:
    void sceneChanged();                          // entités ajoutées / supprimées / renommées
    void selectionChanged(Entity e);              // NULL_ENTITY si plus rien n'est sélectionné
    void entityModified(Entity e, QObject* source);
    void followTargetChanged(Entity camera);
    void componentsChanged(Entity e);             // composants ou comportements ajoutés / retirés (Inspector)      // la caméra suit une autre entité (ou plus rien)
    void toolChanged(EditorTool tool);
    void paintTileChanged(uint16_t id);
    void tilemapChanged();                        // autre tilemap active, ou ses tiles / réglages ont changé
    void brushSizeChanged(int size);
    void dirtyChanged(bool dirty);
    void documentChanged();
    void statusMessage(const QString& text);      // pour la barre d'état (glisser-déposer, prefabs...)                       // autre projet / scène ouverte, ou enregistrée sous un autre nom

private:
    Registry m_scene;
    Entity m_selected = NULL_ENTITY;
    QStringList m_categories;
    QHash<quint64, QString> m_entityCategory;   // clé : index + génération de l'entité

    Project m_project;
    QString m_scenePath;
    bool m_dirty = false;
    bool m_loading = false;   // pendant un chargement : les signaux ne rendent pas la scène « modifiée »

    void resetScene();                          // vide la scène, les catégories et la sélection
    Json editorData();                          // catégories -> section "editor" du fichier
    void applyEditorData(const Json& editor);
    SceneMeta sceneMeta();

    EditorTool m_tool = EditorTool::Select;
    uint16_t m_paintTile = 1;
    Entity m_activeTilemap = NULL_ENTITY;
    void refreshActiveTilemap();   // émet tilemapChanged si la tilemap active change
    int m_brushSize = 1;
};
