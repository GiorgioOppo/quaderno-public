#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>
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

public:
    explicit MacDocumentExchange(Notebook *notebook, bool tablet, QObject *parent = nullptr);
    ~MacDocumentExchange() override;

    bool available() const;
    bool importPaused() const { return m_importPaused; }
    QString status() const { return m_status; }
    void setImportPaused(bool paused);
    void setLocked(bool locked);

    Q_INVOKABLE bool hasSupportedFiles(const QList<QUrl> &files) const;
    Q_INVOKABLE bool importFiles(const QList<QUrl> &files, const QString &folderId);
    Q_INVOKABLE void openAppleBooks();
    Q_INVOKABLE void chooseAppleBooksFiles(const QString &folderId);
    Q_INVOKABLE void beginPdfDrag(bool currentPage);

signals:
    void imported(const QString &bookId);
    void statusChanged();
    void importPausedChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool canImport() const;
    void setStatus(const QString &status);
    void openFinderFiles();

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
