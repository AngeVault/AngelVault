#pragma once
#include <QColor>
#include <QMainWindow>
#include <QRect>
#include <QPoint>

class QMenu;
class QMenuBar;
class TitleBar;
#ifdef Q_OS_WIN
class ChildHitTestFilter;
#endif

// Fenêtre sans barre Windows mais avec le comportement natif :
//  - redimensionnement par les bords/coins, Aero Snap (glisser sur un bord), Win + flèches
//  - double-clic sur la barre = agrandir, clic droit = menu système, Alt+Espace
//  - animations réduire/agrandir, ombre, coins arrondis et Snap Layouts (Windows 11)
//
// C'est un QMainWindow normal : setCentralWidget, addDockWidget, addToolBar, statusBar... marchent.
// La barre de titre occupe l'emplacement du menu (setMenuWidget) : n'appelez pas setMenuBar().
//
// La couleur du liseré Windows (Windows 11) se règle depuis le QSS :
//     QMainWindow#MainWindow { qproperty-borderColor: #ff3ea5; }
class CustomWindow : public QMainWindow {
    Q_OBJECT
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor)
public:
    explicit CustomWindow(QWidget* parent = nullptr);
    ~CustomWindow() override;

    TitleBar* titleBar() const { return m_titleBar; }

    // Masque QMainWindow::menuBar() (qui remplacerait la barre de titre) :
    // renvoie la barre de menus intégrée à la barre de titre.
    QMenuBar* menuBar() const;
    QMenu* addMenu(const QString& title);

    void toggleMaximized();
    void minimize();

    QColor borderColor() const { return m_borderColor; }
    void setBorderColor(const QColor& color);

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void changeEvent(QEvent* e) override;
    void showEvent(QShowEvent* e) override;

private:
    TitleBar* m_titleBar = nullptr;
    QColor m_borderColor{ 48, 48, 52 };

#ifdef Q_OS_WIN
    friend class ChildHitTestFilter;
    void applyNativeFrame();
    void updateMaximizedMargins();
    int hitTest(int screenX, int screenY) const;   // coordonnées écran physiques -> code HT*
    void setMaximizeHovered(bool hovered);
    void setMaximizePressed(bool pressed);

    ChildHitTestFilter* m_childFilter = nullptr;
    bool m_maxHovered = false;
    bool m_maxPressed = false;
#endif
};
