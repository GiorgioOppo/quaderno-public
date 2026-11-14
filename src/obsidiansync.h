#pragma once

#include <QHash>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class Notebook;

class ObsidianSync final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Fornito dall’applicazione")
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool installed READ installed NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList notes READ notes NOTIFY changed)

public:
    ObsidianSync(Notebook *notebook, QString directory, bool available, QObject *parent = nullptr);
    bool available() const { return m_available; }
    bool installed() const;
    bool busy() const { return m_process.state() != QProcess::NotRunning || m_phase != Idle; }
    QString status() const { return m_status; }
    QVariantList notes() const { return m_notes; }
    void setLocked(bool locked);

    Q_INVOKABLE void reload();
    Q_INVOKABLE void syncNow();
    Q_INVOKABLE void openNote(const QString &path, const QString &folderId);
    Q_INVOKABLE void openCloudCopy(const QString &path, const QString &folderId);
    Q_INVOKABLE void sendNote(const QString &path);

signals:
    void changed();
    void opened(const QString &bookId);

private:
    struct Link {
        QString bookId;
        QString hash;
        QString revision;
        QString pendingHash;
        QString pendingRevision;
    };
    struct Note {
        QString text;
        QString hash;
    };
    enum Phase { Idle, Download, BeforeUpload, Upload };
    QString vaultId() const;
    QString mirrorPath(const QString &path) const;
    Note readNote(const QString &path) const;
    bool loadLinks();
    bool saveLinks(const QHash<QString, Link> &links);
    void rebuildNotes(bool scan);
    void run(Phase phase);
    void finish(bool success);
    void upload();
    bool reconcile();
    void report(const QString &message);

    Notebook *const m_notebook;
    const QString m_directory;
    const bool m_available;
    QProcess m_process;
    QTimer m_timeout;
    Phase m_phase = Idle;
    bool m_locked = false;
    bool m_timedOut = false;
    bool m_complete = false;
    bool m_uploadConflict = false;
    bool m_openConflict = false;
    QString m_openConflictPath;
    QByteArray m_output;
    bool m_loaded = false;
    QString m_vault;
    QString m_status;
    QString m_uploadPath;
    QString m_uploadRevision;
    QHash<QString, Link> m_links;
    QHash<QString, QString> m_files;
    QVariantList m_notes;
};
