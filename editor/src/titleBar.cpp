#include "titleBar.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStyle>
#include <QWindow>

namespace {
void repolish(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}
}

TitleBar::TitleBar(QWidget* parent) : QWidget(parent) {
    setObjectName("titleBar");
    setAttribute(Qt::WA_StyledBackground);   // pour que le QSS dessine le fond
    setFixedHeight(kHeight);
    setProperty("active", true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Icône de l'application
    m_icon = new QLabel(this);
    m_icon->setObjectName("titleIcon");
    m_icon->setFixedSize(40, kHeight);
    m_icon->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_icon);

    // Menus déroulants
    m_menuBar = new QMenuBar(this);
    m_menuBar->setObjectName("titleMenuBar");
    m_menuBar->setNativeMenuBar(false);
    m_menuBar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    layout->addWidget(m_menuBar, 0, Qt::AlignVCenter);

    layout->addStretch(1);

    // Zone droite : widgets perso puis boutons système
    m_rightArea = new QWidget(this);
    auto* right = new QHBoxLayout(m_rightArea);
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(0);
    m_customLayout = new QHBoxLayout;
    m_customLayout->setContentsMargins(0, 0, 8, 0);
    m_customLayout->setSpacing(6);
    right->addLayout(m_customLayout);

    m_minimize = makeCaptionButton("minimizeButton", ":/resources/icons/minimize.png", tr("Réduire"));
    m_maximize = makeCaptionButton("maximizeButton", ":/resources/icons/maximize.png", tr("Agrandir"));
    m_close    = makeCaptionButton("closeButton",    ":/resources/icons/close.png",    tr("Fermer"));
    right->addWidget(m_minimize);
    right->addWidget(m_maximize);
    right->addWidget(m_close);
    layout->addWidget(m_rightArea);

    connect(m_minimize, &QPushButton::clicked, this, &TitleBar::minimizeRequested);
    connect(m_maximize, &QPushButton::clicked, this, &TitleBar::maximizeRequested);
    connect(m_close,    &QPushButton::clicked, this, &TitleBar::closeRequested);

    // Titre centré sur toute la largeur (hors layout), ne capte pas la souris
    m_title = new QLabel(this);
    m_title->setObjectName("titleLabel");
    m_title->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_title->setAlignment(Qt::AlignCenter);

    m_menuBar->installEventFilter(this);
    m_rightArea->installEventFilter(this);
}

QPushButton* TitleBar::makeCaptionButton(const char* objectName, const QString& iconPath, const QString& tip) {
    auto* b = new QPushButton(this);
    b->setObjectName(objectName);
    b->setIcon(captionIcon(iconPath));
    b->setIconSize(QSize(14, 14));
    b->setFixedSize(46, kHeight);
    b->setFocusPolicy(Qt::NoFocus);
    b->setFlat(true);
    b->setToolTip(tip);
    return b;
}

QMenu* TitleBar::addMenu(const QString& title) {
    return m_menuBar->addMenu(title);
}

void TitleBar::addRightWidget(QWidget* widget) {
    m_customLayout->addWidget(widget, 0, Qt::AlignVCenter);
}

void TitleBar::setTitle(const QString& title) {
    m_fullTitle = title;
    updateTitleGeometry();
}

void TitleBar::setIcon(const QIcon& icon) {
    m_icon->setPixmap(icon.isNull() ? QPixmap() : icon.pixmap(QSize(16, 16), devicePixelRatioF()));
}

bool TitleBar::isDraggableAt(const QPoint& pos) const {
    QWidget* w = childAt(pos);   // ignore les widgets "transparents" (le titre)
    if (!w || w == m_icon || w == m_rightArea)
        return true;
    if (w == m_menuBar)          // vide de la barre de menus = draggable, un menu = cliquable
        return m_menuBar->actionAt(m_menuBar->mapFrom(this, pos)) == nullptr;
    return w->property("titleBarDraggable").toBool();
}

bool TitleBar::isMaximizeButtonAt(const QPoint& pos) const {
    return m_maximize->isVisible() && m_maximize->geometry().contains(m_rightArea->mapFrom(this, pos));
}

QIcon TitleBar::captionIcon(const QString& path) const {
    if (!m_iconColor.isValid()) return QIcon(path);
    QPixmap pix(path);
    QPainter p(&pix);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);   // garde la forme, remplace la couleur
    p.fillRect(pix.rect(), m_iconColor);
    p.end();
    return QIcon(pix);
}

void TitleBar::setIconColor(const QColor& color) {
    if (color == m_iconColor) return;
    m_iconColor = color;
    refreshCaptionIcons();
}

void TitleBar::refreshCaptionIcons() {
    m_minimize->setIcon(captionIcon(":/resources/icons/minimize.png"));
    m_close->setIcon(captionIcon(":/resources/icons/close.png"));
    setMaximized(m_maximized);
}

void TitleBar::setMaximized(bool maximized) {
    m_maximized = maximized;
    m_maximize->setIcon(captionIcon(maximized ? ":/resources/icons/default_size.png" : ":/resources/icons/maximize.png"));
    m_maximize->setToolTip(maximized ? tr("Restaurer") : tr("Agrandir"));
}

void TitleBar::setActive(bool active) {
    if (property("active").toBool() == active) return;
    setProperty("active", active);
    for (QWidget* w : {static_cast<QWidget*>(this), static_cast<QWidget*>(m_title),
                       static_cast<QWidget*>(m_menuBar), static_cast<QWidget*>(m_minimize),
                       static_cast<QWidget*>(m_maximize), static_cast<QWidget*>(m_close)})
        repolish(w);
}

void TitleBar::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    updateTitleGeometry();
}

bool TitleBar::eventFilter(QObject* watched, QEvent* e) {
    if (e->type() == QEvent::Resize || e->type() == QEvent::Move)
        updateTitleGeometry();
    return QWidget::eventFilter(watched, e);
}

void TitleBar::updateTitleGeometry() {
    const int left  = m_menuBar->geometry().right() + 16;
    const int right = m_rightArea->geometry().left() - 16;
    const int avail = right - left;
    if (m_fullTitle.isEmpty() || avail < 40) { m_title->hide(); return; }

    m_title->ensurePolished();
    const QFontMetrics fm = m_title->fontMetrics();
    const QString text = fm.elidedText(m_fullTitle, Qt::ElideRight, avail);
    const int w = qMin(avail, fm.horizontalAdvance(text) + 4);
    const int x = qBound(left, (width() - w) / 2, right - w);   // centré si possible
    m_title->setText(text);
    m_title->setGeometry(x, 0, w, height());
    m_title->show();
}

#ifndef Q_OS_WIN
void TitleBar::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && isDraggableAt(e->position().toPoint()))
        if (QWindow* h = window()->windowHandle()) h->startSystemMove();
    QWidget::mousePressEvent(e);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && isDraggableAt(e->position().toPoint()))
        emit maximizeRequested();
    QWidget::mouseDoubleClickEvent(e);
}
#endif
