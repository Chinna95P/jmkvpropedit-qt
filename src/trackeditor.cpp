#include "trackeditor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>

namespace {
QComboBox *choiceBox(QWidget *parent)
{
    auto *box = new QComboBox(parent);
    box->addItems({QObject::tr("Keep"), QObject::tr("Yes"), QObject::tr("No")});
    return box;
}

QString boolValue(const QComboBox *box)
{
    return box->currentIndex() == 1 ? QStringLiteral("1") : QStringLiteral("0");
}
}

TrackEditor::TrackEditor(QString type, QString selectorPrefix, QWidget *parent)
    : QWidget(parent), m_type(std::move(type)), m_selectorPrefix(std::move(selectorPrefix))
{
    auto *layout = new QVBoxLayout(this);
    m_table = new QTableWidget(0, 9, this);
    m_table->setHorizontalHeaderLabels({tr("Apply"), tr("Track"), tr("Enabled"), tr("Default"),
                                        tr("Forced"), tr("Set name"), tr("Name"),
                                        tr("Set language"), tr("Language")});
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    m_table->setMinimumHeight(260);
    layout->addWidget(m_table);

    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton(tr("Add track selector"), this);
    auto *remove = new QPushButton(tr("Remove selector"), this);
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addStretch();
    layout->addLayout(buttons);
    connect(add, &QPushButton::clicked, this, [this] { addRow(); });
    connect(remove, &QPushButton::clicked, this, [this] {
        const int row = m_table->currentRow();
        if (row >= 0)
            m_table->removeRow(row);
    });
    addRow();
}

void TrackEditor::addRow(const QString &summary)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    auto *apply = new QCheckBox(m_table);
    m_table->setCellWidget(row, 0, apply);
    auto *track = new QTableWidgetItem(summary.isEmpty()
                                           ? tr("%1 #%2").arg(m_type).arg(row + 1)
                                           : summary);
    track->setData(Qt::UserRole, row + 1);
    track->setFlags(track->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(row, 1, track);
    m_table->setCellWidget(row, 2, choiceBox(m_table));
    m_table->setCellWidget(row, 3, choiceBox(m_table));
    m_table->setCellWidget(row, 4, choiceBox(m_table));
    m_table->setCellWidget(row, 5, new QCheckBox(m_table));
    m_table->setCellWidget(row, 6, new QLineEdit(m_table));
    m_table->setCellWidget(row, 7, new QCheckBox(m_table));
    auto *language = new QComboBox(m_table);
    language->setEditable(true);
    language->addItems({"eng", "jpn", "und", "hin", "tam", "tel", "kan", "mal", "spa", "fra", "deu", "ita", "por", "zho", "kor"});
    m_table->setCellWidget(row, 8, language);
}

QStringList TrackEditor::arguments() const
{
    QStringList result;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const auto *apply = qobject_cast<QCheckBox *>(m_table->cellWidget(row, 0));
        if (!apply || !apply->isChecked())
            continue;
        QStringList changes;
        const auto addFlag = [&](int column, const QString &name) {
            const auto *box = qobject_cast<QComboBox *>(m_table->cellWidget(row, column));
            if (box && box->currentIndex() != 0)
                changes << "--set" << name + "=" + boolValue(box);
        };
        addFlag(2, "flag-enabled");
        addFlag(3, "flag-default");
        addFlag(4, "flag-forced");
        const auto *setName = qobject_cast<QCheckBox *>(m_table->cellWidget(row, 5));
        const auto *name = qobject_cast<QLineEdit *>(m_table->cellWidget(row, 6));
        if (setName && setName->isChecked())
            changes << "--set" << "name=" + name->text();
        const auto *setLanguage = qobject_cast<QCheckBox *>(m_table->cellWidget(row, 7));
        const auto *language = qobject_cast<QComboBox *>(m_table->cellWidget(row, 8));
        if (setLanguage && setLanguage->isChecked())
            changes << "--set" << "language=" + language->currentText().trimmed();
        if (!changes.isEmpty()) {
            const int selector = m_table->item(row, 1)->data(Qt::UserRole).toInt();
            result << "--edit" << QStringLiteral("track:%1%2").arg(m_selectorPrefix).arg(selector);
            result << changes;
        }
    }
    return result;
}

void TrackEditor::populate(const QJsonArray &tracks)
{
    m_table->setRowCount(0);
    int number = 0;
    for (const QJsonValue &value : tracks) {
        const QJsonObject track = value.toObject();
        if (track.value("type").toString() != m_type)
            continue;
        ++number;
        const QJsonObject props = track.value("properties").toObject();
        QStringList details;
        const QString codec = track.value("codec").toString();
        const QString language = props.value("language").toString();
        const QString name = props.value("track_name").toString();
        if (!codec.isEmpty()) details << codec;
        if (!language.isEmpty()) details << language;
        if (!name.isEmpty()) details << name;
        addRow(tr("%1 #%2 — %3").arg(m_type).arg(number).arg(details.join(" · ")));
        m_table->item(m_table->rowCount() - 1, 1)->setData(Qt::UserRole, number);
    }
    if (number == 0)
        addRow();
}
