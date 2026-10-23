#include "customWindow.h"
#include <QMouseEvent>
#include <QApplication>

enum ResizeEdge {
    None = 0,
    Left = 1,
    Right = 2,
    Top = 4,
    Bottom = 8
};

CustomWindow::CustomWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    qApp->installEventFilter(this);
}

bool CustomWindow::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            QPoint localPos = mapFromGlobal(mouseEvent->globalPosition().toPoint());
            QRect r = rect();
            int borderWidth = 12; 

            bool top = localPos.y() < borderWidth;
            bool bottom = localPos.y() >= r.height() - borderWidth;
            bool left = localPos.x() < borderWidth;
            bool right = localPos.x() >= r.width() - borderWidth;

            if (top || bottom || left || right) {
                m_resizing = true;
                m_resizeEdge = ResizeEdge::None;
                if (top) m_resizeEdge |= ResizeEdge::Top;
                if (bottom) m_resizeEdge |= ResizeEdge::Bottom;
                if (left) m_resizeEdge |= ResizeEdge::Left;
                if (right) m_resizeEdge |= ResizeEdge::Right;

                m_startGeometry = geometry();
                m_startMousePos = mouseEvent->globalPosition().toPoint();
                return true;
            }
        }
    }
    else if (event->type() == QEvent::MouseMove) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        QPoint globalPos = mouseEvent->globalPosition().toPoint();

        if (m_resizing) {
            QPoint delta = globalPos - m_startMousePos;
            QRect newGeo = m_startGeometry;

            if (m_resizeEdge & ResizeEdge::Left) {
                int newWidth = m_startGeometry.width() - delta.x();
                if (newWidth >= minimumWidth())
                    newGeo.setLeft(m_startGeometry.left() + delta.x());
            }
            if (m_resizeEdge & ResizeEdge::Right) {
                int newWidth = m_startGeometry.width() + delta.x();
                if (newWidth >= minimumWidth())
                    newGeo.setRight(m_startGeometry.right() + delta.x());
            }
            if (m_resizeEdge & ResizeEdge::Top) {
                int newHeight = m_startGeometry.height() - delta.y();
                if (newHeight >= minimumHeight())
                    newGeo.setTop(m_startGeometry.top() + delta.y());
            }
            if (m_resizeEdge & ResizeEdge::Bottom) {
                int newHeight = m_startGeometry.height() + delta.y();
                if (newHeight >= minimumHeight())
                    newGeo.setBottom(m_startGeometry.bottom() + delta.y());
            }

            setGeometry(newGeo);
            return true;
        }
        else {
            QPoint localPos = mapFromGlobal(globalPos);
            QRect r = rect();
            int borderWidth = 12;

            bool top = localPos.y() < borderWidth;
            bool bottom = localPos.y() >= r.height() - borderWidth;
            bool left = localPos.x() < borderWidth;
            bool right = localPos.x() >= r.width() - borderWidth;

            if ((top && left) || (bottom && right)) setCursor(Qt::SizeFDiagCursor);
            else if ((top && right) || (bottom && left)) setCursor(Qt::SizeBDiagCursor);
            else if (top || bottom) setCursor(Qt::SizeVerCursor);
            else if (left || right) setCursor(Qt::SizeHorCursor);
            else setCursor(Qt::ArrowCursor);
        }
    }
    else if (event->type() == QEvent::MouseButtonRelease) {
        if (m_resizing) {
            m_resizing = false;
            m_resizeEdge = ResizeEdge::None;
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}