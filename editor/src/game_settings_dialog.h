#pragma once
#include "game_settings.h"

#include <QDialog>
#include <QString>

class QFormLayout;
class QLabel;
class QLineEdit;

// Édition > Paramètres du jeu... : fenêtre et post-process (shader + ses réglages @param)
// chaque modification est enregistrée tout de suite dans assets/settings.cfg du projet
class GameSettingsDialog : public QDialog {
    Q_OBJECT
public:
    GameSettingsDialog(const QString& path, const QString& projectDir, QWidget* parent = nullptr);

signals:
    void saved(const GameSettings& settings);   // le jeu lancé dans l'éditeur suit en direct

private:
    void setShader(const QString& relative);
    void rebuildParams();   // un curseur par @param du shader
    void save();

    QString m_path;
    QString m_projectDir;
    GameSettings m_settings;
    QLineEdit* m_shader = nullptr;
    QFormLayout* m_params = nullptr;
    QLabel* m_status = nullptr;
};
