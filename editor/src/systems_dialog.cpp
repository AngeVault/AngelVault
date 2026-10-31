#include "systems_dialog.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

SystemsDialog::SystemsDialog(const std::vector<Row>& rows, QWidget* parent) : QDialog(parent) {
    setObjectName("systemsDialog");
    setWindowTitle(tr("Systèmes"));
    resize(520, 420);

    auto* layout = new QVBoxLayout(this);
    auto* help = new QLabel(tr("Décochez un système pour le désactiver (ex. RenderCollisionSystem, ForceDebugSystem : "
                               "les tracés de debug). Enregistré dans le projet (.angproj), appliqué au lancement "
                               "et immédiatement si le jeu tourne."), this);
    help->setObjectName("hintLabel");
    help->setWordWrap(true);
    layout->addWidget(help);

    m_table = new QTableWidget(int(rows.size()), 3, this);
    m_table->setHorizontalHeaderLabels({ tr("Système"), tr("Priorité"), tr("Origine") });
    m_table->verticalHeader()->hide();
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    for (int i = 0; i < int(rows.size()); ++i) {
        const Row& r = rows[size_t(i)];
        auto* name = new QTableWidgetItem(r.name);
        name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        name->setCheckState(r.enabled ? Qt::Checked : Qt::Unchecked);
        m_table->setItem(i, 0, name);
        auto* prio = new QTableWidgetItem(QString::number(r.priority));
        prio->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(i, 1, prio);
        m_table->setItem(i, 2, new QTableWidgetItem(r.fromGame ? tr("Jeu") : tr("Moteur")));
    }
    connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (item->column() != 0) return;
        emit systemToggled(item->text(), item->checkState() == Qt::Checked);
    });
    layout->addWidget(m_table);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
