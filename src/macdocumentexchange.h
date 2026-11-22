#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

class Notebook;
class QTemporaryDir;

class MacDocumentExchange final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Fornito dall’applicazione")
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool importPaused READ importPaused WRITE setImportPaused NOTIFY importPausedChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool importing READ importing NOTIFY importReportChanged)
    Q_PROPERTY(QVariantList importReport READ importReport NOTIFY importReportChanged)
    Q_PROPERTY(bool canRetryImports READ canRetryImports NOTIFY importReportChanged)

public:
    explicit MacDocumentExchange(Notebook *notebook, bool tablet, QObject *parent = nullptr);
    ~MacDocumentExchange() override;

    bool available() const;
    bool importPaused() const { return m_importPaused; }
    QString status() const { return m_status; }
    bool importing() const { return m_importing; }
    QVariantList importReport() const { return m_importReport; }
    bool canRetryImports() const;
    void setImportPaused(bool paused);
    void setLocked(bool locked);

    Q_INVOKABLE bool hasSupportedFiles(const QList<QUrl> &files) const;
    Q_INVOKABLE QVariantMap importPreview(const QList<QUrl> &files) const;
    Q_INVOKABLE bool importFiles(const QList<QUrl> &files, const QString &folderId);
    Q_INVOKABLE bool retryFailedImports();
    Q_INVOKABLE void cancelImports();
    Q_INVOKABLE void clearImportReport();
    Q_INVOKABLE void openAppleBooks();
    Q_INVOKABLE void chooseAppleBooksFiles(const QString &folderId);
    Q_INVOKABLE void beginPdfDrag(bool currentPage);

signals:
    void imported(const QString &bookId);
    void statusChanged();
    void importPausedChanged();
    void importReportChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool canImport() const;
    void setStatus(const QString &status);
    void openFinderFiles();
    void importNext();
    void updateImportEntry(int index, const QString &state, const QString &message = {});
    void finishImportEntry(bool success);
    QList<QUrl> m_importFiles;
    QList<int> m_importRows;
    QVariantList m_importReport;
    QList<QUrl> m_reportFiles;
    int m_activeImportRow = -1;
    QString m_importFolder;
    QString m_lastImportedBook;
    QString m_importError;
    int m_importedCount = 0;
    int m_importFileCount = 0;
    bool m_waitingImport = false;
    bool m_importCancelled = false;

    QPointer<Notebook> m_notebook;
    QList<std::shared_ptr<QTemporaryDir>> m_exports;
    QList<QUrl> m_finderFiles;
    QString m_status;
    bool m_tablet;
    bool m_locked = false;
    bool m_importPaused = false;
    bool m_finderDispatchPending = false;
    bool m_appleBooksPanelOpen = false;
    bool m_importing = false;
    bool m_dragging = false;
    bool m_dragLocked = false;
};
