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
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../ui_companion_driver.h"
#include "../../core.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../msg_hash.h"
#include "../../tasks/tasks_internal.h"

#ifndef CXX_BUILD
}
#endif

#include "ui_qt.h"
#include "qt/viewoptionsdialog.h"

#include <QApplication>
#include <QtWidgets>
#include <QDesktopWidget>
#include <QtGlobal>
#include <QStyle>

#define INITIAL_WIDTH 1280
#define INITIAL_HEIGHT 720

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
#define GROUPED_DRAGGING QMainWindow::GroupedDragging
#else
#define GROUPED_DRAGGING static_cast<QMainWindow::DockOption>(0)
#endif

static bool already_started = false;

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
#ifdef Q_OS_WIN
#ifdef HAVE_ONLINE_UPDATER
   QMenu                        *toolsMenu = NULL;
   QMenu                      *updaterMenu = NULL;
#endif
#endif
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

#ifdef Q_OS_WIN
#ifdef HAVE_ONLINE_UPDATER
   toolsMenu = menu->addMenu(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_TOOLS));
   updaterMenu = toolsMenu->addMenu(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER));
   updaterMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_NIGHTLY), mainwindow, SLOT(updateRetroArchNightly()));
#endif
#endif
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
      QListWidgetItem *item = listWidget->item(i);
      QString path;

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

static void ui_companion_qt_notify_content_loaded(void *data)
{
   (void)data;
}

static void ui_companion_qt_toggle(void *data, bool force)
{
   ui_companion_qt_t *handle  = (ui_companion_qt_t*)data;
   ui_window_qt_t *win_handle = (ui_window_qt_t*)handle->window;
   settings_t *settings       = config_get_ptr();

   if (settings->bools.ui_companion_toggle || force)
   {
      if (settings->bools.video_fullscreen)
         command_event(CMD_EVENT_FULLSCREEN_TOGGLE, NULL);

      win_handle->qtWindow->activateWindow();
      win_handle->qtWindow->raise();
      video_driver_show_mouse();
      win_handle->qtWindow->show();

      if (video_driver_started_fullscreen())
         win_handle->qtWindow->lower();

      if (!already_started)
      {
         already_started = true;

         if (win_handle->qtWindow->settings()->value("show_welcome_screen", true).toBool())
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
   file_list_t *menu_list)
{
   (void)data;
   (void)list;
   (void)menu_list;
}

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

void ui_companion_qt_msg_queue_push(void *data, const char *msg, unsigned priority, unsigned duration, bool flush)
{
   ui_companion_qt_t *handle  = (ui_companion_qt_t*)data;
   ui_window_qt_t *win_handle = NULL;

   if (!handle)
      return;

   win_handle = (ui_window_qt_t*)handle->window;

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
   &ui_browser_window_qt,
   &ui_msg_window_qt,
   &ui_window_qt,
   &ui_application_qt,
   "qt",
};
