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
#include <QWidget>
#include <QDialog>
#include <QLabel>
#include <QRegularExpression>
#include <QPalette>
#include <QPlainTextEdit>

extern "C" {
#include <retro_common_api.h>
#include "../ui_companion_driver.h"
}

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
class QFormLayout;
class QStyle;
class QScrollArea;
class LoadCoreWindow;
class MainWindow;

class ThumbnailWidget : public QWidget
{
   Q_OBJECT
public:
   ThumbnailWidget(QWidget *parent = 0);
   QSize sizeHint() const;
protected:
   void paintEvent(QPaintEvent *event);
   void resizeEvent(QResizeEvent *event);
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
protected slots:
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

class ViewOptionsDialog : public QDialog
{
   Q_OBJECT
public:
   ViewOptionsDialog(MainWindow *mainwindow, QWidget *parent = 0);
public slots:
   void showDialog();
   void hideDialog();
   void onAccepted();
   void onRejected();
private slots:
   void onThemeComboBoxIndexChanged(int index);
   void onHighlightColorChoose();
private:
   void loadViewOptions();
   void saveViewOptions();
   void showOrHideHighlightColor();

   MainWindow *m_mainwindow;
   QSettings *m_settings;
   QCheckBox *m_saveGeometryCheckBox;
   QCheckBox *m_saveDockPositionsCheckBox;
   QCheckBox *m_saveLastTabCheckBox;
   QCheckBox *m_showHiddenFilesCheckBox;
   QComboBox *m_themeComboBox;
   QPushButton *m_highlightColorPushButton;
   QColor m_highlightColor;
   QLabel *m_highlightColorLabel;
   QString m_customThemePath;
   QCheckBox *m_suggestLoadedCoreFirstCheckBox;
};

class CoreInfoLabel : public QLabel
{
   Q_OBJECT
public:
   CoreInfoLabel(QString text = QString(), QWidget *parent = 0);
};

class CoreInfoDialog : public QDialog
{
   Q_OBJECT
public:
   CoreInfoDialog(MainWindow *mainwindow, QWidget *parent = 0);
public slots:
   void showCoreInfo();
private:
   QFormLayout *m_formLayout;
   MainWindow *m_mainwindow;
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
   };

   MainWindow(QWidget *parent = NULL);
   ~MainWindow();
   TreeView* dirTreeView();
   QListWidget* playlistListWidget();
   TableWidget* contentTableWidget();
   QWidget* searchWidget();
   QLineEdit* searchLineEdit();
   QComboBox* launchWithComboBox();
   QToolButton* startCorePushButton();
   QToolButton* coreInfoPushButton();
   QToolButton* runPushButton();
   QToolButton* stopPushButton();
   QTabWidget* browserAndPlaylistTabWidget();
   QList<QHash<QString, QString> > getPlaylistDefaultCores();
   ViewOptionsDialog* viewOptionsDialog();
   QSettings* settings();
   QList<QHash<QString, QString> > getCoreInfo();
   void setTheme(Theme theme = THEME_SYSTEM_DEFAULT);
   Theme theme();
   Theme getThemeFromString(QString themeString);
   QString getThemeString(Theme theme);
   QHash<QString, QString> getSelectedCore();
   void showStatusMessage(QString msg, unsigned priority, unsigned duration, bool flush);
   bool showMessageBox(QString msg, MessageBoxType msgType = MSGBOX_TYPE_INFO, Qt::WindowModality modality = Qt::ApplicationModal);
   bool setCustomThemeFile(QString filePath);
   void setCustomThemeString(QString qss);
   const QString& customThemeString() const;

signals:
   void thumbnailChanged(const QPixmap &pixmap);
   void thumbnail2Changed(const QPixmap &pixmap);
   void thumbnail3Changed(const QPixmap &pixmap);
   void gotLogMessage(const QString &msg);
   void gotStatusMessage(QString msg, unsigned priority, unsigned duration, bool flush);
   void gotReloadPlaylists();

public slots:
   void onBrowserDownloadsClicked();
   void onBrowserUpClicked();
   void onBrowserStartClicked();
   void initContentTableWidget();
   void onViewClosedDocksAboutToShow();
   void onShowHiddenDockWidgetAction();
   void setCoreActions();
   void onRunClicked();
   void onStartCoreClicked();
   void onTableWidgetEnterPressed();
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
   void showWelcomeScreen();

private slots:
   void onLoadCoreClicked(const QStringList &extensionFilters = QStringList());
   void onUnloadCoreMenuAction();
   void onTimeout();
   void onCoreLoaded();
   void onCurrentListItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
   void onCurrentTableItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);
   void onSearchEnterPressed();
   void onSearchLineEditEdited(const QString &text);
   void addPlaylistItemsToTable(QString path);
   void onContentItemDoubleClicked(QTableWidgetItem *item);
   void onCoreLoadWindowClosed();
   void onTabWidgetIndexChanged(int index);
   void onTreeViewItemsSelected(QModelIndexList selectedIndexes);
   void onSearchResetClicked();
   void onLaunchWithComboBoxIndexChanged(int index);
   void onFileBrowserTreeContextMenuRequested(const QPoint &pos);
   void onPlaylistWidgetContextMenuRequested(const QPoint &pos);
   void onStopClicked();

private:
   void setCurrentCoreLabel();
   void getPlaylistFiles();
   bool isCoreLoaded();
   bool isContentLessCore();

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

protected:
   void closeEvent(QCloseEvent *event);
   void keyPressEvent(QKeyEvent *event);
};

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
