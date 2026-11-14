#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class ChartRecognizer : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit ChartRecognizer(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE QVariantMap recognize(const QVariantList &strokes) const;
    Q_INVOKABLE QString csv(const QVariantMap &chart) const;
    Q_INVOKABLE bool copyCsv(const QVariantMap &chart) const;
};
