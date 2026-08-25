#pragma once

#include <QJsonArray>
#include <QStringList>
#include <QWidget>

class QTableWidget;

class TrackEditor final : public QWidget
{
    Q_OBJECT
public:
    explicit TrackEditor(QString type, QString selectorPrefix, QWidget *parent = nullptr);
    QStringList arguments() const;
    void populate(const QJsonArray &tracks);

private:
    void addRow(const QString &summary = {});
    QString m_type;
    QString m_selectorPrefix;
    QTableWidget *m_table = nullptr;
};
