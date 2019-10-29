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
#include <QDialog>
#include <QLabel>
#include <QRegularExpression>
#include <QPalette>
#include <QPlainTextEdit>
#include <QFutureWatcher>
#include <QPixmap>
#include <QImage>
#include <QPointer>
#include <QProgressBar>
#include <QElapsedTimer>
#include <QSslError>
#include <QNetworkReply>
#include <QStyledItemDelegate>
#include <QCache>
#include <QSortFilterProxyModel>
#include <QDir>

#include "qt/filedropwidget.h"

#ifndef CXX_BUILD
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <retro_assert.h>
#include <retro_common_api.h>
#include <queues/task_queue.h>

#include "../ui_companion_driver.h"
#include "../../retroarch.h"

#ifndef CXX_BUILD
}
#endif

#define ALL_PLAYLISTS_TOKEN "|||ALL|||"
#define ICON_PATH "/xmb/dot-art/png/"
#define THUMBNAIL_BOXART "Named_Boxarts"
#define THUMBNAIL_SCREENSHOT "Named_Snaps"
#define THUMBNAIL_TITLE "Named_Titles"

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
class QSettings;
class QCheckBox;
class QSpinBox;
class QFormLayout;
class QStyle;
class QScrollArea;
class QSlider;
class QDragEnterEvent;
class QDropEvent;
class QNetworkAccessManager;
class QNetworkReply;
class QProgressDialog;
class LoadCoreWindow;
class MainWindow;
class ThumbnailWidget;
class ThumbnailLabel;
class GridView;
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
class ShaderParamsDialog;
#endif
class CoreOptionsDialog;
class CoreInfoDialog;
class PlaylistEntryDialog;
class ViewOptionsDialog;

enum SpecialPlaylist
{
   SPECIAL_PLAYLIST_HISTORY
};

enum ThumbnailType
{
   THUMBNAIL_TYPE_BOXART,
   THUMBNAIL_TYPE_SCREENSHOT,
   THUMBNAIL_TYPE_TITLE_SCREEN,
};

class PlaylistModel : public QAbstractListModel
{
   Q_OBJECT

public:
   enum Roles
   {
      HASH = Qt::UserRole + 1,
      THUMBNAIL
   };

   PlaylistModel(QObject *parent = 0);

   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
   Qt::ItemFlags flags(const QModelIndex &index) const;
   bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
   int rowCount(const QModelIndex &parent = QModelIndex()) const;
   int columnCount(const QModelIndex &parent = QModelIndex()) const;
   void addPlaylistItems(const QStringList &paths, bool add = false);
   void addDir(QString path, QFlags<QDir::Filter> showHidden);
   void setThumbnailType(const ThumbnailType type);
   void loadThumbnail(const QModelIndex &index);
   void reloadThumbnail(const QModelIndex &index);
   void reloadThumbnailPath(const QString path);
   void reloadSystemThumbnails(const QString system);
   void setThumbnailCacheLimit(int limit);
   bool isSupportedImage(const QString path) const;
   QString getPlaylistThumbnailsDir(const QString playlistName) const;
   QString getSanitizedThumbnailName(QString label) const;

signals:
   void imageLoaded(const QImage image, const QModelIndex &index, const QString &path);

private slots:
   void onImageLoaded(const QImage image, const QModelIndex &index, const QString &path);

private:
   QVector<QHash<QString, QString> > m_contents;
   QCache<QString, QPixmap> m_cache;
   QSet<QString> m_pendingImages;
   QVector<QByteArray> m_imageFormats;
   QRegularExpression m_fileSanitizerRegex;
   ThumbnailType m_thumbnailType = THUMBNAIL_TYPE_BOXART;
   QString getThumbnailPath(const QModelIndex &index, QString type) const;
   QString getThumbnailPath(const QHash<QString, QString> &hash, QString type) const;
   QString getCurrentTypeThumbnailPath(const QModelIndex &index) const;
   void getPlaylistItems(QString path);
   void loadImage(const QModelIndex &index, const QString &path);
};

class ThumbnailWidget : public QStackedWidget
{
   Q_OBJECT
public:
   ThumbnailWidget(QWidget *parent = 0);
   ThumbnailWidget(ThumbnailType type, QWidget *parent = 0);
   ThumbnailWidget(const ThumbnailWidget& other) { retro_assert(false && "DONT EVER USE THIS"); }

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
   void resizeEvent(QResizeEvent *event);
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
   void columnCountChanged(int oldCount, int newCount);
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
   bool isEditorOpen();
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
   bool isExiting() const;

private slots:
   void onLastWindowClosed();
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

class FileSystemProxyModel : public QSortFilterProxyModel
{
protected:
   virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;
   void sort(int column, Qt::SortOrder order = Qt::AscendingOrder);
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
   TableView* contentTableView();
   QTableView* fileTableView();
   FileDropWidget* playlistViews();
   GridView* contentGridView();
   QWidget* playlistViewsAndFooter();
   QWidget* searchWidget();
   QLineEdit* searchLineEdit();
   QComboBox* launchWithComboBox();
   QToolButton* startCorePushButton();
   QToolButton* coreInfoPushButton();
   QToolButton* runPushButton();
   QToolButton* stopPushButton();
   QTabWidget* browserAndPlaylistTabWidget();
   QString getPlaylistDefaultCore(QString dbName);
   ViewOptionsDialog* viewOptionsDialog();
   QSettings* settings();
   QVector<QHash<QString, QString> > getCoreInfo();
   void setTheme(Theme theme = THEME_SYSTEM_DEFAULT);
   Theme theme();
   Theme getThemeFromString(QString themeString);
   QString getThemeString(Theme theme);
   QHash<QString, QString> getSelectedCore();
   void showStatusMessage(QString msg, unsigned priority, unsigned duration, bool flush);
   bool showMessageBox(QString msg, MessageBoxType msgType = MSGBOX_TYPE_INFO, Qt::WindowModality modality = Qt::ApplicationModal, bool showDontAsk = true, bool *dontAsk = NULL);
   bool setCustomThemeFile(QString filePath);
   void setCustomThemeString(QString qss);
   const QString& customThemeString() const;
   void setCurrentViewType(ViewType viewType);
   QString getCurrentViewTypeString();
   ViewType getCurrentViewType();
   void setCurrentThumbnailType(ThumbnailType thumbnailType);
   QString getCurrentThumbnailTypeString();
   ThumbnailType getCurrentThumbnailType();
   ThumbnailType getThumbnailTypeFromString(QString thumbnailType);
   void setAllPlaylistsListMaxCount(int count);
   void setAllPlaylistsGridMaxCount(int count);
   void setThumbnailCacheLimit(int count);
   PlaylistEntryDialog* playlistEntryDialog();
   void addFilesToPlaylist(QStringList files);
   QString getCurrentPlaylistPath();
   QModelIndex getCurrentContentIndex();
   QHash<QString, QString> getCurrentContentHash();
   QHash<QString, QString> getFileContentHash(const QModelIndex &index);
   static double lerp(double x, double y, double a, double b, double d);
   QString getSpecialPlaylistPath(SpecialPlaylist playlist);
   QVector<QPair<QString, QString> > getPlaylists();
   QString getScrubbedString(QString str);
   void setDefaultCustomProperties();
   void setIconViewZoom(int zoomValue);

signals:
   void thumbnailChanged(const QPixmap &pixmap);
   void thumbnail2Changed(const QPixmap &pixmap);
   void thumbnail3Changed(const QPixmap &pixmap);
   void gotLogMessage(const QString &msg);
   void gotStatusMessage(QString msg, unsigned priority, unsigned duration, bool flush);
   void gotReloadPlaylists();
   void gotReloadShaderParams();
   void gotReloadCoreOptions();
   void showErrorMessageDeferred(QString msg);
   void showInfoMessageDeferred(QString msg);
   void extractArchiveDeferred(QString path, QString extractionDir, QString tempExtension, retro_task_callback_t cb);
   void itemChanged();
   void updateThumbnails();
   void gridItemChanged(QString title);
   void gotThumbnailDownload(QString system, QString title);
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
   void loadContent(const QHash<QString, QString> &contentHash);
   void onStartCoreClicked();
   void onDropWidgetEnterPressed();
   void selectBrowserDir(QString path);
   void setThumbnail(QString widgetName, QPixmap &pixmap, bool acceptDrop);
   void onResizeThumbnailOne(QPixmap &pixmap, bool acceptDrop);
   void onResizeThumbnailTwo(QPixmap &pixmap, bool acceptDrop);
   void onResizeThumbnailThree(QPixmap &pixmap, bool acceptDrop);
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
   void onTabWidgetIndexChanged(int index);
   void deleteCurrentPlaylistItem();
   void onFileDropWidgetContextMenuRequested(const QPoint &pos);
   void showAbout();
   void showDocs();
   void updateRetroArchNightly();
   void onUpdateRetroArchFinished(bool success);
   void onThumbnailPackExtractFinished(bool success);
   void deferReloadShaderParams();
   void downloadThumbnail(QString system, QString title, QUrl url = QUrl());
   void downloadAllThumbnails(QString system, QUrl url = QUrl());
   void downloadPlaylistThumbnails(QString playlistPath);
   void downloadNextPlaylistThumbnail(QString system, QString title, QString type, QUrl url = QUrl());
   void changeThumbnailType(ThumbnailType type);
   void onThumbnailDropped(const QImage &image, ThumbnailType type);

private slots:
   void onLoadCoreClicked(const QStringList &extensionFilters = QStringList());
   void onUnloadCoreMenuAction();
   void onTimeout();
   void onCoreLoaded();
   void onCurrentTableItemDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);
   void onCurrentListItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
   void onCurrentListItemDataChanged(QListWidgetItem *item);
   void onCurrentItemChanged(const QModelIndex &index);
   void onCurrentItemChanged(const QHash<QString, QString> &hash);
   void onCurrentFileChanged(const QModelIndex &index);
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
   void onFileBrowserTableDirLoaded(const QString &path);
   void onDownloadScroll(QString path);
   void onDownloadScrollAgain(QString path);
   int onExtractArchive(QString path, QString extractionDir, QString tempExtension, retro_task_callback_t cb);

   void onUpdateNetworkError(QNetworkReply::NetworkError code);
   void onUpdateNetworkSslErrors(const QList<QSslError> &errors);
   void onRetroArchUpdateDownloadFinished();
   void onUpdateDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
   void onUpdateDownloadReadyRead();
   void onUpdateDownloadCanceled();

   void onThumbnailDownloadNetworkError(QNetworkReply::NetworkError code);
   void onThumbnailDownloadNetworkSslErrors(const QList<QSslError> &errors);
   void onThumbnailDownloadFinished();
   void onThumbnailDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
   void onThumbnailDownloadReadyRead();
   void onThumbnailDownloadCanceled();
   void onDownloadThumbnail(QString system, QString title);

   void onThumbnailPackDownloadNetworkError(QNetworkReply::NetworkError code);
   void onThumbnailPackDownloadNetworkSslErrors(const QList<QSslError> &errors);
   void onThumbnailPackDownloadFinished();
   void onThumbnailPackDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
   void onThumbnailPackDownloadReadyRead();
   void onThumbnailPackDownloadCanceled();

   void onPlaylistThumbnailDownloadNetworkError(QNetworkReply::NetworkError code);
   void onPlaylistThumbnailDownloadNetworkSslErrors(const QList<QSslError> &errors);
   void onPlaylistThumbnailDownloadFinished();
   void onPlaylistThumbnailDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
   void onPlaylistThumbnailDownloadReadyRead();
   void onPlaylistThumbnailDownloadCanceled();

   void startTimer();
   void updateVisibleItems();

private:
   void setCurrentCoreLabel();
   void getPlaylistFiles();
   bool isCoreLoaded();
   bool isContentLessCore();
   bool updateCurrentPlaylistEntry(const QHash<QString, QString> &contentHash);
   int extractArchive(QString path);
   void removeUpdateTempFiles();
   bool addDirectoryFilesToList(QProgressDialog *dialog, QStringList &list, QDir &dir, QStringList &extensions);
   void renamePlaylistItem(QListWidgetItem *item, QString newName);
   bool currentPlaylistIsSpecial();
   bool currentPlaylistIsAll();
   void applySearch();
   void updateItemsCount();
   QString changeThumbnail(const QImage &image, QString type);

   PlaylistModel *m_playlistModel;
   QSortFilterProxyModel *m_proxyModel;
   FileSystemProxyModel *m_proxyFileModel;
   LoadCoreWindow *m_loadCoreWindow;
   QTimer *m_timer;
   QString m_currentCore;
   QString m_currentCoreVersion;
   QLabel *m_statusLabel;
   TreeView *m_dirTree;
   QFileSystemModel *m_dirModel;
   QFileSystemModel *m_fileModel;
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
   QPixmap *m_thumbnailPixmap;
   QPixmap *m_thumbnailPixmap2;
   QPixmap *m_thumbnailPixmap3;
   QSettings *m_settings;
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
   QVector<QByteArray> m_imageFormats;
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
   QHash<QString, QString> m_currentGridHash;
   QPointer<ThumbnailWidget> m_currentGridWidget;
   int m_allPlaylistsListMaxCount;
   int m_allPlaylistsGridMaxCount;
   PlaylistEntryDialog *m_playlistEntryDialog;
   QElapsedTimer m_statusMessageElapsedTimer;
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   QPointer<ShaderParamsDialog> m_shaderParamsDialog;
#endif
   QPointer<CoreOptionsDialog> m_coreOptionsDialog;
   QNetworkAccessManager *m_networkManager;

   QProgressDialog *m_updateProgressDialog;
   QFile m_updateFile;
   QPointer<QNetworkReply> m_updateReply;

   QProgressDialog *m_thumbnailDownloadProgressDialog;
   QFile m_thumbnailDownloadFile;
   QPointer<QNetworkReply> m_thumbnailDownloadReply;
   QStringList m_pendingThumbnailDownloadTypes;

   QProgressDialog *m_thumbnailPackDownloadProgressDialog;
   QFile m_thumbnailPackDownloadFile;
   QPointer<QNetworkReply> m_thumbnailPackDownloadReply;

   QProgressDialog *m_playlistThumbnailDownloadProgressDialog;
   QFile m_playlistThumbnailDownloadFile;
   QPointer<QNetworkReply> m_playlistThumbnailDownloadReply;
   QVector<QHash<QString, QString> > m_pendingPlaylistThumbnails;
   unsigned m_downloadedThumbnails;
   unsigned m_failedThumbnails;
   bool m_playlistThumbnailDownloadWasCanceled;
   QString m_pendingDirScrollPath;

   QTimer *m_thumbnailTimer;
   GridItem m_gridItem;
   BrowserType m_currentBrowser;
   QRegExp m_searchRegExp;
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
