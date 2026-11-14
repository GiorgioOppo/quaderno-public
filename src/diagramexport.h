#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class DiagramExport : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit DiagramExport(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE QUrl suggestedFile(const QString &title) const;
    Q_INVOKABLE QString save(const QUrl &file, const QVariantMap &diagram, const QString &title) const;
    Q_INVOKABLE QVariantMap load(const QUrl &file) const;
};
