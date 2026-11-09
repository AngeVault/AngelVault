#pragma once
#include "game_settings.h"

#include <QDialog>
#include <QString>

class QLabel;

// Édition > Paramètres du jeu... : plein écran, vsync, antialiasing, FPS max.
// Chaque modification est enregistrée tout de suite dans assets/settings.cfg du projet.
class GameSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit GameSettingsDialog(const QString& path, QWidget* parent = nullptr);

private:
    void save();

    QString m_path;
    GameSettings m_settings;
    QLabel* m_status = nullptr;
};
