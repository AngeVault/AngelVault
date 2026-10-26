#include "customWindow.h"
#include "titleBar.h"

#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QEvent>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
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
