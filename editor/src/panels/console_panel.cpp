#include "console_panel.h"

#include <QCheckBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QRegularExpression>
#include <QToolButton>
#include <QVBoxLayout>

#include <SFML/System/Err.hpp>

#include <algorithm>
#include <iostream>
#include <mutex>
#include <streambuf>
#include <string>

/**
 * @file console_panel.cpp
 * @brief Capture, filtrage et affichage des messages de log dans le panneau de console de l'éditeur.
 *
 * Ce fichier intercepte les flux C++/Qt/SFML, les reformate en lignes, puis les affiche
 * dans une liste Qt avec filtres par niveau et recherche textuelle.
 */

namespace {
/**
 * @brief Rôle Qt utilisé pour stocker le niveau de log d'un élément de liste.
 */
constexpr int kLevelRole = Qt::UserRole + 1;

/**
 * @brief Nombre maximal de lignes conservées dans la console.
 */
constexpr int kMaxLines = 5000;

/**
 * @brief Tampon de flux chargé de découper les sorties texte en lignes et
 *        de les re-routées vers le système de capture de logs.
 *
 * L'objectif est de capturer les flux standard (`std::cout`, `std::cerr`)
 * et les messages SFML, puis de les présenter comme des messages de console
 * avec un niveau d'importance (info, warning, error).
 */
class LineBuffer : public std::streambuf {
public:
    /**
     * @brief Constructeur.
     * @param level Niveau de log associé à ce flux.
     * @param forward Flux d'origine à conserver pour rediriger la sortie
     *        vers la console IDE / terminal.
     */
    LineBuffer(LogLevel level, std::streambuf* forward) : m_level(level), m_forward(forward) {}

    /**
     * @brief Retourne le flux de redirection original.
     */
    std::streambuf* forward() const { return m_forward; }

protected:
    /**
     * @brief Traite un caractère unique.
     *
     * Si un retour à la ligne est détecté, la ligne est flushée vers le système
     * de capture de log. Sinon, le caractère est accumulé dans le buffer courant.
     */
    int overflow(int ch) override {
        if (ch == traits_type::eof()) return traits_type::not_eof(ch);
        std::lock_guard lock(m_mutex);
        if (m_forward) m_forward->sputc(char(ch));
        if (ch == '\n') flushLine();
        else m_line.push_back(char(ch));
        return ch;
    }

    /**
     * @brief Traite un bloc de texte.
     *
     * Chaque séquence de caractères est scannée ; à chaque fin de ligne, la ligne
     * courante est envoyée au système de log.
     */
    std::streamsize xsputn(const char* s, std::streamsize n) override {
        std::lock_guard lock(m_mutex);
        if (m_forward) m_forward->sputn(s, n);
        for (std::streamsize i = 0; i < n; ++i) {
            if (s[i] == '\n') flushLine();
            else m_line.push_back(s[i]);
        }
        return n;
    }

    /**
     * @brief Force l'envoi immédiat du tampon courant vers le moteur de log.
     */
    int sync() override {
        std::lock_guard lock(m_mutex);
        if (m_forward) m_forward->pubsync();
        return 0;
    }

private:
    /**
     * @brief Envoie la ligne accumulée dans le buffer vers `LogCapture`.
     *
     * La ligne est convertie en `QString`, un niveau est déterminé, puis elle est
     * publiée via le singleton de capture.
     */
    void flushLine() {
        if (!m_line.empty() && m_line.back() == '\r') m_line.pop_back();
        LogLevel level = m_level;
        const QString text = QString::fromUtf8(m_line.data(), qsizetype(m_line.size()));

        // Messages d'échec écrits sur std::cout : affichés comme avertissements
        static const QRegularExpression warn(QStringLiteral("fail|couldn't|cannot|impossible|introuvable|warning|attention"),
                                             QRegularExpression::CaseInsensitiveOption);
        if (level == LogLevel::Info && text.contains(warn)) level = LogLevel::Warning;

        LogCapture::instance().post(level, text);
        m_line.clear();
    }

    LogLevel m_level;          ///< Niveau de log associé au flux.
    std::streambuf* m_forward; ///< Flux d'origine pour la redirection.
    std::string m_line;        ///< Ligne courante en construction.
    std::mutex m_mutex;        ///< Protection pour les accès concurrents.
};

/**
 * @brief Flux de sortie standard capturé.
 */
LineBuffer* g_out = nullptr;

/**
 * @brief Flux d'erreur standard capturé.
 */
LineBuffer* g_err = nullptr;

/**
 * @brief Flux d'erreur SFML capturé.
 */
LineBuffer* g_sfErr = nullptr;

/**
 * @brief Gestionnaire de message Qt précédent, conservé pour le restaurer lors du désinstallation.
 */
QtMessageHandler g_previousHandler = nullptr;

/**
 * @brief Callback de capture des messages Qt.
 *
 * Convertit le type de message Qt (`warning`, `critical`, etc.) dans le niveau interne
 * de l'application, puis le publie au système de capture avant de redéclencher le
 * handler Qt enregistré précédemment.
 */
void qtHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
    LogLevel level = LogLevel::Info;
    if (type == QtWarningMsg) level = LogLevel::Warning;
    else if (type == QtCriticalMsg || type == QtFatalMsg) level = LogLevel::Error;
    LogCapture::instance().post(level, msg);
    if (g_previousHandler) g_previousHandler(type, ctx, msg);
}
}

// ---------------------------------------------------------------------------- LogCapture

/**
 * @brief Retourne l'instance unique du gestionnaire de logs.
 *
 * Cette instance centralise la publication des messages vers le panneau de console.
 */
LogCapture& LogCapture::instance() {
    static LogCapture capture;
    return capture;
}

/**
 * @brief Installe les redirections de sortie pour capter les logs.
 *
 * - `std::cout` et `std::cerr`
 * - `sf::err()`
 * - messages Qt via `qInstallMessageHandler`
 */
void LogCapture::install() {
    if (m_installed) return;
    m_installed = true;
    g_out = new LineBuffer(LogLevel::Info, std::cout.rdbuf());
    g_err = new LineBuffer(LogLevel::Error, std::cerr.rdbuf());
    g_sfErr = new LineBuffer(LogLevel::Error, sf::err().rdbuf());
    std::cout.rdbuf(g_out);
    std::cerr.rdbuf(g_err);
    sf::err().rdbuf(g_sfErr);   // erreurs SFML ("Failed to load image"...)
    g_previousHandler = qInstallMessageHandler(qtHandler);
}

/**
 * @brief Restaure les flux standards et le gestionnaire Qt d'origine.
 */
void LogCapture::uninstall() {
    if (!m_installed) return;
    m_installed = false;
    std::cout.flush();
    std::cerr.flush();
    std::cout.rdbuf(g_out->forward());
    std::cerr.rdbuf(g_err->forward());
    sf::err().rdbuf(g_sfErr->forward());
    qInstallMessageHandler(g_previousHandler);
    delete g_out; delete g_err; delete g_sfErr;
    g_out = g_err = g_sfErr = nullptr;
}

/**
 * @brief Publie un message de log dans la file de messages de l'interface.
 *
 * Le signal est envoyé avec `Qt::QueuedConnection` pour garantir la sécurité sur
 * les threads et éviter les accès directs depuis un contexte non-GUI.
 *
 * @param level Niveau du message.
 * @param text Texte à afficher.
 */
void LogCapture::post(LogLevel level, const QString& text) {
    const QString time = QTime::currentTime().toString("HH:mm:ss");
    // Signal en file d'attente : sûr même depuis un autre thread
    QMetaObject::invokeMethod(this, [this, level, text, time] { emit message(int(level), text, time); },
                              Qt::QueuedConnection);
}

// ---------------------------------------------------------------------------- ConsolePanel

/**
 * @brief Constructeur du panneau de console.
 *
 * Crée la barre d'outils, les filtres par niveau, la zone de recherche et
 * la liste affichant les messages.
 */
ConsolePanel::ConsolePanel(QWidget* parent) : QWidget(parent) {
    setObjectName("consolePanel");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    auto* bar = new QHBoxLayout;
    bar->setSpacing(4);

    auto* clear = new QToolButton;
    clear->setObjectName("panelToolButton");
    clear->setText(tr("Effacer"));
    bar->addWidget(clear);
    bar->addSpacing(6);

    const char* names[3] = { "consoleInfo", "consoleWarning", "consoleError" };
    for (int i = 0; i < 3; ++i) {
        m_levels[i] = new QToolButton;
        m_levels[i]->setObjectName(names[i]);
        m_levels[i]->setCheckable(true);
        m_levels[i]->setChecked(true);
        bar->addWidget(m_levels[i]);
        connect(m_levels[i], &QToolButton::toggled, this, [this] { applyFilter(); });
    }
    bar->addStretch();

    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Filtrer..."));
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(220);

    m_autoScroll = new QCheckBox(tr("Défilement auto"));
    m_autoScroll->setChecked(true);

    bar->addWidget(m_search);
    bar->addWidget(m_autoScroll);
    layout->addLayout(bar);

    m_list = new QListWidget;
    m_list->setObjectName("consoleList");
    m_list->setUniformItemSizes(true);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setWordWrap(false);

    QFont mono("Consolas");
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSize(9);
    m_list->setFont(mono);
    layout->addWidget(m_list, 1);

    connect(clear, &QToolButton::clicked, this, [this] {
        m_list->clear();
        m_counts[0] = m_counts[1] = m_counts[2] = 0;
        updateCounts();
    });
    connect(m_search, &QLineEdit::textChanged, this, [this] { applyFilter(); });
    connect(&LogCapture::instance(), &LogCapture::message, this, &ConsolePanel::append);
    updateCounts();
}

/**
 * @brief Ajoute un message à la console.
 *
 * Le message est encodé sous forme d'un `QListWidgetItem` avec un rôle de niveau.
 * Si le nombre maximum de lignes est dépassé, les plus anciennes sont supprimées.
 *
 * @param level Niveau de log.
 * @param text Texte à afficher.
 * @param time Heure d'émission du message.
 */
void ConsolePanel::append(int level, const QString& text, const QString& time) {
    level = std::clamp(level, 0, 2);
    auto* item = new QListWidgetItem(QString("[%1]  %2").arg(time, text));
    item->setData(kLevelRole, level);

    static const QColor colors[3] = { QColor(), QColor(240, 180, 60), QColor(255, 95, 95) };
    if (level > 0) item->setForeground(colors[level]);

    m_list->addItem(item);
    applyFilter(item);

    ++m_counts[level];
    while (m_list->count() > kMaxLines) {
        QListWidgetItem* old = m_list->takeItem(0);
        --m_counts[std::clamp(old->data(kLevelRole).toInt(), 0, 2)];
        delete old;
    }
    updateCounts();

    if (m_autoScroll->isChecked() && !item->isHidden()) m_list->scrollToBottom();
}

/**
 * @brief Applique le filtrage d'un élément de liste.
 *
 * Un item est masqué si :
 * - son niveau de log est désactivé dans la barre d'outils ;
 * - ou s'il ne contient pas le texte recherché.
 *
 * @param item Élément à filtrer.
 */
void ConsolePanel::applyFilter(QListWidgetItem* item) {
    const int level = item->data(kLevelRole).toInt();
    const QString filter = m_search->text().trimmed();
    item->setHidden(!m_levels[level]->isChecked() ||
                    (!filter.isEmpty() && !item->text().contains(filter, Qt::CaseInsensitive)));
}

/**
 * @brief Réapplique le filtre sur tous les éléments de la console.
 */
void ConsolePanel::applyFilter() {
    for (int i = 0; i < m_list->count(); ++i) applyFilter(m_list->item(i));
}

/**
 * @brief Met à jour l'affichage du nombre de messages par catégorie.
 *
 * Les libellés "Infos", "Avertissements" et "Erreurs" sont rafraîchis avec leurs
 * compteurs respectifs.
 */
void ConsolePanel::updateCounts() {
    m_levels[0]->setText(tr("Infos %1").arg(m_counts[0]));
    m_levels[1]->setText(tr("Avertissements %1").arg(m_counts[1]));
    m_levels[2]->setText(tr("Erreurs %1").arg(m_counts[2]));
}