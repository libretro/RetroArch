/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CXX_BUILD
extern "C" {
#endif

#include <file/file_path.h>
#include <file/archive_file.h>
#include <retro_timers.h>
#include <string/stdstring.h>

#ifdef Q_OS_UNIX
#include <locale.h>
#endif

#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/opensslv.h>
#endif

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../ui_companion_driver.h"
#include "../../configuration.h"
#include "../../frontend/frontend.h"
#include "../../paths.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../version.h"
#include "../../msg_hash.h"
#include "../../tasks/task_content.h"
#include "../../tasks/tasks_internal.h"
#include "../../AUTHORS.h"
#ifdef HAVE_GIT_VERSION
#include "../../version_git.h"
#endif

#ifndef CXX_BUILD
}
#endif

#include "ui_qt.h"
#include "qt/invader_png.h"
#include "qt/gridview.h"
#include "qt/ui_qt_themes.h"
#include "qt/ui_qt_load_core_window.h"
#include "qt/coreinfodialog.h"
#include "qt/playlistentrydialog.h"
#include "qt/shaderparamsdialog.h"
#include "qt/coreoptionsdialog.h"
#include "qt/viewoptionsdialog.h"

#include <QApplication>
#include <QAbstractEventDispatcher>
#include <QtWidgets>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtCore/QString>
#include <QDesktopWidget>
#include <QtGlobal>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QStyle>
#include <QTimer>
#include <QLabel>
#include <QFileSystemModel>
#include <QListWidgetItem>
#include <QTableWidgetItem>
#include <QHash>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QDockWidget>
#include <QList>
#include <QInputDialog>
#include <QMimeData>
#include <QProgressDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QtConcurrentRun>
#include <QtNetwork>

#define INITIAL_WIDTH 1280
#define INITIAL_HEIGHT 720

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
#define GROUPED_DRAGGING QMainWindow::GroupedDragging
#else
#define GROUPED_DRAGGING static_cast<QMainWindow::DockOption>(0)
#endif

#define TIMER_MSEC                  1000 /* periodic timer for gathering statistics */
#define STATUS_MSG_THROTTLE_MSEC    250

#define GENERIC_FOLDER_ICON         "/xmb/dot-art/png/folder.png"
#define HIRAGANA_START              0x3041U
#define HIRAGANA_END                0x3096U
#define KATAKANA_START              0x30A1U
#define KATAKANA_END                0x30F6U
#define HIRA_KATA_OFFSET            (KATAKANA_START - HIRAGANA_START)
#define DOCS_URL                    "http://docs.libretro.com/"

enum core_selection
{
   CORE_SELECTION_CURRENT = 0,
   CORE_SELECTION_PLAYLIST_SAVED,
   CORE_SELECTION_PLAYLIST_DEFAULT,
   CORE_SELECTION_ASK,
   CORE_SELECTION_LOAD_CORE
};

static AppHandler *app_handler;
static ui_application_qt_t ui_application;

/* ARGB 16x16 */
static const unsigned retroarch_qt_icon_data[] = {
0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
0x00000000,0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xfff2f2f2,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0xfff2f2f2,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,0x00000000,
0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
};

static ui_window_qt_t ui_window = {0};

static const QPixmap getInvader(void)
{
   QPixmap pix;
   pix.loadFromData(invader_png, invader_png_len, "PNG");

   return pix;
}

#ifdef HAVE_LIBRETRODB
static void scan_finished_handler(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   bool dontAsk      = false;
   bool answer       = false;
#ifdef HAVE_MENU
   menu_ctx_environment_t menu_environ;
   menu_environ.type = MENU_ENVIRON_RESET_HORIZONTAL_LIST;
   menu_environ.data = NULL;

   menu_driver_ctl(RARCH_MENU_CTL_ENVIRONMENT, &menu_environ);
#endif
   if (!ui_window.qtWindow->settings()->value(
            "scan_finish_confirm", true).toBool())
      return;

   answer = ui_window.qtWindow->showMessageBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED), MainWindow::MSGBOX_TYPE_QUESTION_OKCANCEL, Qt::ApplicationModal, true, &dontAsk);

   if (answer && dontAsk)
      ui_window.qtWindow->settings()->setValue("scan_finish_confirm", false);
}
#endif

/* https://stackoverflow.com/questions/7246622/how-to-create-a-slider-with-a-non-linear-scale */
static double expScale(double inputValue, double midValue, double maxValue)
{
   double           M = maxValue / midValue;
   double           C = log(pow(M - 1, 2));
   double           B = maxValue / (exp(C) - 1);
   double           A = -1 * B;
   double returnValue = A + B * exp(C * inputValue);
   return returnValue;
}

TreeView::TreeView(QWidget *parent) :
   QTreeView(parent)
{
}

void TreeView::columnCountChanged(int oldCount, int newCount)
{
   QTreeView::columnCountChanged(oldCount, newCount);
}

void TreeView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
   QModelIndexList list = selected.indexes();

   QTreeView::selectionChanged(selected, deselected);

   emit itemsSelected(list);
}

TableView::TableView(QWidget *parent) :
   QTableView(parent)
{
}

bool TableView::isEditorOpen()
{
   return (state() == QAbstractItemView::EditingState);
}

ListWidget::ListWidget(QWidget *parent) :
   QListWidget(parent)
{
}

bool ListWidget::isEditorOpen()
{
   return (state() == QAbstractItemView::EditingState);
}

void ListWidget::keyPressEvent(QKeyEvent *event)
{
   if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
      emit enterPressed();
   else if (event->key() == Qt::Key_Delete)
      emit deletePressed();

   QListWidget::keyPressEvent(event);
}

CoreInfoLabel::CoreInfoLabel(QString text, QWidget *parent) :
   QLabel(text, parent)
{
   setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
}

CoreInfoWidget::CoreInfoWidget(CoreInfoLabel *label, QWidget *parent) :
   QWidget(parent)
   ,m_label(label)
   ,m_scrollArea(new QScrollArea(this))
{
   m_scrollArea->setWidgetResizable(true);
   m_scrollArea->setWidget(m_label);
}

QSize CoreInfoWidget::sizeHint() const
{
   return QSize(256, 256);
}

void CoreInfoWidget::resizeEvent(QResizeEvent *event)
{
   QWidget::resizeEvent(event);
   m_scrollArea->resize(event->size());
}

LogTextEdit::LogTextEdit(QWidget *parent) :
   QPlainTextEdit(parent)
{

}

void LogTextEdit::appendMessage(const QString& text)
{
   if (text.isEmpty())
      return;

   appendPlainText(text);
   verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

/* only accept indexes from current path. https://www.qtcentre.org/threads/50700-QFileSystemModel-and-QSortFilterProxyModel-don-t-work-well-together */
bool FileSystemProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
   QFileSystemModel  *sm = qobject_cast<QFileSystemModel*>(sourceModel());
   QModelIndex rootIndex = sm->index(sm->rootPath());

   if (sourceParent == rootIndex)
      return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
   return true;
}

void FileSystemProxyModel::sort(int column, Qt::SortOrder order)
{
   /* sort the source (QFileSystemModel to keep directories before files) */
   sourceModel()->sort(column, order);
}

MainWindow::MainWindow(QWidget *parent) :
   QMainWindow(parent)
   ,m_loadCoreWindow(new LoadCoreWindow(this))
   ,m_timer(new QTimer(this))
   ,m_currentCore()
   ,m_currentCoreVersion()
   ,m_statusLabel(new QLabel(this))
   ,m_dirTree(new TreeView(this))
   ,m_dirModel(new QFileSystemModel(m_dirTree))
   ,m_fileModel(new QFileSystemModel(this))
   ,m_listWidget(new ListWidget(this))
   ,m_centralWidget(new QStackedWidget(this))
   ,m_tableView(new TableView(this))
   ,m_fileTableView(new QTableView(this))
   ,m_playlistViews(new FileDropWidget(this))
   ,m_searchWidget(new QWidget(this))
   ,m_searchLineEdit(new QLineEdit(this))
   ,m_searchDock(new QDockWidget(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SEARCH), this))
   ,m_playlistFiles()
   ,m_launchWithComboBox(new QComboBox(this))
   ,m_startCorePushButton(new QToolButton(this))
   ,m_coreInfoPushButton(new QToolButton(this))
   ,m_runPushButton(new QToolButton(this))
   ,m_stopPushButton(new QToolButton(this))
   ,m_browserAndPlaylistTabWidget(new QTabWidget(this))
   ,m_pendingRun(false)
   ,m_thumbnailPixmap(NULL)
   ,m_thumbnailPixmap2(NULL)
   ,m_thumbnailPixmap3(NULL)
   ,m_settings(NULL)
   ,m_viewOptionsDialog(NULL)
   ,m_coreInfoDialog(new CoreInfoDialog(this, NULL))
   ,m_defaultStyle(NULL)
   ,m_defaultPalette()
   ,m_currentTheme(THEME_SYSTEM_DEFAULT)
   ,m_coreInfoDock(new QDockWidget(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE_INFO), this))
   ,m_coreInfoLabel(new CoreInfoLabel(QString(), this))
   ,m_coreInfoWidget(new CoreInfoWidget(m_coreInfoLabel, this))
   ,m_logDock(new QDockWidget(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_LOG), this))
   ,m_logWidget(new QFrame(this))
   ,m_logTextEdit(new LogTextEdit(m_logWidget))
   ,m_historyPlaylistsItem(NULL)
   ,m_folderIcon()
   ,m_customThemeString()
   ,m_gridView(new GridView(this))
   ,m_playlistViewsAndFooter(new QWidget(this))
   ,m_zoomSlider(NULL)
   ,m_lastZoomSliderValue(0)
   ,m_viewType(VIEW_TYPE_LIST)
   ,m_thumbnailType(THUMBNAIL_TYPE_BOXART)
   ,m_gridProgressBar(NULL)
   ,m_gridProgressWidget(NULL)
   ,m_currentGridHash()
   ,m_currentGridWidget(NULL)
   ,m_allPlaylistsListMaxCount(0)
   ,m_allPlaylistsGridMaxCount(0)
   ,m_playlistEntryDialog(NULL)
   ,m_statusMessageElapsedTimer()
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   ,m_shaderParamsDialog(new ShaderParamsDialog())
#endif
   ,m_coreOptionsDialog(new CoreOptionsDialog())
   ,m_networkManager(new QNetworkAccessManager(this))
   ,m_updateProgressDialog(new QProgressDialog())
   ,m_updateFile()
   ,m_updateReply()
   ,m_thumbnailDownloadProgressDialog(new QProgressDialog())
   ,m_thumbnailDownloadFile()
   ,m_thumbnailDownloadReply()
   ,m_pendingThumbnailDownloadTypes()
   ,m_thumbnailPackDownloadProgressDialog(new QProgressDialog())
   ,m_thumbnailPackDownloadFile()
   ,m_thumbnailPackDownloadReply()
   ,m_playlistThumbnailDownloadProgressDialog(new QProgressDialog())
   ,m_playlistThumbnailDownloadFile()
   ,m_playlistThumbnailDownloadReply()
   ,m_pendingPlaylistThumbnails()
   ,m_downloadedThumbnails(0)
   ,m_failedThumbnails(0)
   ,m_playlistThumbnailDownloadWasCanceled(false)
   ,m_pendingDirScrollPath()
   ,m_thumbnailTimer(new QTimer(this))
   ,m_gridItem(this)
   ,m_currentBrowser(BROWSER_TYPE_PLAYLISTS)
   ,m_searchRegExp()
   ,m_zoomWidget(new QWidget(this))
   ,m_itemsCountLiteral(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ITEMS_COUNT))
   ,m_itemsCountLabel(new QLabel(this))
{
   settings_t                   *settings = config_get_ptr();
   const char *path_dir_playlist          = settings->paths.directory_playlist;
   const char *path_dir_assets            = settings->paths.directory_assets;
   const char *path_dir_menu_content      = settings->paths.directory_menu_content;
   QDir playlistDir(path_dir_playlist);
   QString                      configDir = QFileInfo(path_get(RARCH_PATH_CONFIG)).dir().absolutePath();
   QToolButton   *searchResetButton       = NULL;
   QHBoxLayout   *zoomLayout              = new QHBoxLayout();
   QLabel   *zoomLabel                    = new QLabel(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ZOOM), m_zoomWidget);
   QPushButton   *thumbnailTypePushButton = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_TYPE), m_zoomWidget);
   QMenu               *thumbnailTypeMenu = new QMenu(thumbnailTypePushButton);
   QAction     *thumbnailTypeBoxartAction = NULL;
   QAction *thumbnailTypeScreenshotAction = NULL;
   QAction *thumbnailTypeTitleAction      = NULL;
   QPushButton *viewTypePushButton        = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_VIEW), m_zoomWidget);
   QMenu                    *viewTypeMenu = new QMenu(viewTypePushButton);
   QAction           *viewTypeIconsAction = NULL;
   QAction            *viewTypeListAction = NULL;
   QHBoxLayout        *gridProgressLayout = new QHBoxLayout();
   QLabel              *gridProgressLabel = NULL;
   QHBoxLayout          *gridFooterLayout = NULL;

   qRegisterMetaType<QPointer<ThumbnailWidget> >("ThumbnailWidget");
   qRegisterMetaType<retro_task_callback_t>("retro_task_callback_t");

   /* Cancel all progress dialogs immediately since 
    * they show as soon as they're constructed. */
   m_updateProgressDialog->cancel();
   m_thumbnailDownloadProgressDialog->cancel();
   m_thumbnailPackDownloadProgressDialog->cancel();
   m_playlistThumbnailDownloadProgressDialog->cancel();

   m_gridProgressWidget                   = new QWidget();
   gridProgressLabel                      = new QLabel(
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_PROGRESS),
         m_gridProgressWidget);

   thumbnailTypePushButton->setObjectName("thumbnailTypePushButton");
   thumbnailTypePushButton->setFlat(true);

   thumbnailTypeBoxartAction              = thumbnailTypeMenu->addAction(
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART));
   thumbnailTypeScreenshotAction          = thumbnailTypeMenu->addAction(
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT));
   thumbnailTypeTitleAction               = thumbnailTypeMenu->addAction(
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN));

   thumbnailTypePushButton->setMenu(thumbnailTypeMenu);

   viewTypePushButton->setObjectName("viewTypePushButton");
   viewTypePushButton->setFlat(true);

   viewTypeIconsAction                    = viewTypeMenu->addAction(
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS));
   viewTypeListAction                     = viewTypeMenu->addAction(
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST));

   viewTypePushButton->setMenu(viewTypeMenu);

   gridProgressLabel->setObjectName("gridProgressLabel");

   m_gridProgressBar                      = new QProgressBar(
         m_gridProgressWidget);

   m_gridProgressBar->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred));

   zoomLabel->setObjectName("zoomLabel");

   m_zoomSlider                           = new QSlider(
         Qt::Horizontal, m_zoomWidget);

   m_zoomSlider->setMinimum(0);
   m_zoomSlider->setMaximum(100);
   m_zoomSlider->setValue(50);
   m_zoomSlider->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred));

   m_lastZoomSliderValue = m_zoomSlider->value();

   m_playlistViewsAndFooter->setLayout(new QVBoxLayout());

   m_gridView->setSelectionMode(QAbstractItemView::SingleSelection);
   m_gridView->setEditTriggers(QAbstractItemView::NoEditTriggers);

   m_playlistViews->addWidget(m_gridView);
   m_playlistViews->addWidget(m_tableView);
   m_centralWidget->setObjectName("centralWidget");

   m_playlistViewsAndFooter->layout()->addWidget(m_playlistViews);
   m_playlistViewsAndFooter->layout()->setAlignment(Qt::AlignCenter);
   m_playlistViewsAndFooter->layout()->setContentsMargins(0, 0, 0, 0);

   m_gridProgressWidget->setLayout(gridProgressLayout);
   gridProgressLayout->setContentsMargins(0, 0, 0, 0);
   gridProgressLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Preferred));
   gridProgressLayout->addWidget(gridProgressLabel);
   gridProgressLayout->addWidget(m_gridProgressBar);

   m_playlistViewsAndFooter->layout()->addWidget(m_gridProgressWidget);

   m_zoomWidget->setLayout(zoomLayout);
   zoomLayout->setContentsMargins(0, 0, 0, 0);
   zoomLayout->addWidget(zoomLabel);
   zoomLayout->addWidget(m_zoomSlider);

   m_itemsCountLabel->setObjectName("itemsCountLabel");

   gridFooterLayout = new QHBoxLayout();
   gridFooterLayout->addWidget(m_itemsCountLabel);
   gridFooterLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Preferred));
   gridFooterLayout->addWidget(m_gridProgressWidget);
   gridFooterLayout->addWidget(m_zoomWidget);
   gridFooterLayout->addWidget(thumbnailTypePushButton);
   gridFooterLayout->addWidget(viewTypePushButton);

   static_cast<QVBoxLayout*>(m_playlistViewsAndFooter->layout())->addLayout(gridFooterLayout);

   m_gridProgressWidget->hide();

   m_playlistModel = new PlaylistModel(this);
   m_proxyModel    = new QSortFilterProxyModel(this);
   m_proxyModel->setSourceModel(m_playlistModel);
   m_proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);

   m_proxyFileModel = new FileSystemProxyModel();
   m_proxyFileModel->setSourceModel(m_fileModel);
   m_proxyFileModel->setSortCaseSensitivity(Qt::CaseInsensitive);

   m_tableView->setAlternatingRowColors(true);
   m_tableView->setModel(m_proxyModel);
   m_tableView->setSortingEnabled(true);
   m_tableView->verticalHeader()->setVisible(false);
   m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
   m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
   m_tableView->setEditTriggers(QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
   m_tableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
   m_tableView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
   m_tableView->horizontalHeader()->setStretchLastSection(true);
   m_tableView->setWordWrap(false);

   m_fileTableView->setModel(m_fileModel);
   m_fileTableView->sortByColumn(0, Qt::AscendingOrder);
   m_fileTableView->setSortingEnabled(true);
   m_fileTableView->setAlternatingRowColors(true);
   m_fileTableView->verticalHeader()->setVisible(false);
   m_fileTableView->setSelectionMode(QAbstractItemView::SingleSelection);
   m_fileTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
   m_fileTableView->horizontalHeader()->setStretchLastSection(true);
   m_fileTableView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
   m_fileTableView->setWordWrap(false);

   m_gridView->setItemDelegate(new ThumbnailDelegate(m_gridItem, this));
   m_gridView->setModel(m_proxyModel);

   m_gridView->setSelectionModel(m_tableView->selectionModel());

   m_logWidget->setObjectName("logWidget");

   m_folderIcon     = QIcon(QString(path_dir_assets) + GENERIC_FOLDER_ICON);
   m_imageFormats   = QVector<QByteArray>::fromList(QImageReader::supportedImageFormats());
   m_defaultStyle   = QApplication::style();
   m_defaultPalette = QApplication::palette();

   /* ViewOptionsDialog needs m_settings set before it's constructed */
   m_settings            = new QSettings(configDir + "/retroarch_qt.cfg", QSettings::IniFormat, this);
   m_viewOptionsDialog   = new ViewOptionsDialog(this, 0);
   m_playlistEntryDialog = new PlaylistEntryDialog(this, 0);

   /* default NULL parameter for parent wasn't added until 5.7 */
   m_startCorePushButton->setDefaultAction(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_START_CORE), m_startCorePushButton));
   m_startCorePushButton->setFixedSize(m_startCorePushButton->sizeHint());

   m_runPushButton->setDefaultAction(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RUN), m_runPushButton));
   m_runPushButton->setFixedSize(m_runPushButton->sizeHint());

   m_stopPushButton->setDefaultAction(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_STOP), m_stopPushButton));
   m_stopPushButton->setFixedSize(m_stopPushButton->sizeHint());

   m_coreInfoPushButton->setDefaultAction(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_INFO), m_coreInfoPushButton));
   m_coreInfoPushButton->setFixedSize(m_coreInfoPushButton->sizeHint());

   searchResetButton = new QToolButton(m_searchWidget);
   searchResetButton->setDefaultAction(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR), searchResetButton));
   searchResetButton->setFixedSize(searchResetButton->sizeHint());

   connect(searchResetButton, SIGNAL(clicked()), this, SLOT(onSearchResetClicked()));

   m_dirModel->setFilter(QDir::NoDotAndDotDot |
                         QDir::AllDirs |
                         QDir::Drives |
                         (m_settings->value("show_hidden_files", true).toBool() ? (QDir::Hidden | QDir::System) : static_cast<QDir::Filter>(0)));

   m_fileModel->setFilter(QDir::NoDot |
                          QDir::AllEntries |
                          (m_settings->value("show_hidden_files", true).toBool() ? (QDir::Hidden | QDir::System) : static_cast<QDir::Filter>(0)));

#if defined(Q_OS_WIN)
   m_dirModel->setRootPath("");
   m_fileModel->setRootPath("");
#else
   m_dirModel->setRootPath("/");
   m_fileModel->setRootPath("/");
#endif

   m_dirTree->setModel(m_dirModel);
   m_dirTree->setSelectionMode(QAbstractItemView::SingleSelection);
   m_dirTree->header()->setVisible(false);

   m_fileTableView->setModel(m_proxyFileModel);

   if (m_dirModel->columnCount() > 3)
   {
      /* size */
      m_dirTree->hideColumn(1);
      /* type */
      m_dirTree->hideColumn(2);
      /* date modified */
      m_dirTree->hideColumn(3);
   }

   reloadPlaylists();

   m_searchWidget->setLayout(new QHBoxLayout());
   m_searchWidget->layout()->addWidget(m_searchLineEdit);
   m_searchWidget->layout()->addWidget(searchResetButton);

   m_searchDock->setObjectName("searchDock");
   m_searchDock->setProperty("default_area", Qt::LeftDockWidgetArea);
   m_searchDock->setProperty("menu_text", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SEARCH));
   m_searchDock->setWidget(m_searchWidget);
   m_searchDock->setFixedHeight(m_searchDock->minimumSizeHint().height());

   addDockWidget(static_cast<Qt::DockWidgetArea>(m_searchDock->property("default_area").toInt()), m_searchDock);

   m_coreInfoLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
   m_coreInfoLabel->setTextFormat(Qt::RichText);
   m_coreInfoLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
   m_coreInfoLabel->setOpenExternalLinks(true);

   m_coreInfoDock->setObjectName("coreInfoDock");
   m_coreInfoDock->setProperty("default_area", Qt::RightDockWidgetArea);
   m_coreInfoDock->setProperty("menu_text", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE_INFO));
   m_coreInfoDock->setWidget(m_coreInfoWidget);

   addDockWidget(static_cast<Qt::DockWidgetArea>(m_coreInfoDock->property("default_area").toInt()), m_coreInfoDock);

   m_logWidget->setLayout(new QVBoxLayout());
   m_logWidget->layout()->addWidget(m_logTextEdit);
   m_logWidget->layout()->setContentsMargins(0, 0, 0, 0);

   m_logDock->setObjectName("logDock");
   m_logDock->setProperty("default_area", Qt::BottomDockWidgetArea);
   m_logDock->setProperty("menu_text", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_LOG));
   m_logDock->setWidget(m_logWidget);

   addDockWidget(static_cast<Qt::DockWidgetArea>(m_logDock->property("default_area").toInt()), m_logDock);

   /* Hide the log by default. If user has saved their dock positions with the log visible,
    * then this hide() call will be reversed later by restoreState().
    * FIXME: If user unchecks "save dock positions", the log will not be unhidden even if
    * it was previously saved in the config.
    */
   m_logDock->hide();

   m_dirTree->setContextMenuPolicy(Qt::CustomContextMenu);
   m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);

   connect(m_searchLineEdit, SIGNAL(returnPressed()), this, SLOT(onSearchEnterPressed()));
   connect(m_searchLineEdit, SIGNAL(textEdited(const QString&)), this, SLOT(onSearchLineEditEdited(const QString&)));
   connect(m_timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
   connect(m_loadCoreWindow, SIGNAL(coreLoaded()), this, SLOT(onCoreLoaded()));
   connect(m_loadCoreWindow, SIGNAL(windowClosed()), this, SLOT(onCoreLoadWindowClosed()));
   connect(m_listWidget, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), this, SLOT(onCurrentListItemChanged(QListWidgetItem*, QListWidgetItem*)));
   connect(m_startCorePushButton, SIGNAL(clicked()), this, SLOT(onStartCoreClicked()));
   connect(m_coreInfoPushButton, SIGNAL(clicked()), m_coreInfoDialog, SLOT(showCoreInfo()));
   connect(m_runPushButton, SIGNAL(clicked()), this, SLOT(onRunClicked()));
   connect(m_stopPushButton, SIGNAL(clicked()), this, SLOT(onStopClicked()));
   connect(m_dirTree, SIGNAL(itemsSelected(QModelIndexList)), this, SLOT(onTreeViewItemsSelected(QModelIndexList)));
   connect(m_dirTree, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onFileBrowserTreeContextMenuRequested(const QPoint&)));
   connect(m_listWidget, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onPlaylistWidgetContextMenuRequested(const QPoint&)));
   connect(m_launchWithComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onLaunchWithComboBoxIndexChanged(int)));
   connect(m_zoomSlider, SIGNAL(valueChanged(int)), this, SLOT(onZoomValueChanged(int)));
   connect(thumbnailTypeBoxartAction, SIGNAL(triggered()), this, SLOT(onBoxartThumbnailClicked()));
   connect(thumbnailTypeScreenshotAction, SIGNAL(triggered()), this, SLOT(onScreenshotThumbnailClicked()));
   connect(thumbnailTypeTitleAction, SIGNAL(triggered()), this, SLOT(onTitleThumbnailClicked()));
   connect(viewTypeIconsAction, SIGNAL(triggered()), this, SLOT(onIconViewClicked()));
   connect(viewTypeListAction, SIGNAL(triggered()), this, SLOT(onListViewClicked()));
   connect(m_dirModel, SIGNAL(directoryLoaded(const QString&)), this, SLOT(onFileSystemDirLoaded(const QString&)));
   connect(m_fileModel, SIGNAL(directoryLoaded(const QString&)), this, SLOT(onFileBrowserTableDirLoaded(const QString&)));

   m_dirTree->setCurrentIndex(m_dirModel->index(path_dir_menu_content));
   m_dirTree->scrollTo(m_dirTree->currentIndex(), QAbstractItemView::PositionAtTop);
   m_dirTree->expand(m_dirTree->currentIndex());

   /* must use queued connection */
   connect(this, SIGNAL(scrollToDownloads(QString)), this, SLOT(onDownloadScroll(QString)), Qt::QueuedConnection);
   connect(this, SIGNAL(scrollToDownloadsAgain(QString)), this, SLOT(onDownloadScrollAgain(QString)), Qt::QueuedConnection);

   connect(m_playlistThumbnailDownloadProgressDialog, SIGNAL(canceled()), m_playlistThumbnailDownloadProgressDialog, SLOT(cancel()));
   connect(m_playlistThumbnailDownloadProgressDialog, SIGNAL(canceled()), this, SLOT(onPlaylistThumbnailDownloadCanceled()));

   connect(m_thumbnailDownloadProgressDialog, SIGNAL(canceled()), m_thumbnailDownloadProgressDialog, SLOT(cancel()));
   connect(m_thumbnailDownloadProgressDialog, SIGNAL(canceled()), this, SLOT(onThumbnailDownloadCanceled()));

   connect(m_thumbnailPackDownloadProgressDialog, SIGNAL(canceled()), m_thumbnailPackDownloadProgressDialog, SLOT(cancel()));
   connect(m_thumbnailPackDownloadProgressDialog, SIGNAL(canceled()), this, SLOT(onThumbnailPackDownloadCanceled()));

   connect(this, SIGNAL(itemChanged()), this, SLOT(onItemChanged()));
   connect(this, SIGNAL(gotThumbnailDownload(QString,QString)), this, SLOT(onDownloadThumbnail(QString,QString)));

   m_thumbnailTimer->setSingleShot(true);
   connect(m_thumbnailTimer, SIGNAL(timeout()), this, SLOT(updateVisibleItems()));
   connect(this, SIGNAL(updateThumbnails()), this, SLOT(updateVisibleItems()));

   /* TODO: Handle scroll and resize differently. */
   connect(m_gridView, SIGNAL(visibleItemsChangedMaybe()), this, SLOT(startTimer()));

   connect(m_tableView->selectionModel(), SIGNAL(currentChanged(const QModelIndex&, const QModelIndex&)), this, SLOT(onCurrentItemChanged(const QModelIndex&)));
   connect(m_fileTableView->selectionModel(), SIGNAL(currentChanged(const QModelIndex&, const QModelIndex&)), this, SLOT(onCurrentFileChanged(const QModelIndex&)));

   connect(m_gridView, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(onContentItemDoubleClicked(const QModelIndex&)));
   connect(m_tableView, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(onContentItemDoubleClicked(const QModelIndex&)));
   connect(m_fileTableView, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(onFileDoubleClicked(const QModelIndex&)));

   connect(m_playlistModel, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&, const QVector<int>&)), this, SLOT(onCurrentTableItemDataChanged(const QModelIndex&, const QModelIndex&, const QVector<int>&)));

   /* make sure these use an auto connection so it will be queued if called from a different thread (some facilities in RA log messages from other threads) */
   connect(this, SIGNAL(gotLogMessage(const QString&)), this, SLOT(onGotLogMessage(const QString&)), Qt::AutoConnection);
   connect(this, SIGNAL(gotStatusMessage(QString,unsigned,unsigned,bool)), this, SLOT(onGotStatusMessage(QString,unsigned,unsigned,bool)), Qt::AutoConnection);
   connect(this, SIGNAL(gotReloadPlaylists()), this, SLOT(onGotReloadPlaylists()), Qt::AutoConnection);
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   connect(this, SIGNAL(gotReloadShaderParams()), this, SLOT(onGotReloadShaderParams()), Qt::AutoConnection);
#endif
   connect(this, SIGNAL(gotReloadCoreOptions()), this, SLOT(onGotReloadCoreOptions()), Qt::AutoConnection);

   /* these are always queued */
   connect(this, SIGNAL(showErrorMessageDeferred(QString)), this, SLOT(onShowErrorMessage(QString)), Qt::QueuedConnection);
   connect(this, SIGNAL(showInfoMessageDeferred(QString)), this, SLOT(onShowInfoMessage(QString)), Qt::QueuedConnection);
   connect(this, SIGNAL(extractArchiveDeferred(QString,QString,QString,retro_task_callback_t)), this, SLOT(onExtractArchive(QString,QString,QString,retro_task_callback_t)), Qt::QueuedConnection);

   m_timer->start(TIMER_MSEC);

   statusBar()->addPermanentWidget(m_statusLabel);

   setCurrentCoreLabel();
   setCoreActions();

   /* both of these are necessary to get the folder to scroll to the top of the view */
   qApp->processEvents();
   QTimer::singleShot(0, this, SLOT(onBrowserStartClicked()));

   m_searchLineEdit->setFocus();
   m_loadCoreWindow->setWindowModality(Qt::ApplicationModal);

   m_statusMessageElapsedTimer.start();

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
   resizeDocks(QList<QDockWidget*>() << m_searchDock, QList<int>() << 1, Qt::Vertical);
#endif

#ifdef HAVE_OPENSSL
   {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
      const SSL_METHOD *method = TLS_method();
      SSL_CTX *ctx = SSL_CTX_new(method);

      if (ctx)
         SSL_CTX_free(ctx);
#else
      const SSL_METHOD *method = TLSv1_method();
      RARCH_LOG("[Qt]: TLS supports %d ciphers.\n", method->num_ciphers());
#endif
      RARCH_LOG("[Qt]: Using %s\n", OPENSSL_VERSION_TEXT);
   }
#endif
}

MainWindow::~MainWindow()
{
   if (m_thumbnailPixmap)
      delete m_thumbnailPixmap;
   if (m_thumbnailPixmap2)
      delete m_thumbnailPixmap2;
   if (m_thumbnailPixmap3)
      delete m_thumbnailPixmap3;
   if (m_proxyFileModel)
      delete m_proxyFileModel;
}

void MainWindow::startTimer()
{
   if (m_thumbnailTimer->isActive())
   {
      m_thumbnailTimer->stop();
      m_thumbnailTimer->start(50);
   }
   else
      m_thumbnailTimer->start(50);
}

void MainWindow::updateVisibleItems()
{
   if (m_currentBrowser == BROWSER_TYPE_PLAYLISTS && m_viewType == VIEW_TYPE_ICONS)
   {
      unsigned i;
      QVector<QModelIndex> indexes = m_gridView->visibleIndexes();
      size_t size                  = indexes.size();

      for (i = 0; i < size; i++)
         m_playlistModel->loadThumbnail(m_proxyModel->mapToSource(indexes.at(i)));
   }
}

void MainWindow::setThumbnailCacheLimit(int count)
{
   if (count < 1)
      count = 0;

   m_playlistModel->setThumbnailCacheLimit(count);
}

void MainWindow::onFileSystemDirLoaded(const QString &path)
{
   if (path.isEmpty() || m_pendingDirScrollPath.isEmpty())
      return;

   if (QDir(path) == QDir(m_pendingDirScrollPath))
   {
      m_pendingDirScrollPath = QString();

      emit scrollToDownloads(path);
   }
}

/* workaround for columns being resized */
void MainWindow::onFileBrowserTableDirLoaded(const QString &path)
{
   if (path.isEmpty())
      return;

   m_fileTableView->horizontalHeader()->restoreState(m_fileTableHeaderState);
}

QVector<QPair<QString, QString> > MainWindow::getPlaylists()
{
   unsigned i;
   QVector<QPair<QString, QString> > playlists;
   size_t size  = m_listWidget->count();

   for (i = 0; i < size; i++)
   {
      QString label, path;
      QPair<QString, QString> pair;
      QListWidgetItem *item = m_listWidget->item(i);

      if (!item)
         continue;

      label       = item->text();
      path        = item->data(Qt::UserRole).toString();

      pair.first  = label;
      pair.second = path;

      playlists.append(pair);
   }

   return playlists;
}

void MainWindow::onItemChanged()
{
   QModelIndex index = getCurrentContentIndex();
   m_playlistModel->reloadThumbnail(index);
   onCurrentItemChanged(index);
}

QString MainWindow::getSpecialPlaylistPath(SpecialPlaylist playlist)
{
   switch (playlist)
   {
      case SPECIAL_PLAYLIST_HISTORY:
         if (m_historyPlaylistsItem)
            return m_historyPlaylistsItem->data(Qt::UserRole).toString();
         break;
      default:
         break;
   }

   return QString();
}

double MainWindow::lerp(double x, double y, double a, double b, double d)
{
   return a + (b - a) * ((double)(d - x) / (double)(y - x));
}

void MainWindow::onIconViewClicked()
{
   setCurrentViewType(VIEW_TYPE_ICONS);
}

void MainWindow::onListViewClicked()
{
   setCurrentViewType(VIEW_TYPE_LIST);
}

void MainWindow::onBoxartThumbnailClicked()
{
   setCurrentThumbnailType(THUMBNAIL_TYPE_BOXART);
}

void MainWindow::onScreenshotThumbnailClicked()
{
   setCurrentThumbnailType(THUMBNAIL_TYPE_SCREENSHOT);
}

void MainWindow::onTitleThumbnailClicked()
{
   setCurrentThumbnailType(THUMBNAIL_TYPE_TITLE_SCREEN);
}

void MainWindow::setIconViewZoom(int zoomValue)
{
   m_zoomSlider->setValue(zoomValue);
}

void MainWindow::onZoomValueChanged(int zoomValue)
{
   int newSize               = 0;

   if (zoomValue < 50)
      newSize                = expScale(
            lerp(0, 49, 25, 49, zoomValue) / 50.0, 102, 256);
   else
      newSize                = expScale(zoomValue / 100.0, 256, 1024);

   m_gridView->setGridSize(newSize);

   m_lastZoomSliderValue     = zoomValue;
}

void MainWindow::showWelcomeScreen()
{
   bool dontAsk              = false;
   bool answer               = false;
   const QString welcomeText = QStringLiteral(""
      "Welcome to the RetroArch Desktop Menu!<br>\n"
      "<br>\n"
      "Many settings and actions are currently only available in the familiar Big Picture menu, "
      "but this Desktop Menu should be functional for launching content and managing playlists.<br>\n"
      "<br>\n"
      "Some useful hotkeys for interacting with the Big Picture menu include:\n"
      "<ul><li>F1 - Bring up the Big Picture menu</li>\n"
      "<li>F - Switch between fullscreen and windowed modes</li>\n"
      "<li>F5 - Bring the Desktop Menu back if closed</li>\n"
      "<li>Esc - Exit RetroArch</li></ul>\n"
      "\n"
      "For more hotkeys and their assignments, see:<br>\n"
      "Settings -> Input -> Input Hotkey Binds<br>\n"
      "<br>\n"
      "Documentation for RetroArch, libretro and cores:<br>\n"
      "<a href=\"https://docs.libretro.com/\">https://docs.libretro.com/</a>");

   if (!m_settings->value("show_welcome_screen", true).toBool())
      return;

   answer = showMessageBox(welcomeText, MainWindow::MSGBOX_TYPE_QUESTION_OKCANCEL, Qt::ApplicationModal, true, &dontAsk);

   if (answer && dontAsk)
      m_settings->setValue("show_welcome_screen", false);
}

const QString& MainWindow::customThemeString() const
{
   return m_customThemeString;
}

bool MainWindow::setCustomThemeFile(QString filePath)
{
   if (filePath.isEmpty())
   {
      QMessageBox::critical(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME), msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FILE_PATH_IS_BLANK));
      return false;
   }

   QFile file(filePath);

   if (file.exists())
   {
      bool opened = file.open(QIODevice::ReadOnly);

      if (!opened)
      {
         QMessageBox::critical(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME), msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED));
         return false;
      }

      {
         QByteArray fileArray = file.readAll();
         QString fileStr      = QString::fromUtf8(fileArray);

         file.close();

         if (fileStr.isEmpty())
         {
            QMessageBox::critical(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME), msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FILE_IS_EMPTY));
            return false;
         }

         setCustomThemeString(fileStr);
      }
   }
   else
   {
      QMessageBox::critical(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME), msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST));
      return false;
   }

   return true;
}

void MainWindow::setCustomThemeString(QString qss)
{
   m_customThemeString = qss;
}

bool MainWindow::showMessageBox(QString msg, MessageBoxType msgType, Qt::WindowModality modality, bool showDontAsk, bool *dontAsk)
{
   QCheckBox *checkBox             = NULL;

   QPointer<QMessageBox> msgBoxPtr = new QMessageBox(this);
   QMessageBox             *msgBox = msgBoxPtr.data();

   msgBox->setWindowModality(modality);
   msgBox->setTextFormat(Qt::RichText);
   msgBox->setTextInteractionFlags(Qt::TextBrowserInteraction);

   if (showDontAsk)
   {
      checkBox = new QCheckBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_DONT_SHOW_AGAIN), msgBox);
      /* QMessageBox::setCheckBox() is available since 5.2 */
      msgBox->setCheckBox(checkBox);
   }

   switch (msgType)
   {
      case MSGBOX_TYPE_INFO:
         msgBox->setIcon(QMessageBox::Information);
         msgBox->setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_INFORMATION));
         break;
      case MSGBOX_TYPE_WARNING:
         msgBox->setIcon(QMessageBox::Warning);
         msgBox->setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_WARNING));
         break;
      case MSGBOX_TYPE_ERROR:
         msgBox->setIcon(QMessageBox::Critical);
         msgBox->setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ERROR));
         break;
      case MSGBOX_TYPE_QUESTION_YESNO:
         msgBox->setIcon(QMessageBox::Question);
         msgBox->setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_QUESTION));
         msgBox->setStandardButtons(QMessageBox::Yes | QMessageBox::No);
         break;
      case MSGBOX_TYPE_QUESTION_OKCANCEL:
         msgBox->setIcon(QMessageBox::Question);
         msgBox->setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_QUESTION));
         msgBox->setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
         break;
      default:
         break;
   }

   msgBox->setText(msg);
   msgBox->exec();

   if (!msgBoxPtr)
      return true;

   if (
            msgBox->result() != QMessageBox::Ok 
         && msgBox->result() != QMessageBox::Yes)
      return false;

   if (checkBox)
      if (dontAsk)
         *dontAsk = checkBox->isChecked();

   return true;
}

void MainWindow::onFileBrowserTreeContextMenuRequested(const QPoint&)
{
#ifdef HAVE_LIBRETRODB
   QDir dir;
   QByteArray dirArray;
   QPointer<QAction> action;
   QList<QAction*> actions;
   QScopedPointer<QAction> scanAction;
   QString currentDirString      = QDir::toNativeSeparators(
         m_dirModel->filePath(m_dirTree->currentIndex()));
   settings_t *settings          = config_get_ptr();
   const char *fullpath          = NULL;
   const char *path_dir_playlist = settings->paths.directory_playlist;
   const char *path_content_db   = settings->paths.path_content_database;

   if (currentDirString.isEmpty())
      return;

   dir                           = currentDirString;

   if (!dir.exists())
      return;

   /* default NULL parameter for parent wasn't added until 5.7 */
   scanAction.reset(new QAction(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY), 0));

   actions.append(scanAction.data());

   action                        = QMenu::exec(actions, QCursor::pos(), NULL, m_dirTree);

   if (!action)
      return;

   dirArray                      = currentDirString.toUtf8();
   fullpath                      = dirArray.constData();

   task_push_dbscan(
         path_dir_playlist,
         path_content_db,
         fullpath, true,
         m_settings->value("show_hidden_files", true).toBool(),
         scan_finished_handler);
#endif
}

void MainWindow::showStatusMessage(QString msg,
      unsigned priority, unsigned duration, bool flush)
{
   emit gotStatusMessage(msg, priority, duration, flush);
}

void MainWindow::onGotStatusMessage(
      QString msg, unsigned priority, unsigned duration, bool flush)
{
   int msecDuration   = 0;
   QScreen *screen    = qApp->primaryScreen();
   QStatusBar *status = statusBar();

   Q_UNUSED(priority)

   if (msg.isEmpty())
      return;

   if (!status)
      return;

   if (screen)
      msecDuration    = (duration / screen->refreshRate()) * 1000;

   if (msecDuration <= 0)
      msecDuration    = 1000;

   if (status->currentMessage().isEmpty() || flush)
   {
      if (m_statusMessageElapsedTimer.elapsed() >= STATUS_MSG_THROTTLE_MSEC)
      {
         qint64 msgDuration = qMax(msecDuration, STATUS_MSG_THROTTLE_MSEC);
         m_statusMessageElapsedTimer.restart();
         status->showMessage(msg, msgDuration);
      }
   }
}

void MainWindow::deferReloadShaderParams()
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   emit gotReloadShaderParams();
#endif
}

void MainWindow::onShaderParamsClicked()
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   if (!m_shaderParamsDialog)
      return;

   m_shaderParamsDialog->show();

   onGotReloadShaderParams();
#endif
}

void MainWindow::onGotReloadShaderParams()
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   if (m_shaderParamsDialog && m_shaderParamsDialog->isVisible())
      m_shaderParamsDialog->reload();
#endif
}

void MainWindow::onCoreOptionsClicked()
{
   if (!m_coreOptionsDialog)
      return;

   m_coreOptionsDialog->show();

   onGotReloadCoreOptions();
}

void MainWindow::onGotReloadCoreOptions()
{
   if (m_coreOptionsDialog && m_coreOptionsDialog->isVisible())
      m_coreOptionsDialog->reload();
}

void MainWindow::appendLogMessage(const QString &msg)
{
   emit gotLogMessage(msg);
}

void MainWindow::onGotLogMessage(const QString &msg)
{
   QString newMsg = msg;

   if (newMsg.at(newMsg.size() - 1) == '\n')
      newMsg.chop(1);

   m_logTextEdit->appendMessage(newMsg);
}

void MainWindow::onLaunchWithComboBoxIndexChanged(int)
{
   QString coreInfoText;
   QVector<QHash<QString, QString> > 
      infoList                  = getCoreInfo();
   QVariantMap          coreMap = m_launchWithComboBox->currentData(Qt::UserRole).value<QVariantMap>();
   core_selection coreSelection = static_cast<core_selection>(coreMap.value("core_selection").toInt());
   int i = 0;

   if (infoList.count() == 0)
      return;

   for (i = 0; i < infoList.count(); i++)
   {
      const QHash<QString, QString> &hash = infoList.at(i);
      const QString                  &key = 
         hash.value("html_key", hash.value("key"));
      const QString                &value = 
         hash.value("html_value", hash.value("value"));

      if (!key.isEmpty())
         coreInfoText                    += key;

      if (!value.isEmpty())
      {
         if (!key.isEmpty())
            coreInfoText                 += " ";

         coreInfoText                    += value;
      }

      if (i < infoList.count() - 1)
         coreInfoText                    += "<br>\n";
   }

   m_coreInfoLabel->setText(coreInfoText);

   if (coreSelection == CORE_SELECTION_LOAD_CORE)
      onLoadCoreClicked();
   else
      m_loadCoreWindow->setProperty("last_launch_with_index",
            m_launchWithComboBox->currentIndex());
}

MainWindow::Theme MainWindow::getThemeFromString(QString themeString)
{
   if (themeString == "default")
      return THEME_SYSTEM_DEFAULT;
   else if (themeString == "dark")
      return THEME_DARK;
   else if (themeString == "custom")
      return THEME_CUSTOM;

   return THEME_SYSTEM_DEFAULT;
}

QString MainWindow::getThemeString(Theme theme)
{
   switch (theme)
   {
      case THEME_SYSTEM_DEFAULT:
         return "default";
      case THEME_DARK:
         return "dark";
      case THEME_CUSTOM:
         return "custom";
      default:
         break;
   }

   return "default";
}

MainWindow::Theme MainWindow::theme()
{
   return m_currentTheme;
}

void MainWindow::setTheme(Theme theme)
{
   m_currentTheme = theme;

   setDefaultCustomProperties();

   switch(theme)
   {
      case THEME_SYSTEM_DEFAULT:
         qApp->setStyleSheet(qt_theme_default_stylesheet.arg(m_settings->value("highlight_color", "palette(highlight)").toString()));
         break;
      case THEME_DARK:
         qApp->setStyleSheet(qt_theme_dark_stylesheet.arg(m_settings->value("highlight_color", "palette(highlight)").toString()));
         break;
      case THEME_CUSTOM:
         qApp->setStyleSheet(m_customThemeString);
         break;
      default:
         break;
   }
#ifdef HAVE_MENU
   m_viewOptionsDialog->repaintIcons();
#endif
}

void MainWindow::setDefaultCustomProperties()
{
   m_gridView->setLayout(QString(DEFAULT_GRID_LAYOUT));
   m_gridView->setSpacing(DEFAULT_GRID_SPACING);
   m_gridItem.setThumbnailVerticalAlign(QString(DEFAULT_GRID_ITEM_THUMBNAIL_ALIGNMENT));
   m_gridItem.setPadding(DEFAULT_GRID_ITEM_MARGIN);
}

void MainWindow::changeThumbnailType(ThumbnailType type)
{
   m_playlistModel->setThumbnailType(type);
   updateVisibleItems();
   m_gridView->viewport()->update();
}

QString MainWindow::changeThumbnail(const QImage &image, QString type)
{
   QHash<QString, QString> hash = getCurrentContentHash();
   QString dirString            = m_playlistModel->getPlaylistThumbnailsDir(hash["db_name"]) + "/" + type;
   QString thumbPath            = dirString + "/" + m_playlistModel->getSanitizedThumbnailName(hash["label_noext"]);
   QByteArray   dirArray        = QDir::toNativeSeparators(dirString).toUtf8();
   const char   *dirData        = dirArray.constData();
   QByteArray thumbArray        = QDir::toNativeSeparators(thumbPath).toUtf8();
   const char *thumbData        = thumbArray.constData();
   int quality                  = -1;
   QDir dir(dirString);
   QImage scaledImage(image);

   if (!dir.exists())
   {
      if (dir.mkpath("."))
         RARCH_LOG("[Qt]: Created directory: %s\n", dirData);
      else
      {
         RARCH_ERR("[Qt]: Could not create directory: %s\n", dirData);
         return QString();
      }
   }

   if (m_settings->contains("thumbnail_max_size"))
   {
      int size = m_settings->value("thumbnail_max_size", 0).toInt();

      if (size != 0 && (image.height() > size ||  image.width() > size))
         scaledImage = image.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
   }

   if (m_settings->contains("thumbnail_quality"))
      quality = m_settings->value("thumbnail_quality", -1).toInt();

   if (scaledImage.save(thumbPath, "png", quality))
   {
      RARCH_LOG("[Qt]: Saved image: %s\n", thumbData);
      m_playlistModel->reloadThumbnailPath(thumbPath);
      updateVisibleItems();

      return thumbPath;
   }

   RARCH_ERR("[Qt]: Could not save image: %s\n", thumbData);
   return QString();
}

void MainWindow::onThumbnailDropped(const QImage &image,
      ThumbnailType thumbnailType)
{
   switch (thumbnailType)
   {
      case THUMBNAIL_TYPE_BOXART:
      {
         QString path = changeThumbnail(image, THUMBNAIL_BOXART);

         if (path.isNull())
            return;

         if (m_thumbnailPixmap)
            delete m_thumbnailPixmap;

         m_thumbnailPixmap = new QPixmap(path);

         onResizeThumbnailOne(*m_thumbnailPixmap, true);
         break;
      }

      case THUMBNAIL_TYPE_TITLE_SCREEN:
      {
         QString path = changeThumbnail(image, THUMBNAIL_TITLE);

         if (path.isNull())
            return;

         if (m_thumbnailPixmap2)
            delete m_thumbnailPixmap2;

         m_thumbnailPixmap2 = new QPixmap(path);

         onResizeThumbnailTwo(*m_thumbnailPixmap2, true);
         break;
      }

      case THUMBNAIL_TYPE_SCREENSHOT:
      {
         QString path = changeThumbnail(image, THUMBNAIL_SCREENSHOT);

         if (path.isNull())
            return;

         if (m_thumbnailPixmap3)
            delete m_thumbnailPixmap3;

         m_thumbnailPixmap3 = new QPixmap(path);

         onResizeThumbnailThree(*m_thumbnailPixmap3, true);
         break;
      }
   }
}

QVector<QHash<QString, QString> > MainWindow::getCoreInfo()
{
   unsigned i;
   QVector<QHash<QString, QString> > infoList;
   core_info_ctx_find_t core_info_finder;
   QHash<QString, QString> currentCore = getSelectedCore();
   const core_info_t        *core_info = NULL;
   QByteArray currentCorePathArray     = currentCore["core_path"].toUtf8();
   const char *current_core_path_data  = currentCorePathArray.constData();

   /* Search for current core */
   core_info_finder.inf                = NULL;
   core_info_finder.path               = current_core_path_data;

   if (core_info_find(&core_info_finder))
      core_info                        = core_info_finder.inf;

   if (     currentCore["core_path"].isEmpty() 
         || !core_info 
         || !core_info->config_data)
   {
      QHash<QString, QString> hash;

      hash["key"]   = msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE);
      hash["value"] = "";

      infoList.append(hash);

      return infoList;
   }

   if (core_info->core_name)
   {
      QHash<QString, QString> hash;

      hash["key"]   = QString(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME)) + ":";
      hash["value"] = core_info->core_name;

      infoList.append(hash);
   }

   if (core_info->display_name)
   {
      QHash<QString, QString> hash;

      hash["key"]   = QString(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL)) + ":";
      hash["value"] = core_info->display_name;

      infoList.append(hash);
   }

   if (core_info->systemname)
   {
      QHash<QString, QString> hash;

      hash["key"]   = QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME)) + ":";
      hash["value"] = core_info->systemname;

      infoList.append(hash);
   }

   if (core_info->system_manufacturer)
   {
      QHash<QString, QString> hash;

      hash["key"]   = QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER)) + ":";
      hash["value"] = core_info->system_manufacturer;

      infoList.append(hash);
   }

   if (core_info->categories_list)
   {
      QHash<QString, QString> hash;
      QString categories;

      for (i = 0; i < core_info->categories_list->size; i++)
      {
         categories += core_info->categories_list->elems[i].data;

         if (i < core_info->categories_list->size - 1)
            categories += ", ";
      }

      hash["key"]   = QString(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES)) + ":";
      hash["value"] = categories;

      infoList.append(hash);
   }

   if (core_info->authors_list)
   {
      QHash<QString, QString> hash;
      QString authors;

      for (i = 0; i < core_info->authors_list->size; i++)
      {
         authors += core_info->authors_list->elems[i].data;

         if (i < core_info->authors_list->size - 1)
            authors += ", ";
      }

      hash["key"]   = QString(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS)) + ":";
      hash["value"] = authors;

      infoList.append(hash);
   }

   if (core_info->permissions_list)
   {
      QHash<QString, QString> hash;
      QString permissions;

      for (i = 0; i < core_info->permissions_list->size; i++)
      {
         permissions += core_info->permissions_list->elems[i].data;

         if (i < core_info->permissions_list->size - 1)
            permissions += ", ";
      }

      hash["key"]   = QString(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS)) + ":";
      hash["value"] = permissions;

      infoList.append(hash);
   }

   if (core_info->licenses_list)
   {
      QHash<QString, QString> hash;
      QString licenses;

      for (i = 0; i < core_info->licenses_list->size; i++)
      {
         licenses += core_info->licenses_list->elems[i].data;

         if (i < core_info->licenses_list->size - 1)
            licenses += ", ";
      }

      hash["key"]   = QString(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES)) + ":";
      hash["value"] = licenses;

      infoList.append(hash);
   }

   if (core_info->supported_extensions_list)
   {
      QHash<QString, QString> hash;
      QString supported_extensions;

      for (i = 0; i < core_info->supported_extensions_list->size; i++)
      {
         supported_extensions += core_info->supported_extensions_list->elems[i].data;

         if (i < core_info->supported_extensions_list->size - 1)
            supported_extensions += ", ";
      }

      hash["key"]   = QString(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS)) + ":";
      hash["value"] = supported_extensions;

      infoList.append(hash);
   }

   if (core_info->firmware_count > 0)
   {
      core_info_ctx_firmware_t firmware_info;
      bool update_missing_firmware   = false;
      bool set_missing_firmware      = false;
      settings_t *settings           = config_get_ptr();

      firmware_info.path             = core_info->path;
      firmware_info.directory.system = settings->paths.directory_system;

      rarch_ctl(RARCH_CTL_UNSET_MISSING_BIOS, NULL);

      update_missing_firmware        = core_info_list_update_missing_firmware(&firmware_info, &set_missing_firmware);

      if (set_missing_firmware)
         rarch_ctl(RARCH_CTL_SET_MISSING_BIOS, NULL);

      if (update_missing_firmware)
      {
         QHash<QString, QString> hash;

         hash["key"]   = QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE)) + ":";
         hash["value"] = "";

         infoList.append(hash);

         /* FIXME: This looks hacky and probably
          * needs to be improved for good translation support. */

         for (i = 0; i < core_info->firmware_count; i++)
         {
            if (core_info->firmware[i].desc)
            {
               QString valueText;
               QHash<QString, QString> hash;
               QString labelText = "(!) ";
               bool missing      = false;

               if (core_info->firmware[i].missing)
               {
                  missing        = true;
                  labelText     += msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_MISSING);
               }
               else
                  labelText     += msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_PRESENT);

               labelText        += ", ";

               if (core_info->firmware[i].optional)
                  labelText     += msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_OPTIONAL);
               else
                  labelText     += msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_REQUIRED);

               labelText        += ":";

               if (core_info->firmware[i].desc)
                  valueText      = core_info->firmware[i].desc;
               else
                  valueText      = msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME);

               hash["key"]       = labelText;
               hash["value"]     = valueText;

               if (missing)
               {
                  QString style       = "font-weight: bold; color: #ff0000";
                  hash["label_style"] = style;
                  hash["value_style"] = style;
                  hash["html_key"]    = "<b><font color=\"#ff0000\">" + hash["key"] + "</font></b>";
                  hash["html_value"]  = "<b><font color=\"#ff0000\">" + hash["value"] + "</font></b>";
               }
               else
               {
                  QString style       = "font-weight: bold; color: rgb(0, 175, 0)";
                  hash["label_style"] = style;
                  hash["value_style"] = style;
                  hash["html_key"]    = "<b><font color=\"#00af00\">" + hash["key"] + "</font></b>";
                  hash["html_value"]  = "<b><font color=\"#00af00\">" + hash["value"] + "</font></b>";
               }

               infoList.append(hash);
            }
         }
      }
   }

   if (core_info->notes)
   {
      for (i = 0; i < core_info->note_list->size; i++)
      {
         QHash<QString, QString> hash;

         hash["key"]   = "";
         hash["value"] = core_info->note_list->elems[i].data;

         infoList.append(hash);
      }
   }

   return infoList;
}

void MainWindow::onSearchResetClicked()
{
   m_searchLineEdit->clear();
   onSearchEnterPressed();
}

QToolButton* MainWindow::coreInfoPushButton()
{
   return m_coreInfoPushButton;
}

void MainWindow::onTreeViewItemsSelected(QModelIndexList selectedIndexes)
{
   QString dir;

   if (selectedIndexes.isEmpty())
      return;

   dir = m_dirModel->filePath(selectedIndexes.first());

   selectBrowserDir(dir);
}

void MainWindow::onFileDoubleClicked(const QModelIndex &proxyIndex)
{
   const QModelIndex index = m_proxyFileModel->mapToSource(proxyIndex);

   if (m_fileModel->isDir(index))
      m_dirTree->setCurrentIndex(m_dirModel->index(m_fileModel->filePath(index)));
   else
      loadContent(getFileContentHash(index));
}

void MainWindow::selectBrowserDir(QString path)
{
   if (!path.isEmpty())
   {
      QModelIndex sourceIndex = m_fileModel->setRootPath(path);
      QModelIndex proxyIndex  = m_proxyFileModel->mapFromSource(sourceIndex);
      m_fileTableHeaderState  = m_fileTableView->horizontalHeader()->saveState();

      if (proxyIndex.isValid())
         m_fileTableView->setRootIndex(proxyIndex);
      else
      {
         /* the directory is filtered out. Remove the filter for a moment. 
          * FIXME: Find a way to not have to do this 
          * (not filtering dirs is one). */
         m_proxyFileModel->setFilterRegExp(QRegExp());
         m_fileTableView->setRootIndex(m_proxyFileModel->mapFromSource(sourceIndex));
         m_proxyFileModel->setFilterRegExp(m_searchRegExp);
      }
   }
   setCoreActions();
}

QTabWidget* MainWindow::browserAndPlaylistTabWidget()
{
   return m_browserAndPlaylistTabWidget;
}

void MainWindow::onDropWidgetEnterPressed()
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
   /* entry is being renamed, ignore this enter press */
   if (m_tableView->isPersistentEditorOpen(m_tableView->currentIndex()))
#else
   /* we can only check if any editor at all is open */
   if (m_tableView->isEditorOpen())
#endif
      return;
   onRunClicked();
}

QModelIndex MainWindow::getCurrentContentIndex()
{
   if (m_viewType == VIEW_TYPE_LIST)
      return m_tableView->currentIndex();
   else if (m_viewType == VIEW_TYPE_ICONS)
      return m_gridView->currentIndex();
   return QModelIndex();
}

QHash<QString, QString> MainWindow::getCurrentContentHash()
{
   return getCurrentContentIndex().data(PlaylistModel::HASH).value<QHash<QString, QString> >();
}

QHash<QString, QString> MainWindow::getFileContentHash(const QModelIndex &index)
{
   QHash<QString, QString> hash;
   QFileInfo fileInfo  = m_fileModel->fileInfo(index);

   hash["path"]        = QDir::toNativeSeparators(m_fileModel->filePath(index));
   hash["label"]       = hash["path"];
   hash["label_noext"] = fileInfo.completeBaseName();
   hash["db_name"]     = fileInfo.dir().dirName();

   return hash;
}

void MainWindow::onContentItemDoubleClicked(const QModelIndex &index)
{
   Q_UNUSED(index);
   onRunClicked();
}

void MainWindow::onStartCoreClicked()
{
   content_ctx_info_t content_info;

   content_info.argc                   = 0;
   content_info.argv                   = NULL;
   content_info.args                   = NULL;
   content_info.environ_get            = NULL;

   path_clear(RARCH_PATH_BASENAME);

   if (!task_push_start_current_core(&content_info))
      QMessageBox::critical(this, msg_hash_to_str(MSG_ERROR), msg_hash_to_str(MSG_FAILED_TO_LOAD_CONTENT));
}

QHash<QString, QString> MainWindow::getSelectedCore()
{
   QVariantMap coreMap = m_launchWithComboBox->currentData(Qt::UserRole).value<QVariantMap>();
   core_selection coreSelection = static_cast<core_selection>(coreMap.value("core_selection").toInt());
   QHash<QString, QString> coreHash;
   QHash<QString, QString> contentHash;
   ViewType viewType = getCurrentViewType();

   if (viewType == VIEW_TYPE_LIST)
      contentHash = m_tableView->currentIndex().data(PlaylistModel::HASH).value<QHash<QString, QString> >();
   else if (viewType == VIEW_TYPE_ICONS)
      contentHash = m_gridView->currentIndex().data(PlaylistModel::HASH).value<QHash<QString, QString> >();
   else
      return coreHash;

   switch(coreSelection)
   {
      case CORE_SELECTION_CURRENT:
         coreHash["core_path"] = path_get(RARCH_PATH_CORE);
         break;
      case CORE_SELECTION_PLAYLIST_SAVED:
         if (contentHash.isEmpty() || contentHash["core_path"].isEmpty())
            break;

         coreHash["core_path"] = contentHash["core_path"];

         break;
      case CORE_SELECTION_PLAYLIST_DEFAULT:
      {
         QString defaultCorePath;

         if (contentHash.isEmpty() || contentHash["db_name"].isEmpty())
            break;

         defaultCorePath = getPlaylistDefaultCore(contentHash["db_name"]);

         if (!defaultCorePath.isEmpty())
            coreHash["core_path"] = defaultCorePath;

         break;
      }
      default:
         break;
   }

   return coreHash;
}

/* the hash typically has the following keys:
path - absolute path to the content file
core_path - absolute path to the core, or "DETECT" to ask the user
db_name - the display name of the rdb database this content is from
label - the display name of the content, usually comes from the database
crc32 - an upper-case, 8 byte string representation of the hex CRC32 checksum (e.g. ABCDEF12) followed by "|crc"
core_name - the display name of the core, or "DETECT" if unknown
label_noext - the display name of the content that is guaranteed not to contain a file extension
*/
void MainWindow::loadContent(const QHash<QString, QString> &contentHash)
{
   content_ctx_info_t content_info;
   QByteArray corePathArray;
   QByteArray contentPathArray;
   QByteArray contentLabelArray;
   QByteArray contentDbNameArray;
   QByteArray contentCrc32Array;
   char contentDbNameFull[PATH_MAX_LENGTH];
   const char *corePath        = NULL;
   const char *contentPath     = NULL;
   const char *contentLabel    = NULL;
   const char *contentDbName   = NULL;
   const char *contentCrc32    = NULL;
   QVariantMap coreMap         = m_launchWithComboBox->currentData(Qt::UserRole).value<QVariantMap>();
   core_selection coreSelection = static_cast<core_selection>(coreMap.value("core_selection").toInt());
   core_info_ctx_find_t core_info;

   contentDbNameFull[0] = '\0';

   if (m_pendingRun)
      coreSelection = CORE_SELECTION_CURRENT;

   if (coreSelection == CORE_SELECTION_ASK)
   {
      QStringList extensionFilters;

      if (contentHash.contains("path"))
      {
         QString extensionStr;
         int lastIndex        = contentHash["path"].lastIndexOf('.');
         QByteArray pathArray = contentHash["path"].toUtf8();
         const char *pathData = pathArray.constData();

         if (lastIndex >= 0)
         {
            extensionStr = contentHash["path"].mid(lastIndex + 1);

            if (!extensionStr.isEmpty())
               extensionFilters.append(extensionStr.toLower());
         }

         if (path_is_compressed_file(pathData))
         {
            unsigned i = 0;
            struct string_list *list = file_archive_get_file_list(pathData, NULL);

            if (list)
            {
               if (list->size > 0)
               {
                  for (i = 0; i < list->size; i++)
                  {
                     const char *filePath = list->elems[i].data;
                     const char *extension = path_get_extension(filePath);

                     if (!extensionFilters.contains(extension, Qt::CaseInsensitive))
                        extensionFilters.append(extension);
                  }
               }

               string_list_free(list);
            }
         }
      }

      m_pendingRun = true;
      onLoadCoreClicked(extensionFilters);

      return;
   }

   switch (coreSelection)
   {
      case CORE_SELECTION_CURRENT:
         corePathArray     = path_get(RARCH_PATH_CORE);
         contentPathArray  = contentHash["path"].toUtf8();
         contentLabelArray = contentHash["label_noext"].toUtf8();
         break;
      case CORE_SELECTION_PLAYLIST_SAVED:
         corePathArray     = contentHash["core_path"].toUtf8();
         contentPathArray  = contentHash["path"].toUtf8();
         contentLabelArray = contentHash["label_noext"].toUtf8();
         break;
      case CORE_SELECTION_PLAYLIST_DEFAULT:
      {
         QString defaultCorePath = getPlaylistDefaultCore(contentHash["db_name"]);

         if (!defaultCorePath.isEmpty())
         {
            corePathArray = defaultCorePath.toUtf8();
            contentPathArray = contentHash["path"].toUtf8();
            contentLabelArray = contentHash["label_noext"].toUtf8();
         }

         break;
      }
      default:
         return;
   }

   contentDbNameArray                  = contentHash["db_name"].toUtf8();
   contentCrc32Array                   = contentHash["crc32"].toUtf8();

   corePath                            = corePathArray.constData();
   contentPath                         = contentPathArray.constData();
   contentLabel                        = contentLabelArray.constData();
   contentDbName                       = contentDbNameArray.constData();
   contentCrc32                        = contentCrc32Array.constData();

   /* Search for specified core - ensures path
    * is 'sanitised' */
   core_info.inf                       = NULL;
   core_info.path                      = corePath;

   if (core_info_find(&core_info) &&
       !string_is_empty(core_info.inf->path))
      corePath = core_info.inf->path;

   /* Add lpl extension to db_name, if required */
   if (!string_is_empty(contentDbName))
   {
      const char *extension = NULL;

      strlcpy(contentDbNameFull, contentDbName, sizeof(contentDbNameFull));
      extension = path_get_extension(contentDbNameFull);

      if (      string_is_empty(extension) 
            || !string_is_equal_noncase(
            extension, FILE_PATH_LPL_EXTENSION_NO_DOT))
         strlcat(
               contentDbNameFull, FILE_PATH_LPL_EXTENSION,
                     sizeof(contentDbNameFull));
   }

   content_info.argc                   = 0;
   content_info.argv                   = NULL;
   content_info.args                   = NULL;
   content_info.environ_get            = NULL;

#ifdef HAVE_MENU
   menu_navigation_set_selection(0);
#endif

   command_event(CMD_EVENT_UNLOAD_CORE, NULL);

   if (!task_push_load_content_with_new_core_from_companion_ui(
         corePath, contentPath, contentLabel, contentDbNameFull, contentCrc32,
         &content_info, NULL, NULL))
   {
      QMessageBox::critical(this, msg_hash_to_str(MSG_ERROR),
            msg_hash_to_str(MSG_FAILED_TO_LOAD_CONTENT));
      return;
   }

#ifdef HAVE_MENU
   menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);
#endif
}

void MainWindow::onRunClicked()
{
   QHash<QString, QString> contentHash;

   switch (m_currentBrowser)
   {
      case BROWSER_TYPE_FILES:
         contentHash = getFileContentHash(
               m_proxyFileModel->mapToSource(m_fileTableView->currentIndex()));
         break;
      case BROWSER_TYPE_PLAYLISTS:
         contentHash = getCurrentContentHash();
         break;
   }

   if (contentHash.isEmpty())
      return;

   loadContent(contentHash);
}

bool MainWindow::isContentLessCore()
{
   rarch_system_info_t *system = runloop_get_system_info();

   return system->load_no_content;
}

bool MainWindow::isCoreLoaded()
{
   if (  m_currentCore.isEmpty() ||
         m_currentCore == msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE))
      return false;

   return true;
}

PlaylistEntryDialog* MainWindow::playlistEntryDialog()
{
   return m_playlistEntryDialog;
}

ViewOptionsDialog* MainWindow::viewOptionsDialog()
{
   return m_viewOptionsDialog;
}

void MainWindow::setCoreActions()
{
   QListWidgetItem *currentPlaylistItem = m_listWidget->currentItem();
   ViewType                    viewType = getCurrentViewType();
   QHash<QString, QString>         hash = getCurrentContentHash();
   QString      currentPlaylistFileName = QString();

   m_launchWithComboBox->clear();

   if (isContentLessCore())
      m_startCorePushButton->show();
   else
      m_startCorePushButton->hide();

   if (     isCoreLoaded() 
         && m_settings->value("suggest_loaded_core_first", false).toBool())
   {
      QVariantMap comboBoxMap;
      comboBoxMap["core_name"]      = m_currentCore;
      comboBoxMap["core_path"]      = path_get(RARCH_PATH_CORE);
      comboBoxMap["core_selection"] = CORE_SELECTION_CURRENT;
      m_launchWithComboBox->addItem(m_currentCore, QVariant::fromValue(comboBoxMap));
   }

   if (m_currentBrowser == BROWSER_TYPE_PLAYLISTS)
   {
      if (!hash.isEmpty())
      {
         QString coreName = hash["core_name"];

         if (coreName.isEmpty())
            coreName = "<n/a>";
         else
         {
            const char *detect_str = "DETECT";

            if (coreName != detect_str)
            {
               if (m_launchWithComboBox->findText(coreName) == -1)
               {
                  int i = 0;
                  bool found_existing = false;

                  for (i = 0; i < m_launchWithComboBox->count(); i++)
                  {
                     QVariantMap map = m_launchWithComboBox->itemData(i, Qt::UserRole).toMap();

                     if (map.value("core_path").toString() == hash["core_path"] || map.value("core_name").toString() == coreName)
                     {
                        found_existing = true;
                        break;
                     }
                  }

                  if (!found_existing)
                  {
                     QVariantMap comboBoxMap;
                     comboBoxMap["core_name"]      = coreName;
                     comboBoxMap["core_path"]      = hash["core_path"];
                     comboBoxMap["core_selection"] = CORE_SELECTION_PLAYLIST_SAVED;
                     m_launchWithComboBox->addItem(coreName, QVariant::fromValue(comboBoxMap));
                  }
               }
            }
         }
      }
   }

   switch(m_currentBrowser)
   {
      case BROWSER_TYPE_PLAYLISTS:
         currentPlaylistFileName = hash["db_name"];
         break;
      case BROWSER_TYPE_FILES:
         currentPlaylistFileName = m_fileModel->rootDirectory().dirName();
         break;
   }

   if (!currentPlaylistFileName.isEmpty())
   {
      QString defaultCorePath = getPlaylistDefaultCore(currentPlaylistFileName);

      if (!defaultCorePath.isEmpty())
      {
         QString currentPlaylistItemDataString;
         core_info_ctx_find_t core_info_finder;
         bool allPlaylists                  = false;
         int row                            = 0;
         QByteArray defaultCorePathArray    = defaultCorePath.toUtf8();
         const char *default_core_path_data = defaultCorePathArray.constData();

         if (currentPlaylistItem)
         {
            currentPlaylistItemDataString   = currentPlaylistItem->data(Qt::UserRole).toString();
            allPlaylists                    = (
                  currentPlaylistItemDataString == ALL_PLAYLISTS_TOKEN);
         }

         for (row = 0; row < m_listWidget->count(); row++)
         {
            if (allPlaylists)
            {
               QFileInfo info;
               QListWidgetItem *listItem = m_listWidget->item(row);
               QString    listItemString = listItem->data(Qt::UserRole).toString();

               info.setFile(listItemString);

               if (listItemString == ALL_PLAYLISTS_TOKEN)
                  continue;
            }

            /* Search for default core */
            core_info_finder.inf         = NULL;
            core_info_finder.path        = default_core_path_data;

            if (core_info_find(&core_info_finder))
            {
               const core_info_t *info = core_info_finder.inf;

               if (m_launchWithComboBox->findText(info->core_name) == -1)
               {
                  int i               = 0;
                  bool found_existing = false;

                  for (i = 0; i < m_launchWithComboBox->count(); i++)
                  {
                     QVariantMap map            = 
                        m_launchWithComboBox->itemData(
                              i, Qt::UserRole).toMap();
                     QByteArray CorePathArray   = 
                        map.value("core_path").toString().toUtf8();
                     const char *core_path_data = CorePathArray.constData();

                     if (string_starts_with(path_basename(core_path_data),
                              info->core_file_id.str) ||
                           map.value("core_name").toString() == info->core_name ||
                           map.value("core_name").toString() == info->display_name)
                     {
                        found_existing = true;
                        break;
                     }
                  }

                  if (!found_existing)
                  {
                     QVariantMap comboBoxMap;
                     comboBoxMap["core_name"] = info->core_name;
                     comboBoxMap["core_path"] = info->path;
                     comboBoxMap["core_selection"] = CORE_SELECTION_PLAYLIST_DEFAULT;
                     m_launchWithComboBox->addItem(info->core_name, QVariant::fromValue(comboBoxMap));
                  }
               }
            }

            if (!allPlaylists)
               break;
         }
      }
   }

   {
      QVariantMap comboBoxMap;
      comboBoxMap["core_selection"] = CORE_SELECTION_ASK;
      m_launchWithComboBox->addItem(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK), QVariant::fromValue(comboBoxMap));
      m_launchWithComboBox->insertSeparator(m_launchWithComboBox->count());
      comboBoxMap["core_selection"] = CORE_SELECTION_LOAD_CORE;
      m_launchWithComboBox->addItem(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE)) + "...", QVariant::fromValue(comboBoxMap));
   }
}

void MainWindow::onTabWidgetIndexChanged(int index)
{
   if (m_browserAndPlaylistTabWidget->tabText(index) == msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER))
   {
      m_currentBrowser = BROWSER_TYPE_FILES;

      m_centralWidget->setCurrentWidget(m_fileTableView);

      onCurrentFileChanged(m_fileTableView->currentIndex());
   }
   else if (m_browserAndPlaylistTabWidget->tabText(index) == msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS))
   {
      m_currentBrowser = BROWSER_TYPE_PLAYLISTS;

      m_centralWidget->setCurrentWidget(m_playlistViewsAndFooter);

      onCurrentItemChanged(m_tableView->currentIndex());
   }

   applySearch();

   setCoreActions();
}

QToolButton* MainWindow::runPushButton()
{
   return m_runPushButton;
}

QToolButton* MainWindow::stopPushButton()
{
   return m_stopPushButton;
}

QToolButton* MainWindow::startCorePushButton()
{
   return m_startCorePushButton;
}

QComboBox* MainWindow::launchWithComboBox()
{
   return m_launchWithComboBox;
}

void MainWindow::onSearchLineEditEdited(const QString &text)
{
   int i = 0;
   QVector<unsigned> textUnicode = text.toUcs4();
   QVector<unsigned> textHiraToKata;
   QVector<unsigned> textKataToHira;
   bool foundHira = false;
   bool foundKata = false;

   for (i = 0; i < textUnicode.size(); i++)
   {
      unsigned code = textUnicode.at(i);

      if (code >= HIRAGANA_START && code <= HIRAGANA_END)
      {
         foundHira       = true;
         textHiraToKata += code + HIRA_KATA_OFFSET;
      }
      else if (code >= KATAKANA_START && code <= KATAKANA_END)
      {
         foundKata       = true;
         textKataToHira += code - HIRA_KATA_OFFSET;
      }
      else
      {
         textHiraToKata += code;
         textKataToHira += code;
      }
   }

   if (!foundHira && !foundKata)
      m_searchRegExp = QRegExp(text, Qt::CaseInsensitive);
   else if (foundHira && !foundKata)
      m_searchRegExp = QRegExp(text + "|" 
            + QString::fromUcs4(textHiraToKata.constData(),
               textHiraToKata.size()), Qt::CaseInsensitive);
   else if (!foundHira && foundKata)
      m_searchRegExp = QRegExp(text + "|" 
            + QString::fromUcs4(textKataToHira.constData(),
               textKataToHira.size()), Qt::CaseInsensitive);
   else
      m_searchRegExp = QRegExp(text + "|" 
            + QString::fromUcs4(textHiraToKata.constData(),
               textHiraToKata.size()) + "|" + 
            QString::fromUcs4(textKataToHira.constData(),
               textKataToHira.size()), Qt::CaseInsensitive);

   applySearch();
}

void MainWindow::applySearch()
{
   switch (m_currentBrowser)
   {
      case BROWSER_TYPE_PLAYLISTS:
         if (m_proxyModel->filterRegExp() != m_searchRegExp)
         {
            m_proxyModel->setFilterRegExp(m_searchRegExp);
            updateItemsCount();
         }
         break;
      case BROWSER_TYPE_FILES:
         if (m_proxyFileModel->filterRegExp() != m_searchRegExp)
            m_proxyFileModel->setFilterRegExp(m_searchRegExp);
         break;
   }
}

void MainWindow::onViewClosedDocksAboutToShow()
{
   QList<QDockWidget*> dockWidgets;
   int i               = 0;
   QMenu         *menu = qobject_cast<QMenu*>(sender());
   bool found          = false;

   if (!menu)
      return;

   dockWidgets         = findChildren<QDockWidget*>();

   menu->clear();

   if (dockWidgets.isEmpty())
   {
      menu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE));
      return;
   }

   for (i = 0; i < dockWidgets.count(); i++)
   {
      const QDockWidget *dock = dockWidgets.at(i);

      if (!dock->isVisible())
      {
         QAction *action = menu->addAction(
               dock->property("menu_text").toString(),
               this, SLOT(onShowHiddenDockWidgetAction()));
         action->setProperty("dock_name", dock->objectName());
         found = true;
      }
   }

   if (!found)
      menu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE));
}

void MainWindow::onShowHiddenDockWidgetAction()
{
   QAction *action = qobject_cast<QAction*>(sender());
   QDockWidget *dock = NULL;

   if (!action)
      return;

   dock = findChild<QDockWidget*>(action->property("dock_name").toString());

   if (!dock)
      return;

   if (!dock->isVisible())
   {
      addDockWidget(static_cast<Qt::DockWidgetArea>(
               dock->property("default_area").toInt()), dock);
      dock->setVisible(true);
      dock->setFloating(false);
   }
}

QWidget* MainWindow::searchWidget()
{
   return m_searchWidget;
}

QLineEdit* MainWindow::searchLineEdit()
{
   return m_searchLineEdit;
}

void MainWindow::onSearchEnterPressed()
{
   onSearchLineEditEdited(m_searchLineEdit->text());
}

void MainWindow::onCurrentTableItemDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
   QHash<QString, QString> hash;

   if (!roles.contains(Qt::EditRole))
      return;

   if (topLeft != bottomRight)
      return;

   hash = topLeft.data(PlaylistModel::HASH).value<QHash<QString, QString>>();

   updateCurrentPlaylistEntry(hash);

   onCurrentItemChanged(topLeft);
}

void MainWindow::onCurrentListItemDataChanged(QListWidgetItem *item)
{
   renamePlaylistItem(item, item->text());
}

void MainWindow::renamePlaylistItem(QListWidgetItem *item, QString newName)
{
   QString oldPath;
   QString newPath;
   QString extension;
   QString oldName;
   QFile file;
   QFileInfo info;
   QFileInfo playlistInfo;
   QString playlistPath;
   bool specialPlaylist          = false;
   settings_t *settings          = config_get_ptr();
   const char *path_dir_playlist = settings->paths.directory_playlist;
   QDir playlistDir(path_dir_playlist);

   if (!item)
      return;

   playlistPath                  = item->data(Qt::UserRole).toString();
   playlistInfo                  = playlistPath;
   oldName                       = playlistInfo.completeBaseName();

   /* Don't just compare strings in case there are
    * case differences on Windows that should be ignored. */
   if (QDir(playlistInfo.absoluteDir()) != QDir(playlistDir))
   {
      /* special playlists like history etc. can't have an association */
      specialPlaylist = true;
   }

   if (specialPlaylist)
   {
      /* special playlists shouldn't be editable already,
       * but just in case, set the old name back and
       * early return if they rename it */
      item->setText(oldName);
      return;
   }

   /* block this signal because setData() would trigger
    * an infinite loop here */
   disconnect(m_listWidget, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(onCurrentListItemDataChanged(QListWidgetItem*)));

   oldPath   = item->data(Qt::UserRole).toString();

   file.setFileName(oldPath);
   info      = file;

   extension = info.suffix();

   newPath   = info.absolutePath();

   /* absolutePath() will always use / even on Windows */
   if (newPath.at(newPath.count() - 1) != '/')
      /* add trailing slash if the path doesn't have one */
      newPath += '/';

   newPath += newName + "." + extension;

   item->setData(Qt::UserRole, newPath);

   if (!file.rename(newPath))
   {
      RARCH_ERR("[Qt]: Could not rename playlist.\n");
      item->setText(oldName);
   }

   connect(m_listWidget, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(onCurrentListItemDataChanged(QListWidgetItem*)));
}

void MainWindow::onCurrentItemChanged(const QModelIndex &index)
{
   onCurrentItemChanged(index.data(PlaylistModel::HASH).value<QHash<QString, QString>>());
}

void MainWindow::onCurrentFileChanged(const QModelIndex &index)
{
   onCurrentItemChanged(getFileContentHash(m_proxyFileModel->mapToSource(index)));
}

void MainWindow::onCurrentItemChanged(const QHash<QString, QString> &hash)
{
   QString    path = hash["path"];
   bool acceptDrop = false;

   if (m_thumbnailPixmap)
      delete m_thumbnailPixmap;
   if (m_thumbnailPixmap2)
      delete m_thumbnailPixmap2;
   if (m_thumbnailPixmap3)
      delete m_thumbnailPixmap3;

   if (m_playlistModel->isSupportedImage(path))
   {
      /* use thumbnail widgets to show regular image files */
      m_thumbnailPixmap = new QPixmap(path);
      m_thumbnailPixmap2 = new QPixmap(*m_thumbnailPixmap);
      m_thumbnailPixmap3 = new QPixmap(*m_thumbnailPixmap);
   }
   else
   {
      QString thumbnailsDir = m_playlistModel->getPlaylistThumbnailsDir(hash["db_name"]);
      QString thumbnailName = m_playlistModel->getSanitizedThumbnailName(hash["label_noext"]);

      m_thumbnailPixmap     = new QPixmap(thumbnailsDir + "/" + THUMBNAIL_BOXART + "/" + thumbnailName);
      m_thumbnailPixmap2    = new QPixmap(thumbnailsDir + "/" + THUMBNAIL_TITLE + "/" + thumbnailName);
      m_thumbnailPixmap3    = new QPixmap(thumbnailsDir + "/" + THUMBNAIL_SCREENSHOT + "/" + thumbnailName);

      if (      m_currentBrowser == BROWSER_TYPE_PLAYLISTS 
            && !currentPlaylistIsSpecial())
         acceptDrop = true;
   }

   onResizeThumbnailOne(*m_thumbnailPixmap, acceptDrop);
   onResizeThumbnailTwo(*m_thumbnailPixmap2, acceptDrop);
   onResizeThumbnailThree(*m_thumbnailPixmap3, acceptDrop);

   setCoreActions();
}

void MainWindow::setThumbnail(QString widgetName,
      QPixmap &pixmap, bool acceptDrop)
{
   ThumbnailWidget *thumbnail = findChild<ThumbnailWidget*>(widgetName);

   if (thumbnail)
      thumbnail->setPixmap(pixmap, acceptDrop);
}

void MainWindow::onResizeThumbnailOne(QPixmap &pixmap, bool acceptDrop)
{
   setThumbnail("thumbnail", pixmap, acceptDrop);
}

void MainWindow::onResizeThumbnailTwo(QPixmap &pixmap, bool acceptDrop)
{
   setThumbnail("thumbnail2", pixmap, acceptDrop);
}

void MainWindow::onResizeThumbnailThree(QPixmap &pixmap, bool acceptDrop)
{
   setThumbnail("thumbnail3", pixmap, acceptDrop);
}

void MainWindow::setCurrentViewType(ViewType viewType)
{
   m_viewType = viewType;

   switch (viewType)
   {
      case VIEW_TYPE_ICONS:
         m_playlistViews->setCurrentWidget(m_gridView);
         m_zoomWidget->show();
         break;
      case VIEW_TYPE_LIST:
      default:
         m_playlistViews->setCurrentWidget(m_tableView);
         m_zoomWidget->hide();
         break;
   }
}

void MainWindow::setCurrentThumbnailType(ThumbnailType thumbnailType)
{
   m_thumbnailType = thumbnailType;

   m_playlistModel->setThumbnailType(thumbnailType);
   updateVisibleItems();
   m_gridView->viewport()->update();
}

MainWindow::ViewType MainWindow::getCurrentViewType()
{
   return m_viewType;
}

ThumbnailType MainWindow::getCurrentThumbnailType()
{
   return m_thumbnailType;
}

void MainWindow::onCurrentListItemChanged(
      QListWidgetItem *current, QListWidgetItem *previous)
{
   Q_UNUSED(current)
   Q_UNUSED(previous)

   initContentTableWidget();

   setCoreActions();
}

TableView* MainWindow::contentTableView()
{
   return m_tableView;
}

QTableView* MainWindow::fileTableView()
{
   return m_fileTableView;
}

QStackedWidget* MainWindow::centralWidget()
{
   return m_centralWidget;
}

FileDropWidget* MainWindow::playlistViews()
{
   return m_playlistViews;
}

QWidget* MainWindow::playlistViewsAndFooter()
{
   return m_playlistViewsAndFooter;
}

GridView* MainWindow::contentGridView()
{
   return m_gridView;
}

void MainWindow::onBrowserDownloadsClicked()
{
   settings_t *settings = config_get_ptr();
   QDir dir(settings->paths.directory_core_assets);
   QString path = dir.absolutePath();
   QModelIndex index;

   m_pendingDirScrollPath = path;

   index = m_dirModel->index(path);

   m_dirTree->setCurrentIndex(index);

   onDownloadScroll(path);
}

void MainWindow::onDownloadScroll(QString path)
{
   QModelIndex index = m_dirModel->index(path);
   m_dirTree->scrollTo(index, QAbstractItemView::PositionAtTop);
   m_dirTree->expand(index);

   /* FIXME: Find a way to make this unnecessary */
   emit scrollToDownloadsAgain(path);
}

void MainWindow::onDownloadScrollAgain(QString path)
{
   QModelIndex index = m_dirModel->index(path);
   m_dirTree->scrollTo(index, QAbstractItemView::PositionAtTop);
   m_dirTree->expand(index);
}

void MainWindow::onBrowserUpClicked()
{
   QDir dir(m_dirModel->filePath(m_dirTree->currentIndex()));

   dir.cdUp();

   m_dirTree->setCurrentIndex(m_dirModel->index(dir.absolutePath()));
   m_dirTree->scrollTo(m_dirTree->currentIndex(),
         QAbstractItemView::EnsureVisible);
}

void MainWindow::onBrowserStartClicked()
{
   settings_t *settings = config_get_ptr();

   m_dirTree->setCurrentIndex(
         m_dirModel->index(settings->paths.directory_menu_content));
   m_dirTree->scrollTo(m_dirTree->currentIndex(), QAbstractItemView::PositionAtTop);
}

ListWidget* MainWindow::playlistListWidget()
{
   return m_listWidget;
}

TreeView* MainWindow::dirTreeView()
{
   return m_dirTree;
}

void MainWindow::onTimeout()
{
   bool contentless = false;
   bool is_inited   = false;

   content_get_status(&contentless, &is_inited);

   if (is_inited)
   {
      if (m_runPushButton->isVisible())
         m_runPushButton->hide();
      if (!m_stopPushButton->isVisible())
         m_stopPushButton->show();
   }
   else
   {
      if (!m_runPushButton->isVisible())
         m_runPushButton->show();
      if (m_stopPushButton->isVisible())
         m_stopPushButton->hide();
   }

   setCurrentCoreLabel();
}

void MainWindow::onStopClicked()
{
#ifdef HAVE_MENU
   menu_navigation_set_selection(0);
#endif
   command_event(CMD_EVENT_UNLOAD_CORE, NULL);
   setCurrentCoreLabel();
   activateWindow();
   raise();
}

void MainWindow::setCurrentCoreLabel()
{
   bool update                      = false;
   struct retro_system_info *system = runloop_get_libretro_system_info();
   QString libraryName              = system->library_name;
   const char *no_core_str          = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE);

   if (     (m_statusLabel->text().isEmpty())
         || (m_currentCore != no_core_str && libraryName.isEmpty())
      )
   {
      m_currentCore           = no_core_str;
      m_currentCoreVersion    = "";
      update                  = true;
   }
   else
   {
      if (m_currentCore != libraryName && !libraryName.isEmpty())
      {
         m_currentCore        = system->library_name;
         m_currentCoreVersion = (string_is_empty(system->library_version) ? "" : system->library_version);
         update = true;
      }
   }

   if (update)
   {
      QAction *unloadCoreAction = findChild<QAction*>("unloadCoreAction");
      QString text              = QString(PACKAGE_VERSION) + " - " + m_currentCore + " " + m_currentCoreVersion;
      m_statusLabel->setText(text);
      m_loadCoreWindow->setStatusLabel(text);
      setCoreActions();

      if (unloadCoreAction)
      {
         if (libraryName.isEmpty())
            unloadCoreAction->setEnabled(false);
         else
            unloadCoreAction->setEnabled(true);
      }
   }
}

void MainWindow::onCoreLoadWindowClosed()
{
   QVariant lastLaunchWithVariant = m_loadCoreWindow->property("last_launch_with_index");
   int lastLaunchWithIndex        = lastLaunchWithVariant.toInt();

   m_pendingRun                   = false;

   if (lastLaunchWithVariant.isValid() && lastLaunchWithIndex >= 0)
   {
      m_launchWithComboBox->setCurrentIndex(lastLaunchWithIndex);
      m_loadCoreWindow->setProperty("last_launch_with_index", -1);
   }
}

void MainWindow::onCoreLoaded()
{
   QAction *unloadAction = findChild<QAction*>("unloadCoreAction");

   activateWindow();
   raise();
   setCurrentCoreLabel();
   setCoreActions();

   if (unloadAction)
      unloadAction->setEnabled(true);

   m_loadCoreWindow->hide();

   if (m_pendingRun)
   {
      onRunClicked();
      m_pendingRun = false;
   }
}

void MainWindow::onUnloadCoreMenuAction()
{
   QAction *action = qobject_cast<QAction*>(sender());

#ifdef HAVE_MENU
   menu_navigation_set_selection(0);
#endif

   /* TODO */
   if (!command_event(CMD_EVENT_UNLOAD_CORE, NULL))
      return;

   setCurrentCoreLabel();
   setCoreActions();

   if (!action)
      return;

   action->setEnabled(false);
   activateWindow();
   raise();
}

void MainWindow::onLoadCoreClicked(const QStringList &extensionFilters)
{
   m_loadCoreWindow->show();
   m_loadCoreWindow->resize(width() / 2, height());
   m_loadCoreWindow->setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, m_loadCoreWindow->size(), geometry()));
   m_loadCoreWindow->initCoreList(extensionFilters);
}

void MainWindow::initContentTableWidget()
{
   QString path;
   QListWidgetItem *item = m_listWidget->currentItem();

   if (!item)
      return;

   m_currentGridHash.clear();

   if (m_currentGridWidget)
   {
      m_currentGridWidget->setObjectName("thumbnailWidget");
      m_currentGridWidget->style()->unpolish(m_currentGridWidget);
      m_currentGridWidget->style()->polish(m_currentGridWidget);
   }

   m_currentGridWidget = NULL;

   path = item->data(Qt::UserRole).toString();

   if (path == ALL_PLAYLISTS_TOKEN)
   {
      unsigned i;
      settings_t *settings = config_get_ptr();
      QDir playlistDir(settings->paths.directory_playlist);
      QStringList playlists;

      for (i = 0; i < m_playlistFiles.count(); i++)
      {
         const QString &playlist = m_playlistFiles.at(i);
         playlists.append(playlistDir.absoluteFilePath(playlist));
      }

      m_playlistModel->addPlaylistItems(playlists, true);
   }
   else
      m_playlistModel->addPlaylistItems(QStringList() << path);

   if (item != m_historyPlaylistsItem)
      m_tableView->sortByColumn(0, Qt::AscendingOrder);
   else
      m_proxyModel->sort(-1);

   updateItemsCount();

   m_gridView->scrollToTop();
   m_gridView->setCurrentIndex(m_proxyModel->index(0, 0));
}

void MainWindow::updateItemsCount()
{
   m_itemsCountLabel->setText(
         m_itemsCountLiteral.arg(m_proxyModel->rowCount()));
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
   QMainWindow::keyPressEvent(event);
}

QSettings* MainWindow::settings()
{
   return m_settings;
}

QString MainWindow::getCurrentViewTypeString()
{
   switch (m_viewType)
   {
      case VIEW_TYPE_ICONS:
         return QStringLiteral("icons");
      case VIEW_TYPE_LIST:
      default:
         break;
   }

   return QStringLiteral("list");
}

QString MainWindow::getCurrentThumbnailTypeString()
{
   switch (m_thumbnailType)
   {
      case THUMBNAIL_TYPE_SCREENSHOT:
         return QStringLiteral("screenshot");
      case THUMBNAIL_TYPE_TITLE_SCREEN:
         return QStringLiteral("title");
      case THUMBNAIL_TYPE_BOXART:
      default:
         return QStringLiteral("boxart");
   }

   return QStringLiteral("list");
}

ThumbnailType MainWindow::getThumbnailTypeFromString(QString thumbnailType)
{
   if (thumbnailType == "boxart")
      return THUMBNAIL_TYPE_BOXART;
   else if (thumbnailType == "screenshot")
      return THUMBNAIL_TYPE_SCREENSHOT;
   else if (thumbnailType == "title")
      return THUMBNAIL_TYPE_TITLE_SCREEN;

   return THUMBNAIL_TYPE_BOXART;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
   if (m_settings->value("save_geometry", false).toBool())
      m_settings->setValue("geometry", saveGeometry());
   if (m_settings->value("save_dock_positions", false).toBool())
      m_settings->setValue("dock_positions", saveState());
   if (m_settings->value("save_last_tab", false).toBool())
      m_settings->setValue("last_tab", m_browserAndPlaylistTabWidget->currentIndex());

   m_settings->setValue("view_type", getCurrentViewTypeString());
   m_settings->setValue("file_browser_table_headers", m_fileTableView->horizontalHeader()->saveState());
   m_settings->setValue("icon_view_zoom", m_lastZoomSliderValue);
   m_settings->setValue("icon_view_thumbnail_type", getCurrentThumbnailTypeString());
   m_settings->setValue("options_dialog_geometry", m_viewOptionsDialog->saveGeometry());

   QMainWindow::closeEvent(event);
}

void MainWindow::onContributorsClicked()
{
   QScopedPointer<QDialog> dialog(new QDialog());
   QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
   QTextEdit *textEdit = new QTextEdit(dialog.data());

   connect(buttonBox, SIGNAL(accepted()), dialog.data(), SLOT(accept()));
   connect(buttonBox, SIGNAL(rejected()), dialog.data(), SLOT(reject()));

   dialog->setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS));
   dialog->setLayout(new QVBoxLayout());

   dialog->layout()->addWidget(textEdit);

   dialog->layout()->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Minimum));
   dialog->layout()->addWidget(buttonBox);

   textEdit->setReadOnly(true);
   textEdit->setHtml(QString("<pre>") + retroarch_contributors_list + "</pre>");

   dialog->resize(480, 640);
   dialog->exec();
}

void MainWindow::showAbout()
{
   QScopedPointer<QDialog> dialog(new QDialog());
   QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
   QString text = QString("RetroArch ") + PACKAGE_VERSION +
         "<br><br>" + "<a href=\"https://www.libretro.com/\">www.libretro.com</a>"
         "<br><br>" + "<a href=\"https://www.retroarch.com/\">www.retroarch.com</a>"
#ifdef HAVE_GIT_VERSION
         "<br><br>" + msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION) + ": " + retroarch_git_version +
#endif
         "<br>" + msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE) + ": " + __DATE__;
   QLabel *label = new QLabel(text, dialog.data());
   QPixmap pix = getInvader();
   QLabel *pixLabel = new QLabel(dialog.data());
   QPushButton *contributorsPushButton = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS), dialog.data());

   connect(contributorsPushButton, SIGNAL(clicked()), this, SLOT(onContributorsClicked()));
   connect(buttonBox, SIGNAL(accepted()), dialog.data(), SLOT(accept()));
   connect(buttonBox, SIGNAL(rejected()), dialog.data(), SLOT(reject()));

   label->setTextFormat(Qt::RichText);
   label->setAlignment(Qt::AlignCenter);
   label->setTextInteractionFlags(Qt::TextBrowserInteraction);
   label->setOpenExternalLinks(true);

   pixLabel->setAlignment(Qt::AlignCenter);
   pixLabel->setPixmap(pix);

   dialog->setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT));
   dialog->setLayout(new QVBoxLayout());

   dialog->layout()->addWidget(pixLabel);
   dialog->layout()->addWidget(label);
   dialog->layout()->addWidget(contributorsPushButton);

   dialog->layout()->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));
   dialog->layout()->addWidget(buttonBox);

   dialog->exec();
}

void MainWindow::showDocs()
{
   QDesktopServices::openUrl(QUrl(DOCS_URL));
}

void MainWindow::onShowErrorMessage(QString msg)
{
   showMessageBox(msg, MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
}

void MainWindow::onShowInfoMessage(QString msg)
{
   showMessageBox(msg, MainWindow::MSGBOX_TYPE_INFO, Qt::ApplicationModal, false);
}

int MainWindow::onExtractArchive(QString path, QString extractionDir, QString tempExtension, retro_task_callback_t cb)
{
   unsigned i;
   file_archive_transfer_t state;
   struct archive_extract_userdata userdata;
   QByteArray pathArray          = path.toUtf8();
   QByteArray dirArray           = extractionDir.toUtf8();
   const char *file              = pathArray.constData();
   const char *dir               = dirArray.constData();
   struct string_list *file_list = file_archive_get_file_list(file, NULL);
   bool returnerr                = true;
   retro_task_t *decompress_task = NULL;

   if (!file_list || file_list->size == 0)
   {
      showMessageBox("Error: Archive is empty.", MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
      RARCH_ERR("[Qt]: Downloaded archive is empty?\n");
      return -1;
   }

   for (i = 0; i < file_list->size; i++)
   {
      QFile fileObj(file_list->elems[i].data);

      if (fileObj.exists())
      {
         if (!fileObj.remove())
         {
            /* if we cannot delete the existing file to update it, rename it for now and delete later */
            QFile fileTemp(fileObj.fileName() + tempExtension);

            if (fileTemp.exists())
            {
               if (!fileTemp.remove())
               {
                  showMessageBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE), MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
                  RARCH_ERR("[Qt]: Could not delete file: %s\n", file_list->elems[i].data);
                  return -1;
               }
            }

            if (!fileObj.rename(fileTemp.fileName()))
            {
               showMessageBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE), MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
               RARCH_ERR("[Qt]: Could not rename file: %s\n", file_list->elems[i].data);
               return -1;
            }
         }
      }
   }

   string_list_free(file_list);

   memset(&state,    0, sizeof(state));
   memset(&userdata, 0, sizeof(userdata));

   state.type = ARCHIVE_TRANSFER_INIT;

   m_updateProgressDialog->setWindowModality(Qt::NonModal);
   m_updateProgressDialog->setMinimumDuration(0);
   m_updateProgressDialog->setRange(0, 0);
   m_updateProgressDialog->setAutoClose(true);
   m_updateProgressDialog->setAutoReset(true);
   m_updateProgressDialog->setValue(0);
   m_updateProgressDialog->setLabelText(QString(msg_hash_to_str(MSG_EXTRACTING)) + "...");
   m_updateProgressDialog->setCancelButtonText(QString());
   m_updateProgressDialog->show();

   decompress_task = (retro_task_t*)task_push_decompress(
         file, dir,
         NULL, NULL, NULL,
         cb, this, NULL, false);

   if (!decompress_task)
   {
      m_updateProgressDialog->cancel();
      return -1;
   }

   return returnerr;
}

QString MainWindow::getScrubbedString(QString str)
{
   const QString chars("&*/:`\"<>?\\|");
   int i;

   for (i = 0; i < chars.count(); i++)
      str.replace(chars.at(i), '_');

   return str;
}

static void* ui_window_qt_init(void)
{
   ui_window.qtWindow = new MainWindow();

   return &ui_window;
}

static void ui_window_qt_destroy(void *data)
{
#if 0
   ui_window_qt_t *window = (ui_window_qt_t*)data;

   delete window->qtWindow;
#endif
}

static void ui_window_qt_set_focused(void *data)
{
#if 0
   ui_window_qt_t *window = (ui_window_qt_t*)data;

   window->qtWindow->raise();
   window->qtWindow->activateWindow();
#endif
}

static void ui_window_qt_set_visible(void *data,
        bool set_visible)
{
   (void)data;
   (void)set_visible;
   /* TODO/FIXME */
}

static void ui_window_qt_set_title(void *data, char *buf)
{
#if 0
   ui_window_qt_t *window = (ui_window_qt_t*)data;

   window->qtWindow->setWindowTitle(QString::fromUtf8(buf));
#endif
}

static void ui_window_qt_set_droppable(void *data, bool droppable)
{
#if 0
   ui_window_qt_t *window = (ui_window_qt_t*)data;

   window->qtWindow->setAcceptDrops(droppable);
#endif
}

static bool ui_window_qt_focused(void *data)
{
#if 0
   ui_window_qt_t *window = (ui_window_qt_t*)data;
   return window->qtWindow->isActiveWindow() && !window->qtWindow->isMinimized();
#else
   return true;
#endif
}

static ui_window_t ui_window_qt = {
   ui_window_qt_init,
   ui_window_qt_destroy,
   ui_window_qt_set_focused,
   ui_window_qt_set_visible,
   ui_window_qt_set_title,
   ui_window_qt_set_droppable,
   ui_window_qt_focused,
   "qt"
};

static enum ui_msg_window_response ui_msg_window_qt_response(ui_msg_window_state *state, QMessageBox::StandardButtons response)
{
	switch (response)
   {
      case QMessageBox::Ok:
         return UI_MSG_RESPONSE_OK;
      case QMessageBox::Cancel:
         return UI_MSG_RESPONSE_CANCEL;
      case QMessageBox::Yes:
         return UI_MSG_RESPONSE_YES;
      case QMessageBox::No:
         return UI_MSG_RESPONSE_NO;
      default:
         break;
   }

	switch (state->buttons)
	{
	   case UI_MSG_WINDOW_OK:
		   return UI_MSG_RESPONSE_OK;
	   case UI_MSG_WINDOW_OKCANCEL:
		   return UI_MSG_RESPONSE_CANCEL;
	   case UI_MSG_WINDOW_YESNO:
		   return UI_MSG_RESPONSE_NO;
	   case UI_MSG_WINDOW_YESNOCANCEL:
		   return UI_MSG_RESPONSE_CANCEL;
	   default:
		   break;
	}

	return UI_MSG_RESPONSE_NA;
}

static QFlags<QMessageBox::StandardButton> 
ui_msg_window_qt_buttons(ui_msg_window_state *state)
{
	switch (state->buttons)
	{
	   case UI_MSG_WINDOW_OK:
		   return QMessageBox::Ok;
	   case UI_MSG_WINDOW_OKCANCEL:
		   return QMessageBox::Cancel;
	   case UI_MSG_WINDOW_YESNO:
		   return (QMessageBox::Yes | QMessageBox::No);
	   case UI_MSG_WINDOW_YESNOCANCEL:
		   return (QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
	}

	return QMessageBox::NoButton;
}

static enum ui_msg_window_response 
ui_msg_window_qt_error(ui_msg_window_state *state)
{
   QFlags<QMessageBox::StandardButton> flags = ui_msg_window_qt_buttons(state);
   return ui_msg_window_qt_response(state, QMessageBox::critical((QWidget*)state->window, state->title, state->text, flags));
}

static enum ui_msg_window_response ui_msg_window_qt_information(ui_msg_window_state *state)
{
   QFlags<QMessageBox::StandardButton> flags = ui_msg_window_qt_buttons(state);
   return ui_msg_window_qt_response(state, QMessageBox::information((QWidget*)state->window, state->title, state->text, flags));
}

static enum ui_msg_window_response ui_msg_window_qt_question(ui_msg_window_state *state)
{
   QFlags<QMessageBox::StandardButton> flags = ui_msg_window_qt_buttons(state);
   return ui_msg_window_qt_response(state, QMessageBox::question((QWidget*)state->window, state->title, state->text, flags));
}

static enum ui_msg_window_response ui_msg_window_qt_warning(ui_msg_window_state *state)
{
   QFlags<QMessageBox::StandardButton> flags = ui_msg_window_qt_buttons(state);
   return ui_msg_window_qt_response(state, QMessageBox::warning((QWidget*)state->window, state->title, state->text, flags));
}

static ui_msg_window_t ui_msg_window_qt = {
   ui_msg_window_qt_error,
   ui_msg_window_qt_information,
   ui_msg_window_qt_question,
   ui_msg_window_qt_warning,
   "qt"
};

static bool ui_browser_window_qt_open(ui_browser_window_state_t *state)
{
   return true;
}

static bool ui_browser_window_qt_save(ui_browser_window_state_t *state)
{
   return false;
}

static ui_browser_window_t ui_browser_window_qt = {
   ui_browser_window_qt_open,
   ui_browser_window_qt_save,
   "qt"
};

static void* ui_application_qt_initialize(void)
{
   /* These must last for the lifetime of the QApplication */
   static int app_argc     = 1;
   static char app_name[]  = "retroarch";
   static char *app_argv[] = { app_name, NULL };

   app_handler             = new AppHandler();

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
   /* HiDpi supported since Qt 5.6 */
   QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

   /* Create QApplication() before calling QApplication::setStyle()
    * to ensure that plugin path is determined correctly */
   ui_application.app = new QApplication(app_argc, app_argv);
   QApplication::setStyle("fusion");
   ui_application.app->setOrganizationName("libretro");
   ui_application.app->setApplicationName("RetroArch");
   ui_application.app->setApplicationVersion(PACKAGE_VERSION);
   ui_application.app->connect(ui_application.app, SIGNAL(lastWindowClosed()),
         app_handler, SLOT(onLastWindowClosed()));

#ifdef Q_OS_UNIX
   setlocale(LC_NUMERIC, "C");
#endif
   {
      /* Can't declare the pixmap at the top, because: "QPixmap: Must construct a QGuiApplication before a QPixmap" */
      QImage iconImage(16, 16, QImage::Format_ARGB32);
      QPixmap iconPixmap;
      unsigned char *bits = iconImage.bits();

      memcpy(bits, retroarch_qt_icon_data, 16 * 16 * sizeof(unsigned));

      iconPixmap = QPixmap::fromImage(iconImage);

      ui_application.app->setWindowIcon(QIcon(iconPixmap));
   }

   return &ui_application;
}

static void ui_application_qt_process_events(void)
{
   QAbstractEventDispatcher *dispatcher = QApplication::eventDispatcher();
   if (dispatcher && dispatcher->hasPendingEvents())
      QApplication::processEvents();
}

static void ui_application_qt_quit(void)
{
   if (app_handler)
      app_handler->exit();
}

#ifdef HAVE_MAIN
#if defined(__cplusplus) && !defined(CXX_BUILD)
extern "C"
#endif
int main(int argc, char *argv[])
{
   return rarch_main(argc, argv, NULL);
}
#endif

static ui_application_t ui_application_qt = {
   ui_application_qt_initialize,
   ui_application_qt_process_events,
   ui_application_qt_quit,
   false,
   "qt"
};


AppHandler::AppHandler(QObject *parent) :
   QObject(parent)
{
}

AppHandler::~AppHandler()
{
}

void AppHandler::exit()
{
   ui_application_qt.exiting = true;

   if (qApp)
      qApp->closeAllWindows();
}

bool AppHandler::isExiting() const
{
   return ui_application_qt.exiting;
}

void AppHandler::onLastWindowClosed()
{
}

typedef struct ui_companion_qt
{
   ui_application_qt_t *app;
   ui_window_qt_t *window;
} ui_companion_qt_t;

ThumbnailWidget::ThumbnailWidget(ThumbnailType type, QWidget *parent) :
   QStackedWidget(parent)
   ,m_thumbnailType(type)
   ,m_thumbnailLabel(new ThumbnailLabel(this))
   ,m_dropIndicator(new QLabel(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_DROP_IMAGE_HERE), this))
{
   m_dropIndicator->setObjectName("dropIndicator");
   m_dropIndicator->setAlignment(Qt::AlignCenter);
   addWidget(m_dropIndicator);
   addWidget(m_thumbnailLabel);
}

void ThumbnailWidget::setPixmap(const QPixmap &pixmap, bool acceptDrops)
{
   m_thumbnailLabel->setPixmap(pixmap);

   if (acceptDrops && pixmap.isNull())
      setCurrentWidget(m_dropIndicator);
   else
      setCurrentWidget(m_thumbnailLabel);

   m_thumbnailLabel->update();

   QWidget::setAcceptDrops(acceptDrops);
}

void ThumbnailWidget::dragEnterEvent(QDragEnterEvent *event)
{
   const QMimeData *data = event->mimeData();

   if (data->hasUrls())
      event->acceptProposedAction();
}

/* Workaround for QTBUG-72844. Without it, you can't 
 * drop on this if you first drag over another 
 * widget that doesn't accept drops. */
void ThumbnailWidget::dragMoveEvent(QDragMoveEvent *event)
{
   event->acceptProposedAction();
}

void ThumbnailWidget::dropEvent(QDropEvent *event)
{
   const QMimeData *data = event->mimeData();

   if (data->hasUrls())
   {
      const QString imageString = data->urls().at(0).toLocalFile();
      const QImage image(imageString);

      if (!image.isNull())
         emit(filesDropped(image, m_thumbnailType));
      else
      {
         const char *string_data = QDir::toNativeSeparators(imageString).toUtf8().constData();
         RARCH_ERR("[Qt]: Could not read image: %s\n", string_data);
      }
   }
}

ThumbnailLabel::ThumbnailLabel(QWidget *parent) :
   QWidget(parent)
   ,m_pixmap(NULL)
   ,m_pixmapWidth(0)
   ,m_pixmapHeight(0)
{
}

ThumbnailLabel::~ThumbnailLabel()
{
   if (m_pixmap)
      delete m_pixmap;
}

void ThumbnailLabel::setPixmap(const QPixmap &pixmap)
{
   m_pixmapWidth = pixmap.width();
   m_pixmapHeight = pixmap.height();

   if (m_pixmap)
      delete m_pixmap;

   m_pixmap = new QPixmap(pixmap);
}

QSize ThumbnailLabel::sizeHint() const
{
   return QSize(256, 256);
}

void ThumbnailLabel::paintEvent(QPaintEvent *event)
{
   QStyleOption o;
   QPainter p;
   int w = width();
   int h = height();

   event->accept();

   o.initFrom(this);
   p.begin(this);
   style()->drawPrimitive(
     QStyle::PE_Widget, &o, &p, this);
   p.end();

   if (!m_pixmap || m_pixmap->isNull())
   {
      if (m_pixmap)
         delete m_pixmap;
      m_pixmap = new QPixmap(sizeHint());
      m_pixmap->fill(QColor(0, 0, 0, 0));
   }

   if (w > 0 && h > 0 && m_pixmap && !m_pixmap->isNull())
   {
      int newHeight = (m_pixmap->height() / static_cast<float>(m_pixmap->width())) * width();
      QPixmap pixmapScaled = *m_pixmap;
      QPixmap pixmap;
      QPainter pScale;
      int pw = 0;
      int ph = 0;
      unsigned *buf = new unsigned[w * h];

      if (newHeight > h)
         pixmapScaled = pixmapScaled.scaledToHeight(h, Qt::SmoothTransformation);
      else
         pixmapScaled = pixmapScaled.scaledToWidth(w, Qt::SmoothTransformation);

      pw = pixmapScaled.width();
      ph = pixmapScaled.height();

      pixmap = QPixmap(w, h);
      pixmap.fill(QColor(0, 0, 0, 0));

      pScale.begin(&pixmap);
      pScale.drawPixmap(QRect((w - pw) / 2, (h - ph) / 2, pw, ph), pixmapScaled, pixmapScaled.rect());
      pScale.end();

      if (!pixmap.isNull())
      {
         p.begin(this);
         p.drawPixmap(rect(), pixmap, pixmap.rect());
         p.end();
      }

      delete []buf;
   }
   else
      QWidget::paintEvent(event);
}

void ThumbnailLabel::resizeEvent(QResizeEvent *event)
{
   QWidget::resizeEvent(event);
}

static void ui_companion_qt_deinit(void *data)
{
   ui_companion_qt_t *handle = (ui_companion_qt_t*)data;

   if (!handle)
      return;

   /* why won't deleteLater() here call the destructor? */
   delete handle->window->qtWindow;

   free(handle);
}

static void* ui_companion_qt_init(void)
{
   int i = 0;
   QString initialPlaylist;
   QRect desktopRect;
   ui_companion_qt_t               *handle = (ui_companion_qt_t*)
      calloc(1, sizeof(*handle));
   MainWindow                  *mainwindow = NULL;
   QHBoxLayout   *browserButtonsHBoxLayout = NULL;
   QVBoxLayout                     *layout = NULL;
   QVBoxLayout     *launchWithWidgetLayout = NULL;
   QHBoxLayout         *coreComboBoxLayout = NULL;
   QMenuBar                          *menu = NULL;
   QDesktopWidget                 *desktop = NULL;
   QMenu                         *fileMenu = NULL;
   QMenu                         *editMenu = NULL;
   QMenu                         *viewMenu = NULL;
   QMenu              *viewClosedDocksMenu = NULL;
   QMenu                         *helpMenu = NULL;
   QDockWidget              *thumbnailDock = NULL;
   QDockWidget             *thumbnail2Dock = NULL;
   QDockWidget             *thumbnail3Dock = NULL;
   QDockWidget  *browserAndPlaylistTabDock = NULL;
   QDockWidget          *coreSelectionDock = NULL;
   QTabWidget *browserAndPlaylistTabWidget = NULL;
   QStackedWidget           *centralWidget = NULL;
   QStackedWidget                  *widget = NULL;
   QFrame                   *browserWidget = NULL;
   QFrame                  *playlistWidget = NULL;
   QWidget            *coreSelectionWidget = NULL;
   QWidget               *launchWithWidget = NULL;
   ThumbnailWidget        *thumbnailWidget = NULL;
   ThumbnailWidget       *thumbnail2Widget = NULL;
   ThumbnailWidget       *thumbnail3Widget = NULL;
   QPushButton     *browserDownloadsButton = NULL;
   QPushButton            *browserUpButton = NULL;
   QPushButton         *browserStartButton = NULL;
   ThumbnailLabel               *thumbnail = NULL;
   ThumbnailLabel              *thumbnail2 = NULL;
   ThumbnailLabel              *thumbnail3 = NULL;
   QAction               *editSearchAction = NULL;
   QAction                 *loadCoreAction = NULL;
   QAction               *unloadCoreAction = NULL;
   QAction                     *exitAction = NULL;
   QComboBox           *launchWithComboBox = NULL;
   QSettings                    *qsettings = NULL;
   QListWidget                 *listWidget = NULL;
   bool                      foundPlaylist = false;

   if (!handle)
      return NULL;

   handle->app     = static_cast<ui_application_qt_t*>
      (ui_application_qt.initialize());
   handle->window  = static_cast<ui_window_qt_t*>(ui_window_qt.init());

   desktop         = qApp->desktop();
   desktopRect     = desktop->availableGeometry();

   mainwindow      = handle->window->qtWindow;

   qsettings       = mainwindow->settings();

   initialPlaylist = qsettings->value("initial_playlist", mainwindow->getSpecialPlaylistPath(SPECIAL_PLAYLIST_HISTORY)).toString();

   mainwindow->resize(qMin(desktopRect.width(), INITIAL_WIDTH), qMin(desktopRect.height(), INITIAL_HEIGHT));
   mainwindow->setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, mainwindow->size(), desktopRect));

   mainwindow->setWindowTitle("RetroArch");
   mainwindow->setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks | GROUPED_DRAGGING);

   listWidget      = mainwindow->playlistListWidget();

   widget          = mainwindow->playlistViews();
   widget->setContextMenuPolicy(Qt::CustomContextMenu);

   QObject::connect(widget, SIGNAL(filesDropped(QStringList)), mainwindow, SLOT(onPlaylistFilesDropped(QStringList)));
   QObject::connect(widget, SIGNAL(enterPressed()), mainwindow, SLOT(onDropWidgetEnterPressed()));
   QObject::connect(widget, SIGNAL(deletePressed()), mainwindow, SLOT(deleteCurrentPlaylistItem()));
   QObject::connect(widget, SIGNAL(customContextMenuRequested(const QPoint&)), mainwindow, SLOT(onFileDropWidgetContextMenuRequested(const QPoint&)));

   centralWidget = mainwindow->centralWidget();

   centralWidget->addWidget(mainwindow->playlistViewsAndFooter());
   centralWidget->addWidget(mainwindow->fileTableView());

   mainwindow->setCentralWidget(centralWidget);

   menu = mainwindow->menuBar();

   fileMenu = menu->addMenu(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_FILE));

   loadCoreAction = fileMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE), mainwindow, SLOT(onLoadCoreClicked()));
   loadCoreAction->setShortcut(QKeySequence("Ctrl+L"));

   unloadCoreAction = fileMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE), mainwindow, SLOT(onUnloadCoreMenuAction()));
   unloadCoreAction->setObjectName("unloadCoreAction");
   unloadCoreAction->setEnabled(false);
   unloadCoreAction->setShortcut(QKeySequence("Ctrl+U"));

   exitAction = fileMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_EXIT), mainwindow, SLOT(close()));
   exitAction->setShortcut(QKeySequence::Quit);

   editMenu = menu->addMenu(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT));
   editSearchAction = editMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH), mainwindow->searchLineEdit(), SLOT(setFocus()));
   editSearchAction->setShortcut(QKeySequence::Find);

   viewMenu = menu->addMenu(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW));
   viewClosedDocksMenu = viewMenu->addMenu(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_CLOSED_DOCKS));
   viewClosedDocksMenu->setObjectName("viewClosedDocksMenu");

   QObject::connect(viewClosedDocksMenu, SIGNAL(aboutToShow()), mainwindow, SLOT(onViewClosedDocksAboutToShow()));

   viewMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS), mainwindow, SLOT(onCoreOptionsClicked()));
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   viewMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS), mainwindow, SLOT(onShaderParamsClicked()));
#endif

   viewMenu->addSeparator();
   viewMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS), mainwindow, SLOT(onIconViewClicked()));
   viewMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST), mainwindow, SLOT(onListViewClicked()));
   viewMenu->addSeparator();
   viewMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS), mainwindow->viewOptionsDialog(), SLOT(showDialog()));

   helpMenu = menu->addMenu(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_HELP));
   helpMenu->addAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_DOCUMENTATION)), mainwindow, SLOT(showDocs()));
   helpMenu->addAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT)) + "...", mainwindow, SLOT(showAbout()));
   helpMenu->addAction("About Qt...", qApp, SLOT(aboutQt()));

   playlistWidget = new QFrame();
   playlistWidget->setLayout(new QVBoxLayout());
   playlistWidget->setObjectName("playlistWidget");
   playlistWidget->layout()->setContentsMargins(0, 0, 0, 0);

   playlistWidget->layout()->addWidget(mainwindow->playlistListWidget());

   browserWidget = new QFrame();
   browserWidget->setLayout(new QVBoxLayout());
   browserWidget->setObjectName("browserWidget");
   browserWidget->layout()->setContentsMargins(0, 0, 0, 0);

   browserDownloadsButton = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY));
   browserUpButton = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_UP));
   browserStartButton = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES));

   QObject::connect(browserDownloadsButton, SIGNAL(clicked()), mainwindow, SLOT(onBrowserDownloadsClicked()));
   QObject::connect(browserUpButton, SIGNAL(clicked()), mainwindow, SLOT(onBrowserUpClicked()));
   QObject::connect(browserStartButton, SIGNAL(clicked()), mainwindow, SLOT(onBrowserStartClicked()));

   browserButtonsHBoxLayout = new QHBoxLayout();
   browserButtonsHBoxLayout->addWidget(browserUpButton);
   browserButtonsHBoxLayout->addWidget(browserStartButton);
   browserButtonsHBoxLayout->addWidget(browserDownloadsButton);

   qobject_cast<QVBoxLayout*>(browserWidget->layout())->addLayout(browserButtonsHBoxLayout);
   browserWidget->layout()->addWidget(mainwindow->dirTreeView());

   browserAndPlaylistTabWidget = mainwindow->browserAndPlaylistTabWidget();
   browserAndPlaylistTabWidget->setObjectName("browserAndPlaylistTabWidget");

   /* Several functions depend on the same tab title strings here, so if you change these, make sure to change those too
    * setCoreActions()
    * onTabWidgetIndexChanged()
    * onCurrentListItemChanged()
    */
   browserAndPlaylistTabWidget->addTab(playlistWidget, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS));
   browserAndPlaylistTabWidget->addTab(browserWidget, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER));

   browserAndPlaylistTabDock = new QDockWidget(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER), mainwindow);
   browserAndPlaylistTabDock->setObjectName("browserAndPlaylistTabDock");
   browserAndPlaylistTabDock->setProperty("default_area", Qt::LeftDockWidgetArea);
   browserAndPlaylistTabDock->setProperty("menu_text", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER));
   browserAndPlaylistTabDock->setWidget(browserAndPlaylistTabWidget);

   mainwindow->addDockWidget(static_cast<Qt::DockWidgetArea>(browserAndPlaylistTabDock->property("default_area").toInt()), browserAndPlaylistTabDock);

   browserButtonsHBoxLayout->addItem(new QSpacerItem(browserAndPlaylistTabWidget->tabBar()->width(), 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

   thumbnailWidget = new ThumbnailWidget(THUMBNAIL_TYPE_BOXART);
   thumbnailWidget->setObjectName("thumbnail");

   thumbnail2Widget = new ThumbnailWidget(THUMBNAIL_TYPE_TITLE_SCREEN);
   thumbnail2Widget->setObjectName("thumbnail2");

   thumbnail3Widget = new ThumbnailWidget(THUMBNAIL_TYPE_SCREENSHOT);
   thumbnail3Widget->setObjectName("thumbnail3");

   QObject::connect(thumbnailWidget, SIGNAL(filesDropped(const QImage&, ThumbnailType)), mainwindow, SLOT(onThumbnailDropped(const QImage&, ThumbnailType)));
   QObject::connect(thumbnail2Widget, SIGNAL(filesDropped(const QImage&, ThumbnailType)), mainwindow, SLOT(onThumbnailDropped(const QImage&, ThumbnailType)));
   QObject::connect(thumbnail3Widget, SIGNAL(filesDropped(const QImage&, ThumbnailType)), mainwindow, SLOT(onThumbnailDropped(const QImage&, ThumbnailType)));

   thumbnailDock = new QDockWidget(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART), mainwindow);
   thumbnailDock->setObjectName("thumbnailDock");
   thumbnailDock->setProperty("default_area", Qt::RightDockWidgetArea);
   thumbnailDock->setProperty("menu_text", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART));
   thumbnailDock->setWidget(thumbnailWidget);

   mainwindow->addDockWidget(static_cast<Qt::DockWidgetArea>(thumbnailDock->property("default_area").toInt()), thumbnailDock);

   thumbnail2Dock = new QDockWidget(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN), mainwindow);
   thumbnail2Dock->setObjectName("thumbnail2Dock");
   thumbnail2Dock->setProperty("default_area", Qt::RightDockWidgetArea);
   thumbnail2Dock->setProperty("menu_text", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN));
   thumbnail2Dock->setWidget(thumbnail2Widget);

   mainwindow->addDockWidget(static_cast<Qt::DockWidgetArea>(thumbnail2Dock->property("default_area").toInt()), thumbnail2Dock);

   thumbnail3Dock = new QDockWidget(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT), mainwindow);
   thumbnail3Dock->setObjectName("thumbnail3Dock");
   thumbnail3Dock->setProperty("default_area", Qt::RightDockWidgetArea);
   thumbnail3Dock->setProperty("menu_text", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT));
   thumbnail3Dock->setWidget(thumbnail3Widget);

   mainwindow->addDockWidget(static_cast<Qt::DockWidgetArea>(thumbnail3Dock->property("default_area").toInt()), thumbnail3Dock);

   mainwindow->tabifyDockWidget(thumbnailDock, thumbnail2Dock);
   mainwindow->tabifyDockWidget(thumbnailDock, thumbnail3Dock);

   /* when tabifying the dock widgets, the last tab added is selected by default, so we need to re-select the first tab */
   thumbnailDock->raise();

   coreSelectionWidget = new QWidget();
   coreSelectionWidget->setLayout(new QVBoxLayout());

   launchWithComboBox = mainwindow->launchWithComboBox();

   launchWithWidgetLayout = new QVBoxLayout();

   launchWithWidget = new QWidget();
   launchWithWidget->setLayout(launchWithWidgetLayout);

   coreComboBoxLayout = new QHBoxLayout();

   mainwindow->runPushButton()->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding));
   mainwindow->stopPushButton()->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding));
   mainwindow->startCorePushButton()->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding));

   coreComboBoxLayout->addWidget(launchWithComboBox);
   coreComboBoxLayout->addWidget(mainwindow->startCorePushButton());
   coreComboBoxLayout->addWidget(mainwindow->coreInfoPushButton());
   coreComboBoxLayout->addWidget(mainwindow->runPushButton());
   coreComboBoxLayout->addWidget(mainwindow->stopPushButton());

   mainwindow->stopPushButton()->hide();

   coreComboBoxLayout->setStretchFactor(launchWithComboBox, 1);

   launchWithWidgetLayout->addLayout(coreComboBoxLayout);

   coreSelectionWidget->layout()->addWidget(launchWithWidget);

   coreSelectionWidget->layout()->addItem(new QSpacerItem(20, browserAndPlaylistTabWidget->height(), QSizePolicy::Minimum, QSizePolicy::Expanding));

   coreSelectionDock = new QDockWidget(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE), mainwindow);
   coreSelectionDock->setObjectName("coreSelectionDock");
   coreSelectionDock->setProperty("default_area", Qt::LeftDockWidgetArea);
   coreSelectionDock->setProperty("menu_text", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE));
   coreSelectionDock->setWidget(coreSelectionWidget);
   coreSelectionDock->setFixedHeight(coreSelectionDock->minimumSizeHint().height());

   mainwindow->addDockWidget(static_cast<Qt::DockWidgetArea>(coreSelectionDock->property("default_area").toInt()), coreSelectionDock);

   mainwindow->splitDockWidget(browserAndPlaylistTabDock, coreSelectionDock, Qt::Vertical);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
   mainwindow->resizeDocks(QList<QDockWidget*>() << coreSelectionDock, QList<int>() << 1, Qt::Vertical);
#endif

   if (qsettings->contains("all_playlists_list_max_count"))
      mainwindow->setAllPlaylistsListMaxCount(qsettings->value("all_playlists_list_max_count", 0).toInt());

   if (qsettings->contains("all_playlists_grid_max_count"))
      mainwindow->setAllPlaylistsGridMaxCount(qsettings->value("all_playlists_grid_max_count", 5000).toInt());

   if (qsettings->contains("thumbnail_cache_limit"))
      mainwindow->setThumbnailCacheLimit(qsettings->value("thumbnail_cache_limit", 500).toInt());
   else
      mainwindow->setThumbnailCacheLimit(500);

   if (qsettings->contains("geometry"))
      if (qsettings->contains("save_geometry"))
         mainwindow->restoreGeometry(qsettings->value("geometry").toByteArray());

   if (qsettings->contains("options_dialog_geometry"))
      mainwindow->viewOptionsDialog()->restoreGeometry(qsettings->value("options_dialog_geometry").toByteArray());

   if (qsettings->contains("save_dock_positions"))
      if (qsettings->contains("dock_positions"))
         mainwindow->restoreState(qsettings->value("dock_positions").toByteArray());

   if (qsettings->contains("file_browser_table_headers"))
      mainwindow->fileTableView()->horizontalHeader()->restoreState(qsettings->value("file_browser_table_headers").toByteArray());
   else
      mainwindow->fileTableView()->horizontalHeader()->resizeSection(0, 300);

   if (qsettings->contains("icon_view_zoom"))
      mainwindow->setIconViewZoom(qsettings->value("icon_view_zoom", 50).toInt());

   if (qsettings->contains("theme"))
   {
      QString themeStr = qsettings->value("theme").toString();
      MainWindow::Theme theme = mainwindow->getThemeFromString(themeStr);

      if (qsettings->contains("custom_theme") && theme == MainWindow::THEME_CUSTOM)
      {
         QString customThemeFilePath = qsettings->value("custom_theme").toString();

         mainwindow->setCustomThemeFile(customThemeFilePath);
      }

      mainwindow->setTheme(theme);
   }
   else
      mainwindow->setTheme();

   if (qsettings->contains("view_type"))
   {
      QString viewType = qsettings->value("view_type", "list").toString();

      if (viewType == "list")
         mainwindow->setCurrentViewType(MainWindow::VIEW_TYPE_LIST);
      else if (viewType == "icons")
         mainwindow->setCurrentViewType(MainWindow::VIEW_TYPE_ICONS);
      else
         mainwindow->setCurrentViewType(MainWindow::VIEW_TYPE_LIST);
   }
   else
      mainwindow->setCurrentViewType(MainWindow::VIEW_TYPE_LIST);

   if (qsettings->contains("icon_view_thumbnail_type"))
   {
      QString thumbnailType = qsettings->value("icon_view_thumbnail_type", "boxart").toString();

      if (thumbnailType == "boxart")
         mainwindow->setCurrentThumbnailType(THUMBNAIL_TYPE_BOXART);
      else if (thumbnailType == "screenshot")
         mainwindow->setCurrentThumbnailType(THUMBNAIL_TYPE_SCREENSHOT);
      else if (thumbnailType == "title")
         mainwindow->setCurrentThumbnailType(THUMBNAIL_TYPE_TITLE_SCREEN);
      else
         mainwindow->setCurrentThumbnailType(THUMBNAIL_TYPE_BOXART);
   }

   /* We make sure to hook up the tab widget callback only after the tabs themselves have been added,
    * but before changing to a specific one, to avoid the callback firing before the view type is set.
    */
   QObject::connect(browserAndPlaylistTabWidget, SIGNAL(currentChanged(int)), mainwindow, SLOT(onTabWidgetIndexChanged(int)));

   /* setting the last tab must come after setting the view type */
   if (qsettings->contains("save_last_tab"))
   {
      int lastTabIndex = qsettings->value("last_tab", 0).toInt();

      if (lastTabIndex >= 0 && browserAndPlaylistTabWidget->count() > lastTabIndex)
      {
         browserAndPlaylistTabWidget->setCurrentIndex(lastTabIndex);
         mainwindow->onTabWidgetIndexChanged(lastTabIndex);
      }
   }
   else
   {
      browserAndPlaylistTabWidget->setCurrentIndex(0);
      mainwindow->onTabWidgetIndexChanged(0);
   }

   /* the initial playlist that is selected is based on the user's setting (initialPlaylist) */
   for (i = 0; listWidget->count() && i < listWidget->count(); i++)
   {
      QString path;
      QListWidgetItem *item = listWidget->item(i);

      if (!item)
         continue;

      path = item->data(Qt::UserRole).toString();

      if (path == initialPlaylist)
      {
         foundPlaylist = true;
         listWidget->setRowHidden(i, false);
         listWidget->setCurrentRow(i);
         break;
      }
   }

   /* couldn't find the user's initial playlist, just find anything */
   if (!foundPlaylist)
   {
      for (i = 0; listWidget->count() && i < listWidget->count(); i++)
      {
         /* select the first non-hidden row */
         if (!listWidget->isRowHidden(i))
         {
            listWidget->setCurrentRow(i);
            break;
         }
      }
   }

   mainwindow->initContentTableWidget();

   return handle;
}

static void ui_companion_qt_notify_content_loaded(void *data) { }

static void ui_companion_qt_toggle(void *data, bool force)
{
   static bool already_started = false;
   ui_companion_qt_t *handle   = (ui_companion_qt_t*)data;
   ui_window_qt_t *win_handle  = (ui_window_qt_t*)handle->window;
   settings_t *settings        = config_get_ptr();
   bool ui_companion_toggle    = settings->bools.ui_companion_toggle;
   bool video_fullscreen       = settings->bools.video_fullscreen;
   bool mouse_grabbed          = input_mouse_grabbed();

   if (ui_companion_toggle || force)
   {
      if (mouse_grabbed)
         command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);
      video_driver_show_mouse();

      if (video_fullscreen)
         command_event(CMD_EVENT_FULLSCREEN_TOGGLE, NULL);

      win_handle->qtWindow->activateWindow();
      win_handle->qtWindow->raise();
      win_handle->qtWindow->show();

      if (video_driver_started_fullscreen())
         win_handle->qtWindow->lower();

      if (!already_started)
      {
         already_started = true;

         if (win_handle->qtWindow->settings()->value(
                  "show_welcome_screen", true).toBool())
            win_handle->qtWindow->showWelcomeScreen();
      }
   }
}

static void ui_companion_qt_event_command(void *data, enum event_command cmd)
{
   ui_companion_qt_t *handle  = (ui_companion_qt_t*)data;
   ui_window_qt_t *win_handle = (ui_window_qt_t*)handle->window;

   if (!handle)
      return;

   switch (cmd)
   {
      case CMD_EVENT_SHADERS_APPLY_CHANGES:
      case CMD_EVENT_SHADER_PRESET_LOADED:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         RARCH_LOG("[Qt]: Reloading shader parameters.\n");
         win_handle->qtWindow->deferReloadShaderParams();
#endif
         break;
      default:
         break;
   }
}

static void ui_companion_qt_notify_list_pushed(void *data, file_list_t *list,
   file_list_t *menu_list) { }

static void ui_companion_qt_notify_refresh(void *data)
{
   ui_companion_qt_t *handle  = (ui_companion_qt_t*)data;
   ui_window_qt_t *win_handle = (ui_window_qt_t*)handle->window;

   win_handle->qtWindow->deferReloadPlaylists();
}

static void ui_companion_qt_log_msg(void *data, const char *msg)
{
   ui_companion_qt_t *handle  = (ui_companion_qt_t*)data;
   ui_window_qt_t *win_handle = (ui_window_qt_t*)handle->window;

   win_handle->qtWindow->appendLogMessage(msg);
}

static bool ui_companion_qt_is_active(void *data)
{
   ui_companion_qt_t *handle  = (ui_companion_qt_t*)data;
   ui_window_qt_t *win_handle = (ui_window_qt_t*)handle->window;

   return win_handle->qtWindow->isVisible();
}

void ui_companion_qt_msg_queue_push(void *data,
      const char *msg, unsigned priority, unsigned duration, bool flush)
{
   ui_companion_qt_t *handle  = (ui_companion_qt_t*)data;
   ui_window_qt_t *win_handle = NULL;

   if (!handle)
      return;

   win_handle                 = (ui_window_qt_t*)handle->window;

   if (win_handle)
      win_handle->qtWindow->showStatusMessage(msg, priority, duration, flush);
}

ui_companion_driver_t ui_companion_qt = {
   ui_companion_qt_init,
   ui_companion_qt_deinit,
   ui_companion_qt_toggle,
   ui_companion_qt_event_command,
   ui_companion_qt_notify_content_loaded,
   ui_companion_qt_notify_list_pushed,
   ui_companion_qt_notify_refresh,
   ui_companion_qt_msg_queue_push,
   NULL,
   NULL,
   ui_companion_qt_log_msg,
   ui_companion_qt_is_active,
   &ui_browser_window_qt,
   &ui_msg_window_qt,
   &ui_window_qt,
   &ui_application_qt,
   "qt",
};

QStringList string_split_to_qt(QString str, char delim)
{
   int at;
   QStringList list = QStringList();

   for (at = 0;;)
   {
      /* Find next split */
      int spl = str.indexOf(delim, at);

      /* Store split into list of extensions */
      list << str.mid(at, (spl < 0 ? -1 : spl - at));

      /* No more splits */
      if (spl < 0)
         break;

      at = spl + 1;
   }

   return list;
}
