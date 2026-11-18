#include "customWindow.h"
#include "titleBar.h"

#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QEvent>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPushButton>
#include <QWindow>
#include <QtMath>

#include <algorithm>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <windowsx.h>
#  include <dwmapi.h>
#  include <shellapi.h>

namespace {
// Attributs DWM (valeurs fixes, absentes des vieux SDK). Ignorés sans erreur sous Windows 10.
constexpr DWORD kDwmUseImmersiveDarkMode   = 20;
constexpr DWORD kDwmWindowCornerPreference = 33;
constexpr DWORD kDwmBorderColor            = 34;
constexpr DWORD kCornerRound      = 2;
constexpr DWORD kCornerRoundSmall = 3;

HWND hwndOf(const QWidget* w) { return reinterpret_cast<HWND>(w->internalWinId()); }

// Épaisseur de la bordure de redimensionnement invisible (≈ 8 px à 100 %), en pixels physiques
int frameThickness(HWND hwnd) {
    const UINT dpi = GetDpiForWindow(hwnd);
    return GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
}

// Barre des tâches en masquage automatique sur cet écran ? Renvoie son bord (ABE_*) ou -1
int autoHideTaskbarEdge(HMONITOR monitor) {
    APPBARDATA state{ sizeof(APPBARDATA) };
    if (!(SHAppBarMessage(ABM_GETSTATE, &state) & ABS_AUTOHIDE)) return -1;
    MONITORINFO mi{ sizeof(MONITORINFO) };
    if (!GetMonitorInfoW(monitor, &mi)) return -1;
    for (UINT edge : { ABE_BOTTOM, ABE_TOP, ABE_LEFT, ABE_RIGHT }) {
        APPBARDATA abd{ sizeof(APPBARDATA) };
        abd.uEdge = edge;
        abd.rc = mi.rcMonitor;
        if (SHAppBarMessage(ABM_GETAUTOHIDEBAREX, &abd)) return int(edge);
    }
    return -1;
}

// Menu système Windows (Restaurer / Déplacer / Taille / Réduire / Agrandir / Fermer)
void showSystemMenu(HWND hwnd, int screenX, int screenY) {
    HMENU menu = GetSystemMenu(hwnd, FALSE);
    if (!menu) return;
    const bool maxed = IsZoomed(hwnd);
    auto enable = [menu](UINT id, bool on) {
        MENUITEMINFOW mii{ sizeof(MENUITEMINFOW) };
        mii.fMask = MIIM_STATE;
        mii.fState = on ? MFS_ENABLED : MFS_DISABLED;
        SetMenuItemInfoW(menu, id, FALSE, &mii);
    };
    enable(SC_RESTORE, maxed);
    enable(SC_MOVE, !maxed);
    enable(SC_SIZE, !maxed);
    enable(SC_MINIMIZE, true);
    enable(SC_MAXIMIZE, !maxed);
    enable(SC_CLOSE, true);
    SetMenuDefaultItem(menu, UINT_MAX, FALSE);

    const UINT align = GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN;
    const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | align,
                                    screenX, screenY, 0, hwnd, nullptr);
    if (cmd) PostMessageW(hwnd, WM_SYSCOMMAND, cmd, 0);
}

// Menus déroulants Qt : coins arrondis natifs de Windows 11
class MenuCornerFilter : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject* o, QEvent* e) override {
        if (e->type() == QEvent::Show) {
            if (auto* menu = qobject_cast<QMenu*>(o)) {
                const DWORD pref = kCornerRoundSmall;
                DwmSetWindowAttribute(reinterpret_cast<HWND>(menu->winId()),
                                      kDwmWindowCornerPreference, &pref, sizeof(pref));
            }
        }
        return false;
    }
};
} // namespace

// Les widgets natifs enfants (la vue SFML) reçoivent eux-mêmes WM_NCHITTEST et
// bloqueraient le redimensionnement : sur les bords on renvoie HTTRANSPARENT
// pour que le message remonte jusqu'à la fenêtre principale.
class ChildHitTestFilter : public QAbstractNativeEventFilter {
public:
    explicit ChildHitTestFilter(CustomWindow* w) : m_window(w) {}

    bool nativeEventFilter(const QByteArray& type, void* message, qintptr* result) override {
        if (type != "windows_generic_MSG") return false;
        const MSG* msg = static_cast<const MSG*>(message);
        if (msg->message != WM_NCHITTEST) return false;
        const HWND top = hwndOf(m_window);
        if (!top || msg->hwnd == top || !IsChild(top, msg->hwnd)) return false;
        if (m_window->hitTest(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)) == HTCLIENT)
            return false;
        *result = HTTRANSPARENT;
        return true;
    }

private:
    CustomWindow* m_window;
};
#endif // Q_OS_WIN

#ifndef Q_OS_WIN
namespace {
constexpr int kResizeMargin = 6;
}

// hors windows : bords et barre de titre gérés sur la fenêtre native, qui voit passer toute la souris
// avant les widgets (pas de marge visible, et le vide de la barre de menus déplace aussi la fenêtre)
// posé sur l'appli : la fenêtre native peut être recréée, on compare à celle du moment
// x11 : géométrie suivie à la main, startSystemMove y est capricieux ; wayland : le compositeur s'en charge
class FrameFilter : public QObject {
public:
    explicit FrameFilter(CustomWindow* window) : QObject(window), m_window(window) {}

    bool eventFilter(QObject* watched, QEvent* e) override {
        if (watched != m_window->windowHandle() || !watched) return false;
        switch (e->type()) {
        case QEvent::MouseMove: return mouseMove(static_cast<QMouseEvent*>(e));
        case QEvent::MouseButtonPress: return mousePress(static_cast<QMouseEvent*>(e));
        case QEvent::MouseButtonRelease: return mouseRelease(static_cast<QMouseEvent*>(e));
        case QEvent::MouseButtonDblClick: return doubleClick(static_cast<QMouseEvent*>(e));
        case QEvent::Leave: if (m_drag == Drag::None) setEdgeCursor({}); return false;
        default: return false;
        }
    }

private:
    enum class Drag { None, Move, Resize };

    bool mouseMove(QMouseEvent* e) {
        const QPoint global = e->globalPosition().toPoint();
        if (m_drag == Drag::None) {
            setEdgeCursor(e->buttons() == Qt::NoButton ? edgesAt(m_window->mapFromGlobal(global)) : Qt::Edges{});
            return false;
        }
        const QPoint delta = global - m_pressGlobal;
        if (m_drag == Drag::Move) {
            m_window->move(m_startGeometry.topLeft() + delta);
            return true;
        }
        QRect g = m_startGeometry;
        const QSize min = m_window->minimumSizeHint().expandedTo(m_window->minimumSize()).expandedTo(QSize(200, 120));
        if (m_edges & Qt::LeftEdge) g.setLeft(std::min(g.left() + delta.x(), g.right() - min.width()));
        if (m_edges & Qt::RightEdge) g.setRight(std::max(g.right() + delta.x(), g.left() + min.width()));
        if (m_edges & Qt::TopEdge) g.setTop(std::min(g.top() + delta.y(), g.bottom() - min.height()));
        if (m_edges & Qt::BottomEdge) g.setBottom(std::max(g.bottom() + delta.y(), g.top() + min.height()));
        m_window->setGeometry(g);
        return true;
    }

    bool mousePress(QMouseEvent* e) {
        if (e->button() != Qt::LeftButton) return false;
        const QPoint global = e->globalPosition().toPoint();
        const Qt::Edges edges = edgesAt(m_window->mapFromGlobal(global));
        if (edges) return begin(Drag::Resize, edges, global);
        if (!onTitleBar(global)) return false;
        return begin(Drag::Move, {}, global);
    }

    bool mouseRelease(QMouseEvent* e) {
        if (m_drag == Drag::None || e->button() != Qt::LeftButton) return false;
        m_drag = Drag::None;
        setEdgeCursor(edgesAt(m_window->mapFromGlobal(e->globalPosition().toPoint())));
        return true;
    }

    bool doubleClick(QMouseEvent* e) {
        if (e->button() != Qt::LeftButton || !onTitleBar(e->globalPosition().toPoint())) return false;
        m_drag = Drag::None;
        m_window->toggleMaximized();
        return true;
    }

    bool begin(Drag drag, Qt::Edges edges, const QPoint& global) {
        if (QGuiApplication::platformName().startsWith("wayland")) {
            QWindow* handle = m_window->windowHandle();
            return handle && (drag == Drag::Move ? handle->startSystemMove() : handle->startSystemResize(edges));
        }
        if (drag == Drag::Move && m_window->isMaximized()) {
            // glisser une fenêtre agrandie : elle reprend sa taille sous la souris
            const double ratio = double(m_window->mapFromGlobal(global).x()) / std::max(1, m_window->width());
            m_window->showNormal();
            const QRect normal = m_window->normalGeometry();
            m_window->move(global.x() - int(ratio * normal.width()), global.y() - TitleBar::kHeight / 2);
        }
        m_drag = drag;
        m_edges = edges;
        m_pressGlobal = global;
        m_startGeometry = m_window->geometry();
        return true;
    }

    bool onTitleBar(const QPoint& global) const {
        TitleBar* bar = m_window->titleBar();
        const QPoint local = bar->mapFromGlobal(global);
        return bar->rect().contains(local) && bar->isDraggableAt(local);
    }

    Qt::Edges edgesAt(const QPoint& p) const {
        if (m_window->isMaximized() || m_window->isFullScreen()) return {};
        Qt::Edges edges;
        if (p.x() < kResizeMargin) edges |= Qt::LeftEdge;
        if (p.x() >= m_window->width() - kResizeMargin) edges |= Qt::RightEdge;
        if (p.y() < kResizeMargin) edges |= Qt::TopEdge;
        if (p.y() >= m_window->height() - kResizeMargin) edges |= Qt::BottomEdge;
        return edges;
    }

    // curseur imposé à toute l'appli : sinon le widget sous la souris remet le sien
    void setEdgeCursor(Qt::Edges edges) {
        const bool left = edges & Qt::LeftEdge, right = edges & Qt::RightEdge, top = edges & Qt::TopEdge, bottom = edges & Qt::BottomEdge;
        Qt::CursorShape shape = Qt::ArrowCursor;
        if ((left && top) || (right && bottom)) shape = Qt::SizeFDiagCursor;
        else if ((right && top) || (left && bottom)) shape = Qt::SizeBDiagCursor;
        else if (left || right) shape = Qt::SizeHorCursor;
        else if (top || bottom) shape = Qt::SizeVerCursor;
        const bool want = edges != Qt::Edges{};
        if (want == m_overridden && (!want || QGuiApplication::overrideCursor()->shape() == shape)) return;
        if (m_overridden) QGuiApplication::restoreOverrideCursor();
        if (want) QGuiApplication::setOverrideCursor(shape);
        m_overridden = want;
    }

    CustomWindow* m_window;
    Drag m_drag = Drag::None;
    Qt::Edges m_edges;
    QPoint m_pressGlobal;
    QRect m_startGeometry;
    bool m_overridden = false;
};
#endif

// ---------------------------------------------------------------------------

CustomWindow::CustomWindow(QWidget* parent) : QMainWindow(parent) {
    setObjectName("MainWindow");
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint
                   | Qt::WindowMinMaxButtonsHint);

    m_titleBar = new TitleBar(this);
    setMenuWidget(m_titleBar);

    connect(m_titleBar, &TitleBar::minimizeRequested, this, &CustomWindow::minimize);
    connect(m_titleBar, &TitleBar::maximizeRequested, this, &CustomWindow::toggleMaximized);
    connect(m_titleBar, &TitleBar::closeRequested,    this, &QWidget::close);
    connect(this, &QWidget::windowTitleChanged, m_titleBar, &TitleBar::setTitle);
    connect(this, &QWidget::windowIconChanged,  m_titleBar, &TitleBar::setIcon);

#ifdef Q_OS_WIN
    winId();   // crée la fenêtre native maintenant
    applyNativeFrame();

    m_childFilter = new ChildHitTestFilter(this);
    qApp->installNativeEventFilter(m_childFilter);

    static MenuCornerFilter* menuFilter = nullptr;
    if (!menuFilter) {
        menuFilter = new MenuCornerFilter(qApp);
        qApp->installEventFilter(menuFilter);
    }
#else
    qApp->installEventFilter(new FrameFilter(this));
#endif
}

CustomWindow::~CustomWindow() {
#ifdef Q_OS_WIN
    qApp->removeNativeEventFilter(m_childFilter);
    delete m_childFilter;
#endif
}

QMenuBar* CustomWindow::menuBar() const { return m_titleBar->menuBar(); }
QMenu* CustomWindow::addMenu(const QString& title) { return m_titleBar->addMenu(title); }

void CustomWindow::toggleMaximized() {
#ifdef Q_OS_WIN
    // Passe par le système : animation et comportement identiques à une fenêtre Windows
    PostMessageW(hwndOf(this), WM_SYSCOMMAND, IsZoomed(hwndOf(this)) ? SC_RESTORE : SC_MAXIMIZE, 0);
#else
    isMaximized() ? showNormal() : showMaximized();
#endif
}

void CustomWindow::minimize() {
#ifdef Q_OS_WIN
    PostMessageW(hwndOf(this), WM_SYSCOMMAND, SC_MINIMIZE, 0);
#else
    showMinimized();
#endif
}

void CustomWindow::setBorderColor(const QColor& color) {
    if (!color.isValid() || color == m_borderColor) return;
    m_borderColor = color;
#ifdef Q_OS_WIN
    if (const HWND hwnd = hwndOf(this)) {
        const COLORREF border = RGB(color.red(), color.green(), color.blue());
        DwmSetWindowAttribute(hwnd, kDwmBorderColor, &border, sizeof(border));
    }
#endif
}

void CustomWindow::changeEvent(QEvent* e) {
    if (e->type() == QEvent::WindowStateChange) {
        m_titleBar->setMaximized(isMaximized());
#ifdef Q_OS_WIN
        // La fenêtre a bougé sous la souris : on n'aura pas de WM_NCMOUSELEAVE
        setMaximizeHovered(false);
        setMaximizePressed(false);
        updateMaximizedMargins();
#endif
    } else if (e->type() == QEvent::ActivationChange) {
        m_titleBar->setActive(isActiveWindow());
    }
    QMainWindow::changeEvent(e);
}

void CustomWindow::showEvent(QShowEvent* e) {
#ifdef Q_OS_WIN
    // Qt peut réécrire le style de la fenêtre (changement de flags...) : on le remet si besoin
    if (!(GetWindowLongPtrW(hwndOf(this), GWL_STYLE) & WS_THICKFRAME))
        applyNativeFrame();
#endif
    QMainWindow::showEvent(e);
}

#ifdef Q_OS_WIN

void CustomWindow::applyNativeFrame() {
    const HWND hwnd = hwndOf(this);
    if (!hwnd) return;

    // Styles d'une fenêtre classique : c'est ce qui donne Aero Snap, les animations,
    // Win+flèches et l'ombre. La barre visible est ensuite retirée par WM_NCCALCSIZE.
    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    SetWindowLongPtrW(hwnd, GWL_STYLE,
                      style | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

    // Garde l'ombre DWM. Uniquement en bas : une marge en haut fait dessiner par DWM
    // les anciens boutons Windows (fantômes) par-dessus la barre de titre.
    const MARGINS shadow{ 0, 0, 0, 1 };
    DwmExtendFrameIntoClientArea(hwnd, &shadow);

    const BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkMode, &dark, sizeof(dark));
    const DWORD corner = kCornerRound;
    DwmSetWindowAttribute(hwnd, kDwmWindowCornerPreference, &corner, sizeof(corner));
    const COLORREF border = RGB(m_borderColor.red(), m_borderColor.green(), m_borderColor.blue());
    DwmSetWindowAttribute(hwnd, kDwmBorderColor, &border, sizeof(border));

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
}

// Une fenêtre Windows agrandie déborde de l'écran de l'épaisseur de son cadre invisible :
// on décale le contenu d'autant pour qu'il reste entièrement visible.
void CustomWindow::updateMaximizedMargins() {
    const HWND hwnd = hwndOf(this);
    if (!hwnd) return;
    QMargins m;
    if (IsZoomed(hwnd)) {
        RECT wr;
        GetWindowRect(hwnd, &wr);
        MONITORINFO mi{ sizeof(MONITORINFO) };
        GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        const qreal dpr = devicePixelRatioF();
        auto px = [dpr](LONG v) { return qMax(0, qCeil(v / dpr)); };
        m = QMargins(px(mi.rcWork.left - wr.left), px(mi.rcWork.top - wr.top),
                     px(wr.right - mi.rcWork.right), px(wr.bottom - mi.rcWork.bottom));
    }
    if (contentsMargins() != m) setContentsMargins(m);
}

int CustomWindow::hitTest(int screenX, int screenY) const {
    const HWND hwnd = hwndOf(this);
    POINT pt{ screenX, screenY };
    ScreenToClient(hwnd, &pt);
    RECT rc;
    GetClientRect(hwnd, &rc);

    // 1) Bords et coins de redimensionnement
    if (!IsZoomed(hwnd)) {
        const int b = frameThickness(hwnd);
        const int corner = b * 2;   // coins un peu plus larges, plus faciles à attraper
        const bool top = pt.y < b, bottom = pt.y >= rc.bottom - b;
        const bool left = pt.x < b, right = pt.x >= rc.right - b;
        const bool nearLeft = pt.x < corner, nearRight = pt.x >= rc.right - corner;
        const bool nearTop = pt.y < corner, nearBottom = pt.y >= rc.bottom - corner;
        if ((top && nearLeft) || (left && nearTop))         return HTTOPLEFT;
        if ((top && nearRight) || (right && nearTop))       return HTTOPRIGHT;
        if ((bottom && nearLeft) || (left && nearBottom))   return HTBOTTOMLEFT;
        if ((bottom && nearRight) || (right && nearBottom)) return HTBOTTOMRIGHT;
        if (top)    return HTTOP;
        if (bottom) return HTBOTTOM;
        if (left)   return HTLEFT;
        if (right)  return HTRIGHT;
    }

    // 2) Barre de titre
    if (m_titleBar && m_titleBar->isVisible()) {
        const qreal dpr = devicePixelRatioF();
        const QPoint local(qFloor(pt.x / dpr), qFloor(pt.y / dpr));
        const QPoint p = m_titleBar->mapFrom(this, local);
        if (m_titleBar->rect().contains(p)) {
            if (m_titleBar->isMaximizeButtonAt(p)) return HTMAXBUTTON;   // => Snap Layouts Windows 11
            if (m_titleBar->isDraggableAt(p))      return HTCAPTION;
        }
    }
    return HTCLIENT;
}

void CustomWindow::setMaximizeHovered(bool hovered) {
    if (m_maxHovered == hovered) return;
    m_maxHovered = hovered;
    QPushButton* b = m_titleBar->maximizeButton();
    b->setAttribute(Qt::WA_UnderMouse, hovered);   // active le :hover du QSS
    b->update();
}

void CustomWindow::setMaximizePressed(bool pressed) {
    m_maxPressed = pressed;
    m_titleBar->maximizeButton()->setDown(pressed);
}

bool CustomWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    if (eventType != "windows_generic_MSG")
        return QMainWindow::nativeEvent(eventType, message, result);

    MSG* msg = static_cast<MSG*>(message);
    const HWND hwnd = msg->hwnd;

    switch (msg->message) {
    case WM_NCCALCSIZE:
        // Toute la fenêtre devient zone client : plus de barre de titre Windows
        if (msg->wParam == TRUE) { *result = 0; return true; }
        break;

    case WM_NCACTIVATE:
        // Empêche Windows de redessiner son ancienne barre lors du changement de focus
        *result = DefWindowProcW(hwnd, msg->message, msg->wParam, -1);
        return true;

    case WM_NCHITTEST:
        *result = hitTest(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
        return true;

    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(msg->lParam);
        const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{ sizeof(MONITORINFO) };
        if (GetMonitorInfoW(monitor, &mi)) {
            // Agrandie = zone de travail (sans la barre des tâches) + débordement du cadre
            const int f = frameThickness(hwnd);
            const RECT& w = mi.rcWork;
            const RECT& s = mi.rcMonitor;
            mmi->ptMaxPosition = { w.left - s.left - f, w.top - s.top - f };
            mmi->ptMaxSize     = { (w.right - w.left) + 2 * f, (w.bottom - w.top) + 2 * f };
            // Barre des tâches auto-masquée : laisser 1 px pour qu'elle puisse réapparaître
            switch (autoHideTaskbarEdge(monitor)) {
            case ABE_BOTTOM: mmi->ptMaxSize.y -= 1; break;
            case ABE_TOP:    mmi->ptMaxPosition.y += 1; mmi->ptMaxSize.y -= 1; break;
            case ABE_LEFT:   mmi->ptMaxPosition.x += 1; mmi->ptMaxSize.x -= 1; break;
            case ABE_RIGHT:  mmi->ptMaxSize.x -= 1; break;
            default: break;
            }
            mmi->ptMaxTrackSize = { std::max(mmi->ptMaxTrackSize.x, mmi->ptMaxSize.x),
                                    std::max(mmi->ptMaxTrackSize.y, mmi->ptMaxSize.y) };
        }
        // Taille minimale/maximale définie côté Qt (setMinimumSize...)
        const qreal dpr = devicePixelRatioF();
        const QSize minS = minimumSize().expandedTo(minimumSizeHint());
        mmi->ptMinTrackSize = { LONG(qCeil(minS.width() * dpr)), LONG(qCeil(minS.height() * dpr)) };
        if (maximumWidth() < QWIDGETSIZE_MAX)  mmi->ptMaxTrackSize.x = LONG(maximumWidth() * dpr);
        if (maximumHeight() < QWIDGETSIZE_MAX) mmi->ptMaxTrackSize.y = LONG(maximumHeight() * dpr);
        *result = 0;
        return true;
    }

    case WM_SIZE:
    case WM_DPICHANGED:
        updateMaximizedMargins();
        break;

    // --- Bouton agrandir déclaré en HTMAXBUTTON (Snap Layouts) : on gère survol et clic ---
    case WM_NCMOUSEMOVE:
        setMaximizeHovered(msg->wParam == HTMAXBUTTON);
        if (msg->wParam == HTMAXBUTTON) {
            TRACKMOUSEEVENT tme{ sizeof(TRACKMOUSEEVENT), TME_LEAVE | TME_NONCLIENT, hwnd, 0 };
            TrackMouseEvent(&tme);
        }
        break;

    case WM_NCMOUSELEAVE:
        setMaximizeHovered(false);
        if (m_maxPressed) setMaximizePressed(false);
        break;

    case WM_MOUSEMOVE:
        if (m_maxHovered) setMaximizeHovered(false);
        break;

    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK:
        if (msg->wParam == HTMAXBUTTON) {
            setMaximizePressed(true);
            *result = 0;
            return true;   // sinon Windows dessine son vieux bouton
        }
        break;

    case WM_NCLBUTTONUP:
        if (msg->wParam == HTMAXBUTTON) {
            const bool click = m_maxPressed;
            setMaximizePressed(false);
            if (click) toggleMaximized();
            *result = 0;
            return true;
        }
        if (m_maxPressed) setMaximizePressed(false);
        break;

    case WM_NCRBUTTONUP:
        if (msg->wParam == HTCAPTION) {
            showSystemMenu(hwnd, GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
            *result = 0;
            return true;
        }
        break;

    default:
        break;
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}

#else // !Q_OS_WIN

bool CustomWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    return QMainWindow::nativeEvent(eventType, message, result);
}

#endif
