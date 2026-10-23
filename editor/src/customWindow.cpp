#include "CustomWindow.h"
#include <QApplication>
#include <windows.h>
#include <windowsx.h>

CustomWindow::CustomWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowFlags(Qt::Window | Qt::CustomizeWindowHint);
}

bool CustomWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    MSG* msg = static_cast<MSG*>(message);

    if (msg->message == WM_NCCALCSIZE && msg->wParam == TRUE) {
        *result = 0;
        return true;
    }

    if (msg->message == WM_NCHITTEST) {
        POINT cursor = { GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam) };
        ScreenToClient(msg->hwnd, &cursor);

        int borderX = 15; // On met une zone un peu large (15 pixels) pour tester facilement
        int borderY = 15;

        RECT rect;
        GetClientRect(msg->hwnd, &rect);

        // coin
        if (cursor.y < borderY) {
            if (cursor.x < borderX) { *result = HTTOPLEFT; return true; }
            if (cursor.x >= rect.right - borderX) { *result = HTTOPRIGHT; return true; }
            *result = HTTOP;
            return true;
        }
        if (cursor.y >= rect.bottom - borderY) {
            if (cursor.x < borderX) { *result = HTBOTTOMLEFT; return true; }
            if (cursor.x >= rect.right - borderX) { *result = HTBOTTOMRIGHT; return true; }
            *result = HTBOTTOM;
            return true;
        }

        // bords
        if (cursor.x < borderX) { *result = HTLEFT; return true; }
        if (cursor.x >= rect.right - borderX) { *result = HTRIGHT; return true; }
    }

    return QMainWindow::nativeEvent(eventType, message, result);
}