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

namespace {
constexpr int kLevelRole = Qt::UserRole + 1;
constexpr int kMaxLines = 5000;

// streambuf qui découpe le texte en lignes, les envoie à la Console
// et les recopie dans le flux d'origine (console de l'IDE).
class LineBuffer : public std::streambuf {
public:
    LineBuffer(LogLevel level, std::streambuf* forward) : m_level(level), m_forward(forward) {}
    std::streambuf* forward() const { return m_forward; }

protected:
    int overflow(int ch) override {
        if (ch == traits_type::eof()) return traits_type::not_eof(ch);
        std::lock_guard lock(m_mutex);
        if (m_forward) m_forward->sputc(char(ch));
        if (ch == '\n') flushLine();
        else m_line.push_back(char(ch));
        return ch;
    }
    std::streamsize xsputn(const char* s, std::streamsize n) override {
        std::lock_guard lock(m_mutex);
        if (m_forward) m_forward->sputn(s, n);
        for (std::streamsize i = 0; i < n; ++i) {
            if (s[i] == '\n') flushLine();
            else m_line.push_back(s[i]);
        }
        return n;
    }
    int sync() override {
        std::lock_guard lock(m_mutex);
        if (m_forward) m_forward->pubsync();
        return 0;
    }

private:
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

    LogLevel m_level;
    std::streambuf* m_forward;
    std::string m_line;
    std::mutex m_mutex;
};

LineBuffer* g_out = nullptr;
LineBuffer* g_err = nullptr;
LineBuffer* g_sfErr = nullptr;
QtMessageHandler g_previousHandler = nullptr;

void qtHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
    LogLevel level = LogLevel::Info;
    if (type == QtWarningMsg) level = LogLevel::Warning;
    else if (type == QtCriticalMsg || type == QtFatalMsg) level = LogLevel::Error;
    LogCapture::instance().post(level, msg);
    if (g_previousHandler) g_previousHandler(type, ctx, msg);
}
}

// ---------------------------------------------------------------------------- LogCapture

LogCapture& LogCapture::instance() {
    static LogCapture capture;
    return capture;
}

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

void LogCapture::post(LogLevel level, const QString& text) {
    const QString time = QTime::currentTime().toString("HH:mm:ss");
    // Signal en file d'attente : sûr même depuis un autre thread
    QMetaObject::invokeMethod(this, [this, level, text, time] { emit message(int(level), text, time); },
                              Qt::QueuedConnection);
}

// ---------------------------------------------------------------------------- ConsolePanel

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

void ConsolePanel::applyFilter(QListWidgetItem* item) {
    const int level = item->data(kLevelRole).toInt();
    const QString filter = m_search->text().trimmed();
    item->setHidden(!m_levels[level]->isChecked() ||
                    (!filter.isEmpty() && !item->text().contains(filter, Qt::CaseInsensitive)));
}

void ConsolePanel::applyFilter() {
    for (int i = 0; i < m_list->count(); ++i) applyFilter(m_list->item(i));
}

void ConsolePanel::updateCounts() {
    m_levels[0]->setText(tr("Infos %1").arg(m_counts[0]));
    m_levels[1]->setText(tr("Avertissements %1").arg(m_counts[1]));
    m_levels[2]->setText(tr("Erreurs %1").arg(m_counts[2]));
}
