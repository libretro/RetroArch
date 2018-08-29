/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2018 - Brad Parker
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
#include <QTableWidget>
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

extern "C" {
#include <retro_assert.h>
#include <retro_common_api.h>
#include <queues/task_queue.h>
#include "../ui_companion_driver.h"
#include "../../gfx/video_driver.h"
}

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
class QListWidget;
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
class FlowLayout;
class ShaderParamsDialog;
class CoreOptionsDialog;
class CoreInfoDialog;
class PlaylistEntryDialog;
class ViewOptionsDialog;

enum SpecialPlaylist
{
   SPECIAL_PLAYLIST_HISTORY
};

class GridItem : public QObject
{
   Q_OBJECT
public:
   GridItem();

   QPointer<ThumbnailWidget> widget;
   QPointer<ThumbnailLabel> label;
   QHash<QString, QString> hash;
   QImage image;
   QPixmap pixmap;
   QFutureWatcher<GridItem*> imageWatcher;
   QString labelText;
};

class ThumbnailWidget : public QFrame
{
   Q_OBJECT
public:
   ThumbnailWidget(QWidget *parent = 0);
   ThumbnailWidget(const ThumbnailWidget& other) { retro_assert(false && "DONT EVER USE THIS"); }

   QSize sizeHint() const;
   void setSizeHint(QSize size);
signals:
   void mouseDoubleClicked();
   void mousePressed();
private:
   QSize m_sizeHint;
protected:
   void paintEvent(QPaintEvent *event);
   void resizeEvent(QResizeEvent *event);
   void mouseDoubleClickEvent(QMouseEvent *event);
   void mousePressEvent(QMouseEvent *event);
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

class TableWidget : public QTableWidget
{
   Q_OBJECT
public:
   TableWidget(QWidget *parent = 0);
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

class MainWindow : public QMainWindow
{
   Q_OBJECT

public:
   enum ViewType
   {
      VIEW_TYPE_ICONS,
      VIEW_TYPE_LIST
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
   QListWidget* playlistListWidget();
   TableWidget* contentTableWidget();
   FlowLayout* contentGridLayout();
   QWidget* contentGridWidget();
   QWidget* searchWidget();
   QLineEdit* searchLineEdit();
   QComboBox* launchWithComboBox();
   QToolButton* startCorePushButton();
   QToolButton* coreInfoPushButton();
   QToolButton* runPushButton();
   QToolButton* stopPushButton();
   QTabWidget* browserAndPlaylistTabWidget();
   QVector<QHash<QString, QString> > getPlaylistDefaultCores();
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
   GridItem* doDeferredImageLoad(GridItem *item, QString path);
   void setCurrentViewType(ViewType viewType);
   QString getCurrentViewTypeString();
   ViewType getCurrentViewType();
   void setAllPlaylistsListMaxCount(int count);
   void setAllPlaylistsGridMaxCount(int count);
   PlaylistEntryDialog* playlistEntryDialog();
   void addFilesToPlaylist(QStringList files);
   QString getCurrentPlaylistPath();
   QHash<QString, QString> getCurrentContentHash();
   static double lerp(double x, double y, double a, double b, double d);
   QString getSpecialPlaylistPath(SpecialPlaylist playlist);
   QVector<QPair<QString, QString> > getPlaylists();
   QString getScrubbedString(QString str);

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
   void gridItemChanged(QString title);
   void gotThumbnailDownload(QString system, QString title);

public slots:
   void onBrowserDownloadsClicked();
   void onBrowserUpClicked();
   void onBrowserStartClicked();
   void initContentTableWidget();
   void initContentGridLayout();
   void onViewClosedDocksAboutToShow();
   void onShowHiddenDockWidgetAction();
   void setCoreActions();
   void onRunClicked();
   void loadContent(const QHash<QString, QString> &contentHash);
   void onStartCoreClicked();
   void onTableWidgetEnterPressed();
   void onTableWidgetDeletePressed();
   void selectBrowserDir(QString path);
   void resizeThumbnails(bool one, bool two, bool three);
   void onResizeThumbnailOne();
   void onResizeThumbnailTwo();
   void onResizeThumbnailThree();
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

private slots:
   void onLoadCoreClicked(const QStringList &extensionFilters = QStringList());
   void onUnloadCoreMenuAction();
   void onTimeout();
   void onCoreLoaded();
   void onCurrentListItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
   void onCurrentTableItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);
   void currentItemChanged(const QHash<QString, QString> &hash);
   void onSearchEnterPressed();
   void onSearchLineEditEdited(const QString &text);
   void addPlaylistItemsToTable(const QStringList &paths, bool all = false);
   void addPlaylistHashToTable(const QVector<QHash<QString, QString> > &items);
   void addPlaylistItemsToGrid(const QStringList &paths, bool all = false);
   void addPlaylistHashToGrid(const QVector<QHash<QString, QString> > &items);
   void onContentItemDoubleClicked(QTableWidgetItem *item);
   void onCoreLoadWindowClosed();
   void onTreeViewItemsSelected(QModelIndexList selectedIndexes);
   void onSearchResetClicked();
   void onLaunchWithComboBoxIndexChanged(int index);
   void onFileBrowserTreeContextMenuRequested(const QPoint &pos);
   void onPlaylistWidgetContextMenuRequested(const QPoint &pos);
   void onStopClicked();
   void onDeferredImageLoaded();
   void onZoomValueChanged(int value);
   void onContentGridInited();
   void onUpdateGridItemPixmapFromImage(GridItem *item);
   void onPendingItemUpdates();
   void onGridItemDoubleClicked();
   void onGridItemClicked(ThumbnailWidget *thumbnailWidget = NULL);
   void onPlaylistFilesDropped(QStringList files);
   void onShaderParamsClicked();
   void onCoreOptionsClicked();
   void onShowErrorMessage(QString msg);
   void onShowInfoMessage(QString msg);
   void onContributorsClicked();
   void onItemChanged();
   void onGridItemChanged(QString title);
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

private:
   void setCurrentCoreLabel();
   void getPlaylistFiles();
   bool isCoreLoaded();
   bool isContentLessCore();
   void removeGridItems();
   void loadImageDeferred(GridItem *item, QString path);
   void calcGridItemSize(GridItem *item, int zoomValue);
   bool updateCurrentPlaylistEntry(const QHash<QString, QString> &contentHash);
   int extractArchive(QString path);
   void removeUpdateTempFiles();
   QVector<QHash<QString, QString> > getPlaylistItems(QString pathString);

   LoadCoreWindow *m_loadCoreWindow;
   QTimer *m_timer;
   QString m_currentCore;
   QString m_currentCoreVersion;
   QLabel *m_statusLabel;
   TreeView *m_dirTree;
   QFileSystemModel *m_dirModel;
   QListWidget *m_listWidget;
   TableWidget *m_tableWidget;
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
   QRegularExpression m_fileSanitizerRegex;
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
   QWidget *m_logWidget;
   LogTextEdit *m_logTextEdit;
   QVector<QByteArray> m_imageFormats;
   QListWidgetItem *m_historyPlaylistsItem;
   QIcon m_folderIcon;
   QString m_customThemeString;
   FlowLayout *m_gridLayout;
   QWidget *m_gridWidget;
   QScrollArea *m_gridScrollArea;
   QVector<QPointer<GridItem> > m_gridItems;
   QWidget *m_gridLayoutWidget;
   QSlider *m_zoomSlider;
   int m_lastZoomSliderValue;
   QList<GridItem*> m_pendingItemUpdates;
   ViewType m_viewType;
   QProgressBar *m_gridProgressBar;
   QWidget *m_gridProgressWidget;
   QHash<QString, QString> m_currentGridHash;
   ViewType m_lastViewType;
   QPointer<ThumbnailWidget> m_currentGridWidget;
   int m_allPlaylistsListMaxCount;
   int m_allPlaylistsGridMaxCount;
   PlaylistEntryDialog *m_playlistEntryDialog;
   QElapsedTimer m_statusMessageElapsedTimer;
   QPointer<ShaderParamsDialog> m_shaderParamsDialog;
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

protected:
   void closeEvent(QCloseEvent *event);
   void keyPressEvent(QKeyEvent *event);
};

Q_DECLARE_METATYPE(ThumbnailWidget)
Q_DECLARE_METATYPE(QPointer<ThumbnailWidget>)
Q_DECLARE_METATYPE(struct video_shader_parameter*)

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
