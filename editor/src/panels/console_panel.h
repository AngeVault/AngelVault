#pragma once
#include <QObject>
#include <QString>
#include <QWidget>

class QCheckBox;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QToolButton;

enum class LogLevel { Info = 0, Warning = 1, Error = 2 };

// Récupère tout ce que le moteur et l'éditeur écrivent :
// std::cout (infos), std::cerr et sf::err() (erreurs), qDebug / qWarning / qCritical.
// Le texte continue aussi d'aller dans la console de l'IDE.
class LogCapture : public QObject {
    Q_OBJECT
public:
    static LogCapture& instance();
    void install();     // à appeler une fois, au début de main()
    void uninstall();   // remet les flux d'origine (fin de main())

    void post(LogLevel level, const QString& text);   // utilisable depuis n'importe quel thread

signals:
    void message(int level, const QString& text, const QString& time);

private:
    LogCapture() = default;
    bool m_installed = false;
};

class ConsolePanel : public QWidget {
    Q_OBJECT
public:
    explicit ConsolePanel(QWidget* parent = nullptr);

private:
    void append(int level, const QString& text, const QString& time);
    void applyFilter();
    void applyFilter(QListWidgetItem* item);
    void updateCounts();

    QListWidget* m_list = nullptr;
    QToolButton* m_levels[3] = {};
    QLineEdit* m_search = nullptr;
    QCheckBox* m_autoScroll = nullptr;
    int m_counts[3] = {};
};
