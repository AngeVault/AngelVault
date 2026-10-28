#include "external_tools.h"

#include <QCollator>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>

namespace ExternalTools {
namespace {

QString settingsKey(CodeEditor e) { return e == CodeEditor::VSCode ? "tools/vscode" : "tools/clion"; }

QString env(const char* name) { return QProcessEnvironment::systemEnvironment().value(name); }

// Premier fichier existant ; les jokers (*) dans un nom de dossier prennent la version la plus récente
QString firstExisting(const QStringList& candidates) {
    for (const QString& raw : candidates) {
        if (raw.isEmpty()) continue;
        const QString c = QDir::fromNativeSeparators(raw);   // %ProgramFiles% contient des "\"
        const int star = c.indexOf('*');
        if (star < 0) {
            if (QFileInfo(c).isFile()) return QDir::cleanPath(c);
            continue;
        }
        const QString base = c.left(c.lastIndexOf('/', star));
        const QString pattern = c.mid(base.size() + 1).section('/', 0, 0);
        const QString rest = c.mid(base.size() + 1 + pattern.size());
        QStringList dirs = QDir(base).entryList({ pattern }, QDir::Dirs | QDir::NoDotAndDotDot);
        QCollator collator;
        collator.setNumericMode(true);
        std::sort(dirs.begin(), dirs.end(), [&](const QString& a, const QString& b) { return collator.compare(a, b) > 0; });
        for (const QString& d : dirs) {
            const QString found = firstExisting({ base + "/" + d + rest });
            if (!found.isEmpty()) return found;
        }
    }
    return {};
}

QString detectVSCode() {
#if defined(Q_OS_WIN)
    QString found = firstExisting({
        env("LOCALAPPDATA") + "/Programs/Microsoft VS Code/Code.exe",
        env("ProgramFiles") + "/Microsoft VS Code/Code.exe",
        env("ProgramFiles(x86)") + "/Microsoft VS Code/Code.exe",
    });
    if (found.isEmpty()) {
        // "code" dans le PATH : c'est bin/code.cmd, l'exécutable est un dossier au-dessus
        const QString cmd = QStandardPaths::findExecutable("code");
        if (!cmd.isEmpty()) {
            const QString exe = QFileInfo(cmd).absoluteDir().filePath("../Code.exe");
            found = QFileInfo(exe).isFile() ? QDir::cleanPath(exe) : cmd;
        }
    }
    return found;
#elif defined(Q_OS_MACOS)
    const QString found = firstExisting({ "/Applications/Visual Studio Code.app/Contents/Resources/app/bin/code" });
    return found.isEmpty() ? QStandardPaths::findExecutable("code") : found;
#else
    return QStandardPaths::findExecutable("code");
#endif
}

QString detectCLion() {
#if defined(Q_OS_WIN)
    QString found = firstExisting({
        env("ProgramFiles") + "/JetBrains/CLion*/bin/clion64.exe",
        env("LOCALAPPDATA") + "/Programs/CLion*/bin/clion64.exe",              // Toolbox (versions récentes)
        env("LOCALAPPDATA") + "/JetBrains/Toolbox/apps/CLion/*/*/bin/clion64.exe",
        env("LOCALAPPDATA") + "/JetBrains/Toolbox/scripts/clion.cmd",
    });
    if (found.isEmpty()) found = QStandardPaths::findExecutable("clion64");
    if (found.isEmpty()) found = QStandardPaths::findExecutable("clion");
    return found;
#elif defined(Q_OS_MACOS)
    const QString found = firstExisting({ "/Applications/CLion.app/Contents/MacOS/clion" });
    return found.isEmpty() ? QStandardPaths::findExecutable("clion") : found;
#else
    QString found = QStandardPaths::findExecutable("clion");
    if (found.isEmpty()) found = QStandardPaths::findExecutable("clion.sh");
    if (found.isEmpty())
        found = firstExisting({ QDir::homePath() + "/.local/share/JetBrains/Toolbox/apps/clion/bin/clion.sh",
                                "/opt/clion*/bin/clion.sh", "/snap/bin/clion" });
    return found;
#endif
}

bool launch(const QString& exe, const QStringList& args) {
#ifdef Q_OS_WIN
    // Les .cmd / .bat (scripts Toolbox, code.cmd) doivent passer par cmd.exe
    if (exe.endsWith(".cmd", Qt::CaseInsensitive) || exe.endsWith(".bat", Qt::CaseInsensitive))
        return QProcess::startDetached("cmd.exe", QStringList{ "/c", QDir::toNativeSeparators(exe) } + args);
#endif
    return QProcess::startDetached(exe, args);
}
} // namespace

QString displayName(CodeEditor e) { return e == CodeEditor::VSCode ? QStringLiteral("VS Code") : QStringLiteral("CLion"); }

QString executable(CodeEditor e) {
    const QString custom = QSettings().value(settingsKey(e)).toString();
    if (!custom.isEmpty() && QFileInfo(custom).isFile()) return custom;
    static QString cache[2];
    static bool searched[2] = {};
    const int i = int(e);
    if (!searched[i]) {                 // recherche faite une seule fois par session
        cache[i] = e == CodeEditor::VSCode ? detectVSCode() : detectCLion();
        searched[i] = true;
    }
    return cache[i];
}

void setExecutable(CodeEditor e, const QString& path) { QSettings().setValue(settingsKey(e), path); }

bool openWith(CodeEditor e, const QStringList& paths, QWidget* parent) {
    QString exe = executable(e);
    if (exe.isEmpty()) {
        const auto answer = QMessageBox::question(
            parent, QObject::tr("%1 introuvable").arg(displayName(e)),
            QObject::tr("%1 n'a pas été trouvé automatiquement.\nVoulez-vous indiquer où se trouve son exécutable ?")
                .arg(displayName(e)));
        if (answer != QMessageBox::Yes) return false;
#ifdef Q_OS_WIN
        const QString filter = QObject::tr("Programmes (*.exe *.cmd *.bat)");
#else
        const QString filter;
#endif
        exe = QFileDialog::getOpenFileName(parent, QObject::tr("Exécutable de %1").arg(displayName(e)), QString(), filter);
        if (exe.isEmpty()) return false;
        setExecutable(e, exe);
    }
    QStringList args;
    for (const QString& p : paths) args << QDir::toNativeSeparators(p);
    if (!launch(exe, args)) {
        QMessageBox::warning(parent, QObject::tr("Impossible d'ouvrir"),
                             QObject::tr("Le lancement de %1 a échoué :\n%2").arg(displayName(e), QDir::toNativeSeparators(exe)));
        return false;
    }
    return true;
}

CodeEditor preferredEditor() {
    const QString saved = QSettings().value("tools/preferredEditor").toString();
    if (saved == "clion") return CodeEditor::CLion;
    if (saved == "vscode") return CodeEditor::VSCode;
    return !executable(CodeEditor::VSCode).isEmpty() || executable(CodeEditor::CLion).isEmpty()
               ? CodeEditor::VSCode : CodeEditor::CLion;
}

void setPreferredEditor(CodeEditor e) {
    QSettings().setValue("tools/preferredEditor", e == CodeEditor::CLion ? "clion" : "vscode");
}

void openScript(const QString& path, QWidget* parent) {
    const CodeEditor e = preferredEditor();
    if (!executable(e).isEmpty()) { openWith(e, { path }, parent); return; }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));   // aucun éditeur trouvé : application par défaut
}

void revealInFileManager(const QString& path) {
    const QFileInfo fi(path);
#if defined(Q_OS_WIN)
    if (fi.isFile()) {
        QProcess::startDetached("explorer.exe", { "/select,", QDir::toNativeSeparators(fi.absoluteFilePath()) });
        return;
    }
#elif defined(Q_OS_MACOS)
    QProcess::startDetached("open", { "-R", fi.absoluteFilePath() });
    return;
#endif
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath()));
}

bool isScript(const QFileInfo& file) {
    static const QStringList ext = { "h", "hpp", "hh", "hxx", "c", "cpp", "cc", "cxx", "inl", "ipp" };
    return ext.contains(file.suffix().toLower());
}
} // namespace ExternalTools
