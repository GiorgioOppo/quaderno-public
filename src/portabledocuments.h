#pragma once
#include "notebook.h"
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <memory>

class QTemporaryDir;

// Portable notebook files use the same reviewed merge as USB, with a fresh
// preview token. Credentials and device preferences never enter these files.
class PortableDocuments : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Notebook *notebook READ notebook WRITE setNotebook NOTIFY changed)
    Q_PROPERTY(bool locked READ locked WRITE setLocked NOTIFY changed)
    Q_PROPERTY(QVariantMap preview READ preview NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit PortableDocuments(QObject *parent = nullptr);
    ~PortableDocuments() override;
    Notebook *notebook() const;
    void setNotebook(Notebook *value);
    bool locked() const { return m_locked; }
    void setLocked(bool value);
    QVariantMap preview() const { return m_preview; }
    QString error() const { return m_error; }
    Q_INVOKABLE bool prepareImport(const QUrl &file);
    Q_INVOKABLE bool applyImport();
    Q_INVOKABLE void clear();
    Q_INVOKABLE QUrl exportCurrent();
signals:
    void changed();
private:
    QPointer<Notebook> m_notebook;
    bool m_locked = false;
    QByteArray m_payload;
    QVariantMap m_preview;
    QString m_error;
    std::unique_ptr<QTemporaryDir> m_export;
};
