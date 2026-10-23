#pragma once
#include <QMainWindow>

class CustomWindow : public QMainWindow {
    Q_OBJECT

public:
    CustomWindow(QWidget* parent = nullptr);

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
};