/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _QT_UI
#define _QT_UI

#include <QObject>
#include <QMainWindow>
#include <QTreeView>
#include <QListWidget>
#include <QTableView>
#include <QFrame>
#include <QWidget>
#include <QLabel>
#include <QRegularExpression>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QTimer>
#include <QHash>
#include <QPersistentModelIndex>
#include <QAbstractTableModel>
#include <QIcon>
#include <QImage>
#include <QPointer>
#include <QProgressBar>
#include <QElapsedTimer>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QCache>
#include <QSortFilterProxyModel>
#include <QDir>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "ui_qt_widgets.h"

#ifndef CXX_BUILD
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <retro_common_api.h>
#include <queues/task_queue.h>

#include "../ui_companion_driver.h"
#include "../companion/companion_core.h"

/* Shared companion core owned by the running Qt companion; NULL when
 * the Qt companion has not been initialised. */
companion_core_t *ui_companion_qt_core(void);
#include "../../retroarch.h"
#include <formats/image.h>

#ifndef CXX_BUILD
}
#endif

#define ALL_PLAYLISTS_TOKEN "|||ALL|||"
#define ICON_PATH "/xmb/dot-art/png/"
#define THUMBNAIL_BOXART     COMPANION_THUMB_BOXART
#define THUMBNAIL_SCREENSHOT COMPANION_THUMB_SCREENSHOT
#define THUMBNAIL_TITLE      COMPANION_THUMB_TITLE
#define THUMBNAIL_LOGO       COMPANION_THUMB_LOGO

class QApplication;
class QCloseEvent;
class QKeyEvent;
class QTimer;
class QFileSystemModel;
class QListWidgetItem;
class QTableWidgetItem;
class QResizeEvent;
class QDockWidget;
class QComboBox;
class QPushButton;
class QToolButton;
class QTabWidget;
class QPixmap;
class QPaintEvent;
class QCheckBox;
class QSpinBox;
class QFormLayout;
class QStyle;
class QScrollArea;
class QSlider;
class QDragEnterEvent;
class QDropEvent;
class QProgressDialog;
class ThumbnailLabel;

enum SpecialPlaylist
{
   SPECIAL_PLAYLIST_HISTORY
};

enum ThumbnailType
{
   THUMBNAIL_TYPE_BOXART,
   THUMBNAIL_TYPE_SCREENSHOT,
   THUMBNAIL_TYPE_TITLE_SCREEN,
   THUMBNAIL_TYPE_LOGO,
};

static inline double lerp(double x, double y, double a, double b, double d)
{
   return a + (b - a) * ((double)(d - x) / (double)(y - x));
}


extern "C" {
#include "../companion/companion_thumbs.h"
}

class PlaylistModel : public QAbstractListModel
{
   Q_OBJECT

public:
   enum Roles
   {
      ENTRY = Qt::UserRole + 1,
      THUMBNAIL
   };

   PlaylistModel(QObject *parent = 0);
   ~PlaylistModel();

   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
   Qt::ItemFlags flags(const QModelIndex &index) const;
   bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
   int rowCount(const QModelIndex &parent = QModelIndex()) const;
   int columnCount(const QModelIndex &parent = QModelIndex()) const;
   void addPlaylistItems(const QStringList &paths, bool add = false);
   /* True while addPlaylistItems() is still waiting on the companion
    * core's budgeted parse of one or more playlists. */
   bool isLoadingPlaylists() const;
   /* Companion core notification: the requested playlist finished
    * parsing. Appends its entries and starts the next pending one. */
   void onCorePlaylistChanged();
   void addDir(QString path, QFlags<QDir::Filter> showHidden);
   void setThumbnailType(const ThumbnailType type);
   void loadThumbnail(const QModelIndex &index);
   void reloadThumbnail(const QModelIndex &index);
   void reloadThumbnailPath(const QString path);
   void reloadSystemThumbnails(const QString system);
   void setThumbnailCacheLimit(int limit);
   /* Edge (px) the grid draws thumbnails at; requests are made at this
    * size so the engine hands back cells that need no further scaling. */
   void setThumbnailSize(int size);
   int thumbnailSize() const { return m_thumbSize; }
   /* Any image at any size through the same engine (the sidebar
    * thumbnails and the file-browser preview). imageAt() gives the
    * cached pixmap; requestImage() queues a decode, after which
    * thumbnailReady(path) is emitted. */
   bool imageAt(const QString &path, int w, int h, QPixmap *out) const;
   void requestImage(const QString &path, int w, int h);
   /* Play @path in the sidebar (APNG / animated WEBP / WEBM / MP4 - a
    * still plays nothing): frames come as frameReady(path, pixmap). */
   void animateImage(const QString &path, int w, int h);
   void stopAnimation();
   /* Drop queued decodes and abandon those in flight (the view moved
    * on); cached images stay. */
   void abandonPending();
   bool isSupportedImage(const QString path) const;
   QString getPlaylistThumbnailsDir(const QString playlistName, const QString type) const;
   /* Repository thumbnail file for a label, ignoring whether the content
    * itself is an image (the save target / the sidebar images). */
   QString getRepositoryThumbnailPath(const QString playlistName, const QString labelNoExt, const QString type) const;

signals:
   void imageLoaded(const QImage image, const QModelIndex &index, const QString &path);
   /* Emitted once addPlaylistItems() has populated the model. */
   void playlistsLoaded();

private slots:
   void pollThumbnails();
signals:
   void thumbnailReady(const QString &path);
   void frameReady(const QString &path, const QPixmap &frame);

private:
   /* Thumbnails come from the shared companion engine (decode threads,
    * path+size keyed cache, visible-first queue) - the same one the
    * native Win32 and Cocoa companions draw through. m_cache only holds
    * QPixmap conversions of engine pixels, keyed path@size. */
   companion_thumbs_t *m_engine = NULL;
   int m_thumbSize = 256;
   QHash<QString, QPersistentModelIndex> m_pendingRows;
   QTimer m_pollTimer;
   static void onEngineDone(void *ud, const char *path, int w, int h,
         uintptr_t tag, const uint32_t *bits);
   void thumbnailArrived(const QString &path);
   QVector<PlaylistEntry> m_contents;
   /* addPlaylistItems() state: playlists still to load, and the
    * entries collected so far (committed to m_contents in one reset). */
   QStringList m_pendingPaths;
   QVector<PlaylistEntry> m_pendingContents;
   bool m_loadingPlaylists = false;
   void appendEntriesFromCore();
   void startNextPendingPlaylist();
   mutable QCache<QString, QPixmap> m_cache; /* filled lazily from data() */
   /* stages of imageAt()/data(): one conversion per (path, w, h) */
   QPixmap *pixmapFor(const QString &path, int w, int h) const;
   ThumbnailType m_thumbnailType = THUMBNAIL_TYPE_BOXART;
   QString getThumbnailPath(const QModelIndex &index, QString type) const;
   QString getThumbnailPath(const PlaylistEntry &entry, QString type) const;
   QString getCurrentTypeThumbnailPath(const QModelIndex &index) const;
};

class ThumbnailWidget : public QStackedWidget
{
   Q_OBJECT
public:
   ThumbnailWidget(QWidget *parent = 0);
   ThumbnailWidget(ThumbnailType type, QWidget *parent = 0);
   ThumbnailWidget(const ThumbnailWidget& other) { /* DONT EVER USE THIS */ }

   void setPixmap(const QPixmap &pixmap, bool acceptDrops);
signals:
   void filesDropped(const QImage& image, ThumbnailType type);
private:
   QSize m_sizeHint;
   ThumbnailType m_thumbnailType;
   ThumbnailLabel *m_thumbnailLabel;
   QLabel *m_dropIndicator;
protected:
   void dragEnterEvent(QDragEnterEvent *event);
   void dragMoveEvent(QDragMoveEvent *event);
   void dropEvent(QDropEvent *event);
};

class ThumbnailLabel : public QWidget
{
   Q_OBJECT
public:
   ThumbnailLabel(QWidget *parent = 0);
   ~ThumbnailLabel();
   QSize sizeHint() const;
public slots:
   void setPixmap(const QPixmap &pixmap);
protected:
   void paintEvent(QPaintEvent *event);
private:
   void updateMargins();

   QPixmap *m_pixmap;
   int m_pixmapWidth;
   int m_pixmapHeight;
};

class TreeView : public QTreeView
{
   Q_OBJECT
public:
   TreeView(QWidget *parent = 0);
signals:
   void itemsSelected(QModelIndexList selectedIndexes);
protected slots:
   void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
};

class TableView : public QTableView
{
   Q_OBJECT
public:
   TableView(QWidget *parent = 0);
   bool isEditorOpen();
};

class ListWidget : public QListWidget
{
   Q_OBJECT
public:
   ListWidget(QWidget *parent = 0);
signals:
   void enterPressed();
   void deletePressed();
protected:
   void keyPressEvent(QKeyEvent *event);
};

class AppHandler : public QObject
{
   Q_OBJECT

public:
   AppHandler(QObject *parent = 0);
   ~AppHandler();
   void exit();
};

class CoreInfoLabel : public QLabel
{
   Q_OBJECT
public:
   CoreInfoLabel(QString text = QString(), QWidget *parent = 0);
};

class CoreInfoWidget : public QWidget
{
   Q_OBJECT
public:
   CoreInfoWidget(CoreInfoLabel *label, QWidget *parent = 0);
   QSize sizeHint() const;
protected:
   void resizeEvent(QResizeEvent *event);
private:
   CoreInfoLabel *m_label;
   QScrollArea *m_scrollArea;
};

class LogTextEdit : public QPlainTextEdit
{
   Q_OBJECT
public:
   LogTextEdit(QWidget *parent = 0);
public slots:
   void appendMessage(const QString& text);
};

/* Used to store styling since delegates don't inherit QWidget. */
class GridItem : public QWidget
{
   Q_OBJECT

   Q_PROPERTY(QString thumbnailvalign READ getThumbnailVerticalAlign WRITE setThumbnailVerticalAlign)
   Q_PROPERTY(int padding READ getPadding WRITE setPadding)

public:
   GridItem(QWidget* parent);

   Qt::AlignmentFlag thumbnailVerticalAlignmentFlag;
   int padding;

   int getPadding() const;
   void setPadding(const int value);
   QString getThumbnailVerticalAlign() const;
   void setThumbnailVerticalAlign(const QString valign);
};

/* The file browser's table, backed by the companion core's browse
 * listing (enumerated, stat'ed and formatted off the UI thread, shared
 * by every companion): this class only paints it. Columns are Qt's
 * Name / Size / Type / Date Modified. A search filter keeps a row map. */
class BrowseTableModel : public QAbstractTableModel
{
   Q_OBJECT
public:
   BrowseTableModel(QObject *parent = 0) : QAbstractTableModel(parent) {}
   int rowCount(const QModelIndex &parent = QModelIndex()) const;
   int columnCount(const QModelIndex &parent = QModelIndex()) const { (void)parent; return 4; }
   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
   /* The core listing changed: rebuild the row map and reset. */
   void reload();
   void setFilter(const QRegularExpression &re);
   /* Header click: the core sorts, every companion the same way. */
   void sort(int column, Qt::SortOrder order = Qt::AscendingOrder);
   /* Browse index (into the core listing) behind a table row, or -1. */
   long browseIndex(const QModelIndex &index) const;
   QString pathAt(const QModelIndex &index) const;
   bool isDirAt(const QModelIndex &index) const;
private:
   QVector<long> m_rows;   /* table row -> browse index */
   QRegularExpression m_filter;
   QIcon m_folderIcon, m_fileIcon, m_driveIcon;
};

class LoadCoreTableWidget : public QTableWidget
{
   Q_OBJECT
public:
   LoadCoreTableWidget(QWidget *parent = NULL);
signals:
   void enterPressed();
protected:
   void keyPressEvent(QKeyEvent *event);
};

class LoadCoreWindow : public QMainWindow
{
   Q_OBJECT
public:
   LoadCoreWindow(QWidget *parent = 0);
   /* @contentPath: when non-empty, only cores that can run it (by
    * extension, archive members included) are shown. */
   void initCoreList(const QString &contentPath = QString());
   void setStatusLabel(QString label);
signals:
   void coreLoaded();
   void windowClosed();
private slots:
   void onLoadCustomCoreClicked();
   void onCoreEnterPressed();
   void onCellDoubleClicked(int row, int column);
protected:
   void keyPressEvent(QKeyEvent *event);
   void closeEvent(QCloseEvent *event);
private:
   void loadCore(const char *path);

   QVBoxLayout m_layout;
   LoadCoreTableWidget *m_table;
   QLabel *m_statusLabel;
};

class MainWindow : public QMainWindow
{
   Q_OBJECT

public:
   enum ViewType
   {
      VIEW_TYPE_ICONS,
      VIEW_TYPE_LIST
   };

   enum BrowserType
   {
      BROWSER_TYPE_PLAYLISTS,
      BROWSER_TYPE_FILES
   };

   enum Theme
   {
      THEME_SYSTEM_DEFAULT,
      THEME_DARK,
      THEME_CUSTOM
   };

   enum MessageBoxType
   {
      MSGBOX_TYPE_INFO,
      MSGBOX_TYPE_WARNING,
      MSGBOX_TYPE_ERROR,
      MSGBOX_TYPE_QUESTION_YESNO,
      MSGBOX_TYPE_QUESTION_OKCANCEL,
   };

   MainWindow(QWidget *parent = NULL);
   ~MainWindow();
   TreeView* dirTreeView();
   PlaylistModel* playlistModel();
   ListWidget* playlistListWidget();
   QStackedWidget* centralWidget();
   QTableView* fileTableView();
   FileDropWidget* playlistViews();
   QWidget* playlistViewsAndFooter();
   QLineEdit* searchLineEdit();
   QComboBox* launchWithComboBox();
   QToolButton* startCorePushButton();
   QToolButton* coreInfoPushButton();
   QToolButton* runPushButton();
   QToolButton* stopPushButton();
   QTabWidget* browserAndPlaylistTabWidget();
   QString getPlaylistDefaultCore(QString plName);
   ViewOptionsDialog* viewOptionsDialog();
   QVector<QHash<QString, QString> > getCoreInfo();
   void setTheme(Theme theme = THEME_SYSTEM_DEFAULT);
   Theme theme();
   Theme getThemeFromString(QString themeString);
   const char *getThemeString(Theme theme);
   QString getSelectedCorePath();
   void showStatusMessage(QString msg, unsigned priority, unsigned duration, bool flush);
   bool showMessageBox(QString msg, MessageBoxType msgType = MSGBOX_TYPE_INFO, Qt::WindowModality modality = Qt::ApplicationModal, bool showDontAsk = true, bool *dontAsk = NULL);
   bool setCustomThemeFile(QString filePath);
   void setCustomThemeString(QString qss);
   const QString& customThemeString() const;
   void setCurrentViewType(ViewType viewType);
   const char *getCurrentViewTypeString();
   ViewType getCurrentViewType();
   void setCurrentThumbnailType(ThumbnailType thumbnailType);
   const char *getCurrentThumbnailTypeString();
   ThumbnailType getCurrentThumbnailType();
   ThumbnailType getThumbnailTypeFromString(QString thumbnailType);
   void setAllPlaylistsListMaxCount(int count);
   void setAllPlaylistsGridMaxCount(int count);
   void setThumbnailCacheLimit(int count);
   PlaylistEntryDialog* playlistEntryDialog();
   void addFilesToPlaylist(QStringList files);
   QString getCurrentPlaylistPath();
   QModelIndex getCurrentContentIndex();
   PlaylistEntry getCurrentContentEntry();
   PlaylistEntry getFileContentEntry(const QModelIndex &index);
   QString getSpecialPlaylistPath(SpecialPlaylist playlist);
   QVector<QPair<QString, QString> > getPlaylists();
   void setDefaultCustomProperties();
   void setIconViewZoom(int zoomValue);

signals:
   void gotLogMessage(const QString &msg);
   void gotStatusMessage(QString msg, unsigned priority, unsigned duration, bool flush);
   void gotReloadPlaylists();
   void gotReloadShaderParams();
   void showErrorMessageDeferred(QString msg);
   void showInfoMessageDeferred(QString msg);
   void itemChanged();
   void scrollToDownloads(QString path);
   void scrollToDownloadsAgain(QString path);

public slots:
   void onBrowserDownloadsClicked();
   void onBrowserUpClicked();
   void onBrowserStartClicked();
   void initContentTableWidget();
   void onViewClosedDocksAboutToShow();
   void onShowHiddenDockWidgetAction();
   void setCoreActions();
   void onRunClicked();
   void loadContent(const PlaylistEntry &entry);
   void onStartCoreClicked();
   void onDropWidgetEnterPressed();
   void selectBrowserDir(QString path);
   void setThumbnail(QString widgetName, QPixmap &pixmap, bool acceptDrop);
   void appendLogMessage(const QString &msg);
   void onGotLogMessage(const QString &msg);
   void onGotStatusMessage(QString msg, unsigned priority, unsigned duration, bool flush);
   void reloadPlaylists();
   void deferReloadPlaylists();
   void onGotReloadPlaylists();
   void onGotReloadShaderParams();
   void onGotReloadCoreOptions();
   void showWelcomeScreen();
   void onIconViewClicked();
   void onListViewClicked();
   void onBoxartThumbnailClicked();
   void onScreenshotThumbnailClicked();
   void onTitleThumbnailClicked();
   void onLogoThumbnailClicked();
   void onTabWidgetIndexChanged(int index);
   void deleteCurrentPlaylistItem();
   void onFileDropWidgetContextMenuRequested(const QPoint &pos);
   void showAbout();
   void showDocs();
   /* companion core download results (see ui_companion_qt_core_callbacks) */
   void onCoreThumbnailDownloaded(QString system, QString title, QString path, bool success);
   void onCoreThumbnailPackFinished(int result);
   void onBrowseChanged();
   void onSingleThumbnailDownloadFinishedInternal(QString system, QString title, QString path, bool success);
   void onPlaylistThumbnailDownloadFinishedInternal(QString path, bool success);
   void deferReloadShaderParams();
   void downloadThumbnail(QString system, QString title, QUrl url = QUrl());
   void downloadAllThumbnails(QString system, QUrl url = QUrl());
   void downloadPlaylistThumbnails(QString playlistPath);
   void downloadNextPlaylistThumbnail(QString system, QString title, QString type, QUrl url = QUrl());
   void changeThumbnailType(ThumbnailType type);
   void onThumbnailDropped(const QImage &image, ThumbnailType type);

private slots:
   void onLoadCoreClicked(const QString &contentPath = QString());
   void onUnloadCoreMenuAction();
   void onTimeout();
   void onCoreLoaded();
   void onCurrentTableItemDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);
   void onCurrentListItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
   void onPlaylistModelLoaded();
   void onScanDirectoryClicked();
   void onQuitRetroArchClicked();
   void onCurrentListItemDataChanged(QListWidgetItem *item);
   void onCurrentItemChanged(const QModelIndex &index);
   void onCurrentItemChanged(const PlaylistEntry &entry);
   void onCurrentFileChanged(const QModelIndex &index);
   void onThumbnailReady(const QString &path);
   void onFrameReady(const QString &path, const QPixmap &frame);
   void showSidebarImage(int idx, const QString &path, bool acceptDrop);
   void onSearchEnterPressed();
   void onSearchLineEditEdited(const QString &text);
   void onContentItemDoubleClicked(const QModelIndex &index);
   void onFileDoubleClicked(const QModelIndex &index);
   void onCoreLoadWindowClosed();
   void onTreeViewItemsSelected(QModelIndexList selectedIndexes);
   void onSearchResetClicked();
   void onLaunchWithComboBoxIndexChanged(int index);
   void onFileBrowserTreeContextMenuRequested(const QPoint &pos);
   void onPlaylistWidgetContextMenuRequested(const QPoint &pos);
   void onStopClicked();
   void onZoomValueChanged(int value);
   void onPlaylistFilesDropped(QStringList files);
   void onShaderParamsClicked();
   void onCoreOptionsClicked();
   void onShowErrorMessage(QString msg);
   void onShowInfoMessage(QString msg);
   void onContributorsClicked();
   void onItemChanged();
   void onFileSystemDirLoaded(const QString &path);
   void onDownloadScroll(QString path);
   void onDownloadScrollAgain(QString path);

   void onThumbnailDownloadCanceled();
   void onDownloadThumbnail(QString system, QString title);

   void onThumbnailPackDownloadCanceled();

   void onPlaylistThumbnailDownloadCanceled();

   void startTimer();
   void updateVisibleItems();

private:
   void setCurrentCoreLabel();
   void getPlaylistFiles();
   bool updateCurrentPlaylistEntry(const PlaylistEntry &entry);
   bool addDirectoryFilesToList(QProgressDialog *dialog, QStringList &list, QDir &dir, QStringList &extensions);
   void renamePlaylistItem(QListWidgetItem *item, QString newName);
   bool currentPlaylistIsSpecial();
   bool currentPlaylistIsAll();
   void applySearch();
   void updateItemsCount();
   QString changeThumbnail(const QImage &image, QString type);
   /* Constructor helpers - keep MainWindow::MainWindow readable by
    * pulling self-contained chunks of setup out into their own
    * methods. None take parameters; everything operates on already-
    * initialised members. */
   void setupPlaylistFooter();
   void setupModels();
   void setupFileSystemBrowser();
   void setupDockWidgets();
   void setupSignalConnections();

   PlaylistModel *m_playlistModel;
   QSortFilterProxyModel *m_proxyModel;
   BrowseTableModel *m_browseModel;
   LoadCoreWindow *m_loadCoreWindow;
   QTimer *m_timer;
   QString m_currentCore;
   QString m_currentCoreVersion;
   QLabel *m_statusLabel;
   TreeView *m_dirTree;
   QFileSystemModel *m_dirModel;
   ListWidget *m_listWidget;
   QStackedWidget *m_centralWidget;
   TableView *m_tableView;
   QTableView *m_fileTableView;
   FileDropWidget *m_playlistViews;
   QWidget *m_searchWidget;
   QLineEdit *m_searchLineEdit;
   QDockWidget *m_searchDock;
   QStringList m_playlistFiles;
   QComboBox *m_launchWithComboBox;
   QToolButton *m_startCorePushButton;
   QToolButton *m_coreInfoPushButton;
   QToolButton *m_runPushButton;
   QToolButton *m_stopPushButton;
   QTabWidget *m_browserAndPlaylistTabWidget;
   bool m_pendingRun;
   /* Sidebar (four thumbnail widgets) and file-browser preview: the
    * path each widget is waiting on from the model's engine. */
   QString m_sidebarPending[4];
   bool m_sidebarAcceptDrop = false;
   ViewOptionsDialog *m_viewOptionsDialog;
   CoreInfoDialog *m_coreInfoDialog;
   QStyle *m_defaultStyle;
   QPalette m_defaultPalette;
   Theme m_currentTheme;
   QDockWidget *m_coreInfoDock;
   CoreInfoLabel *m_coreInfoLabel;
   CoreInfoWidget *m_coreInfoWidget;
   QDockWidget *m_logDock;
   QFrame *m_logWidget;
   LogTextEdit *m_logTextEdit;
   QListWidgetItem *m_historyPlaylistsItem;
   QIcon m_folderIcon;
   QString m_customThemeString;
   GridView *m_gridView;
   QWidget *m_playlistViewsAndFooter;
   QWidget *m_gridLayoutWidget;
   QSlider *m_zoomSlider;
   int m_lastZoomSliderValue;
   ViewType m_viewType;
   ThumbnailType m_thumbnailType;
   QProgressBar *m_gridProgressBar;
   QWidget *m_gridProgressWidget;
   QPointer<ThumbnailWidget> m_currentGridWidget;
   int m_allPlaylistsListMaxCount;
   int m_allPlaylistsGridMaxCount;
   PlaylistEntryDialog *m_playlistEntryDialog;
   QElapsedTimer m_statusMessageElapsedTimer;
   QPointer<ShaderParamsDialog> m_shaderParamsDialog;
   QPointer<CoreOptionsDialog> m_coreOptionsDialog;
   bool m_downloadingPlaylistThumbnails;


   QProgressDialog *m_thumbnailDownloadProgressDialog;
   QStringList m_pendingThumbnailDownloadTypes;

   QProgressDialog *m_thumbnailPackDownloadProgressDialog;

   QProgressDialog *m_playlistThumbnailDownloadProgressDialog;
   QList<QHash<QString, QString> > m_pendingPlaylistThumbnails;
   unsigned m_downloadedThumbnails;
   unsigned m_failedThumbnails;
   bool m_playlistThumbnailDownloadWasCanceled;
   QString m_pendingDirScrollPath;

   QTimer *m_thumbnailTimer;
   GridItem m_gridItem;
   BrowserType m_currentBrowser;
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
   QRegularExpression m_searchRegularExpression;
#else
   QRegExp m_searchRegExp;
#endif
   QByteArray m_fileTableHeaderState;
   QWidget *m_zoomWidget;
   QString m_itemsCountLiteral;
   QLabel *m_itemsCountLabel;

protected:
   void closeEvent(QCloseEvent *event);
   void keyPressEvent(QKeyEvent *event);
};

Q_DECLARE_METATYPE(ThumbnailWidget)
Q_DECLARE_METATYPE(QPointer<ThumbnailWidget>)
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
Q_DECLARE_METATYPE(struct video_shader_parameter*)
#endif

RETRO_BEGIN_DECLS

typedef struct ui_application_qt
{
   QApplication *app;
} ui_application_qt_t;

typedef struct ui_window_qt
{
   MainWindow *qtWindow;
} ui_window_qt_t;

RETRO_END_DECLS

#endif
