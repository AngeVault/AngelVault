#include "theme_manager.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>

namespace {
const char* kDefaultTheme = ":/resources/style/appstyles.qss";
const char* kSettingsKey = "ui/theme";
const char* kLastDirKey = "ui/themeDir";
}

ThemeManager::ThemeManager(QWidget* dialogParent) : QObject(dialogParent), m_parent(dialogParent) {
    m_builtins = {
        { tr("Sombre"),    kDefaultTheme },
        { tr("Néon rose"), ":/resources/style/neon_pink.qss" },
        { tr("Néon clair"), ":/resources/style/neon_light.qss" },
        { tr("Sakura clair"), ":/resources/style/sakura_light.qss" },
        { tr("Sakura sombre"), ":/resources/style/sakura_dark.qss" },
        { tr("Sakura chat (image)"), ":/resources/style/sakura_cat.qss" },
    };
}

QMenu* ThemeManager::createMenu(QWidget* parent) {
    auto* menu = new QMenu(tr("Thème"), parent);
    m_group = new QActionGroup(menu);
    m_group->setExclusive(true);

    for (Builtin& theme : m_builtins) {
        theme.action = menu->addAction(theme.name);
        theme.action->setCheckable(true);
        m_group->addAction(theme.action);
        const QString path = theme.path;
        connect(theme.action, &QAction::triggered, this, [this, path] { apply(path); });
    }

    m_customAction = menu->addAction(QString());
    m_customAction->setCheckable(true);
    m_customAction->setVisible(false);
    m_group->addAction(m_customAction);
    connect(m_customAction, &QAction::triggered, this, [this] { reload(); });

    menu->addSeparator();
    connect(menu->addAction(tr("Charger un fichier QSS...")), &QAction::triggered, this, &ThemeManager::chooseFile);
    QAction* reloadAction = menu->addAction(tr("Recharger le thème"));
    reloadAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
    connect(reloadAction, &QAction::triggered, this, &ThemeManager::reload);

    updateChecks();
    return menu;
}

bool ThemeManager::apply(const QString& path, bool showErrors) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (showErrors)
            QMessageBox::warning(m_parent, tr("Thème introuvable"),
                                 tr("Impossible d'ouvrir la feuille de style :\n%1").arg(path));
        updateChecks();   // remet la coche sur le thème réellement actif
        return false;
    }
    qApp->setStyleSheet(QString::fromUtf8(file.readAll()));
    m_current = path;
    QSettings().setValue(kSettingsKey, path);
    updateChecks();
    return true;
}

void ThemeManager::restoreSaved() {
    const QString saved = QSettings().value(kSettingsKey, kDefaultTheme).toString();
    if (!apply(saved, false)) apply(kDefaultTheme, false);
}

void ThemeManager::reload() {
    apply(m_current.isEmpty() ? QString(kDefaultTheme) : m_current);
}

void ThemeManager::chooseFile() {
    QSettings settings;
    const QString path = QFileDialog::getOpenFileName(
        m_parent, tr("Charger un thème"), settings.value(kLastDirKey).toString(),
        tr("Feuilles de style Qt (*.qss);;Tous les fichiers (*)"));
    if (path.isEmpty()) { updateChecks(); return; }
    settings.setValue(kLastDirKey, QFileInfo(path).absolutePath());
    apply(path);
}

void ThemeManager::updateChecks() {
    if (!m_group) return;
    bool builtin = false;
    for (const Builtin& theme : m_builtins) {
        const bool active = theme.path == m_current;
        if (theme.action) theme.action->setChecked(active);
        builtin |= active;
    }
    const bool custom = !builtin && !m_current.isEmpty();
    m_customAction->setVisible(custom);
    if (custom) {
        m_customAction->setText(tr("Personnalisé : %1").arg(QFileInfo(m_current).fileName()));
        m_customAction->setToolTip(m_current);
        m_customAction->setChecked(true);
    }
}
