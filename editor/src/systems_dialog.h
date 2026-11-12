#pragma once
#include <QDialog>
#include <QString>
#include <vector>

class QTableWidget;

// Exécuter > Systèmes... : la liste des systèmes du jeu (moteur + jeu), dans l'ordre d'exécution,
// avec une case pour activer / désactiver chacun. Le choix est enregistré dans le .angproj
// (clé "systems") et appliqué au lancement (F5, rpg.exe) — et tout de suite si le jeu tourne.
class SystemsDialog : public QDialog {
    Q_OBJECT
public:
    struct Row {
        QString name;
        int priority = 0;
        bool fromGame = false;   // déclaré dans le projet (REGISTER_SYSTEM) plutôt que dans le moteur
        bool enabled = true;
    };
    explicit SystemsDialog(const std::vector<Row>& rows, QWidget* parent = nullptr);

signals:
    void systemToggled(const QString& name, bool enabled);

private:
    QTableWidget* m_table = nullptr;
};
