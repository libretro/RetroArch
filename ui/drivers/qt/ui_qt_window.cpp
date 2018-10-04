/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2018 - Brad Parker
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
#include <cmath>

#include "../ui_qt.h"
#include "invader_png.h"
#include "ui_qt_load_core_window.h"
#include "ui_qt_themes.h"
#include "flowlayout.h"
#include "shaderparamsdialog.h"
#include "coreoptionsdialog.h"
#include "filedropwidget.h"
#include "coreinfodialog.h"
#include "playlistentrydialog.h"
#include "viewoptionsdialog.h"

extern "C" {
#include "../../../version.h"
#include "../../../verbosity.h"
#include "../../../retroarch.h"
#include "../../../msg_hash.h"
#include "../../../core_info.h"
#include "../../../content.h"
#include "../../../paths.h"
#include "../../../configuration.h"
#include "../../../file_path_special.h"
#include "../../../playlist.h"
#include "../../../content.h"
#ifdef HAVE_MENU
#include "../../../menu/menu_driver.h"
#endif
#include "../../../config.def.h"
#include "../../../tasks/tasks_internal.h"
#include <string/stdstring.h>
#include <encodings/utf.h>
#include <file/file_path.h>
#include <file/archive_file.h>
#include <streams/file_stream.h>
#include <math.h>
#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/opensslv.h>
#endif
}

#include "../../../AUTHORS.h"

#define TIMER_MSEC 1000 /* periodic timer for gathering statistics */
#define STATUS_MSG_THROTTLE_MSEC 250

#ifndef COLLECTION_SIZE
#define COLLECTION_SIZE 99999
#endif

#define GENERIC_FOLDER_ICON "/xmb/dot-art/png/folder.png"
#define HIRAGANA_START 0x3041U
#define HIRAGANA_END 0x3096U
#define KATAKANA_START 0x30A1U
#define KATAKANA_END 0x30F6U
#define HIRA_KATA_OFFSET (KATAKANA_START - HIRAGANA_START)
#define DOCS_URL "http://docs.libretro.com/"

static ui_window_qt_t ui_window = {0};

enum CoreSelection
{
   CORE_SELECTION_CURRENT,
   CORE_SELECTION_PLAYLIST_SAVED,
   CORE_SELECTION_PLAYLIST_DEFAULT,
   CORE_SELECTION_ASK,
   CORE_SELECTION_LOAD_CORE
};

static const QPixmap getInvader()
{
   QPixmap pix;
   pix.loadFromData(invader_png, invader_png_len, "PNG");

   return pix;
}

#ifdef HAVE_LIBRETRODB
static void scan_finished_handler(void *task_data, void *user_data, const char *err)
{
   bool dontAsk = false;
   bool answer = false;
   menu_ctx_environment_t menu_environ;
   menu_environ.type = MENU_ENVIRON_RESET_HORIZONTAL_LIST;
   menu_environ.data = NULL;

   (void)task_data;
   (void)user_data;
   (void)err;
#ifdef HAVE_MENU
   menu_driver_ctl(RARCH_MENU_CTL_ENVIRONMENT, &menu_environ);
#endif
   if (!ui_window.qtWindow->settings()->value("scan_finish_confirm", true).toBool())
      return;

   answer = ui_window.qtWindow->showMessageBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED), MainWindow::MSGBOX_TYPE_QUESTION_OKCANCEL, Qt::ApplicationModal, true, &dontAsk);

   if (answer && dontAsk)
      ui_window.qtWindow->settings()->setValue("scan_finish_confirm", false);
}
#endif

static double expScale(double inputValue, double midValue, double maxValue)
{
   double returnValue = 0;
   double M = maxValue / midValue;
   double C = log(pow(M - 1, 2));
   double B = maxValue / (exp(C) - 1);
   double A = -1 * B;

   returnValue = A + B * exp(C * inputValue);

   return returnValue;
}

/* https://gist.github.com/andrey-str/0f9c7709cbf0c9c49ef9 */
static void setElidedText(QLabel *label, QWidget *clipWidget, int padding, const QString &text)
{
   QFontMetrics metrix(label->font());
   int width = clipWidget->width() - padding;
   QString clippedText = metrix.elidedText(text, Qt::ElideRight, width);
   label->setText(clippedText);
}

GridItem::GridItem() :
   QObject()
   ,widget(NULL)
   ,label(NULL)
   ,hash()
   ,image()
   ,pixmap()
   ,imageWatcher()
   ,labelText()
{
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

TableWidget::TableWidget(QWidget *parent) :
   QTableWidget(parent)
{
}

bool TableWidget::isEditorOpen()
{
   return (state() == QAbstractItemView::EditingState);
}

void TableWidget::keyPressEvent(QKeyEvent *event)
{
   if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
      emit enterPressed();
   else if (event->key() == Qt::Key_Delete)
      emit deletePressed();

   QTableWidget::keyPressEvent(event);
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

MainWindow::MainWindow(QWidget *parent) :
   QMainWindow(parent)
   ,m_loadCoreWindow(new LoadCoreWindow(this))
   ,m_timer(new QTimer(this))
   ,m_currentCore()
   ,m_currentCoreVersion()
   ,m_statusLabel(new QLabel(this))
   ,m_dirTree(new TreeView(this))
   ,m_dirModel(new QFileSystemModel(m_dirTree))
   ,m_listWidget(new ListWidget(this))
   ,m_tableWidget(new TableWidget(this))
   ,m_searchWidget(new QWidget(this))
   ,m_searchLineEdit(new QLineEdit(this))
   ,m_searchDock(new QDockWidget(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH), this))
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
   ,m_fileSanitizerRegex("[&*/:`<>?\\|]")
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
   ,m_logWidget(new QWidget(this))
   ,m_logTextEdit(new LogTextEdit(m_logWidget))
   ,m_imageFormats()
   ,m_historyPlaylistsItem(NULL)
   ,m_folderIcon()
   ,m_customThemeString()
   ,m_gridLayout(NULL)
   ,m_gridWidget(new QWidget(this))
   ,m_gridScrollArea(new QScrollArea(m_gridWidget))
   ,m_gridItems()
   ,m_gridLayoutWidget(new FileDropWidget())
   ,m_zoomSlider(NULL)
   ,m_lastZoomSliderValue(0)
   ,m_pendingItemUpdates()
   ,m_viewType(VIEW_TYPE_LIST)
   ,m_gridProgressBar(NULL)
   ,m_gridProgressWidget(NULL)
   ,m_currentGridHash()
   ,m_lastViewType(m_viewType)
   ,m_currentGridWidget(NULL)
   ,m_allPlaylistsListMaxCount(0)
   ,m_allPlaylistsGridMaxCount(0)
   ,m_playlistEntryDialog(NULL)
   ,m_statusMessageElapsedTimer()
   ,m_shaderParamsDialog(new ShaderParamsDialog())
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
{
   settings_t *settings = config_get_ptr();
   QDir playlistDir(settings->paths.directory_playlist);
   QString configDir = QFileInfo(path_get(RARCH_PATH_CONFIG)).dir().absolutePath();
   QToolButton *searchResetButton = NULL;
   QWidget *zoomWidget = new QWidget();
   QHBoxLayout *zoomLayout = new QHBoxLayout();
   QLabel *zoomLabel = new QLabel(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ZOOM), zoomWidget);
   QPushButton *viewTypePushButton = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_VIEW), zoomWidget);
   QMenu *viewTypeMenu = new QMenu(viewTypePushButton);
   QAction *viewTypeIconsAction = NULL;
   QAction *viewTypeListAction = NULL;
   QHBoxLayout *gridProgressLayout = new QHBoxLayout();
   QLabel *gridProgressLabel = NULL;
   QHBoxLayout *gridFooterLayout = NULL;

   qRegisterMetaType<QPointer<ThumbnailWidget> >("ThumbnailWidget");
   qRegisterMetaType<retro_task_callback_t>("retro_task_callback_t");

   /* Cancel all progress dialogs immediately since they show as soon as they're constructed. */
   m_updateProgressDialog->cancel();
   m_thumbnailDownloadProgressDialog->cancel();
   m_thumbnailPackDownloadProgressDialog->cancel();
   m_playlistThumbnailDownloadProgressDialog->cancel();

   m_gridProgressWidget = new QWidget();
   gridProgressLabel = new QLabel(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_PROGRESS), m_gridProgressWidget);

   viewTypePushButton->setObjectName("viewTypePushButton");
   viewTypePushButton->setFlat(true);

   viewTypeIconsAction = viewTypeMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS));
   viewTypeListAction = viewTypeMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST));

   viewTypePushButton->setMenu(viewTypeMenu);

   gridProgressLabel->setObjectName("gridProgressLabel");

   m_gridProgressBar = new QProgressBar(m_gridProgressWidget);

   m_gridProgressBar->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred));

   zoomLabel->setObjectName("zoomLabel");

   m_zoomSlider = new QSlider(Qt::Horizontal, zoomWidget);

   m_zoomSlider->setMinimum(0);
   m_zoomSlider->setMaximum(100);
   m_zoomSlider->setValue(50);
   m_zoomSlider->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred));

   m_lastZoomSliderValue = m_zoomSlider->value();

   m_gridWidget->setLayout(new QVBoxLayout());

   m_gridLayout = new FlowLayout(m_gridLayoutWidget);
   m_gridLayoutWidget->setObjectName("gridLayoutWidget");

   m_gridScrollArea->setAlignment(Qt::AlignCenter);
   m_gridScrollArea->setFrameShape(QFrame::NoFrame);
   m_gridScrollArea->setWidgetResizable(true);
   m_gridScrollArea->setWidget(m_gridLayoutWidget);

   m_gridWidget->layout()->addWidget(m_gridScrollArea);
   m_gridWidget->layout()->setAlignment(Qt::AlignCenter);
   m_gridWidget->layout()->setContentsMargins(0, 0, 0, 0);

   m_gridProgressWidget->setLayout(gridProgressLayout);
   gridProgressLayout->setContentsMargins(0, 0, 0, 0);
   gridProgressLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Preferred));
   gridProgressLayout->addWidget(gridProgressLabel);
   gridProgressLayout->addWidget(m_gridProgressBar);

   m_gridWidget->layout()->addWidget(m_gridProgressWidget);

   zoomWidget->setLayout(zoomLayout);
   zoomLayout->setContentsMargins(0, 0, 0, 0);
   zoomLayout->addWidget(zoomLabel);
   zoomLayout->addWidget(m_zoomSlider);
   zoomLayout->addWidget(viewTypePushButton);

   gridFooterLayout = new QHBoxLayout();
   gridFooterLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Preferred));
   gridFooterLayout->addWidget(m_gridProgressWidget);
   gridFooterLayout->addWidget(zoomWidget);

   static_cast<QVBoxLayout*>(m_gridWidget->layout())->addLayout(gridFooterLayout);

   m_gridProgressWidget->hide();

   m_tableWidget->setAlternatingRowColors(true);

   m_logWidget->setObjectName("logWidget");

   m_folderIcon = QIcon(QString(settings->paths.directory_assets) + GENERIC_FOLDER_ICON);
   m_imageFormats = QVector<QByteArray>::fromList(QImageReader::supportedImageFormats());
   m_defaultStyle = QApplication::style();
   m_defaultPalette = QApplication::palette();

   /* ViewOptionsDialog needs m_settings set before it's constructed */
   m_settings = new QSettings(configDir + "/retroarch_qt.cfg", QSettings::IniFormat, this);
   m_viewOptionsDialog = new ViewOptionsDialog(this, 0);
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

#if defined(Q_OS_WIN)
   m_dirModel->setRootPath("");
#else
   m_dirModel->setRootPath("/");
#endif

   m_dirTree->setModel(m_dirModel);
   m_dirTree->setSelectionMode(QAbstractItemView::SingleSelection);

   if (m_dirModel->columnCount() > 3)
   {
      /* size */
      m_dirTree->hideColumn(1);
      /* type */
      m_dirTree->hideColumn(2);
      /* date modified */
      m_dirTree->hideColumn(3);
   }

   m_dirTree->setCurrentIndex(m_dirModel->index(settings->paths.directory_menu_content));
   m_dirTree->scrollTo(m_dirTree->currentIndex(), QAbstractItemView::PositionAtTop);
   m_dirTree->expand(m_dirTree->currentIndex());

   reloadPlaylists();

   m_searchWidget->setLayout(new QHBoxLayout());
   m_searchWidget->layout()->addWidget(m_searchLineEdit);
   m_searchWidget->layout()->addWidget(searchResetButton);

   m_searchDock->setObjectName("searchDock");
   m_searchDock->setProperty("default_area", Qt::LeftDockWidgetArea);
   m_searchDock->setProperty("menu_text", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SEARCH));
   m_searchDock->setWidget(m_searchWidget);

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
   m_gridLayoutWidget->setContextMenuPolicy(Qt::CustomContextMenu);

   connect(m_searchLineEdit, SIGNAL(returnPressed()), this, SLOT(onSearchEnterPressed()));
   connect(m_searchLineEdit, SIGNAL(textEdited(const QString&)), this, SLOT(onSearchLineEditEdited(const QString&)));
   connect(m_timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
   connect(m_loadCoreWindow, SIGNAL(coreLoaded()), this, SLOT(onCoreLoaded()));
   connect(m_loadCoreWindow, SIGNAL(windowClosed()), this, SLOT(onCoreLoadWindowClosed()));
   connect(m_listWidget, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), this, SLOT(onCurrentListItemChanged(QListWidgetItem*, QListWidgetItem*)));
   connect(m_tableWidget, SIGNAL(currentItemChanged(QTableWidgetItem*, QTableWidgetItem*)), this, SLOT(onCurrentTableItemChanged(QTableWidgetItem*, QTableWidgetItem*)));
   connect(m_tableWidget, SIGNAL(itemDoubleClicked(QTableWidgetItem*)), this, SLOT(onContentItemDoubleClicked(QTableWidgetItem*)));
   connect(m_tableWidget, SIGNAL(enterPressed()), this, SLOT(onTableWidgetEnterPressed()));
   connect(m_tableWidget, SIGNAL(deletePressed()), this, SLOT(onTableWidgetDeletePressed()));
   connect(m_startCorePushButton, SIGNAL(clicked()), this, SLOT(onStartCoreClicked()));
   connect(m_coreInfoPushButton, SIGNAL(clicked()), m_coreInfoDialog, SLOT(showCoreInfo()));
   connect(m_runPushButton, SIGNAL(clicked()), this, SLOT(onRunClicked()));
   connect(m_stopPushButton, SIGNAL(clicked()), this, SLOT(onStopClicked()));
   connect(m_dirTree, SIGNAL(itemsSelected(QModelIndexList)), this, SLOT(onTreeViewItemsSelected(QModelIndexList)));
   connect(m_dirTree, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onFileBrowserTreeContextMenuRequested(const QPoint&)));
   connect(m_listWidget, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onPlaylistWidgetContextMenuRequested(const QPoint&)));
   connect(m_launchWithComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onLaunchWithComboBoxIndexChanged(int)));
   connect(m_zoomSlider, SIGNAL(valueChanged(int)), this, SLOT(onZoomValueChanged(int)));
   connect(viewTypeIconsAction, SIGNAL(triggered()), this, SLOT(onIconViewClicked()));
   connect(viewTypeListAction, SIGNAL(triggered()), this, SLOT(onListViewClicked()));
   connect(m_gridLayoutWidget, SIGNAL(filesDropped(QStringList)), this, SLOT(onPlaylistFilesDropped(QStringList)));
   connect(m_gridLayoutWidget, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onFileDropWidgetContextMenuRequested(const QPoint&)));
   connect(m_dirModel, SIGNAL(directoryLoaded(const QString&)), this, SLOT(onFileSystemDirLoaded(const QString&)));

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
   connect(this, SIGNAL(gridItemChanged(QString)), this, SLOT(onGridItemChanged(QString)));
   connect(this, SIGNAL(gotThumbnailDownload(QString,QString)), this, SLOT(onDownloadThumbnail(QString,QString)));

   /* make sure these use an auto connection so it will be queued if called from a different thread (some facilities in RA log messages from other threads) */
   connect(this, SIGNAL(gotLogMessage(const QString&)), this, SLOT(onGotLogMessage(const QString&)), Qt::AutoConnection);
   connect(this, SIGNAL(gotStatusMessage(QString,unsigned,unsigned,bool)), this, SLOT(onGotStatusMessage(QString,unsigned,unsigned,bool)), Qt::AutoConnection);
   connect(this, SIGNAL(gotReloadPlaylists()), this, SLOT(onGotReloadPlaylists()), Qt::AutoConnection);
   connect(this, SIGNAL(gotReloadShaderParams()), this, SLOT(onGotReloadShaderParams()), Qt::AutoConnection);
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

   removeUpdateTempFiles();
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

   removeGridItems();
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

QVector<QPair<QString, QString> > MainWindow::getPlaylists()
{
   QVector<QPair<QString, QString> > playlists;
   int i;

   for (i = 0; i < m_listWidget->count(); i++)
   {
      QListWidgetItem *item = m_listWidget->item(i);
      QPair<QString, QString> pair;
      QString label;
      QString path;

      if (!item)
         continue;

      label = item->text();
      path = item->data(Qt::UserRole).toString();

      pair.first = label;
      pair.second = path;

      playlists.append(pair);
   }

   return playlists;
}

void MainWindow::onGridItemChanged(QString title)
{
   int i;

   for (i = 0; i < m_gridItems.count(); i++)
   {
      const QPointer<GridItem> &item = m_gridItems.at(i);
      const QHash<QString, QString> &hash = item->hash;

      if (hash.value("label_noext") == title)
      {
         loadImageDeferred(item.data(), item->widget->property("image_path").toString());
         break;
      }
   }
}

void MainWindow::onItemChanged()
{
   ViewType viewType = getCurrentViewType();

   currentItemChanged(getCurrentContentHash());

   if (viewType == VIEW_TYPE_ICONS)
   {
      int i;

      for (i = 0; i < m_gridItems.count(); i++)
      {
         const QPointer<GridItem> &item = m_gridItems.at(i);

         if (item->widget == m_currentGridWidget)
         {
            loadImageDeferred(item.data(), m_currentGridWidget->property("image_path").toString());
            break;
         }
      }
   }
}

QString MainWindow::getSpecialPlaylistPath(SpecialPlaylist playlist)
{
   switch (playlist)
   {
      case SPECIAL_PLAYLIST_HISTORY:
         return (m_historyPlaylistsItem ? m_historyPlaylistsItem->data(Qt::UserRole).toString() : QString());
      default:
         return QString();
   }
}

double MainWindow::lerp(double x, double y, double a, double b, double d) {
  return a + (b - a) * ((double)(d - x) / (double)(y - x));
}

void MainWindow::onGridItemClicked(ThumbnailWidget *widget)
{
   QHash<QString, QString> hash;
   ThumbnailWidget *w = static_cast<ThumbnailWidget*>(sender());

   if (!w)
   {
      if (widget)
         w = widget;
      else
         return;
   }

   if (m_currentGridWidget)
   {
      m_currentGridWidget->setObjectName("thumbnailWidget");
      m_currentGridWidget->style()->unpolish(m_currentGridWidget);
      m_currentGridWidget->style()->polish(m_currentGridWidget);
   }

   hash = w->property("hash").value<QHash<QString, QString> >();
   w->setObjectName("thumbnailWidgetSelected");
   w->style()->unpolish(w);
   w->style()->polish(w);

   m_currentGridWidget = w;
   m_currentGridHash = hash;

   currentItemChanged(hash);
}

void MainWindow::onGridItemDoubleClicked()
{
   QHash<QString, QString> hash;
   ThumbnailWidget *w = static_cast<ThumbnailWidget*>(sender());

   if (!w)
      return;

   hash = w->property("hash").value<QHash<QString, QString> >();

   loadContent(hash);
}

void MainWindow::onIconViewClicked()
{
   setCurrentViewType(VIEW_TYPE_ICONS);
   onCurrentListItemChanged(m_listWidget->currentItem(), NULL);
}

void MainWindow::onListViewClicked()
{
   setCurrentViewType(VIEW_TYPE_LIST);
   onCurrentListItemChanged(m_listWidget->currentItem(), NULL);
}

void MainWindow::calcGridItemSize(GridItem *item, int zoomValue)
{
   int newSize = 0;
   QLabel *label = NULL;

   if (zoomValue < 50)
      newSize = expScale(lerp(0, 49, 25, 49, zoomValue) / 50.0, 102, 256);
   else
      newSize = expScale(zoomValue / 100.0, 256, 1024);

   item->widget->setFixedSize(QSize(newSize, newSize));

   label = item->widget->findChild<QLabel*>("thumbnailQLabel");

   if (label)
      setElidedText(label, item->widget, item->widget->layout()->contentsMargins().left() + item->widget->layout()->spacing() + 2, item->labelText);
}

void MainWindow::onZoomValueChanged(int value)
{
   int i;

   for (i = 0; i < m_gridItems.count(); i++)
   {
      GridItem *item = m_gridItems.at(i);
      calcGridItemSize(item, value);
   }

   m_lastZoomSliderValue = value;
}

void MainWindow::showWelcomeScreen()
{
   bool dontAsk = false;
   bool answer = false;
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
   else
   {
      QFile file(filePath);

      if (file.exists())
      {
         bool opened = file.open(QIODevice::ReadOnly);

         if (opened)
         {
            QByteArray fileArray = file.readAll();
            QString fileStr = QString::fromUtf8(fileArray);

            file.close();

            if (fileStr.isEmpty())
            {
               QMessageBox::critical(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME), msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FILE_IS_EMPTY));
               return false;
            }
            else
               setCustomThemeString(fileStr);
         }
         else
         {
            QMessageBox::critical(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME), msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED));
            return false;
         }
      }
      else
      {
         QMessageBox::critical(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME), msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST));
         return false;
      }
   }

   return true;
}

void MainWindow::setCustomThemeString(QString qss)
{
   m_customThemeString = qss;
}

bool MainWindow::showMessageBox(QString msg, MessageBoxType msgType, Qt::WindowModality modality, bool showDontAsk, bool *dontAsk)
{
   QPointer<QMessageBox> msgBoxPtr;
   QMessageBox *msgBox = NULL;
   QCheckBox *checkBox = NULL;

   msgBoxPtr = new QMessageBox(this);
   msgBox = msgBoxPtr.data();

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
      {
         msgBox->setIcon(QMessageBox::Information);
         msgBox->setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_INFORMATION));
         break;
      }
      case MSGBOX_TYPE_WARNING:
      {
         msgBox->setIcon(QMessageBox::Warning);
         msgBox->setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_WARNING));
         break;
      }
      case MSGBOX_TYPE_ERROR:
      {
         msgBox->setIcon(QMessageBox::Critical);
         msgBox->setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ERROR));
         break;
      }
      case MSGBOX_TYPE_QUESTION_YESNO:
      {
         msgBox->setIcon(QMessageBox::Question);
         msgBox->setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_QUESTION));
         msgBox->setStandardButtons(QMessageBox::Yes | QMessageBox::No);
         break;
      }
      case MSGBOX_TYPE_QUESTION_OKCANCEL:
      {
         msgBox->setIcon(QMessageBox::Question);
         msgBox->setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_QUESTION));
         msgBox->setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
         break;
      }
      default:
         break;
   }

   msgBox->setText(msg);
   msgBox->exec();

   if (!msgBoxPtr)
      return true;

   if (msgBox->result() != QMessageBox::Ok && msgBox->result() != QMessageBox::Yes)
      return false;

   if (checkBox)
      if (dontAsk)
         *dontAsk = checkBox->isChecked();

   return true;
}

void MainWindow::onFileBrowserTreeContextMenuRequested(const QPoint&)
{
#ifdef HAVE_LIBRETRODB
   QPointer<QAction> action;
   QList<QAction*> actions;
   QScopedPointer<QAction> scanAction;
   QDir dir;
   QString currentDirString = QDir::toNativeSeparators(m_dirModel->filePath(m_dirTree->currentIndex()));
   settings_t *settings = config_get_ptr();
   QByteArray dirArray;
   const char *fullpath = NULL;

   if (currentDirString.isEmpty())
      return;

   dir = currentDirString;

   if (!dir.exists())
      return;

   /* default NULL parameter for parent wasn't added until 5.7 */
   scanAction.reset(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY), 0));

   actions.append(scanAction.data());

   action = QMenu::exec(actions, QCursor::pos(), NULL, m_dirTree);

   if (!action)
      return;

   dirArray = currentDirString.toUtf8();
   fullpath = dirArray.constData();

   task_push_dbscan(
         settings->paths.directory_playlist,
         settings->paths.path_content_database,
         fullpath, true,
         m_settings->value("show_hidden_files", true).toBool(),
         scan_finished_handler);
#endif
}

void MainWindow::showStatusMessage(QString msg, unsigned priority, unsigned duration, bool flush)
{
   emit gotStatusMessage(msg, priority, duration, flush);
}

void MainWindow::onGotStatusMessage(QString msg, unsigned priority, unsigned duration, bool flush)
{
   int msecDuration = 0;
   QScreen *screen = qApp->primaryScreen();
   QStatusBar *status = statusBar();

   Q_UNUSED(priority)

   if (msg.isEmpty())
      return;

   if (!status)
      return;

   if (screen)
   {
      msecDuration = (duration / screen->refreshRate()) * 1000;
   }

   if (msecDuration <= 0)
      msecDuration = 1000;

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
   emit gotReloadShaderParams();
}

void MainWindow::onShaderParamsClicked()
{
   if (!m_shaderParamsDialog)
      return;

   m_shaderParamsDialog->show();

   onGotReloadShaderParams();
}

void MainWindow::onCoreOptionsClicked()
{
   if (!m_coreOptionsDialog)
      return;

   m_coreOptionsDialog->show();

   onGotReloadCoreOptions();
}

void MainWindow::onGotReloadShaderParams()
{
   if (m_shaderParamsDialog && m_shaderParamsDialog->isVisible())
      m_shaderParamsDialog->reload();
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
   QVector<QHash<QString, QString> > infoList = getCoreInfo();
   QString coreInfoText;
   QVariantMap coreMap = m_launchWithComboBox->currentData(Qt::UserRole).value<QVariantMap>();
   CoreSelection coreSelection = static_cast<CoreSelection>(coreMap.value("core_selection").toInt());
   int i = 0;

   if (infoList.count() == 0)
      return;

   for (i = 0; i < infoList.count(); i++)
   {
      const QHash<QString, QString> &hash = infoList.at(i);
      const QString &key = hash.value("html_key", hash.value("key"));
      const QString &value = hash.value("html_value", hash.value("value"));

      if (!key.isEmpty())
         coreInfoText += key;

      if (!value.isEmpty())
      {
         if (!key.isEmpty())
            coreInfoText += " ";

         coreInfoText += value;
      }

      if (i < infoList.count() - 1)
         coreInfoText += "<br>\n";
   }

   m_coreInfoLabel->setText(coreInfoText);

   if (coreSelection == CORE_SELECTION_LOAD_CORE)
   {
      onLoadCoreClicked();
   }
   else
   {
      m_loadCoreWindow->setProperty("last_launch_with_index", m_launchWithComboBox->currentIndex());
   }
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

   switch(theme)
   {
      case THEME_SYSTEM_DEFAULT:
      {
         qApp->setStyleSheet(qt_theme_default_stylesheet.arg(m_settings->value("highlight_color", "palette(highlight)").toString()));

         break;
      }
      case THEME_DARK:
      {
         qApp->setStyleSheet(qt_theme_dark_stylesheet.arg(m_settings->value("highlight_color", "palette(highlight)").toString()));

         break;
      }
      case THEME_CUSTOM:
      {
         qApp->setStyleSheet(m_customThemeString);

         break;
      }
      default:
         break;
   }
}

QVector<QHash<QString, QString> > MainWindow::getCoreInfo()
{
   QVector<QHash<QString, QString> > infoList;
   QHash<QString, QString> currentCore = getSelectedCore();
   core_info_list_t *core_info_list = NULL;
   const core_info_t *core_info = NULL;
   unsigned i = 0;

   core_info_get_list(&core_info_list);

   if (!core_info_list || core_info_list->count == 0)
      return infoList;

   for (i = 0; i < core_info_list->count; i++)
   {
      const core_info_t *core = &core_info_list->list[i];

      if (currentCore["core_path"] == core->path)
      {
         core_info = core;
         break;
      }
   }

   if (currentCore["core_path"].isEmpty() || !core_info || !core_info->config_data)
   {
      QHash<QString, QString> hash;

      hash["key"] = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE);
      hash["value"] = "";

      infoList.append(hash);

      return infoList;
   }

   if (core_info->core_name)
   {
      QHash<QString, QString> hash;

      hash["key"] = QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME)) + ":";
      hash["value"] = core_info->core_name;

      infoList.append(hash);
   }

   if (core_info->display_name)
   {
      QHash<QString, QString> hash;

      hash["key"] = QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL)) + ":";
      hash["value"] = core_info->display_name;

      infoList.append(hash);
   }

   if (core_info->systemname)
   {
      QHash<QString, QString> hash;

      hash["key"] = QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME)) + ":";
      hash["value"] = core_info->systemname;

      infoList.append(hash);
   }

   if (core_info->system_manufacturer)
   {
      QHash<QString, QString> hash;

      hash["key"] = QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER)) + ":";
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

      hash["key"] = QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES)) + ":";
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

      hash["key"] = QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS)) + ":";
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

      hash["key"] = QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS)) + ":";
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

      hash["key"] = QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES)) + ":";
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

      hash["key"] = QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS)) + ":";
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

         hash["key"] = QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE)) + ":";
         hash["value"] = "";

         infoList.append(hash);

         /* FIXME: This looks hacky and probably
          * needs to be improved for good translation support. */

         for (i = 0; i < core_info->firmware_count; i++)
         {
            if (core_info->firmware[i].desc)
            {
               QString labelText = "(!) ";
               QString valueText;
               QHash<QString, QString> hash;
               bool missing = false;

               if (core_info->firmware[i].missing)
               {
                  missing = true;
                  labelText += msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MISSING);
               }
               else
               {
                  labelText += msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PRESENT);
               }

               labelText += ", ";

               if (core_info->firmware[i].optional)
               {
                  labelText += msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OPTIONAL);
               }
               else
               {
                  labelText += msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REQUIRED);
               }

               labelText += ":";

               if (core_info->firmware[i].desc)
               {
                  valueText = core_info->firmware[i].desc;
               }
               else
               {
                  valueText = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME);
               }

               hash["key"] = labelText;
               hash["value"] = valueText;

               if (missing)
               {
                  QString style = "font-weight: bold; color: #ff0000";
                  hash["label_style"] = style;
                  hash["value_style"] = style;
                  hash["html_key"] = "<b><font color=\"#ff0000\">" + hash["key"] + "</font></b>";
                  hash["html_value"] = "<b><font color=\"#ff0000\">" + hash["value"] + "</font></b>";
               }
               else
               {
                  QString style = "font-weight: bold; color: rgb(0, 175, 0)";
                  hash["label_style"] = style;
                  hash["value_style"] = style;
                  hash["html_key"] = "<b><font color=\"#00af00\">" + hash["key"] + "</font></b>";
                  hash["html_value"] = "<b><font color=\"#00af00\">" + hash["value"] + "</font></b>";
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

         hash["key"] = "";
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

void MainWindow::selectBrowserDir(QString path)
{
   QStringList horizontal_header_labels;
   QDir dir = path;
   QStringList dirList;
   int i = 0;

   horizontal_header_labels << msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NAME);

   /* block this signal because setData() called in addPlaylistHashToTable() would trigger an infinite loop */
   disconnect(m_tableWidget, SIGNAL(itemChanged(QTableWidgetItem*)), this, SLOT(onCurrentTableItemDataChanged(QTableWidgetItem*)));

   m_tableWidget->clear();
   m_tableWidget->setColumnCount(0);
   m_tableWidget->setRowCount(0);
   m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
   m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
   m_tableWidget->setSortingEnabled(false);
   m_tableWidget->setColumnCount(1);
   m_tableWidget->setRowCount(0);
   m_tableWidget->setHorizontalHeaderLabels(horizontal_header_labels);
   m_tableWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
   m_tableWidget->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

   dirList = dir.entryList(QDir::NoDotAndDotDot |
                           QDir::Readable |
                           QDir::Files |
                           (m_settings->value("show_hidden_files", true).toBool() ? (QDir::Hidden | QDir::System) : static_cast<QDir::Filter>(0)),
                           QDir::Name);

   if (dirList.count() == 0)
      return;

   m_tableWidget->setRowCount(dirList.count());

   for (i = 0; i < dirList.count(); i++)
   {
      QString fileName = dirList.at(i);
      QTableWidgetItem *item = new QTableWidgetItem(fileName);
      QHash<QString, QString> hash;
      QString filePath(QDir::toNativeSeparators(dir.absoluteFilePath(fileName)));
      QFileInfo fileInfo(filePath);

      hash["path"] = filePath;
      hash["label"] = hash["path"];
      hash["label_noext"] = fileInfo.completeBaseName();
      hash["db_name"] = fileInfo.dir().dirName();

      item->setData(Qt::UserRole, QVariant::fromValue<QHash<QString, QString> >(hash));
      item->setFlags(item->flags() & ~Qt::ItemIsEditable);

      m_tableWidget->setItem(i, 0, item);
   }

   m_tableWidget->setSortingEnabled(true);
   m_tableWidget->resizeColumnsToContents();
   m_tableWidget->sortByColumn(0, Qt::AscendingOrder);
   m_tableWidget->selectRow(0);

   onSearchEnterPressed();

   connect(m_tableWidget, SIGNAL(itemChanged(QTableWidgetItem*)), this, SLOT(onCurrentTableItemDataChanged(QTableWidgetItem*)));
}

QTabWidget* MainWindow::browserAndPlaylistTabWidget()
{
   return m_browserAndPlaylistTabWidget;
}

void MainWindow::onTableWidgetEnterPressed()
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
   /* entry is being renamed, ignore this enter press */
   if (m_tableWidget->isPersistentEditorOpen(m_tableWidget->currentIndex()))
#else
   /* we can only check if any editor at all is open */
   if (m_tableWidget->isEditorOpen())
#endif
      return;
   onRunClicked();
}

void MainWindow::onTableWidgetDeletePressed()
{
   deleteCurrentPlaylistItem();
}

QHash<QString, QString> MainWindow::getCurrentContentHash()
{
   QTableWidgetItem *contentItem = m_tableWidget->currentItem();
   QHash<QString, QString> contentHash;
   ViewType viewType = getCurrentViewType();

   if (viewType == VIEW_TYPE_LIST)
   {
      if (!contentItem)
         return contentHash;

      contentHash = contentItem->data(Qt::UserRole).value<QHash<QString, QString> >();
   }
   else if (viewType == VIEW_TYPE_ICONS)
      contentHash = m_currentGridHash;

   return contentHash;
}

void MainWindow::onContentItemDoubleClicked(QTableWidgetItem*)
{
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
   {
      QMessageBox::critical(this, msg_hash_to_str(MSG_ERROR), msg_hash_to_str(MSG_FAILED_TO_LOAD_CONTENT));
   }
}

QHash<QString, QString> MainWindow::getSelectedCore()
{
   QVariantMap coreMap = m_launchWithComboBox->currentData(Qt::UserRole).value<QVariantMap>();
   CoreSelection coreSelection = static_cast<CoreSelection>(coreMap.value("core_selection").toInt());
   QHash<QString, QString> coreHash;
   QHash<QString, QString> contentHash;
   QTableWidgetItem *contentItem = m_tableWidget->currentItem();
   ViewType viewType = getCurrentViewType();

   if (viewType == VIEW_TYPE_LIST && contentItem)
      contentHash = contentItem->data(Qt::UserRole).value<QHash<QString, QString> >();
   else if (viewType == VIEW_TYPE_ICONS)
      contentHash = m_currentGridHash;
   else
      return coreHash;

   switch(coreSelection)
   {
      case CORE_SELECTION_CURRENT:
      {
         coreHash["core_path"] = path_get(RARCH_PATH_CORE);

         break;
      }
      case CORE_SELECTION_PLAYLIST_SAVED:
      {
         if (contentHash.isEmpty() || contentHash["core_path"].isEmpty())
            break;

         coreHash["core_path"] = contentHash["core_path"];

         break;
      }
      case CORE_SELECTION_PLAYLIST_DEFAULT:
      {
         QVector<QHash<QString, QString> > cores;
         int i = 0;

         if (contentHash.isEmpty() || contentHash["db_name"].isEmpty())
            break;

         cores = getPlaylistDefaultCores();

         for (i = 0; i < cores.count(); i++)
         {
            if (cores[i]["playlist_filename"] == contentHash["db_name"])
            {
               if (cores[i]["core_path"].isEmpty())
                  break;

               coreHash["core_path"] = cores[i]["core_path"];

               break;
            }
         }

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
#ifdef HAVE_MENU
   content_ctx_info_t content_info;
   QByteArray corePathArray;
   QByteArray contentPathArray;
   QByteArray contentLabelArray;
   const char *corePath = NULL;
   const char *contentPath = NULL;
   const char *contentLabel = NULL;
   QVariantMap coreMap = m_launchWithComboBox->currentData(Qt::UserRole).value<QVariantMap>();
   CoreSelection coreSelection = static_cast<CoreSelection>(coreMap.value("core_selection").toInt());

   if (m_pendingRun)
      coreSelection = CORE_SELECTION_CURRENT;

   if (coreSelection == CORE_SELECTION_ASK)
   {
      QStringList extensionFilters;

      if (contentHash.contains("path"))
      {
         int lastIndex = contentHash["path"].lastIndexOf('.');
         QString extensionStr;
         QByteArray pathArray = contentHash["path"].toUtf8();
         const char *pathData = pathArray.constData();

         if (lastIndex >= 0)
         {
            extensionStr = contentHash["path"].mid(lastIndex + 1);

            if (!extensionStr.isEmpty())
            {
               extensionFilters.append(extensionStr.toLower());
            }
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

   switch(coreSelection)
   {
      case CORE_SELECTION_CURRENT:
      {
         corePathArray = path_get(RARCH_PATH_CORE);
         contentPathArray = contentHash["path"].toUtf8();
         contentLabelArray = contentHash["label_noext"].toUtf8();

         break;
      }
      case CORE_SELECTION_PLAYLIST_SAVED:
      {
         corePathArray = contentHash["core_path"].toUtf8();
         contentPathArray = contentHash["path"].toUtf8();
         contentLabelArray = contentHash["label_noext"].toUtf8();

         break;
      }
      case CORE_SELECTION_PLAYLIST_DEFAULT:
      {
         QVector<QHash<QString, QString> > cores = getPlaylistDefaultCores();
         int i = 0;

         for (i = 0; i < cores.count(); i++)
         {
            if (cores[i]["playlist_filename"] == contentHash["db_name"])
            {
               corePathArray = cores[i]["core_path"].toUtf8();
               contentPathArray = contentHash["path"].toUtf8();
               contentLabelArray = contentHash["label_noext"].toUtf8();
               break;
            }
         }

         break;
      }
      default:
         return;
   }

   corePath = corePathArray.constData();
   contentPath = contentPathArray.constData();
   contentLabel = contentLabelArray.constData();

   content_info.argc                   = 0;
   content_info.argv                   = NULL;
   content_info.args                   = NULL;
   content_info.environ_get            = NULL;

   menu_navigation_set_selection(0);
   command_event(CMD_EVENT_UNLOAD_CORE, NULL);

   if (!task_push_load_content_from_playlist_from_menu(
            corePath, contentPath, contentLabel,
            &content_info,
            NULL, NULL))
   {
      QMessageBox::critical(this, msg_hash_to_str(MSG_ERROR), msg_hash_to_str(MSG_FAILED_TO_LOAD_CONTENT));
      return;
   }

   menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);
#endif
}

void MainWindow::onRunClicked()
{
#ifdef HAVE_MENU
   QTableWidgetItem *item = m_tableWidget->currentItem();
   ViewType viewType = getCurrentViewType();
   QHash<QString, QString> contentHash;

   if (viewType == VIEW_TYPE_LIST)
   {
      if (!item)
         return;

      contentHash = item->data(Qt::UserRole).value<QHash<QString, QString> >();
   }
   else if (viewType == VIEW_TYPE_ICONS)
   {
      contentHash = m_currentGridHash;

      if (contentHash.isEmpty())
         return;
   }
   else
      return;

   loadContent(contentHash);
#endif
}

bool MainWindow::isContentLessCore()
{
   rarch_system_info_t *system = runloop_get_system_info();

   return system->load_no_content;
}

bool MainWindow::isCoreLoaded()
{
   if (m_currentCore.isEmpty() || m_currentCore == msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE))
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
   QTableWidgetItem *currentContentItem = m_tableWidget->currentItem();
   QListWidgetItem *currentPlaylistItem = m_listWidget->currentItem();
   ViewType viewType = getCurrentViewType();
   QHash<QString, QString> hash;

   m_launchWithComboBox->clear();

   if (isContentLessCore())
      m_startCorePushButton->show();
   else
      m_startCorePushButton->hide();

   if (isCoreLoaded() && m_settings->value("suggest_loaded_core_first", false).toBool())
   {
      QVariantMap comboBoxMap;
      comboBoxMap["core_name"] = m_currentCore;
      comboBoxMap["core_path"] = path_get(RARCH_PATH_CORE);
      comboBoxMap["core_selection"] = CORE_SELECTION_CURRENT;
      m_launchWithComboBox->addItem(m_currentCore, QVariant::fromValue(comboBoxMap));
   }

   if (viewType == VIEW_TYPE_LIST && currentContentItem)
      hash = currentContentItem->data(Qt::UserRole).value<QHash<QString, QString> >();
   else if (viewType == VIEW_TYPE_ICONS)
      hash = m_currentGridHash;

   if (m_browserAndPlaylistTabWidget->tabText(m_browserAndPlaylistTabWidget->currentIndex()) == msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS))
   {
      if (!hash.isEmpty())
      {
         QString coreName = hash["core_name"];

         if (coreName.isEmpty())
         {
            coreName = "<n/a>";
         }
         else
         {
            const char *detect_str = file_path_str(FILE_PATH_DETECT);

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
                     comboBoxMap["core_name"] = coreName;
                     comboBoxMap["core_path"] = hash["core_path"];
                     comboBoxMap["core_selection"] = CORE_SELECTION_PLAYLIST_SAVED;
                     m_launchWithComboBox->addItem(coreName, QVariant::fromValue(comboBoxMap));
                  }
               }
            }
         }
      }
   }

   if (!hash["db_name"].isEmpty())
   {
      QVector<QHash<QString, QString> > defaultCores = getPlaylistDefaultCores();
      int i = 0;

      if (defaultCores.count() > 0)
      {
         QString currentPlaylistItemDataString;
         bool allPlaylists = false;
         int row = 0;

         if (currentPlaylistItem)
         {
            currentPlaylistItemDataString = currentPlaylistItem->data(Qt::UserRole).toString();
            allPlaylists = (currentPlaylistItemDataString == ALL_PLAYLISTS_TOKEN);
         }

         for (row = 0; row < m_listWidget->count(); row++)
         {
            if (allPlaylists)
            {
               QListWidgetItem *listItem = m_listWidget->item(row);
               QString listItemString = listItem->data(Qt::UserRole).toString();
               QFileInfo info;

               info.setFile(listItemString);

               if (listItemString == ALL_PLAYLISTS_TOKEN)
                  continue;
            }

            for (i = 0; i < defaultCores.count(); i++)
            {
               QString playlist = defaultCores.at(i)["playlist_filename"];
               QString core = defaultCores.at(i)["core_path"];
               QString currentPlaylistFileName = hash["db_name"];

               playlist.remove(file_path_str(FILE_PATH_LPL_EXTENSION));

               if (currentPlaylistFileName == playlist)
               {
                  core_info_list_t *coreInfoList = NULL;
                  unsigned j = 0;

                  core_info_get_list(&coreInfoList);

                  for (j = 0; j < coreInfoList->count; j++)
                  {
                     const core_info_t *info = &coreInfoList->list[j];

                     if (core == info->path)
                     {
                        if (m_launchWithComboBox->findText(info->core_name) == -1)
                        {
                           int i = 0;
                           bool found_existing = false;

                           for (i = 0; i < m_launchWithComboBox->count(); i++)
                           {
                              QVariantMap map = m_launchWithComboBox->itemData(i, Qt::UserRole).toMap();

                              if (map.value("core_path").toString() == info->path || map.value("core_name").toString() == info->core_name)
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
   Q_UNUSED(index)

   if (m_browserAndPlaylistTabWidget->tabText(m_browserAndPlaylistTabWidget->currentIndex()) == msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER))
   {
      QModelIndex index = m_dirTree->currentIndex();

      /* force list view for file browser, will set it back to whatever the user had when switching back to playlist tab */
      setCurrentViewType(VIEW_TYPE_LIST);

      m_tableWidget->clear();
      m_tableWidget->setColumnCount(0);
      m_tableWidget->setRowCount(0);

      if (index.isValid())
      {
         m_dirTree->clearSelection();
         m_dirTree->setCurrentIndex(index);
      }
   }
   else if (m_browserAndPlaylistTabWidget->tabText(m_browserAndPlaylistTabWidget->currentIndex()) == msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS))
   {
      QListWidgetItem *item = m_listWidget->currentItem();

      if (m_lastViewType != getCurrentViewType())
         setCurrentViewType(m_lastViewType);

      m_tableWidget->clear();
      m_tableWidget->setColumnCount(0);
      m_tableWidget->setRowCount(0);

      if (item)
      {
         m_listWidget->setCurrentItem(NULL);
         m_listWidget->setCurrentItem(item);
      }
   }

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
   QList<QTableWidgetItem*> items;
   QVector<QPointer<GridItem> > gridItems;
   QVector<unsigned> textUnicode = text.toUcs4();
   QVector<unsigned> textHiraToKata;
   QVector<unsigned> textKataToHira;
   ViewType viewType = getCurrentViewType();
   bool foundHira = false;
   bool foundKata = false;

   for (i = 0; i < textUnicode.size(); i++)
   {
      unsigned code = textUnicode.at(i);

      if (code >= HIRAGANA_START && code <= HIRAGANA_END)
      {
         foundHira = true;
         textHiraToKata += code + HIRA_KATA_OFFSET;
      }
      else if (code >= KATAKANA_START && code <= KATAKANA_END)
      {
         foundKata = true;
         textKataToHira += code - HIRA_KATA_OFFSET;
      }
      else
      {
         textHiraToKata += code;
         textKataToHira += code;
      }
   }

   switch(viewType)
   {
      case VIEW_TYPE_LIST:
      {
         if (text.isEmpty())
         {
            for (i = 0; i < m_tableWidget->rowCount(); i++)
            {
               m_tableWidget->setRowHidden(i, false);
            }
            return;
         }

         items.append(m_tableWidget->findItems(text, Qt::MatchContains));

         if (foundHira)
         {
            items.append(m_tableWidget->findItems(QString::fromUcs4(textHiraToKata.constData(), textHiraToKata.size()), Qt::MatchContains));
         }

         if (foundKata)
         {
            items.append(m_tableWidget->findItems(QString::fromUcs4(textKataToHira.constData(), textKataToHira.size()), Qt::MatchContains));
         }

         if (items.isEmpty())
         {
            for (i = 0; i < m_tableWidget->rowCount(); i++)
            {
               m_tableWidget->setRowHidden(i, true);
            }

            return;
         }
         else
         {
            for (i = 0; i < m_tableWidget->rowCount(); i++)
            {
               if (items.contains(m_tableWidget->item(i, 0)))
                  m_tableWidget->setRowHidden(i, false);
               else
                  m_tableWidget->setRowHidden(i, true);
            }
         }

         break;
      }
      case VIEW_TYPE_ICONS:
      {
         int i;

         if (text.isEmpty())
         {
            for (i = 0; i < m_gridItems.size(); i++)
            {
               m_gridItems.at(i)->widget->show();
            }
            return;
         }

         for (i = 0; i < m_gridItems.count(); i++)
         {
            const QPointer<GridItem> &item = m_gridItems.at(i);

            if (item->hash.value("label").contains(text, Qt::CaseInsensitive))
               gridItems.append(item);

            if (foundHira)
            {
               if (item->hash.value("label").contains(QString::fromUcs4(textHiraToKata.constData(), textHiraToKata.size()), Qt::CaseInsensitive))
                  gridItems.append(item);
            }

            if (foundKata)
            {
               if (item->hash.value("label").contains(QString::fromUcs4(textKataToHira.constData(), textKataToHira.size()), Qt::CaseInsensitive))
                  gridItems.append(item);
            }
         }

         if (gridItems.isEmpty())
         {
            for (i = 0; i < m_gridItems.size(); i++)
            {
               m_gridItems.at(i)->widget->hide();
            }

            return;
         }
         else
         {
            for (i = 0; i < m_gridItems.size(); i++)
            {
               const QPointer<GridItem> &item = m_gridItems.at(i);

               if (gridItems.contains(item))
                  item->widget->show();
               else
                  item->widget->hide();
            }
         }

         break;
      }
      default:
         break;
   }
}

void MainWindow::onViewClosedDocksAboutToShow()
{
   QMenu *menu = qobject_cast<QMenu*>(sender());
   QList<QDockWidget*> dockWidgets;
   bool found = false;
   int i = 0;

   if (!menu)
      return;

   dockWidgets = findChildren<QDockWidget*>();

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
         QAction *action = menu->addAction(dock->property("menu_text").toString(), this, SLOT(onShowHiddenDockWidgetAction()));
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
      addDockWidget(static_cast<Qt::DockWidgetArea>(dock->property("default_area").toInt()), dock);
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

void MainWindow::onCurrentTableItemChanged(QTableWidgetItem *current, QTableWidgetItem *)
{
   QHash<QString, QString> hash;

   if (!current)
      return;

   hash = current->data(Qt::UserRole).value<QHash<QString, QString> >();

   currentItemChanged(hash);
}

void MainWindow::onCurrentTableItemDataChanged(QTableWidgetItem *item)
{
   QHash<QString, QString> hash;

   if (!item)
      return;

   /* block this signal because setData() would trigger an infinite loop here */
   disconnect(m_tableWidget, SIGNAL(itemChanged(QTableWidgetItem*)), this, SLOT(onCurrentTableItemDataChanged(QTableWidgetItem*)));

   hash = item->data(Qt::UserRole).value<QHash<QString, QString> >();
   hash["label"] = item->text();
   hash["label_noext"] = QFileInfo(item->text()).completeBaseName();

   item->setData(Qt::UserRole, QVariant::fromValue(hash));

   updateCurrentPlaylistEntry(hash);

   currentItemChanged(hash);

   connect(m_tableWidget, SIGNAL(itemChanged(QTableWidgetItem*)), this, SLOT(onCurrentTableItemDataChanged(QTableWidgetItem*)));
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
   settings_t *settings = config_get_ptr();
   QDir playlistDir(settings->paths.directory_playlist);
   bool specialPlaylist = false;

   if (!item)
      return;

   playlistPath = item->data(Qt::UserRole).toString();
   playlistInfo = playlistPath;
   oldName = playlistInfo.completeBaseName();

   /* Don't just compare strings in case there are case differences on Windows that should be ignored. */
   if (QDir(playlistInfo.absoluteDir()) != QDir(playlistDir))
   {
      /* special playlists like history etc. can't have an association */
      specialPlaylist = true;
   }

   if (specialPlaylist)
   {
      /* special playlists shouldn't be editable already, but just in case, set the old name back and early return if they rename it */
      item->setText(oldName);
      return;
   }

   /* block this signal because setData() would trigger an infinite loop here */
   disconnect(m_listWidget, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(onCurrentListItemDataChanged(QListWidgetItem*)));

   oldPath = item->data(Qt::UserRole).toString();

   file.setFileName(oldPath);
   info = file;

   extension = info.suffix();

   newPath = info.absolutePath();

   /* absolutePath() will always use / even on Windows */
   if (newPath.at(newPath.count() - 1) != '/')
   {
      /* add trailing slash if the path doesn't have one */
      newPath += '/';
   }

   newPath += newName + "." + extension;

   item->setData(Qt::UserRole, newPath);

   if (!file.rename(newPath))
   {
      RARCH_ERR("[Qt]: Could not rename playlist.\n");
      item->setText(oldName);
   }

   connect(m_listWidget, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(onCurrentListItemDataChanged(QListWidgetItem*)));
}

void MainWindow::currentItemChanged(const QHash<QString, QString> &hash)
{
   settings_t *settings = config_get_ptr();
   QString label;
   QString playlist_name;
   QByteArray extension;
   QString extensionStr;
   int lastIndex = -1;

   label = hash["label_noext"];
   label.replace(m_fileSanitizerRegex, "_");

   lastIndex = hash["path"].lastIndexOf('.');

   if (lastIndex >= 0)
   {
      extensionStr = hash["path"].mid(lastIndex + 1);

      if (!extensionStr.isEmpty())
      {
         extension = extensionStr.toLower().toUtf8();
      }
   }

   playlist_name = hash["db_name"];

   if (m_thumbnailPixmap)
      delete m_thumbnailPixmap;
   if (m_thumbnailPixmap2)
      delete m_thumbnailPixmap2;
   if (m_thumbnailPixmap3)
      delete m_thumbnailPixmap3;

   if (!extension.isEmpty() && m_imageFormats.contains(extension))
   {
      /* use thumbnail widgets to show regular image files */
      m_thumbnailPixmap = new QPixmap(hash["path"]);
      m_thumbnailPixmap2 = new QPixmap(*m_thumbnailPixmap);
      m_thumbnailPixmap3 = new QPixmap(*m_thumbnailPixmap);
   }
   else
   {
      m_thumbnailPixmap = new QPixmap(QString(settings->paths.directory_thumbnails) + "/" + playlist_name + "/" + THUMBNAIL_BOXART + "/" + label + ".png");
      m_thumbnailPixmap2 = new QPixmap(QString(settings->paths.directory_thumbnails) + "/" + playlist_name + "/" + THUMBNAIL_TITLE + "/" + label + ".png");
      m_thumbnailPixmap3 = new QPixmap(QString(settings->paths.directory_thumbnails) + "/" + playlist_name + "/" + THUMBNAIL_SCREENSHOT + "/" + label + ".png");
   }

   resizeThumbnails(true, true, true);

   setCoreActions();
}

void MainWindow::onResizeThumbnailOne()
{
   resizeThumbnails(true, false, false);
}

void MainWindow::onResizeThumbnailTwo()
{
   resizeThumbnails(false, true, false);
}

void MainWindow::onResizeThumbnailThree()
{
   resizeThumbnails(false, false, true);
}

void MainWindow::resizeThumbnails(bool one, bool two, bool three)
{
   QPixmap pixmap;
   QPixmap pixmap2;
   QPixmap pixmap3;
   ThumbnailLabel *thumbnail = NULL;
   ThumbnailLabel *thumbnail2 = NULL;
   ThumbnailLabel *thumbnail3 = NULL;

   if (m_thumbnailPixmap)
      pixmap = *m_thumbnailPixmap;
   if (m_thumbnailPixmap2)
      pixmap2 = *m_thumbnailPixmap2;
   if (m_thumbnailPixmap3)
      pixmap3 = *m_thumbnailPixmap3;

   thumbnail = findChild<ThumbnailLabel*>("thumbnail");
   thumbnail2 = findChild<ThumbnailLabel*>("thumbnail2");
   thumbnail3 = findChild<ThumbnailLabel*>("thumbnail3");

   if (thumbnail && one)
   {
      if (pixmap.isNull())
         thumbnail->hide();
      else
      {
         thumbnail->show();
         emit thumbnailChanged(pixmap);
         thumbnail->update();
      }
   }

   if (thumbnail2 && two)
   {
      if (pixmap2.isNull())
         thumbnail2->hide();
      else
      {
         thumbnail2->show();
         emit thumbnail2Changed(pixmap2);
         thumbnail2->update();
      }
   }

   if (thumbnail3 && three)
   {
      if (pixmap3.isNull())
         thumbnail3->hide();
      else
      {
         thumbnail3->show();
         emit thumbnail3Changed(pixmap3);
         thumbnail3->update();
      }
   }
}

void MainWindow::setCurrentViewType(ViewType viewType)
{
   m_lastViewType = m_viewType;
   m_viewType = viewType;

   switch (viewType)
   {
      case VIEW_TYPE_ICONS:
      {
         m_tableWidget->hide();
         m_gridWidget->show();
         break;
      }
      case VIEW_TYPE_LIST:
      default:
      {
         m_viewType = VIEW_TYPE_LIST;
         m_gridWidget->hide();
         m_tableWidget->show();
         break;
      }
   }
}

MainWindow::ViewType MainWindow::getCurrentViewType()
{
   return m_viewType;
}

void MainWindow::onCurrentListItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
   ViewType viewType = getCurrentViewType();

   Q_UNUSED(current)
   Q_UNUSED(previous)

   if (m_browserAndPlaylistTabWidget->tabText(m_browserAndPlaylistTabWidget->currentIndex()) != msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS))
      return;

   switch (viewType)
   {
      case VIEW_TYPE_ICONS:
      {
         initContentGridLayout();
         break;
      }
      case VIEW_TYPE_LIST:
      default:
      {
         initContentTableWidget();
         break;
      }
   }

   setCoreActions();
}

TableWidget* MainWindow::contentTableWidget()
{
   return m_tableWidget;
}

QWidget* MainWindow::contentGridWidget()
{
   return m_gridWidget;
}

FlowLayout* MainWindow::contentGridLayout()
{
   return m_gridLayout;
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
   m_dirTree->scrollTo(m_dirTree->currentIndex(), QAbstractItemView::EnsureVisible);
}

void MainWindow::onBrowserStartClicked()
{
   settings_t *settings = config_get_ptr();

   m_dirTree->setCurrentIndex(m_dirModel->index(settings->paths.directory_menu_content));
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
   bool is_inited = false;

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
   menu_navigation_set_selection(0);
   command_event(CMD_EVENT_UNLOAD_CORE, NULL);
   setCurrentCoreLabel();
   activateWindow();
   raise();
}

void MainWindow::setCurrentCoreLabel()
{
   rarch_system_info_t *system = runloop_get_system_info();
   bool update = false;
   QString libraryName = system->info.library_name;
   const char *no_core_str = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE);

   if (m_statusLabel->text().isEmpty() || (m_currentCore != no_core_str && libraryName.isEmpty()))
   {
      m_currentCore = no_core_str;
      m_currentCoreVersion = "";
      update = true;
   }
   else
   {
      if (m_currentCore != libraryName && !libraryName.isEmpty())
      {
         m_currentCore = system->info.library_name;
         m_currentCoreVersion = (string_is_empty(system->info.library_version) ? "" : system->info.library_version);
         update = true;
      }
   }

   if (update)
   {
      QAction *unloadCoreAction = findChild<QAction*>("unloadCoreAction");
      QString text = QString(PACKAGE_VERSION) + " - " + m_currentCore + " " + m_currentCoreVersion;
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
   int lastLaunchWithIndex = lastLaunchWithVariant.toInt();

   m_pendingRun = false;

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

   menu_navigation_set_selection(0);

   if (!command_event(CMD_EVENT_UNLOAD_CORE, NULL))
   {
      /* TODO */
      return;
   }

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

void MainWindow::removeGridItems()
{
   if (m_gridItems.count() > 0)
   {
      QMutableVectorIterator<QPointer<GridItem> > items(m_gridItems);

      m_pendingItemUpdates.clear();

      while (items.hasNext())
      {
         QPointer<GridItem> item = items.next();

         if (item)
         {
            item->imageWatcher.waitForFinished();

            items.remove();

            m_gridLayout->removeWidget(item->widget);

            delete item->widget;
            delete item;
         }
      }
   }
}

void MainWindow::onDeferredImageLoaded()
{
   const QFutureWatcher<GridItem*> *watcher = static_cast<QFutureWatcher<GridItem*>*>(sender());
   GridItem *item = NULL;

   if (!watcher)
      return;

   item = watcher->result();

   if (!item)
      return;

   if (m_gridItems.contains(item))
   {
      if (!item->image.isNull())
      {
         m_pendingItemUpdates.append(item);
         QTimer::singleShot(0, this, SLOT(onPendingItemUpdates()));
      }
   }
}

void MainWindow::onPendingItemUpdates()
{
   QMutableListIterator<GridItem*> list(m_pendingItemUpdates);

   while (list.hasNext())
   {
      GridItem *item = list.next();

      if (!item)
         continue;

      if (m_gridItems.contains(item))
         onUpdateGridItemPixmapFromImage(item);

      list.remove();
   }
}

void MainWindow::onUpdateGridItemPixmapFromImage(GridItem *item)
{
   if (!item)
      return;

   if (!m_gridItems.contains(item))
      return;

   item->label->setPixmap(QPixmap::fromImage(item->image));
   item->label->update();
}

void MainWindow::loadImageDeferred(GridItem *item, QString path)
{
   connect(&item->imageWatcher, SIGNAL(finished()), this, SLOT(onDeferredImageLoaded()), Qt::QueuedConnection);
   item->imageWatcher.setFuture(QtConcurrent::run<GridItem*>(this, &MainWindow::doDeferredImageLoad, item, path));
}

GridItem* MainWindow::doDeferredImageLoad(GridItem *item, QString path)
{
   /* this runs in another thread */
   if (!item)
      return NULL;

   /* While we are indeed writing across thread boundaries here, the image is never accessed until after
    * its thread finishes, and the item is never deleted without first waiting for the thread to finish.
    */
   item->image = QImage(path);

   return item;
}

void MainWindow::initContentGridLayout()
{
   QListWidgetItem *item = m_listWidget->currentItem();
   QString path;

   if (!item)
      return;

   m_gridProgressBar->setMinimum(0);
   m_gridProgressBar->setMaximum(0);
   m_gridProgressBar->setValue(0);
   m_gridProgressWidget->show();

   removeGridItems();

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
      settings_t *settings = config_get_ptr();
      QDir playlistDir(settings->paths.directory_playlist);
      QStringList playlists;
      int i = 0;

      for (i = 0; i < m_playlistFiles.count(); i++)
      {
         const QString &playlist = m_playlistFiles.at(i);
         playlists.append(playlistDir.absoluteFilePath(playlist));
      }

      addPlaylistItemsToGrid(playlists, true);
   }
   else
      addPlaylistItemsToGrid(QStringList() << path);

   QTimer::singleShot(0, this, SLOT(onContentGridInited()));
}

void MainWindow::onContentGridInited()
{
   ThumbnailWidget *thumbnailWidget = NULL;

   m_gridLayoutWidget->resize(m_gridScrollArea->viewport()->size());

   onZoomValueChanged(m_zoomSlider->value());

   onSearchEnterPressed();

   if (m_gridItems.count() > 0)
   {
      GridItem *gridItem = m_gridItems.at(0);

      if (gridItem)
      {
         thumbnailWidget = m_gridItems.at(0)->widget.data();

         if (thumbnailWidget)
            onGridItemClicked(thumbnailWidget);
      }
   }
}

void MainWindow::initContentTableWidget()
{
   QListWidgetItem *item = m_listWidget->currentItem();
   QStringList horizontal_header_labels;
   QString path;
   int i = 0;

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

   horizontal_header_labels << msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NAME);

   /* block this signal because setData() called in addPlaylistHashToTable() would trigger an infinite loop */
   disconnect(m_tableWidget, SIGNAL(itemChanged(QTableWidgetItem*)), this, SLOT(onCurrentTableItemDataChanged(QTableWidgetItem*)));

   m_tableWidget->clear();
   m_tableWidget->setColumnCount(0);
   m_tableWidget->setRowCount(0);
   m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
   m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
   m_tableWidget->setEditTriggers(QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
   m_tableWidget->setSortingEnabled(false);
   m_tableWidget->setColumnCount(1);
   m_tableWidget->setRowCount(0);
   m_tableWidget->setHorizontalHeaderLabels(horizontal_header_labels);
   m_tableWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
   m_tableWidget->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

   path = item->data(Qt::UserRole).toString();

   if (path == ALL_PLAYLISTS_TOKEN)
   {
      settings_t *settings = config_get_ptr();
      QDir playlistDir(settings->paths.directory_playlist);
      QStringList playlists;
      int i = 0;

      for (i = 0; i < m_playlistFiles.count(); i++)
      {
         const QString &playlist = m_playlistFiles.at(i);
         playlists.append(playlistDir.absoluteFilePath(playlist));
      }

      addPlaylistItemsToTable(playlists, true);
   }
   else
      addPlaylistItemsToTable(QStringList() << path);

   m_tableWidget->setSortingEnabled(true);

   if (item != m_historyPlaylistsItem)
      m_tableWidget->sortByColumn(0, Qt::AscendingOrder);

   m_tableWidget->resizeColumnsToContents();

   for (i = 0; i < m_tableWidget->rowCount(); i++)
   {
      /* select the first non-hidden row */
      if (!m_tableWidget->isRowHidden(i))
      {
         m_tableWidget->selectRow(i);
         break;
      }
   }

   onSearchEnterPressed();

   connect(m_tableWidget, SIGNAL(itemChanged(QTableWidgetItem*)), this, SLOT(onCurrentTableItemDataChanged(QTableWidgetItem*)));
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
/*
   if (event->key() == Qt::Key_F5)
   {
      event->accept();
      hide();

      return;
   }
*/
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
      {
         return QStringLiteral("icons");
      }
      case VIEW_TYPE_LIST:
      default:
      {
         return QStringLiteral("list");
      }
   }

   return QStringLiteral("list");
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
         "<br><br>" + "<a href=\"http://www.libretro.com/\">www.libretro.com</a>"
         "<br><br>" + "<a href=\"http://www.retroarch.com/\">www.retroarch.com</a>"
         "<br>";
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
   QByteArray pathArray = path.toUtf8();
   QByteArray dirArray = extractionDir.toUtf8();
   const char *file = pathArray.constData();
   const char *dir = dirArray.constData();
   file_archive_transfer_t state;
   struct archive_extract_userdata userdata;
   struct string_list *file_list = file_archive_get_file_list(file, NULL);
   bool returnerr = true;
   unsigned i;

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

   memset(&state, 0, sizeof(state));
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

   if (!task_push_decompress(file, dir,
            NULL, NULL, NULL,
            cb, this))
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
   {
      str.replace(chars.at(i), '_');
   }

   return str;
}

static void* ui_window_qt_init(void)
{
   ui_window.qtWindow = new MainWindow();

   return &ui_window;
}

static void ui_window_qt_destroy(void *data)
{
   (void)data;
/*
   ui_window_qt_t *window = (ui_window_qt_t*)data;

   delete window->qtWindow;
*/
}

static void ui_window_qt_set_focused(void *data)
{
   (void)data;
/*
   ui_window_qt_t *window = (ui_window_qt_t*)data;

   window->qtWindow->raise();
   window->qtWindow->activateWindow();
*/
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
   (void)data;
   (void)buf;
/*
   ui_window_qt_t *window = (ui_window_qt_t*)data;

   window->qtWindow->setWindowTitle(QString::fromUtf8(buf));
*/
}

static void ui_window_qt_set_droppable(void *data, bool droppable)
{
   (void)data;
   (void)droppable;
/*
   ui_window_qt_t *window = (ui_window_qt_t*)data;

   window->qtWindow->setAcceptDrops(droppable);
*/
}

static bool ui_window_qt_focused(void *data)
{
   (void)data;
/*
   ui_window_qt_t *window = (ui_window_qt_t*)data;
   return window->qtWindow->isActiveWindow() && !window->qtWindow->isMinimized();
*/
   return true;
}

ui_window_t ui_window_qt = {
   ui_window_qt_init,
   ui_window_qt_destroy,
   ui_window_qt_set_focused,
   ui_window_qt_set_visible,
   ui_window_qt_set_title,
   ui_window_qt_set_droppable,
   ui_window_qt_focused,
   "qt"
};
