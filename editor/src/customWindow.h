#pragma once
#include <QMainWindow>
#include <QRect>
#include <QPoint>

class CustomWindow : public QMainWindow {
    Q_OBJECT
public:
    CustomWindow(QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool m_resizing = false;
    int m_resizeEdge = 0;
    QRect m_startGeometry;
    QPoint m_startMousePos;
};