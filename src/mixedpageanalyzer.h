#pragma once

#include "pdfimport.h"

#include <QObject>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class MixedPageAnalyzer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit MixedPageAnalyzer(QObject *parent = nullptr);
    ~MixedPageAnalyzer() override;

    bool busy() const { return bool(m_job); }
    Q_INVOKABLE void analyze(const QVariantMap &content, const QVariantMap &regions);
    Q_INVOKABLE void cancel();

signals:
    void busyChanged();
    void finished(const QVariantMap &draft, const QString &error);

private:
    DocumentCancellation m_job;
};
