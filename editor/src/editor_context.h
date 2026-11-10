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
#include <memory>
#include <optional>
#include <string>
#include <vector>


// Outils de la vue scène (choisis dans la Tile Palette)
enum class EditorTool { Select, Paint, Erase, Fill, Pick, Resize };

// Outil Taille : ce que les poignées agrandissent sur l'entité sélectionnée
enum class ResizeTarget { Both, Geometry, Collider };

// Forme du pinceau (Pinceau / Gomme)
enum class BrushShape { Square, Circle };

// Ce que l'on peint : un tileset de la carte active, en autotile ou une (ou plusieurs) tile(s) de sa feuille
struct TileBrush {
    int slot = -1;               // n° du tileset dans la carte active (-1 = rien de choisi)
    bool autotile = false;       // peindre le terrain : les pièces sont choisies selon les voisines
    int tile = 0;                // tile choisie (coin haut-gauche du bloc s'il y en a plusieurs)
    int width = 1, height = 1;   // bloc de tiles choisi dans la feuille (« tampon »), en tiles
    bool valid() const { return slot >= 0; }
    bool isStamp() const { return !autotile && (width > 1 || height > 1); }
    bool operator==(const TileBrush&) const = default;
};

// État partagé de l'éditeur : la scène ouverte, la tilemap, la sélection et l'outil actif.
// Tous les panneaux (Hierarchy, Inspector, vue scène...) lisent et modifient CET objet
// et se tiennent à jour grâce à ses signaux : aucun panneau ne parle directement à un autre.
class EditorContext : public QObject {
    Q_OBJECT
public:
    explicit EditorContext(QObject* parent = nullptr);

    friend void load_map(EditorContext& ctx);

    Registry& scene() { return m_scene; }
    // --- Map and layers ---

    // --- Tilemaps (composant Tilemap : facultatif, il peut y en avoir 0, 1 ou plusieurs) ---
    // La tilemap « active » est celle que l'on peint : l'entité sélectionnée si c'est une tilemap,
    // sinon la première de la scène.
    Entity activeTilemap();
    TileMapData* activeMap();                 // nullptr : pas de tilemap dans la scène
    sf::Vector2f activeMapOffset();           // position de la carte (Transform2D)
    Entity createTilemap(TileMapOrientation orientation, int width, int height, QString* error = nullptr);
    Entity addTilemapFromFile(const QString& mapPath, QString* error = nullptr);   // .map existant
    void notifyMapEdited();                   // cases peintes : scène « modifiée »
    void notifyTilesChanged();                // liste des tilesets / réglages changés : palette à refaire
    // Un .tileset vient d'être modifié et enregistré : les autres tilemaps de la scène qui l'utilisent suivent
    void notifyTilesetSaved(const std::shared_ptr<Tileset>& tileset);

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
    // renvoie le nom (rendu unique) ; parent "" = racine
    QString addCategory(const QString& name = tr("Nouvelle catégorie"), const QString& parent = QString());
    QString categoryParent(const QString& name) const { return m_categoryParent.value(name); }
    bool isInsideCategory(const QString& name, const QString& ancestor) const;   // name est (dans) ancestor
    bool setCategoryParent(const QString& name, const QString& parent);         // false si ça fait une boucle
    void renameCategory(const QString& oldName, const QString& newName);
    void removeCategory(const QString& name);                              // son contenu remonte d'un niveau
    void moveCategory(const QString& name, int index);                     // index dans la liste avant le déplacement
    QString categoryOf(Entity e) const;                                    // "" = aucune catégorie
    void setCategory(Entity e, const QString& category);                   // "" = racine

    // --- Caméras liées (CameraFollow::target) ---
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
    // Écran du jeu (window.size du .angproj) : cadre des caméras et vue Jouer
    sf::Vector2u gameScreenSize() const {
        const ProjectWindow& w = m_project.window;
        return (w.width > 0 && w.height > 0) ? sf::Vector2u{ w.width, w.height } : sf::Vector2u{ 1920u, 1080u };
    }
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

    // --- Annuler / Rétablir (Ctrl+Z / Ctrl+Y) ---
    // Après chaque modification (entités, composants, Hierarchy, tiles peintes), l'état de la scène est
    // mémorisé : un « instantané » (scène en JSON + cases des tilemaps + sélection). Les modifications
    // rapprochées (frappe dans l'Inspector) sont regroupées ; un geste à la souris dans la vue
    // (beginInteraction / endInteraction) compte pour une seule étape.
    void undo();
    void redo();
    bool canUndo() const { return !m_undo.empty() || m_pending; }
    bool canRedo() const { return !m_redo.empty(); }
    void beginInteraction();
    void endInteraction();

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
    const TileBrush& brush() const { return m_brush; }
    void setBrush(const TileBrush& brush);
    static constexpr int kMaxBrushSize = 32;
    int brushSize() const { return m_brushSize; }       // côté du pinceau, en cases (1 .. kMaxBrushSize)
    void setBrushSize(int size);
    ResizeTarget resizeTarget() const { return m_resizeTarget; }
    void setResizeTarget(ResizeTarget target);
    BrushShape brushShape() const { return m_brushShape; }
    void setBrushShape(BrushShape shape);

signals:
    void sceneChanged();                          // entités ajoutées / supprimées / renommées
    void selectionChanged(Entity e);              // NULL_ENTITY si plus rien n'est sélectionné
    void entityModified(Entity e, QObject* source);
    void followTargetChanged(Entity camera);
    void componentsChanged(Entity e);             // composants ou comportements ajoutés / retirés (Inspector)      // la caméra suit une autre entité (ou plus rien)
    void toolChanged(EditorTool tool);
    void brushChanged(const TileBrush& brush);
    void tilemapChanged();                        // autre tilemap active, ou ses tiles / réglages ont changé
    void brushSizeChanged(int size);
    void brushShapeChanged(BrushShape shape);
    void resizeTargetChanged(ResizeTarget target);
    void dirtyChanged(bool dirty);
    void documentChanged();
    void statusMessage(const QString& text);
    void undoStateChanged();                      // canUndo() / canRedo() ont pu changer      // pour la barre d'état (glisser-déposer, prefabs...)                       // autre projet / scène ouverte, ou enregistrée sous un autre nom

private:
    Registry m_scene;
    Entity m_selected = NULL_ENTITY;
    QStringList m_categories;                   // noms uniques, l'ordre = l'affichage
    QHash<QString, QString> m_categoryParent;   // catégorie -> parente (absente = racine)
    QHash<quint64, QString> m_entityCategory;   // clé : index + génération de l'entité

    Project m_project;
    QString m_scenePath;
    bool m_dirty = false;
    bool m_loading = false;   // pendant un chargement : les signaux ne rendent pas la scène « modifiée »

    void resetScene();                          // vide la scène, les catégories et la sélection

    // Historique
    struct Snapshot {
        std::string scene;                                                       // sceneToString (+ catégories)
        std::vector<std::pair<uint64_t, std::shared_ptr<const TileMapData>>> maps;   // PersistentId -> cases
        uint64_t selected = 0;                                                   // PersistentId (0 = rien)
    };
    Snapshot captureSnapshot();
    void restoreSnapshot(const Snapshot& snapshot);
    bool sameSnapshot(const Snapshot& a, const Snapshot& b) const;
    void markChanged();       // une modification : l'étape sera enregistrée (tout de suite ou un peu après)
    void commitChanges();     // enregistre l'étape en attente
    void resetHistory();      // nouvelle scène : historique vide
    std::vector<Snapshot> m_undo, m_redo;
    Snapshot m_current;
    bool m_pending = false;
    bool m_restoring = false;
    int m_interaction = 0;
    class QTimer* m_commitTimer = nullptr;
    Json editorData();                          // catégories -> section "editor" du fichier
    void applyEditorData(const Json& editor);
    SceneMeta sceneMeta();

    EditorTool m_tool = EditorTool::Select;
    TileBrush m_brush;
    Entity m_activeTilemap = NULL_ENTITY;
    void refreshActiveTilemap();   // émet tilemapChanged si la tilemap active change
    int m_brushSize = 1;
    BrushShape m_brushShape = BrushShape::Square;
    ResizeTarget m_resizeTarget = ResizeTarget::Both;
};
