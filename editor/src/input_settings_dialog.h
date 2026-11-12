#pragma once
#include "input/input_manager.h"

#include <QDialog>
#include <QPushButton>
#include <functional>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QTableWidget;

// Bouton « touche » : cliquer, puis appuyer sur une touche pour l'enregistrer (Échap : annuler)
class KeyCaptureButton : public QPushButton {
    Q_OBJECT
public:
    explicit KeyCaptureButton(sf::Keyboard::Key key, QWidget* parent = nullptr);
    sf::Keyboard::Key key() const { return m_key; }

signals:
    void keyCaptured(sf::Keyboard::Key key);

protected:
    bool event(QEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;

private:
    void setCapturing(bool on);
    sf::Keyboard::Key m_key;
    bool m_capturing = false;
};

// Fenêtre « Input Manager » (comme Edit > Project Settings > Input Manager dans Unity) :
// liste des actions du projet et de leurs liens (touches, souris, molette, manette).
// Chaque modification est enregistrée tout de suite dans le fichier du projet (assets/input.cfg).
class InputSettingsDialog : public QDialog {
    Q_OBJECT
public:
    InputSettingsDialog(const QString& configPath, std::function<void(InputManager&)> defaults,
                        QWidget* parent = nullptr);

signals:
    void saved();   // le fichier a changé : le jeu en cours peut le recharger

private:
    void rebuildActionList(int select = -1);
    void showAction(int index);
    void rebuildBindings();
    InputAction* currentAction();
    void save();

    QString m_path;
    std::function<void(InputManager&)> m_defaults;
    InputManager m_input;   // copie de travail, écrite dans m_path à chaque modification

    QListWidget* m_list = nullptr;
    QWidget* m_details = nullptr;
    QLineEdit* m_name = nullptr;
    QComboBox* m_type = nullptr;
    QDoubleSpinBox* m_deadZone = nullptr;
    QTableWidget* m_bindings = nullptr;
    QLabel* m_status = nullptr;
    bool m_updating = false;
};
