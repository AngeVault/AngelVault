#pragma once
#include <QColor>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <vector>

class EditorContext;
class QGridLayout;
class QHBoxLayout;
class QLineEdit;
class QListWidget;
class QScrollArea;

// catégories du moteur fixes, favoris et dossiers propres au projet (editor.json)
class ComponentLibraryPanel : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor)
public:
    explicit ComponentLibraryPanel(EditorContext& context, QWidget* parent = nullptr);

    QColor accentColor() const { return m_accent; }
    void setAccentColor(const QColor& c);

    struct Entry { QString key, name, category, icon; };
    bool isFavorite(const QString& key) const { return m_favorites.contains(key); }
    bool inFolderView() const { return m_view.startsWith(kFolderPrefix); }

protected:
    void resizeEvent(QResizeEvent* e) override;

signals:
    void createCodeRequested(int kind);   // 0 behavior, 1 composant (C++ du projet)

private:
    struct Folder { QString name; QStringList items; };
    static constexpr const char* kFolderPrefix = "@dir:";
    static constexpr const char* kFavorites = "@fav";

    void load();
    void save();
    void rebuildChips();
    void rebuildCards();
    void refreshPresence();
    void updateHeaderLayout();   // étroit (dock vertical) : la recherche passe sur sa propre ligne
    void setView(const QString& view);
    void toggleFavorite(const QString& key);
    void removeFromView(const QString& key);
    void addToFolder(int folder, const QString& key);
    void addFolder();
    void renameFolder(int folder);
    void removeFolder(int folder);
    int folderIndex(const QString& name) const;
    QString uniqueFolderName(const QString& base) const;

    EditorContext& m_ctx;
    QGridLayout* m_header = nullptr;
    QScrollArea* m_chipsScroll = nullptr;
    QHBoxLayout* m_chips = nullptr;
    int m_narrow = -1;   // -1 : pas encore placé
    QListWidget* m_cards = nullptr;
    QLineEdit* m_search = nullptr;
    QColor m_accent{ 0x3b, 0x82, 0xf6 };
    std::vector<Entry> m_entries;
    QStringList m_favorites;
    std::vector<Folder> m_folders;
    QList<QWidget*> m_folderChips;
    QString m_view = "base";
    QString m_file;   // <projet>/editor.json, vide sans projet
};
