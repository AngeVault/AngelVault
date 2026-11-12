#pragma once
#include <QString>
#include <QStringList>

class QFileInfo;
class QWidget;

// Ouvrir des fichiers dans les outils externes : CLion, VS Code, explorateur de fichiers.
// Les exécutables sont trouvés automatiquement (emplacements d'installation habituels + PATH) ;
// on peut aussi les choisir à la main, le choix est mémorisé (QSettings, clé "tools/...").
namespace ExternalTools {

enum class CodeEditor { VSCode, CLion };

QString displayName(CodeEditor editor);
QString executable(CodeEditor editor);                         // vide si introuvable
void setExecutable(CodeEditor editor, const QString& path);

// Ouvre des fichiers / dossiers. Si l'éditeur est introuvable, propose de choisir l'exécutable.
bool openWith(CodeEditor editor, const QStringList& paths, QWidget* parent);

// Éditeur utilisé au double-clic sur un script (mémorisé). Par défaut : VS Code, sinon CLion.
CodeEditor preferredEditor();
void setPreferredEditor(CodeEditor editor);
void openScript(const QString& path, QWidget* parent);         // éditeur préféré, sinon l'appli par défaut

void revealInFileManager(const QString& path);                 // explorateur, fichier sélectionné
bool isScript(const QFileInfo& file);                          // .h .hpp .cpp .c .inl ...
}
