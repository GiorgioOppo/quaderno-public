#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariant>
#include <QtQml/qqmlregistration.h>
#include <optional>
#include "documentreader.h"
#include "pagestore.h"

class Notebook final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Il modello è fornito dall’applicazione")
    Q_PROPERTY(QVariantList books READ books NOTIFY booksChanged)
    Q_PROPERTY(QVariantList folders READ folders NOTIFY booksChanged)
    Q_PROPERTY(QVariantList libraryItems READ libraryItems NOTIFY libraryItemsChanged)
    Q_PROPERTY(QString bookId READ bookId NOTIFY documentChanged)
    Q_PROPERTY(QString title READ title NOTIFY documentChanged)
    Q_PROPERTY(QString handwritingLanguage READ handwritingLanguage NOTIFY documentChanged)
    Q_PROPERTY(int page READ page NOTIFY documentChanged)
    Q_PROPERTY(QString pageId READ pageId NOTIFY documentChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY documentChanged)
    Q_PROPERTY(QVariantList strokes READ strokes NOTIFY strokesChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY strokesChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY strokesChanged)
    Q_PROPERTY(QVariantList layers READ layers NOTIFY layersChanged)
    Q_PROPERTY(QString activeLayer READ activeLayer NOTIFY layersChanged)
    Q_PROPERTY(bool canWriteLayer READ canWriteLayer NOTIFY layersChanged)
    Q_PROPERTY(int erasableFrom READ erasableFrom NOTIFY strokesChanged)
    Q_PROPERTY(int erasableCount READ erasableCount NOTIFY strokesChanged)
    Q_PROPERTY(QString pageTemplate READ pageTemplate NOTIFY pageContentChanged)
    Q_PROPERTY(QVariantList pageItems READ pageItems NOTIFY pageContentChanged)
    Q_PROPERTY(QStringList pageTags READ pageTags NOTIFY pageContentChanged)
    Q_PROPERTY(QStringList knownTags READ knownTags NOTIFY booksChanged)
    Q_PROPERTY(QVariantMap pageRenderContent READ pageRenderContent NOTIFY pageContentChanged)
    Q_PROPERTY(bool importing READ importing NOTIFY importChanged)
    Q_PROPERTY(qreal importProgress READ importProgress NOTIFY importChanged)
    Q_PROPERTY(QString importStatus READ importStatus NOTIFY importChanged)
    Q_PROPERTY(QByteArray pageBackground READ pageBackground NOTIFY pageContentChanged)
    Q_PROPERTY(bool hasPageBackground READ hasPageBackground NOTIFY pageContentChanged)
    Q_PROPERTY(bool markdownBook READ markdownBook NOTIFY pageContentChanged)
    Q_PROPERTY(bool diagramBook READ diagramBook NOTIFY documentChanged)
    Q_PROPERTY(bool pageBookmarked READ pageBookmarked NOTIFY pageContentChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QVariantMap localReadingPreferences READ localReadingPreferences NOTIFY localReadingPreferencesChanged)

public:
    static constexpr qsizetype maximumMarkdownBytes = 256 * 1024;

    explicit Notebook(QString filePath, QObject *parent = nullptr);
    ~Notebook() override;
    QString storagePath() const { return m_filePath; }
    QString sourceCacheDirectory() const;
    QString readingPreferencesPath() const;
    static QString validateBackupData(const QByteArray &data);
    QVariantMap localReadingPreferences() const;
    Q_INVOKABLE QVariantMap readingPreferencesForBook(const QString &id) const;
    Q_INVOKABLE bool setReadingPreferences(const QVariantMap &changes);
    Q_INVOKABLE bool clearReadingPreferences();
    bool importing() const { return m_importing; }
    qreal importProgress() const { return m_importProgress; }
    QString importStatus() const { return m_importStatus; }
    QVariantMap pageRenderContent() const;

    QVariantList books() const;
    QVariantList folders() const;
    QVariantList libraryItems() const;
    QString bookId() const;
    QString title() const;
    QString handwritingLanguage() const;
    Q_INVOKABLE bool setHandwritingLanguage(const QString &language);
    int page() const;
    QString pageId() const;
    int pageCount() const;
    QVariantList strokes() const;
    bool canUndo() const;
    bool canRedo() const;
    QVariantList layers() const;
    QString activeLayer() const;
    bool canWriteLayer() const;
    int erasableFrom() const;
    int erasableCount() const;
    QString pageTemplate() const;
    QVariantList pageItems() const;
    QStringList pageTags() const;
    QStringList knownTags() const;
    QByteArray pageBackground() const;
    bool hasPageBackground() const;
    bool markdownBook() const;
    bool diagramBook() const;
    bool pageBookmarked() const;
    QString error() const;
    QString pdfFileName() const;
    bool exportPdf(const QString &path, bool currentPageOnly = false);
    bool importFromFile(const QString &path);
    bool importFromData(const QByteArray &contents);
    QVariantMap previewImportFromData(const QByteArray &contents);
    QVariantMap applyImportFromData(const QByteArray &contents, const QString &token);
    QByteArray exportBookForTransfer(const QString &id);
    QByteArray exportLibraryForTransfer();
    QString bookTransferRevision(const QString &id) const;
    QString bookDisplayTitle(const QString &id) const;
    int bookPageNumber(const QString &id, const QString &pageId) const;
    bool canResumeBook(const QString &id, const QString &revision, const QString &pageId, int pageNumber = 0) const;
    bool openBookAtRevision(const QString &id, const QString &revision, const QString &pageId, int pageNumber = 0);
    QVariantMap receiveBookTransfer(const QByteArray &payload, const QByteArray &expectedToken = {},
                                    std::optional<QByteArray> baseOverride = {});
    bool recordBookTransfer(const QString &id, const QString &revision);
    static QString normalizeMarkdownText(QString text);
    QString markdownTextForBook(const QString &id) const;
    bool hasBookId(const QString &id) const;
    bool canRefreshMarkdownBook(const QString &id) const;
    bool importLinkedMarkdown(const QString &text, const QString &title, const QString &folderId, const QString &id,
                              bool activate = true);
    bool refreshMarkdownBook(const QString &id, const QString &text, const QString &expectedBookRevision);

    Q_INVOKABLE bool createBook(const QString &title);
    Q_INVOKABLE bool createBookInFolder(const QString &title, const QString &folderId);
    Q_INVOKABLE bool createDiagramBook(const QString &title, const QString &folderId);
    Q_INVOKABLE bool createBookWithItem(const QString &title, const QString &folderId, const QVariantMap &item);
    Q_INVOKABLE QVariantMap mixedPageSource() const;
    Q_INVOKABLE bool createMixedDocument(const QString &sourceBookId, const QString &sourcePageId,
                                         const QString &sourceRevision, const QString &title, const QString &markdown,
                                         const QVariantMap &diagram, const QVariantMap &table);
    Q_INVOKABLE bool startDocumentImport(const QUrl &file, const QString &folderId);
    Q_INVOKABLE void cancelDocumentImport();
    Q_INVOKABLE bool importDocument(const QUrl &file, const QString &folderId);
    Q_INVOKABLE bool importMarkdownText(const QString &text, const QString &title, const QString &folderId);
    Q_INVOKABLE bool exportDocument(const QUrl &file);
    Q_INVOKABLE bool exportMarkdown(const QUrl &file);
    Q_INVOKABLE bool exportMarkdownWithAssets(const QUrl &file);
    Q_INVOKABLE QString markdownText() const;
    Q_INVOKABLE QString markdownSource() const;
    Q_INVOKABLE bool updateMarkdownText(const QString &text);
    Q_INVOKABLE QUrl suggestedMarkdownName() const;
    Q_INVOKABLE bool exportChartCsv(const QUrl &file, const QString &data);
    Q_INVOKABLE bool exportTableCsv(const QUrl &file, const QVariantMap &table);
    Q_INVOKABLE QVariantMap readTableCsv(const QUrl &file) const;
    Q_INVOKABLE QVariantList tableCellValues(const QVariantMap &table) const;
    Q_INVOKABLE bool importPageImage(const QUrl &file, const QString &bookId, const QString &pageId);
    Q_INVOKABLE QUrl suggestedPdfName() const;
    Q_INVOKABLE bool openBook(const QString &id);
    Q_INVOKABLE QVariantMap openLibraryItem(const QString &bookId, const QString &pageId, const QString &itemId);
    Q_INVOKABLE QVariantMap openPageLink(const QString &id);
    Q_INVOKABLE QVariantMap linkedPageItem(const QString &id) const;
    Q_INVOKABLE QVariantMap libraryItemCopy(const QString &bookId, const QString &pageId, const QString &itemId) const;
    Q_INVOKABLE bool renameBook(const QString &title);
    Q_INVOKABLE bool createFolder(const QString &title, const QString &parentId);
    Q_INVOKABLE bool renameFolder(const QString &id, const QString &title);
    Q_INVOKABLE bool removeFolder(const QString &id);
    Q_INVOKABLE bool moveBook(const QString &id, const QString &folderId);
    Q_INVOKABLE bool setFavorite(const QString &id, bool favorite);
    Q_INVOKABLE bool setBookTags(const QString &id, const QVariantList &tags);
    Q_INVOKABLE bool trashBook(const QString &id);
    Q_INVOKABLE bool restoreBook(const QString &id);
    Q_INVOKABLE bool purgeBook(const QString &id);
    Q_INVOKABLE bool duplicateBook(const QString &id);
    Q_INVOKABLE bool goToPage(int page);
    Q_INVOKABLE bool openPage(const QString &pageId);
    Q_INVOKABLE bool addPage();
    Q_INVOKABLE QVariantMap pagePreview(int index) const;
    Q_INVOKABLE QVariantList pageSummaries() const;
    Q_INVOKABLE QVariantList searchPages(const QString &query) const;
    Q_INVOKABLE QVariantList pageBookmarks() const;
    Q_INVOKABLE QVariantList pageBookmarkEntries() const;
    Q_INVOKABLE bool togglePageBookmark();
    Q_INVOKABLE bool duplicatePage(int index);
    Q_INVOKABLE bool deletePage(int index);
    Q_INVOKABLE bool movePage(int from, int to);
    Q_INVOKABLE bool transferPage(int index, const QString &targetBookId);
    Q_INVOKABLE bool appendStroke(const QVariantMap &stroke);
    Q_INVOKABLE bool appendCorrectedStroke(const QVariantMap &shape, const QVariantMap &original);
    Q_INVOKABLE QVariantMap selectInk(const QVariantList &polygon) const;
    Q_INVOKABLE QVariantMap selectContent(const QVariantList &polygon, const QString &mode,
                                           const QVariantMap &previous, const QString &operation) const;
    Q_INVOKABLE QVariantMap previewSelection(const QVariantMap &selection, qreal dx, qreal dy, qreal scale, qreal angle) const;
    Q_INVOKABLE bool transformSelection(const QVariantMap &selection, qreal dx, qreal dy, qreal scale, qreal angle);
    Q_INVOKABLE bool removeSelection(const QVariantMap &selection);
    Q_INVOKABLE bool restoreOriginalHandwriting(const QString &itemId);
    Q_INVOKABLE bool restoreSelectedInk(const QVariantList &indexes);
    Q_INVOKABLE bool transformSelectedInk(const QVariantList &indexes, qreal pivotX, qreal pivotY,
                                          qreal dx, qreal dy, qreal scale, qreal angle);
    Q_INVOKABLE bool pasteInk(const QVariantList &strokes, qreal pivotX, qreal pivotY,
                              qreal dx, qreal dy, qreal scale, qreal angle);
    Q_INVOKABLE bool eraseStrokes(const QVariantList &indexes);
    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool redo();
    Q_INVOKABLE bool addLayer(const QString &name);
    Q_INVOKABLE bool setActiveLayer(const QString &id);
    Q_INVOKABLE bool renameLayer(const QString &id, const QString &name);
    Q_INVOKABLE bool setLayerVisible(const QString &id, bool visible);
    Q_INVOKABLE bool deleteLayer(const QString &id);
    Q_INVOKABLE bool moveLayer(const QString &id, int toIndex);
    Q_INVOKABLE bool mergeLayerDown(const QString &id);
    Q_INVOKABLE bool setPageTemplate(const QString &pageTemplate);
    Q_INVOKABLE bool setPageTags(const QVariantList &tags);
    Q_INVOKABLE bool savePageItem(const QVariantMap &item);
    Q_INVOKABLE bool savePageNote(const QVariantMap &note, const QString &expectedBookId,
                                  const QString &expectedPageId, const QVariantMap &expectedOriginal = {});
    Q_INVOKABLE bool removePageNote(const QString &id, const QString &expectedBookId,
                                    const QString &expectedPageId, const QVariantMap &expectedOriginal = {});
    Q_INVOKABLE bool savePageLink(const QVariantMap &target, const QPointF &anchor, const QString &expectedBookId,
                                  const QString &expectedPageId);
    Q_INVOKABLE bool removePageLink(const QString &id, const QString &expectedBookId, const QString &expectedPageId);
    Q_INVOKABLE QVariantMap tableChartPreview(const QVariantMap &options) const;
    Q_INVOKABLE QVariantMap tableChartSnapshot(const QString &bookId, const QString &pageId,
                                               const QString &tableId, const QVariantMap &options) const;
    Q_INVOKABLE bool saveTableChart(const QVariantMap &options);
    Q_INVOKABLE bool savePageItemOnNewPage(const QVariantMap &item);
    Q_INVOKABLE bool saveTranscript(const QVariantMap &item);
    Q_INVOKABLE QVariantMap handwritingPreview(const QVariantMap &selection, const QString &text) const;
    Q_INVOKABLE bool replaceHandwriting(const QVariantMap &selection, const QString &text, bool keepInk = false);
    Q_INVOKABLE QVariantMap diagramHandwritingPlan(const QVariantMap &selection, const QString &targetId = {}) const;
    Q_INVOKABLE bool applyDiagramHandwriting(const QVariantMap &selection, const QVariantMap &plan,
                                             const QVariantMap &reviewedDiagram, const QVariantList &acceptedLabelIds,
                                             bool removeInk);
    Q_INVOKABLE QVariantMap tableHandwritingPlan(const QVariantMap &selection, const QString &tableId) const;
    Q_INVOKABLE bool applyTableHandwriting(const QVariantMap &selection, const QVariantMap &plan,
                                           const QVariantList &updates, bool removeInk, bool overwriteExisting);
    Q_INVOKABLE bool movePageItem(const QString &id, qreal x, qreal y);
    Q_INVOKABLE bool removePageItem(const QString &id);

signals:
    void importChanged();
    void documentImported(const QString &bookId);
    void importFinished(bool success);
    void booksChanged();
    void libraryItemsChanged();
    void documentChanged();
    void strokesChanged();
    void layersChanged();
    void pageContentChanged();
    void errorChanged();
    void localReadingPreferencesChanged();

private:
    struct InkLayer {
        QString id;
        QString name;
        bool visible = true;
        QVariantList strokes;

        bool operator==(const InkLayer &other) const
        {
            return id == other.id && name == other.name && visible == other.visible && strokes == other.strokes;
        }
    };

    struct Page {
        QString id;
        QVariantList strokes;
        QVariantList items;
        QString paper = QStringLiteral("blank");
        QString encodedBackground;
        QVariantMap source;
        bool hasBackground() const { return !encodedBackground.isEmpty() || !source.isEmpty(); }
        QByteArray backgroundData() const { return QByteArray::fromBase64(encodedBackground.toLatin1(), QByteArray::AbortOnBase64DecodingErrors); }
        QList<InkLayer> layers;
        QString activeLayer;
        QStringList tags;
        QString pdfText;
        QString markdownBlockId;
        QStringList transcripts;
        bool bookmarked = false;

        bool operator==(const Page &other) const
        {
            return strokes == other.strokes && items == other.items && paper == other.paper
                && encodedBackground == other.encodedBackground && source == other.source && layers == other.layers && tags == other.tags
                && pdfText == other.pdfText && transcripts == other.transcripts && bookmarked == other.bookmarked;
        }

        bool hasLayout() const
        {
            return paper != QStringLiteral("blank") || !items.isEmpty() || (!encodedBackground.isEmpty() || !source.isEmpty())
                || !layers.isEmpty() || !tags.isEmpty() || !pdfText.isEmpty() || !transcripts.isEmpty() || bookmarked;
        }
    };

    struct Book {
        QString id;
        QString title;
        QString handwritingLanguage = QStringLiteral("it");
        QString workspace;
        QList<Page> pages;
        QVariantMap sourceDocuments;
        QString lastViewedPageId;
        bool hasStoredPageIds = true;
        QByteArray importedRevision;
        QString folderId;
        QStringList tags;
        bool favorite = false;
        bool trashed = false;
    };

    struct Folder {
        QString id;
        QString title;
        QString parentId;
    };

    struct State {
        QList<Book> books;
        QList<Folder> folders;
        int activeBook = -1;
        int page = 0;
    };

    enum Change {
        BooksChanged = 1,
        DocumentChanged = 2,
        StrokesChanged = 4,
        PageContentChanged = 8,
        ResetInkHistory = 16,
        LayersChanged = 32,
        LibraryItemsChanged = 64,
    };

    struct InkEdit {
        QList<int> indexes;
        QVariantList strokes;
        bool removes = false;
        QVariantList previous;
        QVariantMap textItem;
        qsizetype itemIndex = -1;
        QVariantList itemsBefore;
        QVariantList itemsAfter;
        QString removedMarkdownBlock;
    };

    const Book *currentBook() const;
    const Page *currentPage() const;
    Page &currentPage(State &state) const;
    bool createBookWithFirstPage(const QString &title, const QString &folderId, Page page, const QString &workspace = {});
    static int findPageItem(const Page &page, const QString &id);
    static int findLayer(const Page &page, const QString &id);
    static void materializeLayers(Page &page);
    static QVariantList visibleStrokes(const Page &page);
    static QVariantList &inkForEdit(Page &page);
    static const QVariantList &activeLayerInk(const Page &page);
    bool commitLayers(State next);
    static int findBook(const State &state, const QString &id);
    static int findPage(const Book &book, const QString &id);
    static int resumePage(const Book &book);
    static int pageAtRevision(const Book &book, const QString &revision, const QString &pageId, int pageNumber);
    static void resetBookIds(Book &book, const QString &newBookId);
    static void preserveReadingPosition(Book &incoming, const Book &local);
    static void preserveMarkdownBlocks(Book &incoming, const Book &local);
    bool openBookPage(int book, int page);
    struct LinkLocation { int book; int page; int item; };
    std::optional<LinkLocation> pageLinkTarget(const QVariantMap &link) const;
    static int findFolder(const State &state, const QString &id);
    static QString markdownTextOf(const Book &book);
    static bool isPureMarkdown(const Book &book);
    static bool hasMarkdownPageAnnotations(const Page &page);
    static bool hasMarkdownSource(const Book &book);
    static QString markdownSourceOf(const Book &book);
    bool replaceMarkdownSource(int bookIndex, const QString &text);
    static Page newMarkdownPage(const QString &fragment);
    QList<QString> validatedMarkdownFragments(const QString &text);
    static void selectActiveBook(State &state);
    static QVariant pageStorageValue(const Page &page);
    static QJsonArray writePages(const Book &book);
    static QByteArray revisionOf(const Book &book);
    static int dataVersion(const State &state);
    static QByteArray serialize(const State &state);
    static QString readState(const QByteArray &contents, State &state);
    QVariantMap mergeImport(const QByteArray &contents, bool preview, const QString &token = {}, bool localFile = false);
    QString load();
    bool commit(State next, int changes, bool enforceSizeLimit = false);
    bool commitInkEdit(InkEdit edit);
    bool insertPageWithItem(const QVariantMap &item, bool transcript);
    bool applyInkHistory(bool reverse);
    bool applyInkEdit(Page &page, InkEdit &edit, bool reverse);
    bool selectionCurrent(const QVariantMap &selection) const;
    bool fail(const QString &message);
    void loadReadingPreferences();
    bool saveReadingPreferences(const QVariantMap &values);
    void setError(const QString &message);

    QVariantMap sourceForPage(const Book &book, const Page &page) const;
    bool importPreparedDocument(PreparedDocument prepared, const QString &title, const QString &folderId,
                                bool activate = true, QString *importedId = nullptr);
    bool importMarkdownFile(const QUrl &file, const QString &folderId, bool activate = true, QString *importedId = nullptr);
    PageStore::BlobCache m_storageBlobs;
    DocumentCancellation m_importCancellation;
    bool m_importing = false;
    qreal m_importProgress = 0;
    QString m_importStatus;
    QString m_filePath;
    QByteArray m_fileRevision;
    QString m_error;
    QString m_loadError;
    QVariantMap m_readingPreferences;
    QByteArray m_readingPreferencesBytes;
    QString m_readingPreferencesLoadError;
    bool m_recoveredStorage = false;
    State m_state;
    QList<InkEdit> m_undo;
    QList<InkEdit> m_redo;
};
