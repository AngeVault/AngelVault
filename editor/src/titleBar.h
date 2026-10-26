#pragma once
#include <QIcon>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QMenu;
class QMenuBar;
class QPushButton;

// Barre de titre moderne : icône + menus déroulants à gauche, titre centré,
// widgets perso + boutons réduire / agrandir / fermer à droite.
//
// Ajouter un menu :
//     QMenu* fichier = titleBar->addMenu("&Fichier");
//     fichier->addAction("Ouvrir...");
// Ajouter un widget à droite (recherche, bouton de compte...) :
//     titleBar->addRightWidget(new QLineEdit);
//
// Les zones vides (et le titre) servent à déplacer la fenêtre. Un widget ajouté qui
// doit lui aussi laisser déplacer la fenêtre : widget->setProperty("titleBarDraggable", true);
class TitleBar : public QWidget {
    Q_OBJECT
public:
    static constexpr int kHeight = 36;

    explicit TitleBar(QWidget* parent = nullptr);

    QMenuBar* menuBar() const { return m_menuBar; }
    QMenu* addMenu(const QString& title);
    void addRightWidget(QWidget* widget);

    void setTitle(const QString& title);
    void setIcon(const QIcon& icon);

    QPushButton* minimizeButton() const { return m_minimize; }
    QPushButton* maximizeButton() const { return m_maximize; }
    QPushButton* closeButton() const { return m_close; }

    // Utilisés par CustomWindow (coordonnées locales à la TitleBar)
    bool isDraggableAt(const QPoint& pos) const;
    bool isMaximizeButtonAt(const QPoint& pos) const;
    void setMaximized(bool maximized);
    void setActive(bool active);

signals:
    void minimizeRequested();
    void maximizeRequested();   // bascule agrandi / restauré
    void closeRequested();

protected:
    void resizeEvent(QResizeEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* e) override;
#ifndef Q_OS_WIN
    // Hors Windows : déplacement via Qt (sous Windows c'est le système qui gère, via HTCAPTION)
    void mousePressEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
#endif

private:
    QPushButton* makeCaptionButton(const char* objectName, const QString& iconPath, const QString& tip);
    void updateTitleGeometry();

    QLabel* m_icon = nullptr;
    QMenuBar* m_menuBar = nullptr;
    QLabel* m_title = nullptr;
    QWidget* m_rightArea = nullptr;
    QHBoxLayout* m_customLayout = nullptr;
    QPushButton* m_minimize = nullptr;
    QPushButton* m_maximize = nullptr;
    QPushButton* m_close = nullptr;
    QString m_fullTitle;
};
