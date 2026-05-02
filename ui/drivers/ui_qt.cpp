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

#define WIN32_LEAN_AND_MEAN
#include <QApplication>
#include <QAbstractEventDispatcher>
#include <QtWidgets>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtCore/QString>
#include <QtGlobal>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QStyle>
#include <QString>
#include <QTimer>
#include <QLabel>
#include <QFileDialog>
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

#include "ui_qt.h"

#ifndef CXX_BUILD
extern "C" {
#endif

#include <file/file_path.h>
#include <file/archive_file.h>
#include <streams/file_stream.h>
#include <retro_timers.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>

#ifdef Q_OS_UNIX
#include <locale.h>
#endif

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../../core_info.h"
#include "../../command.h"
#include "../ui_companion_driver.h"
#include "../../configuration.h"
#include "../../frontend/frontend.h"
#include "../../frontend/frontend_driver.h"
#include "../../file_path_special.h"
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

#ifdef HAVE_WAYLAND
#include "../../gfx/common/wayland_common.h"
#endif

#ifndef CXX_BUILD
}
#endif

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

static QPixmap pixmapFromPathRA(const QString &path)
{
   QImage img = ThumbnailLoader::loadImageRA(path);
   if (!img.isNull())
      return QPixmap::fromImage(img);
   return QPixmap(path);
}

/* Give btn a default action whose text comes from the localization
 * tables, then size btn to fit. The action is parented to btn so
 * Qt cleans it up automatically; the explicit parent is needed
 * because QAction's default-NULL-parent overload only exists from
 * Qt 5.7 onwards. */
static void qt_button_set_action_label(QToolButton *btn,
      enum msg_hash_enums label)
{
   btn->setDefaultAction(new QAction(msg_hash_to_str(label), btn));
   btn->setFixedSize(btn->sizeHint());
}

/* Add dock to win in the area stored as the dock's "default_area"
 * dynamic property. The property is set elsewhere with a
 * Qt::DockWidgetArea value; this just unpacks and routes it.
 * Wraps a static_cast<Qt::DockWidgetArea>(... toInt()) idiom that
 * otherwise repeats verbatim at every dock-attach site. */
static void qt_dock_add_to(QMainWindow *win, QDockWidget *dock)
{
   win->addDockWidget(static_cast<Qt::DockWidgetArea>(
            dock->property("default_area").toInt()), dock);
}

/* Configure the four standard pieces of a QDockWidget in one call:
 * the QObject name (for QSettings save/restore), the default
 * docking area (read back later by qt_dock_add_to), the localized
 * menu text shown in the View menu, and the widget the dock
 * displays. Replaces a four-line setObjectName / setProperty /
 * setProperty / setWidget block that recurs at every dock site. */
static void qt_dock_configure(QDockWidget *dock,
      const char *object_name, Qt::DockWidgetArea default_area,
      enum msg_hash_enums menu_text, QWidget *widget)
{
   dock->setObjectName(object_name);
   dock->setProperty("default_area", default_area);
   dock->setProperty("menu_text", msg_hash_to_str(menu_text));
   dock->setWidget(widget);
}

/* %1 is a placeholder for palette(highlight) or the equivalent chosen by the user */
static const QString qt_theme_default_stylesheet = QString(R"(
   QPushButton[flat="true"] {
      min-height:20px;
      min-width:80px;
      padding:1px 3px 1px 3px;
      background-color: transparent;
      border: 1px solid #ddd;
   }
   ThumbnailWidget#thumbnailWidget, ThumbnailLabel#thumbnailGridLabel, QLabel#thumbnailQLabel {
      background-color:#d4d4d4;
   }
   QLabel#dropIndicator {
      font-size: 9pt;
      color: darkgrey;
      border: 2px dashed lightgrey;
      border-radius: 5px;
      margin: 20px;
   }
   ThumbnailWidget#thumbnailWidgetSelected {
      background-color:#d4d4d4;
      border:3px solid %1;
   }
   QFrame#playlistWidget, QFrame#browserWidget, QFrame#logWidget {
      padding: 8px;
   }
   QListWidget {
      icon-size: 32px;
   }
   /* color of the icons on the settings dialog */
   /* QLabel#iconColor {
      color: black;
   } */
)");

static const QString qt_theme_dark_stylesheet = QString(R"(
   QWidget {
      color:white;
      background-color:rgb(53,53,53);
      selection-background-color:%1;
   }
   QWidget:disabled {
      color:rgb(127,127,127);
   }
   QFrame#playlistWidget, QFrame#browserWidget, QStackedWidget#centralWidget, QFrame#logWidget {
      padding: 8px;
      background-color:rgb(66,66,66);
      border-top:1px solid rgba(175,175,175,50%);
      border-left:1px solid rgba(125,125,125,50%);
      border-right:1px solid rgba(125,125,125,50%);
      border-bottom:1px solid rgba(25,25,25,75%);
   }
   QListWidget {
      icon-size: 32px;
   }
   QLabel#dropIndicator {
      font-size: 9pt;
      color: #575757;
      border: 2px dashed #575757;
      border-radius: 5px;
      margin: 20px;
   }
   QTextEdit, LogTextEdit {
      background-color:rgb(25,25,25);
   }
   QSpinBox, QDoubleSpinBox, QCheckBox, QRadioButton {
      background-color:rgb(25,25,25);
   }
   QCheckBox:checked, QCheckBox:unchecked, QRadioButton:checked, QRadioButton:unchecked {
      background-color:transparent;
   }
   /* Groupboxes for the settings window, can be restricted later with ViewOptionsDialog QGroupBox */
   QGroupBox {
      background-color:rgba(80,80,80,50%);
      margin-top:27px;
      border:1px solid rgba(25,25,25,127);
      border-top-left-radius:0px;
      border-top-right-radius:4px;
   }
   QGroupBox::title {
      min-height:28px;
      subcontrol-origin:margin;
      subcontrol-position:left top;
      padding:4px 6px 5px 6px;
      margin-left:0px;
      background-color:qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgb(65,65,65),stop: 0.4 rgb(70,70,70),stop:1 rgb(90,90,90));
      border:1px solid rgba(25,25,25,127);
      border-bottom:1px solid rgb(65,65,65);
      border-top-left-radius:4px;
      border-top-right-radius:4px;
   }
   QGroupBox::indicator:checked {
      background-color:%1;
      border:4px solid rgb(45,45,45);
   }
   QGroupBox::indicator:unchecked {
      background-color:rgba(25,25,25,50%);
   }
   QGroupBox::indicator {
      width:16px;
      height:16px;
   }
   QWidget#shaderParamsWidget {
      background-color:rgb(25,25,25);
   }
   QDialog#shaderParamsDialog QGroupBox {
      background-color:rgb(53,53,53);
      border-top-left-radius:0px;
   }
   QDialog#shaderParamsDialog QGroupBox::title {
      margin-left:0px;
      min-height:28px;
      padding:4px 10px;
      background-color:qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgb(53,53,53),stop:1 rgba(125,125,125,127));
      border:1px solid rgba(25,25,25,75);
      border-top:1px solid rgba(175,175,175,50%);
      border-bottom:none transparent;
   }
   QToolTip {
      color:white;
      background-color:rgb(53,53,53);
      border:1px solid rgb(80,80,80);
      border-radius:4px;
   }
   QMenuBar {
      background-color:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
      border-bottom:2px solid rgba(25,25,25,75);
   }
   QMenuBar::item {
      spacing:2px;
      padding:3px 4px;
      background-color:transparent;
   }
   QMenuBar::item:selected {
      background-color:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 rgba(106,106,106,255),stop:1 rgba(106,106,106,75));
      border:1px solid %1;
   }
   QMenuBar::item:pressed {
      background-color:%1;
      border-left:1px solid rgba(25,25,25,127);
      border-right:1px solid rgba(25,25,25,127);
   }
   QMenu {
      background-color:rgb(45,45,45);
      border:1px solid palette(shadow);
   }
   QMenu::item {
      padding:3px 25px 3px 25px;
      border:1px solid transparent;
   }
   QMenu::item:disabled {
      color:rgb(127,127,127);
   }
   QMenu::item:selected {
      border-color:rgba(200,200,200,127);
      background-color:%1;
   }
   QMenu::icon:checked {
      background-color:qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
      border:1px solid %1;
      border-radius:2px;
   }
   QMenu::separator {
      height:1px;
      background-color:rgb(100,100,100);
      margin-left:5px;
      margin-right:5px;
   }
   QMenu::indicator {
      width:18px;
      height:18px;
   }
   QToolBar::top {
      background-color:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
      border-bottom:3px solid qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
   }
   QToolBar::bottom {
      background-color:qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
      border-top:3px solid qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
   }
   QToolBar::left {
      background-color:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
      border-right:3px solid qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
   }
   QToolBar::right {
      background-color:qlineargradient(x1:1,y1:0,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
      border-left:3px solid qlineargradient(x1:1,y1:0,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
   }
   QMainWindow {
      background-color:rgb(53,53,53);
   }
   QMainWindow::separator {
      width:6px;
      height:5px;
      padding:2px;
      background-color:rgba(25,25,25,50%);
   }
   QLineEdit {
      color:white;
      background-color:rgb(25,25,25);
   }
   QLineEdit::focus {
      border:1px solid %1;
      border-radius:3px;
      color:white;
      background-color:rgb(25,25,25);
   }
   QSplitter::handle:horizontal {
      width:10px;
   }
   QSplitter::handle:vertical {
      height:10px;
   }
   QMainWindow::separator:hover, QSplitter::handle:hover {
   }
   QDockWidget {
      font-family:"Segoe UI";
      font-size:9pt;
   }
   QDockWidget::title {
      padding:3px 4px;
      background-color:qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgba(25,25,25,175),stop:1 rgba(53,53,53,75));
      border:1px solid rgba(25,25,25,75);
      border-top:1px solid rgba(175,175,175,50%);
      border-bottom:1px solid rgba(25,25,25,127);
   }
   QDockWidget::close-button, QDockWidget::float-button {
      subcontrol-position:top right;
      subcontrol-origin:margin;
      position:absolute;
      top:3px;
      bottom:0px;
      width:20px;
      height:20px;
   }
   QDockWidget::close-button:hover, QDockWidget::float-button:hover {
      border:1px solid %1;
      border-radius:4px;
   }
   QDockWidget::close-button {
      right:3px;
   }
   QDockWidget::float-button {
      right:25px;
   }
   QTabWidget::pane {
      background-color:rgba(66,66,66,50%);
   }
   QTabWidget::tab-bar {
   }
   QTabBar {
      background-color:transparent;
      qproperty-drawBase:0;
      border-bottom:1px solid rgba(25,25,25,50%);
   }
   QTabBar::tab {
      padding:4px 6px;
      background-color:rgba(25,25,25,127);
      border:1px solid rgba(25,25,25,75);
   }
   QTabBar::tab:selected {
      background-color:rgb(66,66,66);
      border-bottom-color:rgba(66,66,66,75%);
   }
   QTabBar::tab:!selected {
      color:rgb(175,175,175);
   }
   QComboBox {
      min-height:20px;
      padding:1px 6px 1px 6px;
   }
   QComboBox::focus {
      background:qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 rgba(255,255,255,50), stop: 1 rgba(100,100,100,25));
      border:1px solid %1;
      border-radius:4px;
   }
   QComboBox::hover {
      background-color:qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 rgba(255,255,255,50), stop: 1 rgba(127,127,127,50));
      border:1px solid %1;
      border-radius:4px;
   }
   QComboBox::drop-down {
      background-color:transparent;
      width:0px;
   }
   QComboBox::selected:on, QComboBox::selected:off {
      background-color:%1;
   }
   QTabBar::tab:hover {
      color:white;
      background-color:%1;
   }
   QComboBox::separator {
      background-color:rgb(100,100,100);
      height:1px;
      margin-left:4px;
      margin-right:4px;
   }
   QCheckBox::indicator {
      width:18px;
      height:18px;
   }
   QPushButton {
      min-height:20px;
      min-width:80px;
      padding:1px 3px 1px 3px;
      outline:none;
   }
   QPushButton::disabled, QToolButton::disabled {
      color:grey;
      background-color:rgb(25,25,25);
   }
   QPushButton::focus, QToolButton::focus {
      background:qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 rgba(255,255,255,50), stop: 1 rgba(100,100,100,25));
      border:1px solid %1;
      border-radius:4px;
   }
   QPushButton::hover, QToolButton::hover {
      background-color:qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 rgba(255,255,255,75), stop: 1 rgba(100,100,100,50));
      border:1px solid %1;
      border-radius:4px;
   }
   QPushButton::pressed, QToolButton::pressed {
      background-color:transparent;
      border:1px solid %1;
      border-radius:4px;
   }
   QPushButton[flat="true"] {
      background-color:transparent;
   }
   QPushButton[flat="true"]::menu-indicator {
      position:relative;
      bottom:4px;
      right:4px;
   }
   QRadioButton::indicator {
      width:18px;
      height:18px;
   }
   QListWidget::item:selected, QTreeView::item:selected, QTableView::item:selected {
      color:white;
      background-color:%1;
   }
   QTreeView {
      background-color:rgb(25,25,25);
      selection-background-color:%1;
   }
   QTreeView::branch:selected {
      background-color:%1;
   }
   QTreeView::item:selected:disabled, QTableView::item:selected:disabled {
      background-color:rgb(80,80,80);
   }
   QTreeView::branch:open, QTreeView::branch:closed {
      background-color:solid;
   }
   QTableView, QListWidget {
      background-color:rgb(25,25,25);
   }
   QTreeView QHeaderView::section, QTableView QHeaderView::section {
      /*height:24px;*/
      background-color:qlineargradient(x1:0,y1:1,x2:0,y2:0,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
      border-style:none;
      border-bottom:1px solid rgb(65,65,65);
      padding-left:5px;
      padding-right:5px;
   }
   QTableView {
      background-color:rgb(25,25,25);
      alternate-background-color:rgb(40,40,40);
   }
   QScrollBar:vertical, QScrollBar:horizontal {
      background-color:rgb(35,35,35);
   }
   QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
      background-color:rgb(65,65,65);
      border-right:1px solid rgba(175,175,175,50%);
      border-top:1px solid rgba(175,175,175,50%);
      border-bottom:1px solid rgba(25,25,25,75);
      border-radius:2px;
   }
   QScrollBar::handle:horizontal:hover, QScrollBar::handle:vertical:hover {
      border:1px solid %1;
      background-color:qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 rgba(255,255,255,75), stop: 1 rgba(127,127,127,75));
   }
   QScrollBar:vertical {
      border-top-right-radius:2px;
      border-bottom-right-radius:2px;
      width:16px;
      margin:0px;
   }
   QScrollBar::handle:vertical {
      min-height:20px;
      margin:2px 4px 2px 4px;
   }
   QScrollBar::add-line:vertical {
      background:none;
      height:0px;
      subcontrol-position:right;
      subcontrol-origin:margin;
   }
   QScrollBar::sub-line:vertical {
      background:none;
      height:0px;
      subcontrol-position:left;
      subcontrol-origin:margin;
   }
   QScrollBar:horizontal {
      height:16px;
      margin:0px;
   }
   QScrollBar::handle:horizontal {
      min-width:20px;
      margin:4px 2px 4px 2px;
   }
   QScrollBar::add-line:horizontal {
      background:none;
      width:0px;
      subcontrol-position:bottom;
      subcontrol-origin:margin;
   }
   QScrollBar::sub-line:horizontal {
      background:none;
      width:0px;
      subcontrol-position:top;
      subcontrol-origin:margin;
   }
   QSlider {
      background:transparent;
   }
   QSlider::sub-page {
      background:%1;
   }
   QSlider::groove:vertical {
      width:3px;
      background:rgb(25,25,25);
   }
   QSlider::handle:vertical {
      background:qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 rgb(175,175,175), stop: 1 rgb(75,75,75));
      border:1px solid rgb(35,35,35);
      border-radius:2px;
      height:16px;
      margin:0 -4px;
   }
   QSlider::handle:vertical:hover {
      background-color:qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 rgb(200,200,200), stop: 1 rgba(100,100,100));
      border:1px solid %1;
      border-radius:2px;
      height:16px;
      margin:0 -4px;
   }
   QSlider::groove:horizontal {
      height:3px;
      background:rgb(25,25,25);
   }
   QSlider::handle:horizontal {
      background:qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 rgb(175,175,175), stop: 1 rgb(75,75,75));
      border:1px solid rgb(35,35,35);
      border-radius:2px;
      width:16px;
      margin:-4px 0;
   }
   QSlider::handle:horizontal:hover {
      background-color:qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 rgb(200,200,200), stop: 1 rgba(100,100,100));
      border:1px solid %1;
      border-radius:2px;
      width:16px;
      margin:-4px 0;
   }
   QStatusBar {
      color:white;
      background-color:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 rgba(25,25,25,127),stop:1 rgba(53,53,53,75));
   }
   QStatusBar QLabel {
      background-color:transparent;
   }
   QLabel {
      background-color:transparent;
   }
   QSizeGrip {
      background-color:solid;
   }
   GridView::item {
      background-color:rgb(40,40,40);
   }
   GridView::item:selected {
      border:3px solid %1;
   }
   GridView {
      background-color:rgb(25,25,25);
      selection-color: white;
      qproperty-layout: "fixed";
   }
   GridItem {
      qproperty-thumbnailvalign: "center";
   }
   QLabel#itemsCountLabel {
      padding-left: 5px;
   }
)");

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

static unsigned char invader_png[] = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
  0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x7d, 0x00, 0x00, 0x00, 0x44,
  0x08, 0x06, 0x00, 0x00, 0x00, 0xac, 0xf5, 0x3a, 0x40, 0x00, 0x00, 0x00,
  0x19, 0x74, 0x45, 0x58, 0x74, 0x53, 0x6f, 0x66, 0x74, 0x77, 0x61, 0x72,
  0x65, 0x00, 0x41, 0x64, 0x6f, 0x62, 0x65, 0x20, 0x49, 0x6d, 0x61, 0x67,
  0x65, 0x52, 0x65, 0x61, 0x64, 0x79, 0x71, 0xc9, 0x65, 0x3c, 0x00, 0x00,
  0x0f, 0x4a, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0xec, 0x5d, 0x79, 0x50,
  0x95, 0xd7, 0x15, 0xbf, 0x6f, 0xe3, 0xf1, 0x58, 0xa2, 0x28, 0x8a, 0xec,
  0x82, 0x58, 0x8c, 0xc6, 0x25, 0xa8, 0xc4, 0xa5, 0x21, 0x1a, 0x97, 0xd6,
  0x50, 0x26, 0xd5, 0x6a, 0x3b, 0x6d, 0x4d, 0xeb, 0x42, 0x27, 0x13, 0xd3,
  0x24, 0xda, 0x31, 0xa6, 0x63, 0xa7, 0xda, 0x3f, 0xda, 0xce, 0x44, 0x27,
  0xc6, 0x24, 0xd3, 0x4c, 0x26, 0xea, 0x38, 0x4d, 0xc6, 0x4e, 0x4d, 0x62,
  0x27, 0x13, 0x71, 0xa3, 0x46, 0x31, 0x46, 0x4c, 0x80, 0x04, 0x0b, 0xc4,
  0x22, 0x8a, 0x82, 0x86, 0x25, 0x8a, 0x02, 0x0a, 0xc8, 0xe3, 0x6d, 0x3d,
  0x3f, 0x38, 0xcf, 0x12, 0xf3, 0xbe, 0xe5, 0xc1, 0x7b, 0xbc, 0x4f, 0x7c,
  0x67, 0xe6, 0x37, 0xe3, 0x72, 0xdf, 0xf7, 0xdd, 0xef, 0xfc, 0xee, 0x3d,
  0xf7, 0x9c, 0x73, 0x37, 0x9d, 0xcb, 0xe5, 0x12, 0x1a, 0x12, 0x3d, 0x61,
  0x24, 0x21, 0x96, 0x60, 0x50, 0x28, 0xdb, 0x45, 0xa8, 0x23, 0x5c, 0x0f,
  0x40, 0x3d, 0x43, 0x08, 0xc9, 0x84, 0x07, 0x08, 0x3a, 0x85, 0xb2, 0x6d,
  0x84, 0xcb, 0x84, 0x0e, 0xad, 0x28, 0xd9, 0x28, 0xb4, 0x25, 0xe1, 0x84,
  0x25, 0x84, 0xe5, 0x84, 0x30, 0x15, 0xca, 0x2c, 0x20, 0xec, 0x26, 0x54,
  0x0f, 0x30, 0xe1, 0x8b, 0x08, 0x2b, 0x98, 0x78, 0xbd, 0x4c, 0x59, 0xf4,
  0xa8, 0x72, 0xc2, 0x36, 0x42, 0x19, 0xc1, 0x19, 0x24, 0xfd, 0xbb, 0x82,
  0xde, 0xd0, 0xcc, 0xbd, 0x3d, 0xa5, 0xa9, 0xa9, 0x49, 0x77, 0xfb, 0xf6,
  0x6d, 0x8f, 0x05, 0x87, 0x0e, 0x1d, 0xea, 0x8a, 0x8c, 0x8c, 0x4c, 0xe5,
  0x86, 0xf2, 0x0a, 0xe1, 0xeb, 0x01, 0xd2, 0xd7, 0x42, 0xc2, 0xf3, 0x84,
  0x99, 0xf5, 0xf5, 0xf5, 0xa1, 0x0e, 0x87, 0xc3, 0x63, 0xc1, 0x51, 0xa3,
  0x46, 0x09, 0x93, 0xc9, 0xd4, 0x4e, 0x7f, 0x2c, 0x21, 0xdc, 0xd0, 0x0a,
  0xe1, 0x3d, 0x4d, 0x91, 0xcc, 0xbb, 0xc6, 0x30, 0x9c, 0xb0, 0x8d, 0x70,
  0x35, 0x3f, 0x3f, 0xdf, 0x49, 0x8a, 0x73, 0x71, 0x8f, 0xf9, 0x16, 0xe2,
  0xe3, 0xe3, 0x5d, 0xe7, 0xcf, 0x9f, 0x77, 0x50, 0xb9, 0xcb, 0x84, 0xdf,
  0x13, 0x22, 0xfc, 0x5c, 0x2f, 0x1d, 0x61, 0x21, 0xe1, 0xa8, 0xd3, 0xe9,
  0x6c, 0xdf, 0xb8, 0x71, 0xa3, 0xc7, 0x7a, 0x01, 0x4b, 0x97, 0x2e, 0x75,
  0x51, 0x19, 0x2b, 0x95, 0x3d, 0x41, 0x98, 0xc9, 0xbf, 0xd5, 0x8c, 0x8e,
  0xb5, 0x48, 0x3a, 0x30, 0x9e, 0xf0, 0x4f, 0x42, 0xeb, 0x8e, 0x1d, 0x3b,
  0x9c, 0x52, 0xca, 0x9d, 0x32, 0x65, 0x8a, 0xab, 0xb3, 0xb3, 0xd3, 0x4e,
  0xe5, 0xbe, 0x22, 0x2c, 0x27, 0x18, 0xfd, 0x58, 0xa7, 0x34, 0xae, 0x53,
  0xfb, 0x9b, 0x6f, 0xbe, 0x29, 0x59, 0xa7, 0xd1, 0xa3, 0x47, 0xbb, 0x9a,
  0x9b, 0x9b, 0x51, 0xa7, 0xb3, 0x84, 0x95, 0x04, 0x83, 0xd6, 0xf4, 0xab,
  0x55, 0xd2, 0xa1, 0xa8, 0x2c, 0xc2, 0x01, 0xc2, 0xed, 0x67, 0x9f, 0x7d,
  0x56, 0xb2, 0x57, 0xcd, 0x9f, 0x3f, 0xdf, 0xd5, 0xd6, 0xd6, 0xd6, 0xc5,
  0xbd, 0xea, 0x07, 0x7e, 0x22, 0x3e, 0x81, 0xad, 0x4f, 0xd3, 0xc1, 0x83,
  0x07, 0x9d, 0x61, 0x61, 0x61, 0x1e, 0xeb, 0x12, 0x17, 0x17, 0xe7, 0xaa,
  0xae, 0xae, 0x76, 0x52, 0xb9, 0x1a, 0xc2, 0x1f, 0xd9, 0x6a, 0x89, 0x20,
  0xe9, 0xea, 0xa1, 0x27, 0x2c, 0x25, 0x94, 0x50, 0x6f, 0xee, 0x4a, 0x4f,
  0x4f, 0x97, 0x24, 0x3e, 0x37, 0x37, 0x97, 0x8a, 0xb9, 0xda, 0x08, 0x1f,
  0x11, 0xbe, 0xef, 0xe3, 0x7a, 0x0c, 0x23, 0xfc, 0x01, 0xc3, 0x48, 0x71,
  0x71, 0xb1, 0xe4, 0x70, 0x03, 0x1c, 0x38, 0x70, 0x00, 0x84, 0xdf, 0x20,
  0xfc, 0x8d, 0x10, 0xa7, 0x55, 0xdd, 0x6a, 0x99, 0x74, 0x20, 0x92, 0xf0,
  0x5b, 0x42, 0x55, 0x49, 0x49, 0x89, 0x23, 0x22, 0x22, 0xc2, 0xa3, 0xb2,
  0x75, 0x3a, 0x9d, 0x6b, 0xf7, 0xee, 0xdd, 0x50, 0xf8, 0x4d, 0xc2, 0x5e,
  0xc2, 0x18, 0x1f, 0xbd, 0xdf, 0x42, 0x78, 0x1e, 0xa6, 0xba, 0xb1, 0xb1,
  0xd1, 0x96, 0x9a, 0x9a, 0x2a, 0x49, 0x38, 0xac, 0x11, 0xbf, 0xff, 0x1f,
  0x84, 0x87, 0xb5, 0xac, 0x57, 0xad, 0x93, 0x0e, 0x3c, 0xc0, 0x8e, 0xda,
  0xd5, 0xa2, 0xa2, 0x22, 0xd7, 0x90, 0x21, 0x43, 0x3c, 0x2a, 0x1d, 0x3d,
  0xf0, 0xdd, 0x77, 0xdf, 0x85, 0xe2, 0x9b, 0x09, 0xaf, 0x13, 0x52, 0xfb,
  0xf9, 0xde, 0x10, 0xc2, 0x2f, 0x08, 0xe7, 0x5a, 0x5b, 0x5b, 0xed, 0x99,
  0x99, 0x99, 0x92, 0x84, 0x2f, 0x5b, 0xb6, 0xcc, 0xd5, 0xd5, 0xd5, 0x65,
  0xe3, 0xe1, 0x28, 0x53, 0x6b, 0x8e, 0xdb, 0xbd, 0x48, 0xba, 0x60, 0x02,
  0x3f, 0x21, 0x38, 0x76, 0xee, 0xdc, 0x29, 0xa9, 0x7c, 0xf4, 0x78, 0x78,
  0xfc, 0x54, 0xae, 0x9e, 0xb0, 0x95, 0x10, 0xdb, 0xc7, 0xf7, 0xc1, 0x2f,
  0xc8, 0x26, 0x1c, 0xb3, 0x5a, 0xad, 0x5d, 0xb3, 0x67, 0xcf, 0x96, 0x7c,
  0xe7, 0xb4, 0x69, 0xd3, 0x5c, 0x36, 0x9b, 0xcd, 0xdd, 0xd8, 0x9e, 0xd1,
  0x3a, 0xe1, 0x80, 0x4e, 0x45, 0x46, 0x0e, 0xb1, 0x69, 0x34, 0x61, 0x98,
  0x42, 0x96, 0x0c, 0x31, 0x69, 0x23, 0xe1, 0x36, 0x2b, 0x44, 0x4a, 0x90,
  0xc5, 0x1a, 0x45, 0x30, 0x7b, 0x11, 0x59, 0xc6, 0x11, 0xfe, 0x4c, 0xc8,
  0xc0, 0x58, 0x4f, 0x3d, 0x4b, 0xec, 0xdb, 0xb7, 0x4f, 0x32, 0x3e, 0xfe,
  0xec, 0xb3, 0xcf, 0x9c, 0xc9, 0xc9, 0xc9, 0xc8, 0x82, 0xed, 0x21, 0xec,
  0xf7, 0x32, 0x1b, 0x86, 0x6f, 0x9c, 0xcc, 0xc9, 0x97, 0x4c, 0xf2, 0x17,
  0xc2, 0x76, 0xed, 0xda, 0xe5, 0xb1, 0x20, 0x59, 0x1d, 0xf1, 0xf9, 0xe7,
  0x9f, 0x0b, 0xf2, 0x37, 0xf0, 0xd7, 0x5b, 0x9c, 0x84, 0xd9, 0xcf, 0xd9,
  0x42, 0x35, 0x82, 0x28, 0xa0, 0x95, 0xf0, 0x0d, 0xc1, 0xa6, 0xc0, 0x41,
  0x0c, 0x21, 0x4a, 0x21, 0x03, 0xe8, 0xe6, 0x40, 0xf6, 0x7b, 0xd5, 0x90,
  0x8e, 0x04, 0xc8, 0x6a, 0xc2, 0x0c, 0x82, 0x49, 0xa6, 0x5c, 0x0d, 0xe1,
  0x2d, 0x42, 0x11, 0xc1, 0x2e, 0x51, 0x26, 0x94, 0xf0, 0x24, 0xe1, 0xe7,
  0xdc, 0x88, 0xd4, 0x88, 0x8e, 0x1b, 0xca, 0x18, 0x4e, 0xc4, 0x88, 0x96,
  0x96, 0x16, 0x41, 0xe1, 0x9a, 0xa8, 0xad, 0xad, 0xf5, 0xf8, 0x83, 0x47,
  0x1e, 0x79, 0x44, 0x7c, 0xfa, 0xe9, 0xa7, 0x4e, 0xa3, 0xd1, 0x88, 0xa4,
  0xc8, 0x25, 0x42, 0xa7, 0x97, 0x09, 0x18, 0xa4, 0x81, 0xe3, 0xc8, 0x4f,
  0x30, 0xad, 0x5a, 0xb5, 0x4a, 0x52, 0xc9, 0x68, 0x0c, 0xf4, 0xff, 0xee,
  0xbf, 0x3a, 0x58, 0xe1, 0x57, 0x14, 0x08, 0xbc, 0x9b, 0xf4, 0x33, 0x84,
  0xb7, 0x09, 0x67, 0x65, 0xca, 0xa1, 0x3e, 0xcf, 0x10, 0xb2, 0x14, 0x32,
  0x80, 0x48, 0x50, 0xbd, 0x43, 0x38, 0x26, 0xd7, 0xf0, 0x8c, 0x2a, 0x5a,
  0x3d, 0xc8, 0x5e, 0x56, 0x51, 0x51, 0x91, 0x52, 0x53, 0x53, 0xe3, 0x51,
  0x01, 0xd4, 0xab, 0xc4, 0xc4, 0x89, 0x13, 0x13, 0xe8, 0x8f, 0x87, 0x99,
  0x74, 0x29, 0x49, 0x02, 0xe1, 0xed, 0xed, 0xed, 0x0b, 0x8f, 0x1f, 0x3f,
  0x1e, 0xe2, 0x05, 0x11, 0xba, 0xbb, 0x3f, 0x76, 0xed, 0xda, 0xb5, 0xe2,
  0xc5, 0x17, 0x5f, 0x14, 0x76, 0xfb, 0x77, 0xdb, 0x17, 0x7a, 0xdf, 0xe2,
  0xc5, 0x8b, 0xf5, 0x4f, 0x3f, 0xfd, 0xb4, 0xdb, 0x42, 0xb9, 0xbc, 0x7c,
  0x97, 0xee, 0xda, 0xb5, 0x6b, 0xba, 0x17, 0x5e, 0x78, 0x41, 0xb2, 0x50,
  0x46, 0x46, 0x86, 0x18, 0x31, 0x62, 0x84, 0xc8, 0xcb, 0xcb, 0xeb, 0xad,
  0xab, 0x38, 0x26, 0x48, 0xd5, 0xfb, 0x62, 0x62, 0x62, 0xc4, 0xf4, 0xe9,
  0xd3, 0x53, 0x39, 0x8d, 0x0c, 0x58, 0x25, 0x8a, 0x5a, 0x08, 0x53, 0x6e,
  0xdd, 0xba, 0x35, 0xeb, 0xc4, 0x89, 0x13, 0x92, 0xa4, 0x2f, 0x58, 0xb0,
  0xa0, 0xdd, 0x6c, 0x36, 0x83, 0xec, 0xaf, 0xb8, 0xf1, 0xf5, 0x29, 0x23,
  0x07, 0xef, 0xf9, 0x2f, 0x84, 0x96, 0xac, 0xac, 0x2c, 0xc9, 0x71, 0x6d,
  0xcb, 0x96, 0x2d, 0x18, 0x47, 0xff, 0x43, 0x98, 0xab, 0xf0, 0xbc, 0x1f,
  0x11, 0x2a, 0xc9, 0x34, 0x4b, 0x26, 0x37, 0xee, 0x27, 0xa4, 0xa4, 0xa4,
  0xc0, 0x17, 0xe8, 0x20, 0xec, 0x24, 0x8c, 0x90, 0xd1, 0x5b, 0x14, 0xe1,
  0xed, 0x9b, 0x37, 0x6f, 0x5a, 0xf5, 0x7a, 0xbd, 0xe4, 0xf3, 0xc8, 0xba,
  0x21, 0x43, 0x59, 0xc2, 0x59, 0x40, 0x49, 0x1e, 0xf4, 0x0a, 0x8d, 0x11,
  0xbd, 0x64, 0x1c, 0xb5, 0x30, 0x0b, 0x8d, 0x93, 0x92, 0x85, 0x66, 0xcc,
  0x98, 0xe1, 0xe2, 0x96, 0x55, 0x2b, 0xf3, 0x2c, 0x98, 0xf6, 0x71, 0x84,
  0xe1, 0x07, 0x0f, 0x1e, 0xd4, 0x89, 0xa0, 0x88, 0x4b, 0x97, 0x2e, 0x61,
  0x88, 0x82, 0xc5, 0x4b, 0x63, 0x3f, 0x47, 0x4a, 0xe0, 0x2f, 0x9c, 0x8f,
  0x8c, 0x8c, 0xb4, 0xce, 0x9a, 0x35, 0x4b, 0xb2, 0x10, 0x71, 0xa4, 0xe7,
  0xe7, 0x8c, 0x55, 0x9a, 0xca, 0x94, 0x93, 0xd1, 0xa8, 0x10, 0xb5, 0x20,
  0x23, 0x85, 0x24, 0x1e, 0x0b, 0x50, 0x45, 0x30, 0x86, 0xe2, 0x3f, 0xff,
  0xcb, 0x0e, 0x89, 0x94, 0xc0, 0x09, 0x99, 0xe4, 0x70, 0x38, 0x22, 0x3e,
  0xfc, 0xf0, 0xc3, 0x20, 0xe3, 0x2c, 0x05, 0x05, 0x05, 0x06, 0x9e, 0xad,
  0xfb, 0x9e, 0x4c, 0x31, 0x8c, 0x61, 0x15, 0x98, 0x8c, 0x82, 0x13, 0x2b,
  0x25, 0x64, 0xfa, 0xbb, 0xe7, 0xa2, 0x08, 0x13, 0xb9, 0x93, 0xf5, 0x89,
  0x74, 0xb4, 0x98, 0x91, 0xdc, 0x82, 0x3c, 0xca, 0xdc, 0xb9, 0x73, 0x45,
  0x48, 0x48, 0x08, 0xbc, 0xc6, 0x73, 0x0a, 0x5e, 0x23, 0xbc, 0xcf, 0xd4,
  0xb3, 0x67, 0xcf, 0x9a, 0xae, 0x5f, 0xbf, 0x1e, 0x64, 0xfb, 0xff, 0xbd,
  0xd3, 0x4d, 0x54, 0xba, 0x42, 0x74, 0x04, 0x87, 0xb4, 0xf1, 0x89, 0x27,
  0x9e, 0x70, 0xca, 0x3d, 0x8b, 0xcc, 0xb7, 0x99, 0x9d, 0xde, 0xc8, 0xbe,
  0x90, 0x6e, 0xe1, 0x8a, 0x44, 0xc2, 0x31, 0x92, 0x92, 0xec, 0xec, 0x6c,
  0xc1, 0xd3, 0xa1, 0xe7, 0x15, 0x1c, 0x18, 0x98, 0xb0, 0x78, 0xaa, 0x98,
  0x21, 0x48, 0xf5, 0xb7, 0x9d, 0x4e, 0x5e, 0x3b, 0x80, 0x9e, 0x1e, 0x21,
  0x53, 0xb4, 0x09, 0xc3, 0x67, 0x5a, 0x5a, 0x9a, 0x23, 0x31, 0x31, 0xd1,
  0x63, 0x01, 0x72, 0x3e, 0x45, 0x59, 0x59, 0x99, 0xdb, 0x72, 0x8c, 0xea,
  0x0b, 0xe9, 0x68, 0x29, 0x48, 0x67, 0x9a, 0xe5, 0x48, 0xe7, 0x96, 0x57,
  0x2f, 0xeb, 0x2d, 0xf6, 0xb4, 0x60, 0x7c, 0xd4, 0x50, 0x39, 0xdf, 0xe0,
  0x7e, 0x14, 0x22, 0x49, 0x90, 0x83, 0x86, 0x28, 0x2a, 0x45, 0x21, 0x8c,
  0xbd, 0xc9, 0x1d, 0xcb, 0x8a, 0x90, 0x54, 0x4a, 0xd8, 0x5f, 0x8a, 0xe1,
  0xa1, 0xd9, 0x6b, 0xd2, 0xd1, 0x52, 0x92, 0x8b, 0x8b, 0x8b, 0x0d, 0x88,
  0x8b, 0x3d, 0xc9, 0x98, 0x31, 0x63, 0x44, 0x42, 0x42, 0x82, 0x9d, 0x4d,
  0xcf, 0x0d, 0x99, 0x67, 0x45, 0x30, 0xe9, 0x61, 0x72, 0x0d, 0xe8, 0x7e,
  0x14, 0x2c, 0xc2, 0xc8, 0xcf, 0xcf, 0xd7, 0x73, 0xb8, 0x97, 0x28, 0x53,
  0xb4, 0x8b, 0x87, 0xd0, 0x56, 0x72, 0x9c, 0xe5, 0x48, 0x77, 0x27, 0xc0,
  0xc6, 0x49, 0x85, 0xe4, 0x46, 0x05, 0x27, 0x2e, 0xe6, 0xc0, 0x81, 0x03,
  0x3a, 0x19, 0xaf, 0xdd, 0xbd, 0xda, 0xa5, 0x4a, 0xf4, 0x2c, 0x5f, 0x92,
  0x92, 0xe1, 0xbc, 0x12, 0xc6, 0x48, 0x63, 0xba, 0xac, 0x12, 0x28, 0xd6,
  0x14, 0x7b, 0xf7, 0xee, 0x15, 0x3c, 0x54, 0x38, 0x65, 0x86, 0x0c, 0x28,
  0x4a, 0xdf, 0xdc, 0xdc, 0xdc, 0xdd, 0xf8, 0x06, 0x4a, 0x96, 0x2c, 0x59,
  0x22, 0x76, 0xee, 0xdc, 0xa9, 0xba, 0x7e, 0xc7, 0x8e, 0x1d, 0x13, 0x4b,
  0x97, 0x2e, 0x95, 0x7d, 0x26, 0xe9, 0x18, 0x65, 0xa2, 0xd8, 0x87, 0xfa,
  0x44, 0xa6, 0x28, 0x7a, 0xfa, 0x37, 0xa4, 0xf7, 0x58, 0xa9, 0x0e, 0x5b,
  0x58, 0x58, 0x28, 0x28, 0xda, 0x32, 0x93, 0x83, 0x3d, 0x96, 0xad, 0x75,
  0xb3, 0x5a, 0xd2, 0x8d, 0x3c, 0x9e, 0x3f, 0xc0, 0x2d, 0x47, 0x8e, 0xf4,
  0x16, 0x26, 0xdd, 0x2e, 0x53, 0x59, 0xb4, 0xe0, 0xb8, 0xc3, 0x87, 0x0f,
  0xeb, 0x95, 0x32, 0x80, 0x26, 0x93, 0x49, 0x44, 0x45, 0x45, 0x41, 0x99,
  0xf0, 0xf6, 0xca, 0x25, 0x2c, 0x88, 0x89, 0xc7, 0xad, 0x74, 0xcc, 0x84,
  0x0d, 0x64, 0xcf, 0x24, 0xa7, 0xd5, 0xab, 0xfa, 0x91, 0xf2, 0x15, 0xeb,
  0x77, 0xe4, 0xc8, 0x11, 0xe1, 0x74, 0x3a, 0xc3, 0x29, 0x06, 0x1f, 0xc7,
  0xbe, 0xd4, 0x6d, 0x89, 0xa2, 0x18, 0x46, 0x6b, 0x32, 0x32, 0x32, 0x1e,
  0x32, 0x9b, 0xcd, 0x21, 0x56, 0xeb, 0x77, 0x73, 0x39, 0xf4, 0x1c, 0x51,
  0x54, 0x54, 0x64, 0x9c, 0x37, 0x6f, 0xde, 0x18, 0xb6, 0xd6, 0xaa, 0x49,
  0x47, 0xab, 0x9b, 0xd0, 0xd0, 0xd0, 0x10, 0xfa, 0xc5, 0x17, 0x5f, 0x48,
  0x56, 0xf6, 0xb1, 0xc7, 0x1e, 0x73, 0x70, 0x6c, 0x5e, 0xa5, 0x10, 0x16,
  0x4e, 0x20, 0x44, 0xbf, 0xff, 0xfe, 0xfb, 0x6a, 0x75, 0x8b, 0x68, 0xe0,
  0x03, 0xc2, 0x6b, 0x12, 0x4a, 0x85, 0x8f, 0x90, 0x41, 0xd8, 0x4c, 0x98,
  0xa6, 0x22, 0x0a, 0xf1, 0xb5, 0xb8, 0xeb, 0xf7, 0xba, 0xf0, 0xbc, 0x1a,
  0x17, 0xf5, 0x7b, 0x98, 0xf0, 0x27, 0xc2, 0x74, 0xa1, 0xb0, 0x62, 0x96,
  0xf4, 0x2c, 0xca, 0xcb, 0xcb, 0x43, 0x26, 0x4f, 0x9e, 0xfc, 0x20, 0x8f,
  0xc7, 0x35, 0x32, 0xce, 0x5c, 0xb9, 0xc5, 0x62, 0x79, 0x3c, 0x33, 0x33,
  0x33, 0xe4, 0xe4, 0xc9, 0x93, 0x52, 0x61, 0xa0, 0x9e, 0x48, 0x47, 0xf6,
  0x13, 0xcf, 0xab, 0xbc, 0xdb, 0x1a, 0xe9, 0x65, 0xc6, 0xf3, 0x14, 0x78,
  0xda, 0x52, 0x3d, 0x33, 0x22, 0x22, 0x42, 0x4c, 0x98, 0x30, 0xc1, 0xc6,
  0xa4, 0x5f, 0x95, 0xf9, 0x26, 0xe4, 0xcb, 0xd3, 0x6d, 0x36, 0x9b, 0xe5,
  0xe8, 0xd1, 0xa3, 0x6a, 0x95, 0xda, 0xc1, 0xa9, 0x44, 0xa4, 0x26, 0xaf,
  0x79, 0x00, 0x72, 0xdc, 0x58, 0x5d, 0xfa, 0xb5, 0x08, 0xcc, 0x82, 0x43,
  0x9f, 0xd7, 0x8f, 0x74, 0xad, 0x63, 0x8b, 0x38, 0x5a, 0xe1, 0xbd, 0x18,
  0xd7, 0x5b, 0xe4, 0x9c, 0x39, 0x76, 0x96, 0xa3, 0xd8, 0x5a, 0x87, 0xaa,
  0x71, 0xe4, 0xf4, 0x3c, 0xb6, 0xc4, 0x91, 0xd3, 0x25, 0xd9, 0x83, 0xf0,
  0x52, 0x32, 0x47, 0x3a, 0x6e, 0xd5, 0x36, 0x6e, 0xcd, 0x77, 0x03, 0xbf,
  0x9f, 0x84, 0xde, 0x78, 0xe6, 0xcc, 0x99, 0x90, 0x8e, 0x0e, 0xaf, 0x96,
  0x7e, 0x3b, 0x14, 0xfe, 0xdf, 0x25, 0x02, 0xbb, 0xc2, 0xd4, 0xa7, 0xf5,
  0x23, 0x5d, 0xeb, 0xb8, 0xb3, 0xcd, 0xe6, 0x10, 0x4e, 0x27, 0x01, 0xd8,
  0x74, 0xa7, 0x9c, 0x33, 0x57, 0x52, 0x52, 0x02, 0x07, 0x31, 0x9c, 0x7b,
  0xfa, 0x50, 0x35, 0xe6, 0x3d, 0x94, 0x5b, 0x48, 0x94, 0x5c, 0x78, 0xc5,
  0x2d, 0xcd, 0xc8, 0x66, 0xf6, 0x77, 0x1c, 0xb2, 0xdd, 0x6d, 0x16, 0x86,
  0x20, 0x7f, 0x03, 0xe2, 0xb9, 0x25, 0x07, 0x45, 0x42, 0x4e, 0x9f, 0x3e,
  0xed, 0xee, 0x9d, 0xcb, 0x98, 0x83, 0x3a, 0x09, 0x7d, 0xce, 0xc3, 0x10,
  0x20, 0x47, 0x3a, 0xa2, 0xad, 0xca, 0xca, 0x4a, 0x13, 0x59, 0xe2, 0x54,
  0x1e, 0x2e, 0x1a, 0x94, 0x48, 0xc7, 0x83, 0x1f, 0x24, 0x73, 0x1c, 0x2e,
  0x37, 0x9e, 0xf3, 0x4b, 0x0d, 0x6c, 0x15, 0x9e, 0x11, 0x9e, 0xa7, 0xf2,
  0x0c, 0xec, 0x41, 0x86, 0x04, 0x43, 0x35, 0x79, 0xa9, 0xaa, 0xaa, 0x12,
  0x14, 0x89, 0x98, 0xc8, 0x49, 0x84, 0xff, 0x93, 0xa4, 0xa4, 0xcf, 0xf8,
  0xf8, 0xf8, 0xee, 0x79, 0xfc, 0x73, 0xe7, 0xce, 0x49, 0x99, 0x78, 0x03,
  0x91, 0x1e, 0xcf, 0xd9, 0xb9, 0x33, 0x4a, 0xa4, 0xc3, 0xc4, 0xa4, 0x56,
  0x54, 0x54, 0x98, 0xda, 0xda, 0xa4, 0xa3, 0xb0, 0xe3, 0xc7, 0x8f, 0x0b,
  0x32, 0xd9, 0xee, 0x67, 0x44, 0x29, 0x7d, 0xd4, 0xc7, 0x1f, 0x7f, 0x1c,
  0x64, 0x56, 0x46, 0xe0, 0x75, 0x6f, 0xd8, 0xb0, 0x41, 0x24, 0x25, 0x25,
  0xa9, 0xd2, 0x67, 0xb7, 0xb3, 0x14, 0x1e, 0x2e, 0x3b, 0xae, 0xaf, 0x5e,
  0xbd, 0xba, 0x77, 0x7a, 0xd7, 0x21, 0x47, 0x3a, 0x5a, 0x86, 0x62, 0xba,
  0xf4, 0xd5, 0x57, 0x5f, 0xf5, 0xb7, 0x1e, 0xb4, 0x3e, 0x1c, 0xf8, 0xbc,
  0x7e, 0x1c, 0xff, 0xfb, 0x44, 0x3c, 0xa4, 0x77, 0x5b, 0xa5, 0x1c, 0x39,
  0x4c, 0xf3, 0x3d, 0x84, 0x74, 0x20, 0xcf, 0xd8, 0x04, 0x4a, 0x50, 0x8f,
  0x78, 0xf6, 0xfc, 0x8d, 0x1e, 0x60, 0xe2, 0xb1, 0x6a, 0x58, 0x00, 0xc2,
  0xb5, 0xde, 0xf5, 0x8b, 0x90, 0xa9, 0xdf, 0x48, 0xae, 0x5f, 0x40, 0x1a,
  0x2f, 0x59, 0x6a, 0x51, 0x57, 0x57, 0x67, 0xe4, 0xe1, 0x77, 0xa4, 0x9c,
  0x79, 0x0f, 0xe7, 0x42, 0xa1, 0x01, 0xce, 0x91, 0x43, 0x99, 0x39, 0xec,
  0xc8, 0x78, 0x72, 0x68, 0xcc, 0xec, 0x40, 0x4e, 0x0c, 0x90, 0x52, 0xbd,
  0xa9, 0x5f, 0x20, 0x1a, 0x65, 0xf7, 0x62, 0x89, 0x43, 0x87, 0x0e, 0xe9,
  0x73, 0x73, 0x73, 0x63, 0xd9, 0x47, 0x38, 0x2f, 0x45, 0xfa, 0x08, 0xc4,
  0x89, 0x17, 0x2f, 0x5e, 0x34, 0x5e, 0xbe, 0x7c, 0x39, 0x90, 0xa4, 0xa3,
  0xa7, 0x8c, 0xe7, 0xb8, 0xb5, 0x4b, 0x22, 0xac, 0xb4, 0xf4, 0x0a, 0x6d,
  0xb4, 0x5c, 0xbf, 0x80, 0x09, 0xb2, 0xa9, 0x44, 0xba, 0x3b, 0x5e, 0x3f,
  0xee, 0x0e, 0x21, 0xef, 0x26, 0x1d, 0x2d, 0x22, 0x96, 0x0a, 0xeb, 0x03,
  0xb5, 0x6f, 0x1d, 0xb3, 0x4e, 0x2b, 0x57, 0xae, 0x54, 0xed, 0x20, 0x7a,
  0x4a, 0x45, 0xfa, 0x53, 0x30, 0x56, 0x7a, 0x53, 0xbf, 0xfa, 0xfa, 0xfa,
  0x80, 0x91, 0x9e, 0x9f, 0x9f, 0x8f, 0x35, 0x84, 0x16, 0xa3, 0xd1, 0x38,
  0x96, 0x1b, 0x60, 0xdb, 0x1d, 0x33, 0xd0, 0x6b, 0x1b, 0xd1, 0x1a, 0xac,
  0xdf, 0x5e, 0xb4, 0x68, 0xd1, 0x7d, 0xbf, 0x7e, 0x6d, 0xb0, 0x00, 0x3b,
  0x83, 0x88, 0xd3, 0x3c, 0x42, 0x8a, 0xa7, 0x35, 0x72, 0xdd, 0xe3, 0x39,
  0xf5, 0x1c, 0x0b, 0x66, 0x86, 0x82, 0x32, 0x38, 0xa4, 0x57, 0x7a, 0x37,
  0xd9, 0x93, 0xf7, 0x8e, 0xf1, 0x7c, 0xec, 0x97, 0x5f, 0x7e, 0x69, 0x1a,
  0x68, 0x93, 0x19, 0x14, 0xbf, 0x93, 0x0e, 0x6e, 0x1f, 0x64, 0x5f, 0xe4,
  0x5b, 0xa4, 0xa3, 0x25, 0x24, 0x06, 0xd3, 0xa5, 0x83, 0x4b, 0x38, 0x5e,
  0x47, 0x16, 0x6f, 0x1c, 0x47, 0x1d, 0x77, 0x48, 0x0f, 0xe1, 0x7f, 0x1c,
  0xc1, 0x89, 0xff, 0xa0, 0x0c, 0x12, 0xb9, 0x70, 0xe1, 0x82, 0x68, 0x6a,
  0x6a, 0x32, 0x73, 0x92, 0x26, 0xba, 0x37, 0xe9, 0x58, 0x5e, 0x83, 0xd9,
  0xb0, 0x21, 0x01, 0x4e, 0xca, 0x04, 0xc5, 0x0f, 0xf1, 0x3a, 0x71, 0xea,
  0x5e, 0x83, 0x97, 0xd6, 0x3b, 0x64, 0x83, 0xcd, 0x4f, 0x6b, 0x68, 0x68,
  0x08, 0xc1, 0xa6, 0x3c, 0xc0, 0x17, 0x52, 0x5d, 0x5d, 0xed, 0x71, 0xdb,
  0x51, 0x50, 0x94, 0x25, 0x21, 0x21, 0x41, 0x36, 0xb7, 0xee, 0x8d, 0xd4,
  0xd6, 0xd6, 0xc2, 0x7a, 0x0f, 0xe7, 0xde, 0x9e, 0x6f, 0xec, 0xd5, 0xd3,
  0x43, 0x63, 0x63, 0x63, 0xad, 0x95, 0x95, 0x95, 0xb6, 0x7e, 0x3c, 0xdf,
  0xc0, 0xce, 0x82, 0xae, 0xbd, 0xbd, 0x5d, 0x0c, 0x1b, 0x36, 0x2c, 0xc8,
  0x5e, 0x1f, 0x65, 0xfd, 0xfa, 0xf5, 0xa2, 0xd7, 0x5e, 0x3a, 0x3b, 0xa3,
  0x3f, 0xc9, 0x13, 0x3b, 0x27, 0x8c, 0xc2, 0xdd, 0xa4, 0x63, 0x85, 0x07,
  0xd6, 0x32, 0x95, 0x89, 0xbe, 0xa7, 0x0d, 0xdd, 0x2b, 0x3a, 0x33, 0x61,
  0x39, 0x28, 0x0a, 0xd0, 0x49, 0xed, 0x8a, 0x09, 0x8a, 0x2a, 0xaf, 0xdb,
  0x4d, 0x3a, 0x56, 0x9e, 0x94, 0x8a, 0x9e, 0x15, 0x33, 0xfd, 0x51, 0x28,
  0x96, 0x75, 0x15, 0x20, 0x9f, 0xe5, 0x26, 0x1d, 0x93, 0xec, 0xbb, 0x84,
  0xfc, 0x56, 0x64, 0x35, 0x02, 0x2f, 0x71, 0x0b, 0x61, 0x31, 0x55, 0xda,
  0x1c, 0xa4, 0xae, 0xdf, 0x5e, 0x37, 0x04, 0x29, 0xbd, 0x97, 0x09, 0xa7,
  0x44, 0xff, 0x56, 0x0a, 0x61, 0x6a, 0x15, 0x5b, 0xb6, 0xed, 0x6e, 0xd2,
  0xf1, 0xb0, 0x76, 0x1f, 0xd4, 0x15, 0x01, 0x3e, 0x26, 0x20, 0x1c, 0xc1,
  0x4d, 0x0d, 0xfd, 0x13, 0xde, 0xdc, 0xe8, 0x4a, 0x4e, 0x4e, 0xc6, 0xca,
  0x58, 0xac, 0x68, 0xbd, 0xe1, 0xab, 0x67, 0xfb, 0x7a, 0x06, 0x08, 0xb9,
  0x68, 0xe4, 0xef, 0x4d, 0xc1, 0x95, 0x32, 0xfd, 0x17, 0xde, 0xad, 0x32,
  0x84, 0x3d, 0x6f, 0xbd, 0x56, 0x49, 0xc7, 0x9a, 0xac, 0x31, 0xa5, 0xa5,
  0xa5, 0xc6, 0xba, 0xba, 0xba, 0x20, 0x6b, 0xfd, 0x14, 0x5e, 0x32, 0x1e,
  0xc5, 0xe1, 0x74, 0xa4, 0x16, 0x49, 0x87, 0xe7, 0x8e, 0x29, 0x3c, 0xd9,
  0x5d, 0x31, 0x41, 0x51, 0x2f, 0x58, 0xd7, 0xde, 0xd1, 0xd1, 0x61, 0x66,
  0xbd, 0x0e, 0xd3, 0x22, 0xe9, 0x48, 0xf1, 0x21, 0xab, 0x27, 0xbb, 0x2b,
  0x26, 0x28, 0x5e, 0xc4, 0x58, 0x76, 0xbb, 0x28, 0x2e, 0x2e, 0x86, 0xdf,
  0xd5, 0x9d, 0x22, 0xd7, 0x22, 0xe9, 0x08, 0xfe, 0xd3, 0x5b, 0x5b, 0x5b,
  0xcd, 0x41, 0x27, 0xce, 0xa7, 0xa1, 0x9b, 0xfb, 0x0c, 0xfc, 0xb1, 0xbe,
  0x7a, 0xa6, 0x2f, 0x8f, 0xfe, 0xc6, 0x9a, 0xb1, 0xa4, 0xc6, 0xc6, 0x46,
  0xa3, 0xd2, 0x86, 0x3d, 0x35, 0x82, 0x6c, 0x1e, 0xc5, 0xfa, 0x8a, 0xe5,
  0x90, 0x00, 0x9a, 0x37, 0x6f, 0xde, 0x80, 0x91, 0xd0, 0xd4, 0xd4, 0xd4,
  0xbd, 0x12, 0x58, 0x49, 0xb0, 0xe7, 0x2d, 0x27, 0x27, 0x07, 0x1b, 0x42,
  0xfa, 0xf5, 0xbe, 0xb0, 0xb0, 0x30, 0x77, 0x28, 0x0c, 0x13, 0x6f, 0x16,
  0xd2, 0x87, 0x11, 0x05, 0x84, 0x74, 0x2c, 0xd0, 0xb7, 0xa7, 0xa7, 0xa7,
  0xb7, 0xbd, 0xf7, 0xde, 0x7b, 0xde, 0x8c, 0xe9, 0x06, 0xfe, 0x98, 0x3b,
  0xab, 0x6f, 0xb1, 0xb7, 0x0b, 0x47, 0x86, 0x29, 0x09, 0x36, 0x3b, 0xee,
  0xdf, 0xbf, 0x5f, 0xcc, 0x9a, 0x35, 0xcb, 0xce, 0xca, 0xf0, 0xe7, 0x8e,
  0x17, 0x7c, 0x93, 0x09, 0x27, 0x49, 0x2e, 0x5c, 0xb8, 0x50, 0xa7, 0xb4,
  0x45, 0x0b, 0x89, 0x29, 0xec, 0x0d, 0x40, 0x66, 0xad, 0x97, 0x38, 0x39,
  0xc1, 0xe2, 0x6d, 0x76, 0xcd, 0xca, 0xfa, 0xf5, 0x09, 0xe9, 0x3a, 0x1f,
  0x2d, 0x8b, 0x82, 0x42, 0x90, 0x8d, 0x5b, 0xc0, 0x21, 0x9b, 0x37, 0xcd,
  0x1b, 0xde, 0xe9, 0xe3, 0xa2, 0x67, 0xcd, 0x99, 0xbe, 0xb3, 0xb3, 0x53,
  0xcc, 0x9f, 0x3f, 0x5f, 0x9c, 0x3a, 0x75, 0x4a, 0xf1, 0x87, 0xdb, 0xb7,
  0x6f, 0x47, 0xd6, 0x0a, 0x4b, 0x80, 0x60, 0x12, 0x10, 0x23, 0x76, 0xf8,
  0x99, 0x74, 0xf4, 0xb6, 0x39, 0x57, 0xae, 0x5c, 0x19, 0xf1, 0xe8, 0xa3,
  0x8f, 0xea, 0xa5, 0xce, 0xb1, 0x73, 0x8b, 0xd9, 0x6c, 0xee, 0x5e, 0xb2,
  0x94, 0x95, 0x95, 0xd5, 0x3b, 0xd1, 0xf2, 0x6f, 0xd1, 0xb3, 0xff, 0xcf,
  0x1b, 0xc5, 0x77, 0xf2, 0xf7, 0x9d, 0x12, 0xfd, 0xcb, 0xca, 0xf5, 0x88,
  0x8f, 0x0f, 0xc1, 0xd7, 0xf3, 0xb1, 0xdd, 0xde, 0x20, 0x94, 0xf0, 0x32,
  0xc1, 0xea, 0x70, 0x38, 0xba, 0xcf, 0x59, 0x15, 0x2a, 0x96, 0x01, 0x6d,
  0xde, 0xbc, 0x19, 0xc7, 0x71, 0xdd, 0x22, 0xec, 0x23, 0xcc, 0xe0, 0xa3,
  0x3d, 0x0d, 0x7e, 0xc6, 0x28, 0x02, 0x5e, 0x7c, 0x91, 0x88, 0x77, 0xe0,
  0x6c, 0x77, 0xa5, 0x7a, 0xd2, 0xf0, 0xe3, 0xa2, 0x71, 0x19, 0x75, 0xc5,
  0xb2, 0xa5, 0x63, 0x7c, 0xe4, 0x69, 0x5f, 0xde, 0xad, 0x1b, 0x4c, 0x67,
  0xc3, 0x4e, 0x25, 0x1c, 0x22, 0xd8, 0xd6, 0xac, 0x59, 0xa3, 0x8a, 0x70,
  0x34, 0x0c, 0x12, 0xf7, 0x19, 0xef, 0x3f, 0xe4, 0xc6, 0x36, 0x50, 0xf5,
  0x1d, 0x49, 0x78, 0x05, 0x67, 0xbf, 0xd3, 0xd8, 0xae, 0xea, 0x3c, 0x3c,
  0x9c, 0x5e, 0x5d, 0x55, 0x55, 0x85, 0xb3, 0xf6, 0xaa, 0x09, 0xeb, 0xf8,
  0x90, 0xe3, 0xfb, 0xf6, 0x40, 0xe0, 0xc9, 0x84, 0xbf, 0x13, 0xae, 0x6f,
  0xdd, 0xba, 0x55, 0x15, 0xe1, 0xe8, 0x5d, 0x57, 0xaf, 0x5e, 0xb5, 0xf1,
  0x6d, 0x0e, 0xab, 0x70, 0x26, 0x4e, 0x00, 0xea, 0x3d, 0x91, 0xf0, 0x0e,
  0x16, 0x91, 0xbe, 0xf4, 0xd2, 0x4b, 0xaa, 0xea, 0x3d, 0x69, 0xd2, 0x24,
  0x17, 0x45, 0x36, 0x68, 0xa8, 0x15, 0x5c, 0x6f, 0xd3, 0xfd, 0x48, 0x7a,
  0x34, 0x1f, 0x86, 0x7f, 0x1d, 0xe7, 0xb8, 0xca, 0x9d, 0x84, 0x28, 0x7a,
  0x9d, 0xf2, 0x5c, 0x50, 0x50, 0xe0, 0xe4, 0x7b, 0x5b, 0x36, 0x12, 0x86,
  0x06, 0xa8, 0xee, 0x3a, 0x3e, 0x95, 0xf1, 0x90, 0xcd, 0x66, 0xb3, 0xce,
  0x99, 0x33, 0x47, 0x15, 0xf1, 0xcb, 0x97, 0x2f, 0x77, 0x5b, 0xa8, 0x4f,
  0xf8, 0xe6, 0x8a, 0xfb, 0x8a, 0x74, 0x98, 0xb7, 0xb5, 0x84, 0xda, 0x0b,
  0x17, 0x2e, 0x38, 0x93, 0x92, 0x92, 0x14, 0x15, 0x66, 0x30, 0x18, 0x5c,
  0x38, 0xf6, 0x9b, 0x8f, 0xd8, 0xde, 0x4e, 0x48, 0x0c, 0xb0, 0x95, 0x82,
  0x0f, 0xf1, 0x13, 0x58, 0x9c, 0x96, 0x96, 0x16, 0x3b, 0x7a, 0xb2, 0x1a,
  0xe2, 0x37, 0x6c, 0xd8, 0x80, 0x4b, 0x7d, 0xda, 0xe9, 0x77, 0x1f, 0x10,
  0xa6, 0x05, 0xe2, 0xa8, 0xf0, 0x40, 0x28, 0x0b, 0x8e, 0xdb, 0x6f, 0x60,
  0xe6, 0xc8, 0x4c, 0xdb, 0x13, 0x12, 0x12, 0x54, 0x29, 0xeb, 0xb9, 0xe7,
  0x9e, 0x03, 0xe1, 0xb7, 0x79, 0xfc, 0x9f, 0xa0, 0x91, 0xb3, 0xd3, 0xc3,
  0xd9, 0xb1, 0xab, 0x2b, 0x2b, 0x2b, 0xc3, 0xa9, 0xd3, 0xaa, 0xbe, 0xe5,
  0x8d, 0x37, 0xde, 0x70, 0x5f, 0xfb, 0xf1, 0x16, 0x5f, 0x08, 0x34, 0xa8,
  0x49, 0x47, 0xef, 0xf8, 0x31, 0xe1, 0x94, 0x95, 0x64, 0xee, 0xdc, 0xb9,
  0xaa, 0xc7, 0x43, 0xbe, 0x9c, 0xa7, 0x98, 0x7b, 0x97, 0x41, 0x63, 0x17,
  0x10, 0xbc, 0x0d, 0xc7, 0x6e, 0xdb, 0xb6, 0x6d, 0xaa, 0xbe, 0xc7, 0x62,
  0xb1, 0xb8, 0x0a, 0x0b, 0x0b, 0xe1, 0xcd, 0x37, 0xf0, 0x85, 0x40, 0x89,
  0x83, 0x95, 0x74, 0x5c, 0x8f, 0xf1, 0x53, 0xc2, 0x69, 0xdc, 0x59, 0xf6,
  0xd4, 0x53, 0x4f, 0xa9, 0x52, 0x10, 0xee, 0x5f, 0xbb, 0x74, 0xe9, 0x12,
  0x14, 0x74, 0x86, 0xf0, 0x2b, 0xb6, 0x14, 0x5a, 0xbb, 0x2d, 0x01, 0xbd,
  0x75, 0x07, 0x42, 0xc8, 0x4d, 0x9b, 0x36, 0xa9, 0xfa, 0xae, 0xe8, 0xe8,
  0x68, 0x57, 0x69, 0x69, 0x29, 0x7a, 0x7c, 0x23, 0x61, 0xcb, 0x40, 0x12,
  0x3f, 0x90, 0x8a, 0x99, 0x43, 0xc8, 0x87, 0x89, 0x5e, 0xbf, 0x7e, 0xbd,
  0x2a, 0xc5, 0xe0, 0x2a, 0xac, 0xf2, 0xf2, 0x72, 0x27, 0xf7, 0x08, 0xdc,
  0xe3, 0x12, 0xa6, 0xe1, 0x6b, 0x32, 0x66, 0x72, 0x08, 0xd9, 0x85, 0xcb,
  0xf8, 0xd4, 0x7c, 0x1f, 0x6e, 0x91, 0xa4, 0x06, 0x6d, 0xe7, 0x2b, 0xbd,
  0x36, 0xf1, 0x51, 0xeb, 0x83, 0x86, 0xf4, 0x87, 0x08, 0xff, 0xc2, 0x6d,
  0x46, 0x34, 0x9e, 0xa9, 0xde, 0x87, 0xc5, 0x37, 0x30, 0xb5, 0x70, 0x2f,
  0x4a, 0xd0, 0xf8, 0xdd, 0x28, 0x26, 0x0e, 0xc5, 0xbe, 0x42, 0x48, 0x39,
  0x7e, 0xfc, 0x78, 0x55, 0xdf, 0x38, 0x75, 0xea, 0x54, 0x17, 0x1c, 0x41,
  0x5c, 0x10, 0x44, 0xf8, 0x25, 0x5b, 0xc4, 0x7b, 0x9e, 0xf4, 0x18, 0x4e,
  0x66, 0xb4, 0x1c, 0x39, 0x72, 0x44, 0xb5, 0xb3, 0xb3, 0x6e, 0xdd, 0x3a,
  0xf7, 0x01, 0xf8, 0x79, 0x4a, 0x87, 0xd6, 0x6b, 0x08, 0x43, 0x39, 0x94,
  0xbc, 0x5c, 0x5f, 0x5f, 0xef, 0x80, 0x09, 0x57, 0xf3, 0xad, 0x33, 0x67,
  0xce, 0xc4, 0x4d, 0x92, 0x5d, 0x9c, 0xb1, 0xcb, 0xf6, 0xf3, 0x4d, 0x92,
  0x7e, 0x27, 0xfd, 0x4e, 0xda, 0xf2, 0xe4, 0xc9, 0x93, 0x92, 0x37, 0x17,
  0xde, 0x8d, 0x9c, 0x9c, 0x1c, 0x97, 0xdd, 0x6e, 0x87, 0x12, 0x0a, 0x09,
  0x4f, 0x06, 0x32, 0x91, 0xd1, 0x07, 0x24, 0x72, 0x48, 0x79, 0x2d, 0x2f,
  0x2f, 0x4f, 0x55, 0xfe, 0x01, 0x58, 0xb1, 0x62, 0x05, 0x1a, 0x79, 0x3b,
  0x0f, 0x81, 0x73, 0xfc, 0x19, 0xca, 0xf9, 0xf3, 0x56, 0x65, 0xac, 0xac,
  0xfd, 0x19, 0xe1, 0xd7, 0x98, 0x84, 0xa9, 0xae, 0xae, 0xd6, 0x91, 0x93,
  0xa3, 0xea, 0x87, 0xb9, 0xb9, 0xb9, 0x0e, 0x8a, 0xcb, 0x31, 0x29, 0x81,
  0x4b, 0x68, 0x0e, 0x09, 0xf5, 0x17, 0xe1, 0x68, 0x41, 0x70, 0xb4, 0xda,
  0x0e, 0xc2, 0xa8, 0xec, 0xec, 0xec, 0x45, 0x7b, 0xf6, 0xec, 0x89, 0xe4,
  0xcd, 0x06, 0x8a, 0x42, 0xd6, 0xc1, 0x12, 0x17, 0x17, 0x37, 0x53, 0xf4,
  0x5c, 0x94, 0x54, 0x23, 0xa4, 0x4f, 0x8e, 0xd4, 0xc4, 0x2c, 0x9b, 0x27,
  0xc1, 0x4a, 0x8f, 0xbf, 0x62, 0x56, 0x4a, 0x78, 0xb7, 0xb4, 0x1a, 0x15,
  0xc2, 0x89, 0x8b, 0xb8, 0x83, 0x0b, 0xb7, 0x18, 0x5d, 0xbd, 0x07, 0xd7,
  0x3e, 0x60, 0x9a, 0x18, 0xf7, 0x6e, 0xac, 0x11, 0x3d, 0x87, 0x01, 0x86,
  0x78, 0xf9, 0xfd, 0x58, 0x92, 0x8e, 0x2b, 0xc9, 0x3e, 0x12, 0xf2, 0x67,
  0xee, 0x6a, 0x8e, 0x74, 0x2c, 0x9f, 0xc2, 0xfd, 0x66, 0xd1, 0xde, 0x4e,
  0xfc, 0x89, 0x9e, 0xa3, 0x36, 0x71, 0x0c, 0xe7, 0xcd, 0x7b, 0x78, 0xd1,
  0x0b, 0xa6, 0x97, 0x47, 0x8b, 0x9e, 0xad, 0x44, 0xde, 0xee, 0x01, 0x70,
  0xdf, 0xb8, 0xe4, 0x97, 0x63, 0x50, 0xfd, 0x49, 0x7a, 0x50, 0x34, 0x2a,
  0xff, 0x13, 0x60, 0x00, 0x43, 0x56, 0xa7, 0xa6, 0x51, 0x00, 0x63, 0x17,
  0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
static unsigned int invader_png_len = 4008;

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
   bool dont_ask              = false;
   bool answer                = false;
#ifdef HAVE_MENU
   struct menu_state *menu_st = menu_state_get_ptr();
   if (menu_st->driver_ctx->environ_cb)
      menu_st->driver_ctx->environ_cb(MENU_ENVIRON_RESET_HORIZONTAL_LIST,
            NULL, menu_st->userdata);
#endif
   if (!ui_window.qtWindow->settings()->value(
            "scan_finish_confirm", true).toBool())
      return;

   answer = ui_window.qtWindow->showMessageBox(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED),
         MainWindow::MSGBOX_TYPE_QUESTION_OKCANCEL, Qt::ApplicationModal, true, &dont_ask);

   if (answer && dont_ask)
      ui_window.qtWindow->settings()->setValue("scan_finish_confirm", false);
}
#endif

/* https://stackoverflow.com/questions/7246622/how-to-create-a-slider-with-a-non-linear-scale */
static double exp_scale(double input_val, double mid_val, double max_val)
{
   double    M = max_val / mid_val;
   double base = M - 1;
   double    C = log(base * base);
   double    B = max_val / (exp(C) - 1);
   double    A = -1 * B;
   double  ret = A + B * exp(C * input_val);
   return ret;
}

/* Status codes carried in string_list_elem_attr.i for the
 * core-info value list returned by qt_core_info_collect(). */
enum qt_core_info_row_status
{
   QT_CORE_INFO_ROW_NORMAL = 0,
   /* Firmware section: header rows with a key but empty value. */
   QT_CORE_INFO_ROW_FIRMWARE_NOTE,
   /* Firmware status rows that should render in green. */
   QT_CORE_INFO_ROW_FIRMWARE_PRESENT,
   /* Firmware status rows that should render in red. */
   QT_CORE_INFO_ROW_FIRMWARE_MISSING,
   /* Notes: no key, free-form value. */
   QT_CORE_INFO_ROW_NOTE_NO_KEY
};

/* Append a single (key, value) row.  status applies to value->attr.i. */
static void qt_core_info_append_row(
      struct string_list *keys,
      struct string_list *values,
      const char *key, const char *value,
      enum qt_core_info_row_status status)
{
   union string_list_elem_attr attr;
   attr.i = 0;
   string_list_append(keys, key ? key : "", attr);
   attr.i = (int)status;
   string_list_append(values, value ? value : "", attr);
}

/* Append a row whose key is "<msg_hash>:" and whose value is plain. */
static void qt_core_info_append_kv(
      struct string_list *keys,
      struct string_list *values,
      enum msg_hash_enums label_enum, const char *value)
{
   char key[256];
   size_t _len = strlcpy(key, msg_hash_to_str(label_enum), sizeof(key));
   strlcpy(key + _len, ":", sizeof(key) - _len);
   qt_core_info_append_row(keys, values, key, value,
         QT_CORE_INFO_ROW_NORMAL);
}

/* Append a row built from a list of strings joined by ", ". */
static void qt_core_info_append_joined(
      struct string_list *keys,
      struct string_list *values,
      enum msg_hash_enums label_enum,
      const struct string_list *src)
{
   char buf[NAME_MAX_LENGTH * 4];
   size_t i, _len = 0;
   buf[0] = '\0';
   for (i = 0; i < src->size; i++)
   {
      _len += strlcpy(buf + _len, src->elems[i].data, sizeof(buf) - _len);
      if (i < src->size - 1)
         _len += strlcpy(buf + _len, ", ", sizeof(buf) - _len);
   }
   qt_core_info_append_kv(keys, values, label_enum, buf);
}

/* Collect a list of human-readable rows describing the currently-selected
 * core. Output: two parallel string_lists. values->elems[i].attr.i holds
 * a qt_core_info_row_status for that row.
 *
 * Returns false if no core is selected or its info isn't loaded; in that
 * case a single row with the "no info available" message is appended. */
static bool qt_core_info_collect(
      const char *current_core_path,
      struct string_list *keys,
      struct string_list *values)
{
   size_t i;
   core_info_t *core_info = NULL;

   core_info_find(current_core_path, &core_info);

   if (    !current_core_path
        || !*current_core_path
        || !core_info
        || !(core_info->flags & CORE_INFO_FLAG_HAS_INFO))
   {
      qt_core_info_append_row(keys, values,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE),
            "", QT_CORE_INFO_ROW_NORMAL);
      return false;
   }

   if (core_info->core_name)
      qt_core_info_append_kv(keys, values,
            MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME, core_info->core_name);
   if (core_info->display_name)
      qt_core_info_append_kv(keys, values,
            MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL, core_info->display_name);
   if (core_info->systemname)
      qt_core_info_append_kv(keys, values,
            MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME, core_info->systemname);
   if (core_info->system_manufacturer)
      qt_core_info_append_kv(keys, values,
            MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
            core_info->system_manufacturer);

   if (core_info->categories_list)
      qt_core_info_append_joined(keys, values,
            MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
            core_info->categories_list);
   if (core_info->authors_list)
      qt_core_info_append_joined(keys, values,
            MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
            core_info->authors_list);
   if (core_info->permissions_list)
      qt_core_info_append_joined(keys, values,
            MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
            core_info->permissions_list);
   if (core_info->licenses_list)
      qt_core_info_append_joined(keys, values,
            MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
            core_info->licenses_list);
   if (core_info->supported_extensions_list)
      qt_core_info_append_joined(keys, values,
            MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
            core_info->supported_extensions_list);

   if (core_info->firmware_count > 0)
   {
      char tmp_path[PATH_MAX_LENGTH];
      core_info_ctx_firmware_t firmware_info;
      bool update_missing_firmware    = false;
      settings_t *settings            = config_get_ptr();
      uint8_t flags                   = content_get_flags();
      bool systemfiles_in_content_dir = settings->bools.systemfiles_in_content_dir;
      bool content_is_inited          = flags & CONTENT_ST_FLAG_IS_INITED;

      firmware_info.path              = core_info->path;

      if (systemfiles_in_content_dir && content_is_inited)
      {
         fill_pathname_basedir(tmp_path,
               path_get(RARCH_PATH_CONTENT),
               sizeof(tmp_path));
         if (!*tmp_path)
            firmware_info.directory.system = settings->paths.directory_system;
         else
         {
            size_t _len = strlen(tmp_path);
            if (     string_count_occurrences_single_character(tmp_path, PATH_DEFAULT_SLASH_C()) > 1
                  && tmp_path[_len - 1] == PATH_DEFAULT_SLASH_C())
                     tmp_path[_len - 1] = '\0';
            firmware_info.directory.system = tmp_path;
         }
      }
      else
         firmware_info.directory.system = settings->paths.directory_system;

      update_missing_firmware = core_info_list_update_missing_firmware(&firmware_info);

      if (update_missing_firmware)
      {
         char firmware_label[256];
         char tmp[PATH_MAX_LENGTH];
         size_t _len = strlcpy(firmware_label,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE),
               sizeof(firmware_label));
         strlcpy(firmware_label + _len, ":",
               sizeof(firmware_label) - _len);

         qt_core_info_append_row(keys, values,
               firmware_label, "", QT_CORE_INFO_ROW_FIRMWARE_NOTE);

         if (systemfiles_in_content_dir)
            qt_core_info_append_row(keys, values,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_IN_CONTENT_DIRECTORY),
                  "", QT_CORE_INFO_ROW_FIRMWARE_NOTE);

         snprintf(tmp, sizeof(tmp),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_PATH),
               firmware_info.directory.system);
         qt_core_info_append_row(keys, values,
               tmp, "", QT_CORE_INFO_ROW_FIRMWARE_NOTE);

         for (i = 0; i < core_info->firmware_count; i++)
         {
            char lbl_txt[256];
            const char *val_txt          = NULL;
            bool missing                 = false;
            enum qt_core_info_row_status status;
            size_t lbl_len               = 0;

            if (!core_info->firmware[i].desc)
               continue;

            lbl_len = strlcpy(lbl_txt, "(!) ", sizeof(lbl_txt));

            if (core_info->firmware[i].missing)
            {
               missing = true;
               if (core_info->firmware[i].optional)
                  strlcpy(lbl_txt + lbl_len,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MISSING_OPTIONAL),
                        sizeof(lbl_txt) - lbl_len);
               else
                  strlcpy(lbl_txt + lbl_len,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MISSING_REQUIRED),
                        sizeof(lbl_txt) - lbl_len);
            }
            else
            {
               if (core_info->firmware[i].optional)
                  strlcpy(lbl_txt + lbl_len,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PRESENT_OPTIONAL),
                        sizeof(lbl_txt) - lbl_len);
               else
                  strlcpy(lbl_txt + lbl_len,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PRESENT_REQUIRED),
                        sizeof(lbl_txt) - lbl_len);
            }

            val_txt = core_info->firmware[i].desc
                  ? core_info->firmware[i].desc
                  : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME);
            status  = missing
                  ? QT_CORE_INFO_ROW_FIRMWARE_MISSING
                  : QT_CORE_INFO_ROW_FIRMWARE_PRESENT;
            qt_core_info_append_row(keys, values,
                  lbl_txt, val_txt, status);
         }
      }
   }

   if (core_info->notes && core_info->note_list)
   {
      for (i = 0; i < core_info->note_list->size; i++)
         qt_core_info_append_row(keys, values,
               "", core_info->note_list->elems[i].data,
               QT_CORE_INFO_ROW_NOTE_NO_KEY);
   }

   return true;
}

/* Thumbnail widgets are addressed by a flat 0..3 index. The index order
 * (boxart, title, screenshot, logo) matches the four QObject names set
 * up in ui_companion_qt_init() and is *not* the same as ThumbnailType
 * enum order (which has SCREENSHOT before TITLE_SCREEN). */
static const char * const qt_thumbnail_widget_names[4] = {
   "thumbnail", "thumbnail2", "thumbnail3", "thumbnail4"
};

static const char * const qt_thumbnail_subdirs[4] = {
   THUMBNAIL_BOXART, THUMBNAIL_TITLE, THUMBNAIL_SCREENSHOT, THUMBNAIL_LOGO
};

static int qt_thumbnail_type_to_widget_idx(ThumbnailType t)
{
   switch (t)
   {
      case THUMBNAIL_TYPE_BOXART:       return 0;
      case THUMBNAIL_TYPE_TITLE_SCREEN: return 1;
      case THUMBNAIL_TYPE_SCREENSHOT:   return 2;
      case THUMBNAIL_TYPE_LOGO:         return 3;
   }
   return 0;
}

TreeView::TreeView(QWidget *parent) : QTreeView(parent) { }

void TreeView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
   QModelIndexList list = selected.indexes();

   QTreeView::selectionChanged(selected, deselected);

   emit itemsSelected(list);
}

TableView::TableView(QWidget *parent) : QTableView(parent) { }

bool TableView::isEditorOpen()
{
   return (state() == QAbstractItemView::EditingState);
}

ListWidget::ListWidget(QWidget *parent) : QListWidget(parent) { }

void ListWidget::keyPressEvent(QKeyEvent *event)
{
   int key = event->key();
   if (     key == Qt::Key_Return
         || key == Qt::Key_Enter)
      emit enterPressed();
   else if (key == Qt::Key_Delete)
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

LogTextEdit::LogTextEdit(QWidget *parent) : QPlainTextEdit(parent) { }

void LogTextEdit::appendMessage(const QString& text)
{
   if (text.isEmpty())
      return;

   appendPlainText(text);
   verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

/* Only accept indexes from current path.
 * https://www.qtcentre.org/threads/50700-QFileSystemModel-and-QSortFilterProxyModel-don-t-work-well-together
 *
 * The root index is cached because this method is invoked once per
 * (row, parent) pair the proxy considers - which can be tens of
 * thousands per directory load on a populous tree - while the
 * QFileSystemModel root path changes only when the user navigates.
 * Resolving the root path to an index via sm->index(...) walks the
 * model on every call without the cache. */
bool FileSystemProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
   QFileSystemModel *sm     = qobject_cast<QFileSystemModel*>(sourceModel());
   const QString currentRoot = sm->rootPath();

   if (currentRoot != m_cachedRootPath)
   {
      m_cachedRootPath  = currentRoot;
      m_cachedRootIndex = sm->index(currentRoot);
   }

   if (sourceParent == m_cachedRootIndex)
      return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
   return true;
}

/* sort the source (QFileSystemModel to keep directories before files) */
void FileSystemProxyModel::sort(int column, Qt::SortOrder order)
{
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
   ,m_currentGridWidget(NULL)
   ,m_allPlaylistsListMaxCount(0)
   ,m_allPlaylistsGridMaxCount(0)
   ,m_playlistEntryDialog(NULL)
   ,m_statusMessageElapsedTimer()
#if defined(HAVE_MENU)
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   ,m_shaderParamsDialog(new ShaderParamsDialog())
#endif
#endif
   ,m_coreOptionsDialog(new CoreOptionsDialog())
   ,m_currentHttpTask(NULL)
   ,m_updateProgressDialog(new QProgressDialog())
   ,m_thumbnailDownloadProgressDialog(new QProgressDialog())
   ,m_pendingThumbnailDownloadTypes()
   ,m_thumbnailPackDownloadProgressDialog(new QProgressDialog())
   ,m_playlistThumbnailDownloadProgressDialog(new QProgressDialog())
   ,m_pendingPlaylistThumbnails()
   ,m_downloadedThumbnails(0)
   ,m_failedThumbnails(0)
   ,m_playlistThumbnailDownloadWasCanceled(false)
   ,m_pendingDirScrollPath()
   ,m_thumbnailTimer(new QTimer(this))
   ,m_gridItem(this)
   ,m_currentBrowser(BROWSER_TYPE_PLAYLISTS)
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
   ,m_searchRegularExpression()
#else
   ,m_searchRegExp()
#endif
   ,m_zoomWidget(new QWidget(this))
   ,m_itemsCountLiteral(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ITEMS_COUNT))
   ,m_itemsCountLabel(new QLabel(this))
{
   settings_t                   *settings = config_get_ptr();
   const char *path_dir_assets            = settings->paths.directory_assets;
   QString                      configDir = QFileInfo(path_get(RARCH_PATH_CONFIG)).dir().absolutePath();

   qRegisterMetaType<QPointer<ThumbnailWidget> >("ThumbnailWidget");
   qRegisterMetaType<retro_task_callback_t>("retro_task_callback_t");
   qRegisterMetaType<PlaylistEntry>("PlaylistEntry");

   memset(m_thumbnailPixmaps, 0, sizeof(m_thumbnailPixmaps));

   /* Background loader for the file-browser preview pane. Decoding
    * happens off the UI thread; results arrive on the imageLoaded
    * signal and are filtered by m_pendingPreviewPath so that rapid
    * selection changes don't flicker stale images in. */
   m_previewLoader = new ThumbnailLoader(this);
   connect(m_previewLoader,
         SIGNAL(imageLoaded(QImage,QModelIndex,QString)), this,
         SLOT(onPreviewImageLoaded(QImage,QModelIndex,QString)));
   m_previewLoader->start();

   /* Cancel all progress dialogs immediately since
    * they show as soon as they're constructed. */
   m_updateProgressDialog->cancel();
   m_thumbnailDownloadProgressDialog->cancel();
   m_thumbnailPackDownloadProgressDialog->cancel();
   m_playlistThumbnailDownloadProgressDialog->cancel();

   /* m_playlistViewsAndFooter holds the playlist/grid views on top
    * and the footer toolbar (zoom, view-type, thumbnail-type) on
    * the bottom. Set up its layout, add the views, then call
    * setupPlaylistFooter() to build and attach the toolbar. */
   m_playlistViewsAndFooter->setLayout(new QVBoxLayout());

   m_gridView->setSelectionMode(QAbstractItemView::SingleSelection);
   m_gridView->setEditTriggers(QAbstractItemView::NoEditTriggers);

   m_playlistViews->addWidget(m_gridView);
   m_playlistViews->addWidget(m_tableView);
   m_centralWidget->setObjectName("centralWidget");

   m_playlistViewsAndFooter->layout()->addWidget(m_playlistViews);
   m_playlistViewsAndFooter->layout()->setAlignment(Qt::AlignCenter);
   m_playlistViewsAndFooter->layout()->setContentsMargins(0, 0, 0, 0);

   setupPlaylistFooter();

   setupModels();

   m_logWidget->setObjectName("logWidget");

   m_folderIcon     = QIcon(QString(path_dir_assets) + GENERIC_FOLDER_ICON);
   m_defaultStyle   = QApplication::style();
   m_defaultPalette = QApplication::palette();

   /* ViewOptionsDialog needs m_settings set before it's constructed */
   m_settings            = new QSettings(configDir
         + QString("/retroarch_qt.cfg"), QSettings::IniFormat, this);
   m_viewOptionsDialog   = new ViewOptionsDialog(this, 0);
   m_playlistEntryDialog = new PlaylistEntryDialog(this, 0);

   /* default NULL parameter for parent wasn't added until 5.7 */
   qt_button_set_action_label(m_startCorePushButton, MENU_ENUM_LABEL_VALUE_START_CORE);
   qt_button_set_action_label(m_runPushButton,       MENU_ENUM_LABEL_VALUE_RUN);
   qt_button_set_action_label(m_stopPushButton,      MENU_ENUM_LABEL_VALUE_QT_STOP);
   qt_button_set_action_label(m_coreInfoPushButton,  MENU_ENUM_LABEL_VALUE_QT_INFO);

   setupFileSystemBrowser();

   reloadPlaylists();

   setupDockWidgets();

   m_dirTree->setContextMenuPolicy(Qt::CustomContextMenu);
   m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);

   setupSignalConnections();

   m_timer->start(TIMER_MSEC);

   statusBar()->addPermanentWidget(m_statusLabel);

   setCurrentCoreLabel();
   setCoreActions();

   /* Both of these are necessary to get the folder to scroll
    * to the top of the view */
   qApp->processEvents();
   QTimer::singleShot(0, this, SLOT(onBrowserStartClicked()));

   m_searchLineEdit->setFocus();
   m_loadCoreWindow->setWindowModality(Qt::ApplicationModal);

   m_statusMessageElapsedTimer.start();

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
   resizeDocks(QList<QDockWidget*>() << m_searchDock,
         QList<int>() << 1, Qt::Vertical);
#endif
}

/* Build the footer toolbar - zoom slider, view-type and thumbnail-
 * type push buttons, items count label, and the always-hidden grid
 * progress widget - and attach it under the playlist views. Owns
 * all its widget locals so they don't pollute the constructor.
 * Mutates several MainWindow members (m_gridProgressWidget,
 * m_gridProgressBar, m_zoomSlider, m_lastZoomSliderValue) and
 * appends to m_playlistViewsAndFooter's layout, which the caller
 * is expected to have created. */
void MainWindow::setupPlaylistFooter()
{
   QHBoxLayout *zoomLayout              = new QHBoxLayout();
   QLabel      *zoomLabel               = new QLabel(
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ZOOM), m_zoomWidget);
   QPushButton *thumbnailTypePushButton = new QPushButton(
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_TYPE),
         m_zoomWidget);
   QMenu       *thumbnailTypeMenu       = new QMenu(thumbnailTypePushButton);
   QPushButton *viewTypePushButton      = new QPushButton(
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_VIEW), m_zoomWidget);
   QMenu       *viewTypeMenu            = new QMenu(viewTypePushButton);
   QHBoxLayout *gridProgressLayout      = new QHBoxLayout();
   QHBoxLayout *gridFooterLayout        = NULL;
   QLabel      *gridProgressLabel       = NULL;

   m_gridProgressWidget = new QWidget();
   gridProgressLabel    = new QLabel(
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_PROGRESS),
         m_gridProgressWidget);

   thumbnailTypePushButton->setObjectName("thumbnailTypePushButton");
   thumbnailTypePushButton->setFlat(true);

   connect(thumbnailTypeMenu->addAction(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART)),
         SIGNAL(triggered()), this, SLOT(onBoxartThumbnailClicked()));
   connect(thumbnailTypeMenu->addAction(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT)),
         SIGNAL(triggered()), this, SLOT(onScreenshotThumbnailClicked()));
   connect(thumbnailTypeMenu->addAction(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN)),
         SIGNAL(triggered()), this, SLOT(onTitleThumbnailClicked()));
   connect(thumbnailTypeMenu->addAction(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_LOGO)),
         SIGNAL(triggered()), this, SLOT(onLogoThumbnailClicked()));

   thumbnailTypePushButton->setMenu(thumbnailTypeMenu);

   viewTypePushButton->setObjectName("viewTypePushButton");
   viewTypePushButton->setFlat(true);

   connect(viewTypeMenu->addAction(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS)),
         SIGNAL(triggered()), this, SLOT(onIconViewClicked()));
   connect(viewTypeMenu->addAction(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST)),
         SIGNAL(triggered()), this, SLOT(onListViewClicked()));

   viewTypePushButton->setMenu(viewTypeMenu);

   gridProgressLabel->setObjectName("gridProgressLabel");

   m_gridProgressBar = new QProgressBar(m_gridProgressWidget);
   m_gridProgressBar->setSizePolicy(
         QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred));

   zoomLabel->setObjectName("zoomLabel");

   m_zoomSlider = new QSlider(Qt::Horizontal, m_zoomWidget);
   m_zoomSlider->setMinimum(0);
   m_zoomSlider->setMaximum(100);
   m_zoomSlider->setValue(50);
   m_zoomSlider->setSizePolicy(
         QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred));

   m_lastZoomSliderValue = m_zoomSlider->value();

   m_gridProgressWidget->setLayout(gridProgressLayout);
   gridProgressLayout->setContentsMargins(0, 0, 0, 0);
   gridProgressLayout->addSpacerItem(new QSpacerItem(
            0, 0, QSizePolicy::Expanding, QSizePolicy::Preferred));
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
   gridFooterLayout->addSpacerItem(new QSpacerItem(
            0, 0, QSizePolicy::Expanding, QSizePolicy::Preferred));
   gridFooterLayout->addWidget(m_gridProgressWidget);
   gridFooterLayout->addWidget(m_zoomWidget);
   gridFooterLayout->addWidget(thumbnailTypePushButton);
   gridFooterLayout->addWidget(viewTypePushButton);

   static_cast<QVBoxLayout*>(m_playlistViewsAndFooter->layout())
      ->addLayout(gridFooterLayout);

   m_gridProgressWidget->hide();
}

/* Configure the playlist + filesystem proxy models and the table /
 * file-table / grid views that consume them. The init list creates
 * the views; this fills in their behaviour (sort, selection, etc.)
 * and hooks them up to their models. */
void MainWindow::setupModels()
{
   m_playlistModel  = new PlaylistModel(this);
   m_proxyModel     = new QSortFilterProxyModel(this);
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
   m_tableView->setEditTriggers(
         QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
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
}

/* Configure the QFileSystemModels and the directory tree view used
 * by the file browser. Sets entry filters (respecting the user's
 * "show hidden files" preference), points both models at the root
 * of the filesystem, and hides the size/type/date columns on the
 * tree so only names are shown. */
void MainWindow::setupFileSystemBrowser()
{
   const bool show_hidden = m_settings->value("show_hidden_files", true).toBool();
   const QDir::Filters hidden_filters = show_hidden
      ? (QDir::Hidden | QDir::System)
      : static_cast<QDir::Filter>(0);

   m_dirModel->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Drives
         | hidden_filters);
   m_fileModel->setFilter(QDir::NoDot | QDir::AllEntries | hidden_filters);

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
      m_dirTree->hideColumn(1);  /* size */
      m_dirTree->hideColumn(2);  /* type */
      m_dirTree->hideColumn(3);  /* date modified */
   }
}

/* Build the search / core-info / log dock widgets and their
 * contents, register them with the main window, and hide the log
 * dock so it stays out of the way until the user opens it. Pulled
 * out of the constructor for readability. */
void MainWindow::setupDockWidgets()
{
   QToolButton *searchResetButton = new QToolButton(m_searchWidget);
   qt_button_set_action_label(searchResetButton,
         MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR);
   connect(searchResetButton, SIGNAL(clicked()), this,
         SLOT(onSearchResetClicked()));

   m_searchWidget->setLayout(new QHBoxLayout());
   m_searchWidget->layout()->addWidget(m_searchLineEdit);
   m_searchWidget->layout()->addWidget(searchResetButton);

   qt_dock_configure(m_searchDock, "searchDock", Qt::LeftDockWidgetArea,
         MENU_ENUM_LABEL_VALUE_SEARCH, m_searchWidget);
   m_searchDock->setFixedHeight(m_searchDock->minimumSizeHint().height());

   qt_dock_add_to(this, m_searchDock);

   m_coreInfoLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
   m_coreInfoLabel->setTextFormat(Qt::RichText);
   m_coreInfoLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
   m_coreInfoLabel->setOpenExternalLinks(true);

   qt_dock_configure(m_coreInfoDock, "coreInfoDock", Qt::RightDockWidgetArea,
         MENU_ENUM_LABEL_VALUE_QT_CORE_INFO, m_coreInfoWidget);

   qt_dock_add_to(this, m_coreInfoDock);

   m_logWidget->setLayout(new QVBoxLayout());
   m_logWidget->layout()->addWidget(m_logTextEdit);
   m_logWidget->layout()->setContentsMargins(0, 0, 0, 0);

   qt_dock_configure(m_logDock, "logDock", Qt::BottomDockWidgetArea,
         MENU_ENUM_LABEL_VALUE_QT_LOG, m_logWidget);

   qt_dock_add_to(this, m_logDock);

   /* Hide the log by default. If user has saved their dock positions
    * with the log visible, then this hide() call will be reversed
    * later by restoreState().
    *
    * FIXME: If user unchecks "save dock positions", the log will
    * not be unhidden even if it was previously saved in the config. */
   m_logDock->hide();
}

/* Wire up signal/slot connections for the main window's child
 * widgets and self-emitted signals. Pulled out of the constructor
 * to keep that body readable. All connections operate on already-
 * constructed members of MainWindow; no parameters needed. */
void MainWindow::setupSignalConnections()
{
   settings_t *settings              = config_get_ptr();
   const char *path_dir_menu_content = settings->paths.directory_menu_content;

   connect(m_searchLineEdit, SIGNAL(returnPressed()), this,
         SLOT(onSearchEnterPressed()));
   connect(m_searchLineEdit, SIGNAL(textEdited(const QString&)), this,
         SLOT(onSearchLineEditEdited(const QString&)));
   connect(m_timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
   connect(m_loadCoreWindow, SIGNAL(coreLoaded()), this,
         SLOT(onCoreLoaded()));
   connect(m_loadCoreWindow, SIGNAL(windowClosed()), this,
         SLOT(onCoreLoadWindowClosed()));
   connect(m_listWidget, SIGNAL(currentItemChanged(QListWidgetItem*,
               QListWidgetItem*)), this,
         SLOT(onCurrentListItemChanged(QListWidgetItem*, QListWidgetItem*)));
   connect(m_startCorePushButton, SIGNAL(clicked()), this,
         SLOT(onStartCoreClicked()));
   connect(m_coreInfoPushButton, SIGNAL(clicked()), m_coreInfoDialog,
         SLOT(showCoreInfo()));
   connect(m_runPushButton, SIGNAL(clicked()), this, SLOT(onRunClicked()));
   connect(m_stopPushButton, SIGNAL(clicked()), this, SLOT(onStopClicked()));
   connect(m_dirTree, SIGNAL(itemsSelected(QModelIndexList)), this,
         SLOT(onTreeViewItemsSelected(QModelIndexList)));
   connect(m_dirTree, SIGNAL(customContextMenuRequested(const QPoint&)), this,
         SLOT(onFileBrowserTreeContextMenuRequested(const QPoint&)));
   connect(m_listWidget, SIGNAL(customContextMenuRequested(const QPoint&)), this,
         SLOT(onPlaylistWidgetContextMenuRequested(const QPoint&)));
   connect(m_launchWithComboBox, SIGNAL(currentIndexChanged(int)), this,
         SLOT(onLaunchWithComboBoxIndexChanged(int)));
   connect(m_zoomSlider, SIGNAL(valueChanged(int)), this,
         SLOT(onZoomValueChanged(int)));
   connect(m_dirModel, SIGNAL(directoryLoaded(const QString&)), this,
         SLOT(onFileSystemDirLoaded(const QString&)));
   connect(m_fileModel, SIGNAL(directoryLoaded(const QString&)), this,
         SLOT(onFileBrowserTableDirLoaded(const QString&)));

   m_dirTree->setCurrentIndex(m_dirModel->index(path_dir_menu_content));
   m_dirTree->scrollTo(m_dirTree->currentIndex(), QAbstractItemView::PositionAtTop);
   m_dirTree->expand(m_dirTree->currentIndex());

   /* must use queued connection */
   connect(this, SIGNAL(scrollToDownloads(QString)), this,
         SLOT(onDownloadScroll(QString)), Qt::QueuedConnection);
   connect(this, SIGNAL(scrollToDownloadsAgain(QString)), this,
         SLOT(onDownloadScrollAgain(QString)), Qt::QueuedConnection);

   connect(m_playlistThumbnailDownloadProgressDialog, SIGNAL(canceled()),
         m_playlistThumbnailDownloadProgressDialog, SLOT(cancel()));
   connect(m_playlistThumbnailDownloadProgressDialog, SIGNAL(canceled()),
         this, SLOT(onPlaylistThumbnailDownloadCanceled()));

   connect(m_thumbnailDownloadProgressDialog, SIGNAL(canceled()),
         m_thumbnailDownloadProgressDialog, SLOT(cancel()));
   connect(m_thumbnailDownloadProgressDialog, SIGNAL(canceled()),
         this, SLOT(onThumbnailDownloadCanceled()));

   connect(m_thumbnailPackDownloadProgressDialog, SIGNAL(canceled()),
         m_thumbnailPackDownloadProgressDialog, SLOT(cancel()));
   connect(m_thumbnailPackDownloadProgressDialog, SIGNAL(canceled()),
         this, SLOT(onThumbnailPackDownloadCanceled()));

   connect(this, SIGNAL(itemChanged()), this, SLOT(onItemChanged()));

   m_thumbnailTimer->setSingleShot(true);
   connect(m_thumbnailTimer, SIGNAL(timeout()), this, SLOT(updateVisibleItems()));

   /* TODO: Handle scroll and resize differently. */
   connect(m_gridView, SIGNAL(visibleItemsChangedMaybe()),
         this, SLOT(startTimer()));

   connect(m_tableView->selectionModel(),
         SIGNAL(currentChanged(const QModelIndex&, const QModelIndex&)), this,
         SLOT(onCurrentItemChanged(const QModelIndex&)));
   connect(m_fileTableView->selectionModel(),
         SIGNAL(currentChanged(const QModelIndex&, const QModelIndex&)), this,
         SLOT(onCurrentFileChanged(const QModelIndex&)));

   connect(m_gridView, SIGNAL(doubleClicked(const QModelIndex&)), this,
         SLOT(onContentItemDoubleClicked(const QModelIndex&)));
   connect(m_tableView, SIGNAL(doubleClicked(const QModelIndex&)), this,
         SLOT(onContentItemDoubleClicked(const QModelIndex&)));
   connect(m_fileTableView, SIGNAL(doubleClicked(const QModelIndex&)), this,
         SLOT(onFileDoubleClicked(const QModelIndex&)));

   connect(m_playlistModel, SIGNAL(dataChanged(const QModelIndex&,
               const QModelIndex&, const QVector<int>&)), this,
         SLOT(onCurrentTableItemDataChanged(const QModelIndex&,
               const QModelIndex&, const QVector<int>&)));

   /* Make sure these use an auto connection so it will be queued if
    * called from a different thread (some facilities in RA log
    * messages from other threads) */
   connect(this, SIGNAL(gotLogMessage(const QString&)), this,
         SLOT(onGotLogMessage(const QString&)), Qt::AutoConnection);
   connect(this, SIGNAL(gotStatusMessage(QString,unsigned,unsigned,bool)),
         this, SLOT(onGotStatusMessage(QString,unsigned,unsigned,bool)),
         Qt::AutoConnection);
   connect(this, SIGNAL(gotReloadPlaylists()), this,
         SLOT(onGotReloadPlaylists()), Qt::AutoConnection);
#if defined(HAVE_MENU)
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   connect(this, SIGNAL(gotReloadShaderParams()), this,
         SLOT(onGotReloadShaderParams()), Qt::AutoConnection);
#endif
#endif

   /* These are always queued */
   connect(this, SIGNAL(showErrorMessageDeferred(QString)), this,
         SLOT(onShowErrorMessage(QString)), Qt::QueuedConnection);
   connect(this, SIGNAL(showInfoMessageDeferred(QString)), this,
         SLOT(onShowInfoMessage(QString)), Qt::QueuedConnection);
}

MainWindow::~MainWindow()
{
   size_t i;
   for (i = 0; i < 4; i++)
      if (m_thumbnailPixmaps[i])
         delete m_thumbnailPixmaps[i];
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
   if (     m_currentBrowser == BROWSER_TYPE_PLAYLISTS
         && m_viewType       == VIEW_TYPE_ICONS)
   {
      size_t i;
      QVector<QModelIndex> indexes = m_gridView->visibleIndexes();
      size_t _len                  = indexes.size();
      for (i = 0; i < _len; i++)
         m_playlistModel->loadThumbnail(
               m_proxyModel->mapToSource(indexes.at(i)));
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
   if (!path.isEmpty())
      m_fileTableView->horizontalHeader()->restoreState(m_fileTableHeaderState);
}

QVector<QPair<QString, QString> > MainWindow::getPlaylists()
{
   size_t i;
   QVector<QPair<QString, QString> > playlists;
   size_t _len  = m_listWidget->count();

   for (i = 0; i < _len; i++)
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

void MainWindow::onIconViewClicked() { setCurrentViewType(VIEW_TYPE_ICONS); }
void MainWindow::onListViewClicked() { setCurrentViewType(VIEW_TYPE_LIST);  }

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

void MainWindow::onLogoThumbnailClicked()
{
   setCurrentThumbnailType(THUMBNAIL_TYPE_LOGO);
}

void MainWindow::setIconViewZoom(int zoom_val)
{
   m_zoomSlider->setValue(zoom_val);
}

void MainWindow::onZoomValueChanged(int zoom_val)
{
   int new_size = 0;
   if (zoom_val < 50)
      new_size  = exp_scale(lerp(0, 49, 25, 49, zoom_val)
		/ 50.0, 102, 256);
   else
      new_size  = exp_scale(zoom_val / 100.0, 256, 1024);
   m_gridView->setGridSize(new_size);
   m_lastZoomSliderValue = zoom_val;
}

void MainWindow::showWelcomeScreen()
{
   bool dont_ask             = false;
   bool answer               = false;

   if (!m_settings->value("show_welcome_screen", true).toBool())
      return;

   const QString welcome_txt = QString(""
      "Welcome to the RetroArch Desktop Menu!<br>\n"
      "<br>\n"
      "Many settings and actions are currently only available in the familiar Big Picture menu, "
      "but this Desktop Menu should be functional for launching content and managing playlists.<br>\n"
      "<br>\n"
      "Some useful hotkeys for interacting with the Big Picture menu include:\n"
      "<ul>\n"
      "<li>F1  - Bring up the Big Picture menu</li>\n"
      "<li>F5  - Bring the Desktop Menu back if closed</li>\n"
      "<li>F   - Switch between fullscreen and windowed modes</li>\n"
      "<li>Esc - Exit RetroArch</li>\n"
      "</ul>\n"
      "\n"
      "For more hotkeys and their assignments, see:<br>\n"
      "Settings -> Input -> Hotkeys<br>\n"
      "<br>\n"
      "Documentation for RetroArch, libretro and cores:<br>\n"
      "<a href=\"https://docs.libretro.com/\">https://docs.libretro.com/</a>");

   answer = showMessageBox(welcome_txt,
         MainWindow::MSGBOX_TYPE_QUESTION_OKCANCEL, Qt::ApplicationModal,
         true, &dont_ask);

   if (answer && dont_ask)
      m_settings->setValue("show_welcome_screen", false);
}

const QString& MainWindow::customThemeString() const
{
   return m_customThemeString;
}

bool MainWindow::setCustomThemeFile(QString filePath)
{
   QByteArray pathArray;
   const char *path_data;
   void   *buf  = NULL;
   int64_t len  = 0;

   if (filePath.isEmpty())
   {
      QMessageBox::critical(this,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FILE_PATH_IS_BLANK));
      return false;
   }

   pathArray = filePath.toUtf8();
   path_data = pathArray.constData();

   if (!filestream_exists(path_data))
   {
      QMessageBox::critical(this,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST));
      return false;
   }

   if (!filestream_read_file(path_data, &buf, &len))
   {
      QMessageBox::critical(this,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED));
      return false;
   }

   if (len <= 0)
   {
      free(buf);
      QMessageBox::critical(this,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FILE_IS_EMPTY));
      return false;
   }

   setCustomThemeString(QString::fromUtf8(static_cast<const char*>(buf),
            static_cast<int>(len)));
   free(buf);
   return true;
}

void MainWindow::setCustomThemeString(QString qss) { m_customThemeString = qss;}

bool MainWindow::showMessageBox(QString msg, MessageBoxType msgType,
      Qt::WindowModality modality, bool show_dont_ask, bool *dont_ask)
{
   QCheckBox *checkbox               = NULL;
   QPointer<QMessageBox> msg_box_ptr = new QMessageBox(this);
   QMessageBox             *msg_box  = msg_box_ptr.data();

   msg_box->setWindowModality(modality);
   msg_box->setTextFormat(Qt::RichText);
   msg_box->setTextInteractionFlags(Qt::TextBrowserInteraction);

   if (show_dont_ask)
   {
      checkbox = new QCheckBox(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_DONT_SHOW_AGAIN), msg_box);
      /* QMessageBox::setCheckBox() is available since 5.2 */
      msg_box->setCheckBox(checkbox);
   }

   switch (msgType)
   {
      case MSGBOX_TYPE_INFO:
         msg_box->setIcon(QMessageBox::Information);
         msg_box->setWindowTitle(msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_QT_INFORMATION));
         break;
      case MSGBOX_TYPE_WARNING:
         msg_box->setIcon(QMessageBox::Warning);
         msg_box->setWindowTitle(msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_QT_WARNING));
         break;
      case MSGBOX_TYPE_ERROR:
         msg_box->setIcon(QMessageBox::Critical);
         msg_box->setWindowTitle(msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_QT_ERROR));
         break;
      case MSGBOX_TYPE_QUESTION_YESNO:
         msg_box->setIcon(QMessageBox::Question);
         msg_box->setWindowTitle(msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_QT_QUESTION));
         msg_box->setStandardButtons(QMessageBox::Yes | QMessageBox::No);
         break;
      case MSGBOX_TYPE_QUESTION_OKCANCEL:
         msg_box->setIcon(QMessageBox::Question);
         msg_box->setWindowTitle(msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_QT_QUESTION));
         msg_box->setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
         break;
      default:
         break;
   }

   msg_box->setText(msg);
   msg_box->exec();

   if (!msg_box_ptr)
      return true;

   int key = msg_box->result();
   if (
            key != QMessageBox::Ok
         && key != QMessageBox::Yes)
      return false;

   if (checkbox && dont_ask)
         *dont_ask = checkbox->isChecked();

   return true;
}

void MainWindow::onFileBrowserTreeContextMenuRequested(const QPoint&)
{
#ifdef HAVE_LIBRETRODB
   QPointer<QAction> action;
   QList<QAction*> actions;
   QScopedPointer<QAction> scanAction;
   QString currentDirString      = QDir::toNativeSeparators(
         m_dirModel->filePath(m_dirTree->currentIndex()));
   settings_t *settings          = config_get_ptr();
   const char *path_dir_playlist = settings->paths.directory_playlist;
   const char *path_content_db   = settings->paths.path_content_database;
   QByteArray dirArray;
   const char *fullpath          = NULL;

   if (currentDirString.isEmpty())
      return;

   dirArray = currentDirString.toUtf8();
   fullpath = dirArray.constData();

   if (!path_is_directory(fullpath))
      return;

   /* Default NULL parameter for parent wasn't added until 5.7 */
   scanAction.reset(new QAction(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY), 0));

   actions.append(scanAction.data());

   if (!(action = QMenu::exec(actions, QCursor::pos(), NULL, m_dirTree)))
      return;

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
   QStatusBar *status = statusBar();

   if (msg.isEmpty() || !status)
      return;

   if (status->currentMessage().isEmpty() || flush)
   {
      if (m_statusMessageElapsedTimer.elapsed() >= STATUS_MSG_THROTTLE_MSEC)
      {
         qint64 msg_duration;
         QScreen *screen    = qApp->primaryScreen();
         int msec_duration  = 0;
         if (screen)
            msec_duration   = (duration / screen->refreshRate()) * 1000;
         if (msec_duration <= 0)
            msec_duration   = 1000;
         msg_duration       = ((msec_duration) > (STATUS_MSG_THROTTLE_MSEC) ? (msec_duration) : (STATUS_MSG_THROTTLE_MSEC));
         m_statusMessageElapsedTimer.restart();
         status->showMessage(msg, msg_duration);
      }
   }
}

void MainWindow::deferReloadShaderParams()
{
#if defined(HAVE_MENU)
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   emit gotReloadShaderParams();
#endif
#endif
}

void MainWindow::onShaderParamsClicked()
{
#if defined(HAVE_MENU)
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   if (!m_shaderParamsDialog)
      return;
   m_shaderParamsDialog->show();
   onGotReloadShaderParams();
#endif
#endif
}

void MainWindow::onGotReloadShaderParams()
{
#if defined(HAVE_MENU)
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   if (m_shaderParamsDialog && m_shaderParamsDialog->isVisible())
      m_shaderParamsDialog->reload();
#endif
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
   int i;
   QString core_info_txt;
   QVector<QHash<QString, QString> >
      infoList                  = getCoreInfo();
   QVariantMap          coreMap = m_launchWithComboBox->currentData(
         Qt::UserRole).value<QVariantMap>();
   core_selection coreSelection = static_cast<core_selection>(
         coreMap.value("core_selection").toInt());

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
         core_info_txt                   += key;

      if (!value.isEmpty())
      {
         if (!key.isEmpty())
            core_info_txt                += QString(" ");

         core_info_txt                   += value;
      }

      if (i < infoList.count() - 1)
         core_info_txt                   += QString("<br>\n");
   }

   m_coreInfoLabel->setText(core_info_txt);

   if (coreSelection == CORE_SELECTION_LOAD_CORE)
      onLoadCoreClicked();
   else
      m_loadCoreWindow->setProperty("last_launch_with_index",
            m_launchWithComboBox->currentIndex());
}

MainWindow::Theme MainWindow::getThemeFromString(QString themeString)
{
   if (themeString == QLatin1String("default"))
      return THEME_SYSTEM_DEFAULT;
   else if (themeString == QLatin1String("dark"))
      return THEME_DARK;
   else if (themeString == QLatin1String("custom"))
      return THEME_CUSTOM;
   return THEME_SYSTEM_DEFAULT;
}

const char *MainWindow::getThemeString(Theme theme)
{
   switch (theme)
   {
      case THEME_DARK:
         return "dark";
      case THEME_CUSTOM:
         return "custom";
      case THEME_SYSTEM_DEFAULT:
      default:
         break;
   }
   return "default";
}

MainWindow::Theme MainWindow::theme() { return m_currentTheme; }

void MainWindow::setTheme(Theme theme)
{
   m_currentTheme = theme;

   setDefaultCustomProperties();

   switch(theme)
   {
      case THEME_SYSTEM_DEFAULT:
         qApp->setStyleSheet(qt_theme_default_stylesheet.arg(
                  m_settings->value("highlight_color",
                     "palette(highlight)").toString()));
         break;
      case THEME_DARK:
         qApp->setStyleSheet(qt_theme_dark_stylesheet.arg(
                  m_settings->value("highlight_color",
                     "palette(highlight)").toString()));
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
   PlaylistEntry entry          = getCurrentContentEntry();
   QString dirString            = m_playlistModel->getPlaylistThumbnailsDir(
                                      entry.dbName)
                                + QString("/") + type;
   QString thumbPath            = m_playlistModel->getSanitizedThumbnailName(
                                      dirString + QString("/"),
                                      entry.labelNoExt);
   QByteArray   dirArray        = QDir::toNativeSeparators(dirString).toUtf8();
   const char   *dirData        = dirArray.constData();
   QByteArray thumbArray        = QDir::toNativeSeparators(thumbPath).toUtf8();
   const char *thumbData        = thumbArray.constData();
   int quality                  = -1;
   QImage scaledImage(image);

   if (!path_is_directory(dirData))
   {
      if (!path_mkdir(dirData))
      {
         RARCH_ERR("[Qt] Could not create directory: \"%s\".\n", dirData);
         return QString();
      }
      RARCH_LOG("[Qt] Created directory: \"%s\".\n", dirData);
   }

   if (m_settings->contains("thumbnail_max_size"))
   {
      int size = m_settings->value("thumbnail_max_size", 0).toInt();

      if (size != 0 && (image.height() > size ||  image.width() > size))
         scaledImage = image.scaled(size, size,
               Qt::KeepAspectRatio, Qt::SmoothTransformation);
   }

   if (m_settings->contains("thumbnail_quality"))
      quality = m_settings->value("thumbnail_quality", -1).toInt();

   if (scaledImage.save(thumbPath, "png", quality))
   {
      RARCH_LOG("[Qt] Saved image: \"%s\".\n", thumbData);
      m_playlistModel->reloadThumbnailPath(thumbPath);
      updateVisibleItems();

      return thumbPath;
   }

   RARCH_ERR("[Qt] Could not save image: \"%s\".\n", thumbData);
   return QString();
}

void MainWindow::onThumbnailDropped(const QImage &image,
      ThumbnailType thumbnailType)
{
   int idx          = qt_thumbnail_type_to_widget_idx(thumbnailType);
   QString path     = changeThumbnail(image, qt_thumbnail_subdirs[idx]);
   QPixmap *new_pix = NULL;

   if (path.isNull())
      return;

   if (m_thumbnailPixmaps[idx])
      delete m_thumbnailPixmaps[idx];

   new_pix                  = new QPixmap(pixmapFromPathRA(path));
   m_thumbnailPixmaps[idx]  = new_pix;

   setThumbnail(qt_thumbnail_widget_names[idx], *new_pix, true);
}

QVector<QHash<QString, QString> > MainWindow::getCoreInfo()
{
   size_t i;
   QVector<QHash<QString, QString> > infoList;
   QByteArray currentCorePathArray     = getSelectedCorePath().toUtf8();
   const char *current_core_path_data  = currentCorePathArray.constData();
   struct string_list *keys            = string_list_new();
   struct string_list *values          = string_list_new();

   if (!keys || !values)
   {
      string_list_free(keys);
      string_list_free(values);
      return infoList;
   }

   qt_core_info_collect(current_core_path_data, keys, values);

   for (i = 0; i < keys->size; i++)
   {
      QHash<QString, QString> hash;
      enum qt_core_info_row_status status =
         (enum qt_core_info_row_status)values->elems[i].attr.i;

      hash["key"]   = keys->elems[i].data;
      hash["value"] = values->elems[i].data;

      if (    status == QT_CORE_INFO_ROW_FIRMWARE_PRESENT
           || status == QT_CORE_INFO_ROW_FIRMWARE_MISSING)
      {
         const char *css_color  = (status == QT_CORE_INFO_ROW_FIRMWARE_MISSING)
            ? "#ff0000" : "#00af00";
         const char *style_rgb  = (status == QT_CORE_INFO_ROW_FIRMWARE_MISSING)
            ? "color: #ff0000" : "color: rgb(0, 175, 0)";
         QString style          = QString("font-weight: bold; ") + style_rgb;

         hash["label_style"]    = style;
         hash["value_style"]    = style;
         hash["html_key"]       = QString("<b><font color=\"") + css_color
            + "\">" + hash["key"]   + "</font></b>";
         hash["html_value"]     = QString("<b><font color=\"") + css_color
            + "\">" + hash["value"] + "</font></b>";
      }

      infoList.append(hash);
   }

   string_list_free(keys);
   string_list_free(values);

   return infoList;
}

void MainWindow::onSearchResetClicked()
{
   m_searchLineEdit->clear();
   onSearchEnterPressed();
}

QToolButton* MainWindow::coreInfoPushButton() { return m_coreInfoPushButton; }

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
      loadContent(getFileContentEntry(index));
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
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
         m_proxyFileModel->setFilterRegularExpression(QRegularExpression());
#else
         m_proxyFileModel->setFilterRegExp(QRegExp());
#endif
         m_fileTableView->setRootIndex(m_proxyFileModel->mapFromSource(sourceIndex));
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
         m_proxyFileModel->setFilterRegularExpression(m_searchRegularExpression);
#else
         m_proxyFileModel->setFilterRegExp(m_searchRegExp);
#endif
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
   /* Entry is being renamed, ignore this enter press */
   if (m_tableView->isPersistentEditorOpen(m_tableView->currentIndex()))
      return;
#else
   /* We can only check if any editor at all is open */
   if (m_tableView->isEditorOpen())
      return;
#endif
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

PlaylistEntry MainWindow::getCurrentContentEntry()
{
   return getCurrentContentIndex().data(PlaylistModel::ENTRY).value<PlaylistEntry>();
}

PlaylistEntry MainWindow::getFileContentEntry(const QModelIndex &index)
{
   PlaylistEntry entry;
   QFileInfo fileInfo  = m_fileModel->fileInfo(index);

   entry.path          = QDir::toNativeSeparators(m_fileModel->filePath(index));
   entry.label         = entry.path;
   entry.labelNoExt    = fileInfo.completeBaseName();
   entry.dbName        = fileInfo.dir().dirName();

   return entry;
}

void MainWindow::onContentItemDoubleClicked(const QModelIndex &index)
{
   (void)(index);
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
      QMessageBox::critical(this, msg_hash_to_str(MSG_ERROR),
            msg_hash_to_str(MSG_FAILED_TO_LOAD_CONTENT));
}

/* Resolve which core path the user has currently selected, given the
 * UI's combo-box mode and the currently-highlighted content row. The
 * result is the empty string if no core can be resolved (e.g. unknown
 * mode, no current item, or no default core for the playlist). */
QString MainWindow::getSelectedCorePath()
{
   PlaylistEntry entry;
   QVariantMap coreMap          = m_launchWithComboBox->currentData(
         Qt::UserRole).value<QVariantMap>();
   core_selection coreSelection = static_cast<core_selection>(
         coreMap.value("core_selection").toInt());
   ViewType viewType            = getCurrentViewType();

   /* The content row only matters for the two playlist branches —
    * CORE_SELECTION_CURRENT just hands back whatever core is loaded.
    * Original behaviour: an "other" view type (e.g. while transitioning)
    * returned an empty entry from all three branches. */
   if (viewType == VIEW_TYPE_LIST || viewType == VIEW_TYPE_ICONS)
      entry = getCurrentContentIndex().data(PlaylistModel::ENTRY)
            .value<PlaylistEntry>();
   else
      return QString();

   switch (coreSelection)
   {
      case CORE_SELECTION_CURRENT:
         return QString::fromUtf8(path_get(RARCH_PATH_CORE));
      case CORE_SELECTION_PLAYLIST_SAVED:
         if (!entry.corePath.isEmpty())
            return entry.corePath;
         break;
      case CORE_SELECTION_PLAYLIST_DEFAULT:
         {
            QString plName;
            QString defaultCorePath;

            plName = entry.plName.isEmpty()
                  ? entry.dbName : entry.plName;

            if (plName.isEmpty())
               break;

            defaultCorePath = getPlaylistDefaultCore(plName);

            if (!defaultCorePath.isEmpty())
               return defaultCorePath;
            break;
         }
      default:
         break;
   }

   return QString();
}

/* the hash typically has the following keys:
path - absolute path to the content file
core_path - absolute path to the core, or "DETECT" to ask the user
db_name - the display name of the rdb database this content is from
label - the display name of the content, usually comes from the database
crc32 - an upper-case, 8 byte string representation of the hex CRC32 checksum
(e.g. ABCDEF12) followed by "|crc"
core_name   - The display name of the core, or "DETECT" if unknown
label_noext - The display name of the content that is guaranteed not
              to contain a file extension
*/
void MainWindow::loadContent(const PlaylistEntry &entry)
{
   content_ctx_info_t content_info;
   QByteArray corePathArray;
   QByteArray contentPathArray;
   QByteArray contentLabelArray;
   QByteArray contentDbNameArray;
   QByteArray contentCrc32Array;
   char content_db_name_full[PATH_MAX_LENGTH];
   char core_path_cached[PATH_MAX_LENGTH];
   const char *core_path        = NULL;
   const char *content_path     = NULL;
   const char *content_label    = NULL;
   const char *content_db_name  = NULL;
   const char *content_crc32    = NULL;
#ifdef HAVE_MENU
   struct menu_state *menu_st   = menu_state_get_ptr();
#endif
   QVariantMap coreMap          = m_launchWithComboBox->currentData(
         Qt::UserRole).value<QVariantMap>();
   core_selection coreSelection = static_cast<core_selection>(
         coreMap.value("core_selection").toInt());
   core_info_t *coreInfo        = NULL;

   content_db_name_full[0]      = '\0';
   core_path_cached[0]          = '\0';

   if (m_pendingRun)
      coreSelection             = CORE_SELECTION_CURRENT;
   else if (coreSelection == CORE_SELECTION_ASK)
   {
      QStringList extensionFilters;

      if (!entry.path.isEmpty())
      {
         QByteArray pathArray = entry.path.toUtf8();
         const char *pathData = pathArray.constData();
         const char *ext      = path_get_extension(pathData);

         if (ext && *ext)
            extensionFilters.append(QString(ext).toLower());

         if (path_is_compressed_file(pathData))
         {
            struct string_list *list = file_archive_get_file_list(pathData, NULL);

            if (list)
            {
               if (list->size > 0)
               {
                  size_t i;
                  for (i = 0; i < list->size; i++)
                  {
                     const char *filePath  = list->elems[i].data;
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
         contentPathArray  = entry.path.toUtf8();
         contentLabelArray = entry.labelNoExt.toUtf8();
         break;
      case CORE_SELECTION_PLAYLIST_SAVED:
         corePathArray     = entry.corePath.toUtf8();
         contentPathArray  = entry.path.toUtf8();
         contentLabelArray = entry.labelNoExt.toUtf8();
         break;
      case CORE_SELECTION_PLAYLIST_DEFAULT:
      {
         QString plName = entry.plName.isEmpty()
               ? entry.dbName : entry.plName;

         QString defaultCorePath = getPlaylistDefaultCore(plName);

         if (!defaultCorePath.isEmpty())
         {
            corePathArray     = defaultCorePath.toUtf8();
            contentPathArray  = entry.path.toUtf8();
            contentLabelArray = entry.labelNoExt.toUtf8();
         }

         break;
      }
      default:
         return;
   }

   contentDbNameArray         = entry.dbName.toUtf8();
   contentCrc32Array          = entry.crc32.toUtf8();

   core_path                  = corePathArray.constData();
   content_path               = contentPathArray.constData();
   content_label              = contentLabelArray.constData();
   content_db_name            = contentDbNameArray.constData();
   content_crc32              = contentCrc32Array.constData();

   /* Search for specified core - ensures path
    * is 'sanitised' */
   if (    core_info_find(core_path, &coreInfo)
       && (coreInfo->path && *coreInfo->path))
      core_path = coreInfo->path;

   /* If a core is currently running, the following
    * call of 'command_event(CMD_EVENT_UNLOAD_CORE, NULL)'
    * will free the global core_info struct, which will
    * in turn free the pointer referenced by coreInfo->path.
    * This will invalidate core_path, so we have to cache
    * its current value here. */
   if (core_path && *core_path)
      strlcpy(core_path_cached, core_path, sizeof(core_path_cached));

   /* Add lpl extension to db_name, if required */
   if (content_db_name && *content_db_name)
      fill_pathname(content_db_name_full, content_db_name,
            ".lpl", sizeof(content_db_name_full));

   content_info.argc                   = 0;
   content_info.argv                   = NULL;
   content_info.args                   = NULL;
   content_info.environ_get            = NULL;

#ifdef HAVE_MENU
   menu_st->selection_ptr              = 0;
#endif

   command_event(CMD_EVENT_UNLOAD_CORE, NULL);

   if (!task_push_load_content_with_new_core_from_companion_ui(
         core_path_cached,
         content_path,
         content_label,
         content_db_name_full,
         content_crc32,
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
   PlaylistEntry entry;

   switch (m_currentBrowser)
   {
      case BROWSER_TYPE_FILES:
         entry = getFileContentEntry(
               m_proxyFileModel->mapToSource(m_fileTableView->currentIndex()));
         break;
      case BROWSER_TYPE_PLAYLISTS:
         entry = getCurrentContentEntry();
         break;
   }

   if (!entry.path.isEmpty())
      loadContent(entry);
}

PlaylistEntryDialog* MainWindow::playlistEntryDialog()
{
   return m_playlistEntryDialog;
}

ViewOptionsDialog* MainWindow::viewOptionsDialog() {return m_viewOptionsDialog;}

void MainWindow::setCoreActions()
{
   QListWidgetItem *currentPlaylistItem = m_listWidget->currentItem();
   PlaylistEntry                  entry = getCurrentContentEntry();
   QString      currentPlaylistFileName = QString();
   rarch_system_info_t *sys_info        = &runloop_state_get_ptr()->system;

   m_launchWithComboBox->clear();

   /* Is contentless core? */
   if (sys_info->load_no_content)
      m_startCorePushButton->show();
   else
      m_startCorePushButton->hide();

   /* Is core loaded? */
   if (    !m_currentCore.isEmpty()
         && m_currentCore != msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE)
         && m_settings->value("suggest_loaded_core_first", false).toBool())
   {
      QVariantMap comboBoxMap;
      comboBoxMap["core_name"]      = m_currentCore;
      comboBoxMap["core_path"]      = path_get(RARCH_PATH_CORE);
      comboBoxMap["core_selection"] = CORE_SELECTION_CURRENT;
      m_launchWithComboBox->addItem(m_currentCore,
            QVariant::fromValue(comboBoxMap));
   }

   if (m_currentBrowser == BROWSER_TYPE_PLAYLISTS)
   {
      const QString &coreName = entry.coreName;

      if (!coreName.isEmpty() && coreName != QLatin1String("DETECT"))
      {
         if (m_launchWithComboBox->findText(coreName) == -1)
         {
            int i;
            bool found_existing = false;

            for (i = 0; i < m_launchWithComboBox->count(); i++)
            {
               QVariantMap map = m_launchWithComboBox->itemData(
                     i, Qt::UserRole).toMap();

               if (     map.value("core_path").toString() == entry.corePath
                     || map.value("core_name").toString() == coreName)
               {
                  found_existing = true;
                  break;
               }
            }

            if (!found_existing)
            {
               QVariantMap comboBoxMap;
               comboBoxMap["core_name"]      = coreName;
               comboBoxMap["core_path"]      = entry.corePath;
               comboBoxMap["core_selection"] = CORE_SELECTION_PLAYLIST_SAVED;
               m_launchWithComboBox->addItem(coreName,
                     QVariant::fromValue(comboBoxMap));
            }
         }
      }
   }

   switch(m_currentBrowser)
   {
      case BROWSER_TYPE_PLAYLISTS:
         currentPlaylistFileName = entry.plName.isEmpty()
               ? entry.dbName : entry.plName;
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
         bool allPlaylists                  = false;
         int row                            = 0;
         QByteArray defaultCorePathArray    = defaultCorePath.toUtf8();
         const char *default_core_path_data = defaultCorePathArray.constData();

         if (currentPlaylistItem)
         {
            currentPlaylistItemDataString   = currentPlaylistItem->data(
                  Qt::UserRole).toString();
            allPlaylists                    = (
                  currentPlaylistItemDataString == ALL_PLAYLISTS_TOKEN);
         }

         for (row = 0; row < m_listWidget->count(); row++)
         {
            core_info_t *coreInfo = NULL;

            if (allPlaylists)
            {
               QListWidgetItem *listItem = m_listWidget->item(row);
               QString    listItemString = listItem->data(
                     Qt::UserRole).toString();

               if (listItemString == ALL_PLAYLISTS_TOKEN)
                  continue;
            }

            /* Search for default core */
            if (core_info_find(default_core_path_data, &coreInfo))
            {
               if (m_launchWithComboBox->findText(coreInfo->core_name) == -1)
               {
                  int i;
                  bool found_existing = false;

                  for (i = 0; i < m_launchWithComboBox->count(); i++)
                  {
                     QVariantMap map            =
                        m_launchWithComboBox->itemData(
                              i, Qt::UserRole).toMap();
                     QByteArray CorePathArray   =
                        map.value("core_path").toString().toUtf8();
                     const char *core_path_data = CorePathArray.constData();

                     if (
                              string_starts_with(path_basename(core_path_data),
                              coreInfo->core_file_id.str)
                           || map.value("core_name").toString() == coreInfo->core_name
                           || map.value("core_name").toString() == coreInfo->display_name)
                     {
                        found_existing = true;
                        break;
                     }
                  }

                  if (!found_existing)
                  {
                     QVariantMap comboBoxMap;
                     comboBoxMap["core_name"]      = QVariant::fromValue(
                           QString(coreInfo->core_name));
                     comboBoxMap["core_path"]      = QVariant::fromValue(
                           QString(coreInfo->path));
                     comboBoxMap["core_selection"] =
                        CORE_SELECTION_PLAYLIST_DEFAULT;
                     m_launchWithComboBox->addItem(coreInfo->core_name,
                           QVariant::fromValue(comboBoxMap));
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
      m_launchWithComboBox->addItem(msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK),
            QVariant::fromValue(comboBoxMap));
      m_launchWithComboBox->insertSeparator(m_launchWithComboBox->count());
      comboBoxMap["core_selection"] = CORE_SELECTION_LOAD_CORE;
      m_launchWithComboBox->addItem(QString(msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE))
                + QString("..."),
            QVariant::fromValue(comboBoxMap));
   }
}

void MainWindow::onTabWidgetIndexChanged(int index)
{
   QString str = m_browserAndPlaylistTabWidget->tabText(index);
   if (str == msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER))
   {
      m_currentBrowser = BROWSER_TYPE_FILES;
      m_centralWidget->setCurrentWidget(m_fileTableView);
      onCurrentFileChanged(m_fileTableView->currentIndex());
   }
   else if (str == msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS))
   {
      m_currentBrowser = BROWSER_TYPE_PLAYLISTS;
      m_centralWidget->setCurrentWidget(m_playlistViewsAndFooter);
      onCurrentItemChanged(m_tableView->currentIndex());
   }

   applySearch();

   setCoreActions();
}

QToolButton* MainWindow::runPushButton()  { return m_runPushButton; }
QToolButton* MainWindow::stopPushButton() { return m_stopPushButton; }
QToolButton* MainWindow::startCorePushButton() { return m_startCorePushButton;}
QComboBox* MainWindow::launchWithComboBox() { return m_launchWithComboBox; }

void MainWindow::onSearchLineEditEdited(const QString &text)
{
   int i;
   QVector<char32_t> textHiraToKata;
   QVector<char32_t> textKataToHira;
   QVector<unsigned> textUnicode = text.toUcs4();
   bool found_hiragana = false;
   bool found_katakana = false;

   for (i = 0; i < textUnicode.size(); i++)
   {
      unsigned code = textUnicode.at(i);

      if (code >= HIRAGANA_START && code <= HIRAGANA_END)
      {
         found_hiragana  = true;
         textHiraToKata += code + HIRA_KATA_OFFSET;
      }
      else if (code >= KATAKANA_START && code <= KATAKANA_END)
      {
         found_katakana  = true;
         textKataToHira += code - HIRA_KATA_OFFSET;
      }
      else
      {
         textHiraToKata += code;
         textKataToHira += code;
      }
   }

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
   if (!found_hiragana && !found_katakana)
      m_searchRegularExpression = QRegularExpression(text,
            QRegularExpression::CaseInsensitiveOption);
   else if (found_hiragana && !found_katakana)
      m_searchRegularExpression = QRegularExpression(text
            + QString("|")
            + QString::fromUcs4(textHiraToKata.constData(),
               textHiraToKata.size()),
            QRegularExpression::CaseInsensitiveOption);
   else if (!found_hiragana && found_katakana)
      m_searchRegularExpression = QRegularExpression(text
            + QString("|")
            + QString::fromUcs4(textKataToHira.constData(),
               textKataToHira.size()),
            QRegularExpression::CaseInsensitiveOption);
   else
      m_searchRegularExpression = QRegularExpression(text
            + QString("|")
            + QString::fromUcs4(textHiraToKata.constData(),
               textHiraToKata.size())
            + QString("|")
            + QString::fromUcs4(textKataToHira.constData(),
               textKataToHira.size()),
            QRegularExpression::CaseInsensitiveOption);
#else
   if (!found_hiragana && !found_katakana)
      m_searchRegExp = QRegExp(text, Qt::CaseInsensitive);
   else if (found_hiragana && !found_katakana)
      m_searchRegExp = QRegExp(text
            + QString("|")
            + QString::fromUcs4(textHiraToKata.constData(),
               textHiraToKata.size()), Qt::CaseInsensitive);
   else if (!found_hiragana && found_katakana)
      m_searchRegExp = QRegExp(text
            + QString("|")
            + QString::fromUcs4(textKataToHira.constData(),
               textKataToHira.size()), Qt::CaseInsensitive);
   else
      m_searchRegExp = QRegExp(text
            + QString("|")
            + QString::fromUcs4(textHiraToKata.constData(),
               textHiraToKata.size())
            + QString("|")
            + QString::fromUcs4(textKataToHira.constData(),
               textKataToHira.size()), Qt::CaseInsensitive);
#endif

   applySearch();
}

void MainWindow::applySearch()
{
   switch (m_currentBrowser)
   {
      case BROWSER_TYPE_PLAYLISTS:
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
         if (     m_proxyModel->filterRegularExpression()
               != m_searchRegularExpression)
         {
            m_proxyModel->setFilterRegularExpression(m_searchRegularExpression);
            updateItemsCount();
         }
#else
         if (m_proxyModel->filterRegExp() != m_searchRegExp)
         {
            m_proxyModel->setFilterRegExp(m_searchRegExp);
            updateItemsCount();
         }
#endif
         break;
      case BROWSER_TYPE_FILES:
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
         if (     m_proxyFileModel->filterRegularExpression()
               != m_searchRegularExpression)
            m_proxyFileModel->setFilterRegularExpression(
                  m_searchRegularExpression);
#else
         if (m_proxyFileModel->filterRegExp() != m_searchRegExp)
            m_proxyFileModel->setFilterRegExp(m_searchRegExp);
#endif
         break;
   }
}

void MainWindow::onViewClosedDocksAboutToShow()
{
   int i;
   QList<QDockWidget*> dockWidgets;
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
   QDockWidget *dock = NULL;
   QAction *action   = qobject_cast<QAction*>(sender());

   if (!action)
      return;

   if (!(dock = findChild<QDockWidget*>(action->property(
                  "dock_name").toString())))
      return;

   if (!dock->isVisible())
   {
      qt_dock_add_to(this, dock);
      dock->setVisible(true);
      dock->setFloating(false);
   }
}

QLineEdit* MainWindow::searchLineEdit() { return m_searchLineEdit; }
void MainWindow::onSearchEnterPressed()
{
   onSearchLineEditEdited(m_searchLineEdit->text());
}

void MainWindow::onCurrentTableItemDataChanged(const QModelIndex &topLeft,
      const QModelIndex &bottomRight, const QVector<int> &roles)
{
   PlaylistEntry entry;

   if (!roles.contains(Qt::EditRole))
      return;
   if (topLeft != bottomRight)
      return;

   entry = topLeft.data(PlaylistModel::ENTRY).value<PlaylistEntry>();

   updateCurrentPlaylistEntry(entry);

   onCurrentItemChanged(topLeft);
}

void MainWindow::onCurrentListItemDataChanged(QListWidgetItem *item)
{
   renamePlaylistItem(item, item->text());
}

void MainWindow::renamePlaylistItem(QListWidgetItem *item, QString newName)
{
   char old_path[PATH_MAX_LENGTH];
   char new_path[PATH_MAX_LENGTH];
   char old_basedir[PATH_MAX_LENGTH];
   char dir_playlist[PATH_MAX_LENGTH];
   char old_name_buf[PATH_MAX_LENGTH];
   const char *ext               = NULL;
   QString oldName;
   QString newPath;
   settings_t *settings          = config_get_ptr();
   const char *path_dir_playlist = settings->paths.directory_playlist;

   if (!item)
      return;

   strlcpy(old_path,
         item->data(Qt::UserRole).toString().toUtf8().constData(),
         sizeof(old_path));

   /* completeBaseName(): strip directory and extension */
   fill_pathname(old_name_buf, path_basename(old_path), "",
         sizeof(old_name_buf));
   oldName = QString::fromUtf8(old_name_buf);

   /* Compare the playlist's directory with path_dir_playlist
    * case-insensitively to match Qt's QDir == QDir behaviour
    * on Windows. */
   strlcpy(old_basedir, old_path, sizeof(old_basedir));
   path_basedir(old_basedir);
   strlcpy(dir_playlist, path_dir_playlist, sizeof(dir_playlist));
   fill_pathname_slash(dir_playlist, sizeof(dir_playlist));

   if (!string_is_equal_case_insensitive(old_basedir, dir_playlist))
   {
      /* Special playlists (history etc.) can't have an association.
       * Set the old name back if user tried to rename one. */
      item->setText(oldName);
      return;
   }

   /* Block this signal because setData() would trigger
    * an infinite loop here */
   disconnect(m_listWidget, SIGNAL(itemChanged(QListWidgetItem*)),
         this, SLOT(onCurrentListItemDataChanged(QListWidgetItem*)));

   /* Build new path: basedir + newName + "." + extension */
   ext = path_get_extension(old_path);
   {
      QByteArray newNameUtf8 = newName.toUtf8();
      size_t _len = strlcpy(new_path, old_basedir, sizeof(new_path));
      _len += strlcpy(new_path + _len, newNameUtf8.constData(),
            sizeof(new_path) - _len);
      if (ext && *ext)
      {
         _len += strlcpy(new_path + _len, ".", sizeof(new_path) - _len);
         strlcpy(new_path + _len, ext, sizeof(new_path) - _len);
      }
   }

   newPath = QString::fromUtf8(new_path);
   item->setData(Qt::UserRole, newPath);

   if (filestream_rename(old_path, new_path) != 0)
   {
      RARCH_ERR("[Qt] Could not rename playlist.\n");
      item->setText(oldName);
   }

   connect(m_listWidget, SIGNAL(itemChanged(QListWidgetItem*)),
         this, SLOT(onCurrentListItemDataChanged(QListWidgetItem*)));
}

void MainWindow::onCurrentItemChanged(const QModelIndex &index)
{
   onCurrentItemChanged(index.data(
            PlaylistModel::ENTRY).value<PlaylistEntry>());
}

void MainWindow::onCurrentFileChanged(const QModelIndex &index)
{
   onCurrentItemChanged(getFileContentEntry(
            m_proxyFileModel->mapToSource(index)));
}

void MainWindow::onCurrentItemChanged(const PlaylistEntry &entry)
{
   size_t i;
   const QString &path = entry.path;
   bool acceptDrop     = false;

   for (i = 0; i < 4; i++)
   {
      if (m_thumbnailPixmaps[i])
         delete m_thumbnailPixmaps[i];
      m_thumbnailPixmaps[i] = NULL;
   }

   if (m_playlistModel->isSupportedImage(path))
   {
      /* Use thumbnail widgets to show regular image files. These can
       * be very large (multi-GiB ARGB32 bitmaps after decoding a
       * high-resolution PNG), so do the decode on the loader thread
       * and update the panes when it arrives. Until then the panes
       * show blank, which also clears any image left from a previous
       * selection. */
      QPixmap blank;

      m_pendingPreviewPath = path;

      for (i = 0; i < 4; i++)
         setThumbnail(qt_thumbnail_widget_names[i], blank, false);

      m_previewLoader->request(QModelIndex(), path);

      setCoreActions();
      return;
   }
   else
   {
      QString thumbnailsDir = m_playlistModel->getPlaylistThumbnailsDir(
            entry.dbName);

      /* Clear any pending file-browser preview request: this code
       * path serves the playlist views, not the file browser, so a
       * preview result arriving now would be unwanted. */
      m_pendingPreviewPath = QString();

      for (i = 0; i < 4; i++)
      {
         QString name = m_playlistModel->getSanitizedThumbnailName(
               thumbnailsDir + QString("/")
               + qt_thumbnail_subdirs[i] + QString("/"),
               entry.labelNoExt);
         m_thumbnailPixmaps[i] = new QPixmap(pixmapFromPathRA(name));
      }

      if (      m_currentBrowser == BROWSER_TYPE_PLAYLISTS
            && !currentPlaylistIsSpecial())
         acceptDrop = true;
   }

   for (i = 0; i < 4; i++)
      setThumbnail(qt_thumbnail_widget_names[i],
            *m_thumbnailPixmaps[i], acceptDrop);

   setCoreActions();
}

void MainWindow::onPreviewImageLoaded(const QImage image,
      const QModelIndex & /* index */, const QString &path)
{
   size_t i;

   /* Drop stale results: if the user moved selection while we were
    * decoding, the path we just got back isn't what's currently
    * showing. */
   if (path != m_pendingPreviewPath)
      return;
   if (image.isNull())
      return;

   for (i = 0; i < 4; i++)
   {
      if (m_thumbnailPixmaps[i])
         delete m_thumbnailPixmaps[i];
      m_thumbnailPixmaps[i] = NULL;
   }

   m_thumbnailPixmaps[0] = new QPixmap(QPixmap::fromImage(image));
   for (i = 1; i < 4; i++)
      m_thumbnailPixmaps[i] = new QPixmap(*m_thumbnailPixmaps[0]);

   for (i = 0; i < 4; i++)
      setThumbnail(qt_thumbnail_widget_names[i],
            *m_thumbnailPixmaps[i], false);
}

void MainWindow::setThumbnail(QString widgetName,
      QPixmap &pixmap, bool acceptDrop)
{
   ThumbnailWidget *thumbnail = findChild<ThumbnailWidget*>(widgetName);
   if (thumbnail)
      thumbnail->setPixmap(pixmap, acceptDrop);
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

MainWindow::ViewType MainWindow::getCurrentViewType() { return m_viewType; }
ThumbnailType MainWindow::getCurrentThumbnailType()   { return m_thumbnailType;}

void MainWindow::onCurrentListItemChanged(
      QListWidgetItem *current, QListWidgetItem *previous)
{
   (void)(current);
   (void)(previous);

   initContentTableWidget();

   setCoreActions();
}

QTableView* MainWindow::fileTableView()       { return m_fileTableView; }
QStackedWidget* MainWindow::centralWidget()   { return m_centralWidget; }
FileDropWidget* MainWindow::playlistViews()   { return m_playlistViews; }
QWidget* MainWindow::playlistViewsAndFooter() {return m_playlistViewsAndFooter;}

void MainWindow::onBrowserDownloadsClicked()
{
   QModelIndex index;
   QDir dir(config_get_ptr()->paths.directory_core_assets);
   QString path           = dir.absolutePath();

   m_pendingDirScrollPath = path;

   index                  = m_dirModel->index(path);

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
   m_dirTree->setCurrentIndex(
         m_dirModel->index(config_get_ptr()->paths.directory_menu_content));
   m_dirTree->scrollTo(m_dirTree->currentIndex(), QAbstractItemView::PositionAtTop);
}

ListWidget* MainWindow::playlistListWidget() { return m_listWidget; }
TreeView* MainWindow::dirTreeView() { return m_dirTree; }

void MainWindow::onTimeout()
{
   uint8_t flags = content_get_flags();

   /* Pump the task queue to process pending HTTP transfers etc. */
   task_queue_check();

   if (flags & CONTENT_ST_FLAG_IS_INITED)
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
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_st->selection_ptr     = 0;
#endif
   command_event(CMD_EVENT_UNLOAD_CORE, NULL);
   setCurrentCoreLabel();
   activateWindow();
   raise();
}

void MainWindow::setCurrentCoreLabel()
{
   bool update                       = false;
   struct retro_system_info *sysinfo = &runloop_state_get_ptr()->system.info;
   QString libraryName               = sysinfo->library_name;
   const char *no_core_str           = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE);

   if (     (m_statusLabel->text().isEmpty())
         || (m_currentCore != no_core_str && libraryName.isEmpty())
      )
   {
      m_currentCore           = no_core_str;
      m_currentCoreVersion    = QLatin1String("");
      update                  = true;
   }
   else
   {
      if (      m_currentCore != libraryName
            && !libraryName.isEmpty())
      {
         m_currentCore        = sysinfo->library_name;
         m_currentCoreVersion = 
		 ((!sysinfo->library_version || !*sysinfo->library_version)
               ? "" : sysinfo->library_version);
         update = true;
      }
   }

   if (update)
   {
      QAction *unloadCoreAction = findChild<QAction*>("unloadCoreAction");
      QString text              = QString(PACKAGE_VERSION)
         + QString(" - ")
         + m_currentCore
         + QString(" ")
         + m_currentCoreVersion;
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
   QAction *action            = qobject_cast<QAction*>(sender());
#ifdef HAVE_MENU
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_st->selection_ptr     = 0;
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
   m_loadCoreWindow->setGeometry(QStyle::alignedRect(
            Qt::LeftToRight, Qt::AlignCenter, m_loadCoreWindow->size(),
            geometry()));
   m_loadCoreWindow->initCoreList(extensionFilters);
}

void MainWindow::initContentTableWidget()
{
   QString path;
   QListWidgetItem *item = m_listWidget->currentItem();

   if (!item)
      return;

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
      size_t i;
      QStringList playlists;
      settings_t *settings = config_get_ptr();
      QDir playlistDir(settings->paths.directory_playlist);
      size_t list_size = (size_t)m_playlistFiles.count();

      for (i = 0; i < list_size; i++)
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

QSettings* MainWindow::settings() { return m_settings; }

const char *MainWindow::getCurrentViewTypeString()
{
   if (m_viewType == VIEW_TYPE_ICONS)
      return "icons";
   return "list";
}

const char *MainWindow::getCurrentThumbnailTypeString()
{
   switch (m_thumbnailType)
   {
      case THUMBNAIL_TYPE_SCREENSHOT:
         return "screenshot";
      case THUMBNAIL_TYPE_TITLE_SCREEN:
         return "title";
      case THUMBNAIL_TYPE_LOGO:
         return "logo";
      case THUMBNAIL_TYPE_BOXART:
      default:
         break;
   }
   return "boxart";
}

ThumbnailType MainWindow::getThumbnailTypeFromString(QString thumbnailType)
{
   if (thumbnailType == QLatin1String("boxart"))
      return THUMBNAIL_TYPE_BOXART;
   else if (thumbnailType == QLatin1String("screenshot"))
      return THUMBNAIL_TYPE_SCREENSHOT;
   else if (thumbnailType == QLatin1String("title"))
      return THUMBNAIL_TYPE_TITLE_SCREEN;
   else if (thumbnailType == QLatin1String("logo"))
      return THUMBNAIL_TYPE_LOGO;

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
   textEdit->setHtml(QString("<pre>") + retroarch_contributors_list + QString("</pre>"));

   dialog->resize(480, 640);
   dialog->exec();
}

void MainWindow::showAbout()
{
   QScopedPointer<QDialog> dialog(new QDialog());
   QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
   QString text = QString("RetroArch ")
      + QString(PACKAGE_VERSION)
      + QString("<br><br>")
      + "<a href=\"https://www.libretro.com/\">www.libretro.com</a>"
         "<br><br>"
      + "<a href=\"https://www.retroarch.com/\">www.retroarch.com</a>"
#ifdef HAVE_GIT_VERSION
         "<br><br>"
      + msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION)
      + QString(": ")
      + retroarch_git_version
#endif
      + QString("<br>")
      + msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE)
      + QString(": ")
      + __DATE__;
   QLabel *label = new QLabel(text, dialog.data());
   QPixmap pix = getInvader();
   QLabel *pixLabel = new QLabel(dialog.data());
   QPushButton *contributorsPushButton = new QPushButton(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS),
         dialog.data());

   connect(contributorsPushButton, SIGNAL(clicked()), this,
         SLOT(onContributorsClicked()));
   connect(buttonBox, SIGNAL(accepted()), dialog.data(), SLOT(accept()));
   connect(buttonBox, SIGNAL(rejected()), dialog.data(), SLOT(reject()));

   label->setTextFormat(Qt::RichText);
   label->setAlignment(Qt::AlignCenter);
   label->setTextInteractionFlags(Qt::TextBrowserInteraction);
   label->setOpenExternalLinks(true);

   pixLabel->setAlignment(Qt::AlignCenter);
   pixLabel->setPixmap(pix);

   dialog->setWindowTitle(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT));
   dialog->setLayout(new QVBoxLayout());

   dialog->layout()->addWidget(pixLabel);
   dialog->layout()->addWidget(label);
   dialog->layout()->addWidget(contributorsPushButton);

   dialog->layout()->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum,
            QSizePolicy::Expanding));
   dialog->layout()->addWidget(buttonBox);

   dialog->exec();
}

void MainWindow::showDocs()
{
   QDesktopServices::openUrl(QUrl(DOCS_URL));
}

void MainWindow::onShowErrorMessage(QString msg)
{
   showMessageBox(msg, MainWindow::MSGBOX_TYPE_ERROR,
         Qt::ApplicationModal, false);
}

void MainWindow::onShowInfoMessage(QString msg)
{
   showMessageBox(msg, MainWindow::MSGBOX_TYPE_INFO,
         Qt::ApplicationModal, false);
}

int MainWindow::onExtractArchive(QString path, QString extractionDir,
      QString tempExtension, retro_task_callback_t cb)
{
   size_t i;
   file_archive_transfer_t state;
   struct archive_extract_userdata userdata;
   QByteArray pathArray          = path.toUtf8();
   QByteArray dirArray           = extractionDir.toUtf8();
   QByteArray tmpExtArray        = tempExtension.toUtf8();
   const char *file              = pathArray.constData();
   const char *dir               = dirArray.constData();
   const char *temp_ext          = tmpExtArray.constData();
   struct string_list *file_list = file_archive_get_file_list(file, NULL);
   retro_task_t *decompress_task = NULL;

   if (!file_list || file_list->size == 0)
   {
      showMessageBox("Error: Archive is empty.",
            MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
      RARCH_ERR("[Qt] Downloaded archive is empty?\n");
      return -1;
   }

   for (i = 0; i < file_list->size; i++)
   {
      const char *target_file = file_list->elems[i].data;

      if (!filestream_exists(target_file))
         continue;

      if (filestream_delete(target_file) == 0)
         continue;

      /* If we cannot delete the existing file to update it,
       * rename it out of the way for later cleanup. */
      {
         char temp_path[PATH_MAX_LENGTH];
         size_t _len = strlcpy(temp_path, target_file, sizeof(temp_path));
         strlcpy(temp_path + _len, temp_ext, sizeof(temp_path) - _len);

         if (filestream_exists(temp_path))
         {
            if (filestream_delete(temp_path) != 0)
            {
               showMessageBox(msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE),
                     MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
               RARCH_ERR("[Qt] Could not delete file: \"%s\".\n",
                     target_file);
               string_list_free(file_list);
               return -1;
            }
         }

         if (filestream_rename(target_file, temp_path) != 0)
         {
            showMessageBox(msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE),
                  MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
            RARCH_ERR("[Qt] Could not rename file: \"%s\".\n",
                  target_file);
            string_list_free(file_list);
            return -1;
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
   m_updateProgressDialog->setLabelText(QString(msg_hash_to_str(MSG_EXTRACTING))
         + QString("..."));
   m_updateProgressDialog->setCancelButtonText(QString());
   m_updateProgressDialog->show();

   if (!(decompress_task = (retro_task_t*)task_push_decompress(
         file, dir,
         NULL, NULL, NULL,
         cb, this, NULL, false)))
   {
      m_updateProgressDialog->cancel();
      return -1;
   }

   return 1;
}

static void* ui_window_qt_init(void)
{
   ui_window.qtWindow = new MainWindow();

   return &ui_window;
}

static void ui_window_qt_destroy(void *data)
{
   /* TODO/FIXME - implement? */
}

static void ui_window_qt_set_focused(void *data)
{
   /* TODO/FIXME - implement */
}

static void ui_window_qt_set_visible(void *data,
        bool set_visible)
{
   /* TODO/FIXME - implement */
}

static void ui_window_qt_set_title(void *data, char *buf)
{
   /* TODO/FIXME - implement? */
}

static void ui_window_qt_set_droppable(void *data, bool droppable)
{
   /* TODO/FIXME - implement */
}

static bool ui_window_qt_focused(void *data)
{
   /* TODO/FIXME - implement? */
   return true;
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

static enum ui_msg_window_response ui_msg_window_qt_response(
      ui_msg_window_state *state, QMessageBox::StandardButtons response)
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
   return ui_msg_window_qt_response(state, QMessageBox::critical(
            (QWidget*)state->window, state->title, state->text, flags));
}

static enum ui_msg_window_response ui_msg_window_qt_information(
      ui_msg_window_state *state)
{
   QFlags<QMessageBox::StandardButton> flags = ui_msg_window_qt_buttons(state);
   return ui_msg_window_qt_response(state, QMessageBox::information(
            (QWidget*)state->window, state->title, state->text, flags));
}

static enum ui_msg_window_response ui_msg_window_qt_question(
      ui_msg_window_state *state)
{
   QFlags<QMessageBox::StandardButton> flags = ui_msg_window_qt_buttons(state);
   return ui_msg_window_qt_response(state, QMessageBox::question(
            (QWidget*)state->window, state->title, state->text, flags));
}

static enum ui_msg_window_response ui_msg_window_qt_warning(
      ui_msg_window_state *state)
{
   QFlags<QMessageBox::StandardButton> flags = ui_msg_window_qt_buttons(state);
   return ui_msg_window_qt_response(state, QMessageBox::warning(
            (QWidget*)state->window, state->title, state->text, flags));
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

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)) && (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
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

#ifdef Q_OS_UNIX
   setlocale(LC_NUMERIC, "C");
#ifdef HAVE_WAYLAND
#if (QT_VERSION >= QT_VERSION_CHECK(5, 7, 0))
   /* This needs to match the name of the .desktop file in order for
    * Windows to be correctly associated on Wayland */
   ui_application.app->setDesktopFileName(WAYLAND_APP_ID);
#endif
#endif
#endif
   {
      QPixmap iconPixmap;
      /* Can't declare the pixmap at the top, because:
       * "QPixmap: Must construct a QGuiApplication before a QPixmap" */
      QImage iconImage(16, 16, QImage::Format_ARGB32);
      unsigned char *bits = iconImage.bits();

      memcpy(bits, retroarch_qt_icon_data, 16 * 16 * sizeof(unsigned));

      iconPixmap = QPixmap::fromImage(iconImage);

      ui_application.app->setWindowIcon(QIcon(iconPixmap));
   }

   return &ui_application;
}

static void ui_application_qt_process_events(void)
{
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
   QAbstractEventDispatcher *dispatcher = QApplication::eventDispatcher();
   if (dispatcher && dispatcher->hasPendingEvents())
#endif
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
   QObject(parent) { }
AppHandler::~AppHandler() { }

void AppHandler::exit()
{
   ui_application_qt.exiting = true;

   if (qApp)
      qApp->closeAllWindows();
}

typedef struct ui_companion_qt
{
   ui_application_qt_t *app;
   ui_window_qt_t *window;
} ui_companion_qt_t;

ThumbnailWidget::ThumbnailWidget(QWidget *parent) { }

ThumbnailWidget::ThumbnailWidget(ThumbnailType type, QWidget *parent) :
   QStackedWidget(parent)
   ,m_thumbnailType(type)
   ,m_thumbnailLabel(new ThumbnailLabel(this))
   ,m_dropIndicator(new QLabel(msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_QT_DROP_IMAGE_HERE), this))
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
         const char *string_data = QDir::toNativeSeparators(
               imageString).toUtf8().constData();
         RARCH_ERR("[Qt] Could not read image: \"%s\".\n", string_data);
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

   if (    !m_pixmap
         || m_pixmap->isNull())
   {
      if (m_pixmap)
         delete m_pixmap;
      m_pixmap = new QPixmap(sizeHint());
      m_pixmap->fill(QColor(0, 0, 0, 0));
   }

   if (w > 0 && h > 0 && m_pixmap && !m_pixmap->isNull())
   {
      QPixmap pixmap;
      QPainter pScale;
      int pw               = 0;
      int ph               = 0;
      int newHeight        = (m_pixmap->height()
                           / static_cast<float>(m_pixmap->width())) * width();
      QPixmap pixmapScaled = *m_pixmap;
      unsigned *buf        = new unsigned[w * h];

      if (newHeight > h)
         pixmapScaled = pixmapScaled.scaledToHeight(h, Qt::SmoothTransformation);
      else
         pixmapScaled = pixmapScaled.scaledToWidth(w,  Qt::SmoothTransformation);

      pw = pixmapScaled.width();
      ph = pixmapScaled.height();

      pixmap = QPixmap(w, h);
      pixmap.fill(QColor(0, 0, 0, 0));

      pScale.begin(&pixmap);
      pScale.drawPixmap(QRect((w - pw) / 2, (h - ph) / 2, pw, ph),
            pixmapScaled, pixmapScaled.rect());
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

static void ui_companion_qt_deinit(void *data)
{
   ui_companion_qt_t *handle = (ui_companion_qt_t*)data;

   if (!handle)
      return;

   /* why won't deleteLater() here call the destructor? */
   delete handle->window->qtWindow;

   free(handle);
}

/* ---------------------------------------------------------------- */
/* Helpers split out of ui_companion_qt_init() for readability.      */
/* No functional changes from the original monolithic flow.          */
/* ---------------------------------------------------------------- */

static void qt_companion_build_menubar(MainWindow *mainwindow)
{
   QMenuBar *menu     = mainwindow->menuBar();
   QMenu *fileMenu    = menu->addMenu(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_FILE));
   QMenu *editMenu;
   QMenu *viewMenu;
   QMenu *viewClosedDocksMenu;
   QMenu *helpMenu;
   QAction *loadCoreAction;
   QAction *unloadCoreAction;
   QAction *exitAction;
   QAction *editSearchAction;

   loadCoreAction = fileMenu->addAction(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE), mainwindow,
         SLOT(onLoadCoreClicked()));
   loadCoreAction->setShortcut(QKeySequence("Ctrl+L"));

   unloadCoreAction = fileMenu->addAction(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE), mainwindow,
         SLOT(onUnloadCoreMenuAction()));
   unloadCoreAction->setObjectName("unloadCoreAction");
   unloadCoreAction->setEnabled(false);
   unloadCoreAction->setShortcut(QKeySequence("Ctrl+U"));

   exitAction = fileMenu->addAction(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_EXIT), mainwindow,
         SLOT(close()));
   exitAction->setShortcut(QKeySequence::Quit);

   editMenu = menu->addMenu(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT));
   editSearchAction = editMenu->addAction(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH),
         mainwindow->searchLineEdit(), SLOT(setFocus()));
   editSearchAction->setShortcut(QKeySequence::Find);

   viewMenu = menu->addMenu(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW));
   viewClosedDocksMenu = viewMenu->addMenu(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_CLOSED_DOCKS));
   viewClosedDocksMenu->setObjectName("viewClosedDocksMenu");

   QObject::connect(viewClosedDocksMenu, SIGNAL(aboutToShow()), mainwindow,
         SLOT(onViewClosedDocksAboutToShow()));

   viewMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS),
         mainwindow, SLOT(onCoreOptionsClicked()));
#if defined(HAVE_MENU)
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   viewMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS),
         mainwindow, SLOT(onShaderParamsClicked()));
#endif
#endif

   viewMenu->addSeparator();
   viewMenu->addAction(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS), mainwindow,
            SLOT(onIconViewClicked()));
   viewMenu->addAction(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST), mainwindow,
            SLOT(onListViewClicked()));
   viewMenu->addSeparator();
   viewMenu->addAction(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS),
            mainwindow->viewOptionsDialog(), SLOT(showDialog()));

   helpMenu = menu->addMenu(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_MENU_HELP));
   helpMenu->addAction(QString(msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_DOCUMENTATION)),
               mainwindow, SLOT(showDocs()));
   helpMenu->addAction(QString(msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT))
              + QString("..."), mainwindow, SLOT(showAbout()));
   helpMenu->addAction(QString("About Qt..."), qApp, SLOT(aboutQt()));
}

/* Build the playlist + file-browser tab dock. Returns the dock so the
 * caller can splitDockWidget() the core-selection dock against it. */
static QDockWidget *qt_companion_build_browser_dock(MainWindow *mainwindow)
{
   QFrame *playlistWidget               = new QFrame();
   QFrame *browserWidget                = new QFrame();
   QPushButton *browserDownloadsButton  = new QPushButton(msg_hash_to_str(
         MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY));
   QPushButton *browserUpButton         = new QPushButton(msg_hash_to_str(
         MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_UP));
   QPushButton *browserStartButton      = new QPushButton(msg_hash_to_str(
         MENU_ENUM_LABEL_VALUE_FAVORITES));
   QHBoxLayout *browserButtonsHBoxLayout = new QHBoxLayout();
   QTabWidget *browserAndPlaylistTabWidget = mainwindow->browserAndPlaylistTabWidget();
   QDockWidget *browserAndPlaylistTabDock;

   playlistWidget->setLayout(new QVBoxLayout());
   playlistWidget->setObjectName("playlistWidget");
   playlistWidget->layout()->setContentsMargins(0, 0, 0, 0);
   playlistWidget->layout()->addWidget(mainwindow->playlistListWidget());

   browserWidget->setLayout(new QVBoxLayout());
   browserWidget->setObjectName("browserWidget");
   browserWidget->layout()->setContentsMargins(0, 0, 0, 0);

   QObject::connect(browserDownloadsButton, SIGNAL(clicked()), mainwindow,
         SLOT(onBrowserDownloadsClicked()));
   QObject::connect(browserUpButton, SIGNAL(clicked()), mainwindow,
         SLOT(onBrowserUpClicked()));
   QObject::connect(browserStartButton, SIGNAL(clicked()), mainwindow,
         SLOT(onBrowserStartClicked()));

   browserButtonsHBoxLayout->addWidget(browserUpButton);
   browserButtonsHBoxLayout->addWidget(browserStartButton);
   browserButtonsHBoxLayout->addWidget(browserDownloadsButton);

   qobject_cast<QVBoxLayout*>(browserWidget->layout())->addLayout(browserButtonsHBoxLayout);
   browserWidget->layout()->addWidget(mainwindow->dirTreeView());

   browserAndPlaylistTabWidget->setObjectName("browserAndPlaylistTabWidget");

   /* Several functions depend on the same tab title strings here,
    * so if you change these, make sure to change those too:
    *   setCoreActions()
    *   onTabWidgetIndexChanged()
    *   onCurrentListItemChanged() */
   browserAndPlaylistTabWidget->addTab(playlistWidget,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS));
   browserAndPlaylistTabWidget->addTab(browserWidget,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER));

   browserAndPlaylistTabDock = new QDockWidget(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER), mainwindow);
   qt_dock_configure(browserAndPlaylistTabDock,
         "browserAndPlaylistTabDock", Qt::LeftDockWidgetArea,
         MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER,
         browserAndPlaylistTabWidget);

   qt_dock_add_to(mainwindow, browserAndPlaylistTabDock);

   browserButtonsHBoxLayout->addItem(new QSpacerItem(
            browserAndPlaylistTabWidget->tabBar()->width(),
            20, QSizePolicy::Expanding, QSizePolicy::Minimum));

   return browserAndPlaylistTabDock;
}

/* Build the four boxart/title/screenshot/logo thumbnail docks.
 * The four docks are tabbed against the first one. */
static void qt_companion_build_thumbnail_docks(MainWindow *mainwindow)
{
   /* Maps widget index -> ThumbnailType + display label hash + dock obj name. */
   static const ThumbnailType types[4] = {
      THUMBNAIL_TYPE_BOXART, THUMBNAIL_TYPE_TITLE_SCREEN,
      THUMBNAIL_TYPE_SCREENSHOT, THUMBNAIL_TYPE_LOGO
   };
   static const msg_hash_enums labels[4] = {
      MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART,
      MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN,
      MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT,
      MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_LOGO
   };
   static const char * const dock_obj_names[4] = {
      "thumbnailDock", "thumbnail2Dock", "thumbnail3Dock", "thumbnail4Dock"
   };
   QDockWidget *docks[4];
   int i;

   for (i = 0; i < 4; i++)
   {
      ThumbnailWidget *tw = new ThumbnailWidget(types[i]);
      tw->setObjectName(qt_thumbnail_widget_names[i]);

      QObject::connect(tw, SIGNAL(filesDropped(const QImage&,
                  ThumbnailType)), mainwindow,
                  SLOT(onThumbnailDropped(const QImage&, ThumbnailType)));

      docks[i] = new QDockWidget(msg_hash_to_str(labels[i]), mainwindow);
      qt_dock_configure(docks[i], dock_obj_names[i],
            Qt::RightDockWidgetArea, labels[i], tw);

      qt_dock_add_to(mainwindow, docks[i]);
   }

   for (i = 1; i < 4; i++)
      mainwindow->tabifyDockWidget(docks[0], docks[i]);

   /* When tabifying the dock widgets, the last tab added is selected
    * by default, so we re-select the first tab here. */
   docks[0]->raise();
}

/* Build the core-selection dock (combo box + run/stop/start buttons)
 * and split it vertically against the existing browser/playlist dock. */
static void qt_companion_build_core_selection_dock(MainWindow *mainwindow,
      QDockWidget *browserAndPlaylistTabDock)
{
   QWidget *coreSelectionWidget    = new QWidget();
   QVBoxLayout *launchWithLayout   = new QVBoxLayout();
   QWidget *launchWithWidget       = new QWidget();
   QHBoxLayout *coreComboBoxLayout = new QHBoxLayout();
   QComboBox *launchWithComboBox   = mainwindow->launchWithComboBox();
   QDockWidget *coreSelectionDock;

   coreSelectionWidget->setLayout(new QVBoxLayout());
   launchWithWidget->setLayout(launchWithLayout);

   mainwindow->runPushButton()->setSizePolicy(
         QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding));
   mainwindow->stopPushButton()->setSizePolicy(
         QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding));
   mainwindow->startCorePushButton()->setSizePolicy(
         QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding));

   coreComboBoxLayout->addWidget(launchWithComboBox);
   coreComboBoxLayout->addWidget(mainwindow->startCorePushButton());
   coreComboBoxLayout->addWidget(mainwindow->coreInfoPushButton());
   coreComboBoxLayout->addWidget(mainwindow->runPushButton());
   coreComboBoxLayout->addWidget(mainwindow->stopPushButton());

   mainwindow->stopPushButton()->hide();

   coreComboBoxLayout->setStretchFactor(launchWithComboBox, 1);

   launchWithLayout->addLayout(coreComboBoxLayout);

   coreSelectionWidget->layout()->addWidget(launchWithWidget);
   coreSelectionWidget->layout()->addItem(new QSpacerItem(20,
            mainwindow->browserAndPlaylistTabWidget()->height(),
            QSizePolicy::Minimum, QSizePolicy::Expanding));

   coreSelectionDock = new QDockWidget(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_CORE), mainwindow);
   qt_dock_configure(coreSelectionDock, "coreSelectionDock",
         Qt::LeftDockWidgetArea, MENU_ENUM_LABEL_VALUE_QT_CORE,
         coreSelectionWidget);
   coreSelectionDock->setFixedHeight(coreSelectionDock->minimumSizeHint().height());

   qt_dock_add_to(mainwindow, coreSelectionDock);

   mainwindow->splitDockWidget(browserAndPlaylistTabDock, coreSelectionDock, Qt::Vertical);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
   mainwindow->resizeDocks(QList<QDockWidget*>() << coreSelectionDock,
         QList<int>() << 1, Qt::Vertical);
#endif
}

/* Restore persistent state (limits, geometry, theme, view type, last tab). */
static void qt_companion_restore_settings(MainWindow *mainwindow,
      QSettings *qsettings)
{
   if (qsettings->contains("all_playlists_list_max_count"))
      mainwindow->setAllPlaylistsListMaxCount(qsettings->value(
               "all_playlists_list_max_count", 0).toInt());

   if (qsettings->contains("all_playlists_grid_max_count"))
      mainwindow->setAllPlaylistsGridMaxCount(qsettings->value(
               "all_playlists_grid_max_count", 5000).toInt());

   if (qsettings->contains("thumbnail_cache_limit"))
      mainwindow->setThumbnailCacheLimit(qsettings->value(
               "thumbnail_cache_limit", 500).toInt());
   else
      mainwindow->setThumbnailCacheLimit(500);

   if (qsettings->contains("geometry"))
      if (qsettings->contains("save_geometry"))
         mainwindow->restoreGeometry(qsettings->value(
                  "geometry").toByteArray());

   if (qsettings->contains("options_dialog_geometry"))
      mainwindow->viewOptionsDialog()->restoreGeometry(
            qsettings->value("options_dialog_geometry").toByteArray());

   if (qsettings->contains("save_dock_positions"))
      if (qsettings->contains("dock_positions"))
         mainwindow->restoreState(qsettings->value(
                  "dock_positions").toByteArray());

   if (qsettings->contains("file_browser_table_headers"))
      mainwindow->fileTableView()->horizontalHeader()->restoreState(
            qsettings->value("file_browser_table_headers").toByteArray());
   else
      mainwindow->fileTableView()->horizontalHeader()->resizeSection(0, 300);

   if (qsettings->contains("icon_view_zoom"))
      mainwindow->setIconViewZoom(qsettings->value(
               "icon_view_zoom", 50).toInt());

   if (qsettings->contains("theme"))
   {
      QString themeStr        = qsettings->value("theme").toString();
      MainWindow::Theme theme = mainwindow->getThemeFromString(themeStr);

      if (     qsettings->contains("custom_theme")
            && theme == MainWindow::THEME_CUSTOM)
         mainwindow->setCustomThemeFile(
               qsettings->value("custom_theme").toString());

      mainwindow->setTheme(theme);
   }
   else
      mainwindow->setTheme();

   if (qsettings->contains("view_type"))
   {
      QString viewType = qsettings->value("view_type", "list").toString();

      if (viewType == QLatin1String("icons"))
         mainwindow->setCurrentViewType(MainWindow::VIEW_TYPE_ICONS);
      else
         mainwindow->setCurrentViewType(MainWindow::VIEW_TYPE_LIST);
   }
   else
      mainwindow->setCurrentViewType(MainWindow::VIEW_TYPE_LIST);

   if (qsettings->contains("icon_view_thumbnail_type"))
   {
      QString thumbnailType = qsettings->value("icon_view_thumbnail_type",
            "boxart").toString();

      if (thumbnailType == QLatin1String("screenshot"))
         mainwindow->setCurrentThumbnailType(THUMBNAIL_TYPE_SCREENSHOT);
      else if (thumbnailType == QLatin1String("title"))
         mainwindow->setCurrentThumbnailType(THUMBNAIL_TYPE_TITLE_SCREEN);
      else if (thumbnailType == QLatin1String("logo"))
         mainwindow->setCurrentThumbnailType(THUMBNAIL_TYPE_LOGO);
      else
         mainwindow->setCurrentThumbnailType(THUMBNAIL_TYPE_BOXART);
   }
}

/* Set the initial playlist row to match the user's saved choice; if not
 * found, fall back to the first non-hidden row. */
static void qt_companion_select_initial_playlist(QListWidget *listWidget,
      const QString &initialPlaylist)
{
   int i;
   bool found = false;

   for (i = 0; i < listWidget->count(); i++)
   {
      QListWidgetItem *item = listWidget->item(i);
      QString path;

      if (!item)
         continue;

      path = item->data(Qt::UserRole).toString();

      if (path == initialPlaylist)
      {
         found = true;
         listWidget->setRowHidden(i, false);
         listWidget->setCurrentRow(i);
         break;
      }
   }

   if (found)
      return;

   /* Couldn't find the user's initial playlist, just find anything. */
   for (i = 0; i < listWidget->count(); i++)
   {
      if (!listWidget->isRowHidden(i))
      {
         listWidget->setCurrentRow(i);
         break;
      }
   }
}

static void* ui_companion_qt_init(void)
{
   QString initialPlaylist;
   QRect desktopRect;
   ui_companion_qt_t *handle               = (ui_companion_qt_t*)
      calloc(1, sizeof(*handle));
   MainWindow *mainwindow                  = NULL;
   QScreen *screen                         = NULL;
   QStackedWidget *centralWidget           = NULL;
   QStackedWidget *widget                  = NULL;
   QTabWidget *browserAndPlaylistTabWidget = NULL;
   QDockWidget *browserAndPlaylistTabDock  = NULL;
   QSettings *qsettings                    = NULL;
   QListWidget *listWidget                 = NULL;

   if (!handle)
      return NULL;

   handle->app     = static_cast<ui_application_qt_t*>
      (ui_application_qt.initialize());
   handle->window  = static_cast<ui_window_qt_t*>(ui_window_qt.init());

   screen          = qApp->primaryScreen();
   desktopRect     = screen->availableGeometry();

   mainwindow      = handle->window->qtWindow;
   qsettings       = mainwindow->settings();

   initialPlaylist = qsettings->value("initial_playlist",
         mainwindow->getSpecialPlaylistPath(SPECIAL_PLAYLIST_HISTORY)).toString();

   mainwindow->resize(((desktopRect.width()) < (INITIAL_WIDTH) ? (desktopRect.width()) : (INITIAL_WIDTH)),
         ((desktopRect.height()) < (INITIAL_HEIGHT) ? (desktopRect.height()) : (INITIAL_HEIGHT)));
   mainwindow->setGeometry(QStyle::alignedRect(Qt::LeftToRight,
            Qt::AlignCenter, mainwindow->size(), desktopRect));

   mainwindow->setWindowTitle("RetroArch");
   mainwindow->setDockOptions(QMainWindow::AnimatedDocks
                            | QMainWindow::AllowNestedDocks
                            | QMainWindow::AllowTabbedDocks
                            | GROUPED_DRAGGING);

   listWidget = mainwindow->playlistListWidget();
   widget     = mainwindow->playlistViews();
   widget->setContextMenuPolicy(Qt::CustomContextMenu);

   QObject::connect(widget, SIGNAL(filesDropped(QStringList)),
         mainwindow, SLOT(onPlaylistFilesDropped(QStringList)));
   QObject::connect(widget, SIGNAL(enterPressed()), mainwindow,
         SLOT(onDropWidgetEnterPressed()));
   QObject::connect(widget, SIGNAL(deletePressed()), mainwindow,
         SLOT(deleteCurrentPlaylistItem()));
   QObject::connect(widget, SIGNAL(customContextMenuRequested(const QPoint&)),
         mainwindow, SLOT(onFileDropWidgetContextMenuRequested(const QPoint&)));

   centralWidget = mainwindow->centralWidget();
   centralWidget->addWidget(mainwindow->playlistViewsAndFooter());
   centralWidget->addWidget(mainwindow->fileTableView());
   mainwindow->setCentralWidget(centralWidget);

   qt_companion_build_menubar(mainwindow);
   browserAndPlaylistTabDock = qt_companion_build_browser_dock(mainwindow);
   qt_companion_build_thumbnail_docks(mainwindow);
   qt_companion_build_core_selection_dock(mainwindow,
         browserAndPlaylistTabDock);
   qt_companion_restore_settings(mainwindow, qsettings);

   browserAndPlaylistTabWidget = mainwindow->browserAndPlaylistTabWidget();

   /* We make sure to hook up the tab widget callback only after the tabs
    * themselves have been added, but before changing to a specific one,
    * to avoid the callback firing before the view type is set. */
   QObject::connect(browserAndPlaylistTabWidget, SIGNAL(currentChanged(int)),
         mainwindow, SLOT(onTabWidgetIndexChanged(int)));

   /* Setting the last tab must come after setting the view type. */
   if (qsettings->contains("save_last_tab"))
   {
      int lastTabIndex = qsettings->value("last_tab", 0).toInt();

      if (     lastTabIndex >= 0
            && browserAndPlaylistTabWidget->count() > lastTabIndex)
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

   qt_companion_select_initial_playlist(listWidget, initialPlaylist);

   mainwindow->initContentTableWidget();

   return handle;
}

static void ui_companion_qt_toggle(void *data, bool force)
{
   static bool already_started = false;
   ui_companion_qt_t *handle   = (ui_companion_qt_t*)data;
   ui_window_qt_t *win_handle  = (ui_window_qt_t*)handle->window;
   settings_t *settings        = config_get_ptr();
   bool ui_companion_toggle    = settings->bools.ui_companion_toggle;
   bool video_fullscreen       = settings->bools.video_fullscreen;
   bool mouse_grabbed          = (input_state_get_ptr()->flags
         & INP_FLAG_GRAB_MOUSE_STATE) ? true : false;

   if (ui_companion_toggle || force)
   {
      video_driver_state_t *video_st = video_state_get_ptr();

      if (mouse_grabbed)
         command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);
      if (     video_st->poke
            && video_st->poke->show_mouse)
         video_st->poke->show_mouse(video_st->data, true);

      if (video_fullscreen)
         command_event(CMD_EVENT_FULLSCREEN_TOGGLE, NULL);

      win_handle->qtWindow->activateWindow();
      win_handle->qtWindow->raise();
      win_handle->qtWindow->show();

      if (    video_st
          && (video_st->flags & VIDEO_FLAG_STARTED_FULLSCREEN))
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
#if defined(HAVE_MENU)
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         RARCH_LOG("[Qt] Reloading shader parameters.\n");
         win_handle->qtWindow->deferReloadShaderParams();
#endif
#endif
         break;
      default:
         break;
   }
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
   if (handle && (win_handle = (ui_window_qt_t*)handle->window))
      win_handle->qtWindow->showStatusMessage(msg, priority, duration, flush);
}

ui_companion_driver_t ui_companion_qt = {
   ui_companion_qt_init,
   ui_companion_qt_deinit,
   ui_companion_qt_toggle,
   ui_companion_qt_event_command,
   ui_companion_qt_notify_refresh,
   ui_companion_qt_msg_queue_push,
   NULL,
   NULL,
   ui_companion_qt_log_msg,
   ui_companion_qt_is_active,
   NULL, /* get_app_icons */
   NULL, /* set_app_icon */
   NULL, /* get_app_icon_texture */
   &ui_browser_window_qt,
   &ui_msg_window_qt,
   &ui_window_qt,
   &ui_application_qt,
   "qt",
};

#define CORE_NAME_COLUMN    0
#define CORE_VERSION_COLUMN 1

LoadCoreTableWidget::LoadCoreTableWidget(QWidget *parent) :
   QTableWidget(parent) { }

void LoadCoreTableWidget::keyPressEvent(QKeyEvent *event)
{
   int key = event->key();
   if (   key == Qt::Key_Return
       || key == Qt::Key_Enter)
   {
      event->accept();
      emit enterPressed();
   }
   else
      QTableWidget::keyPressEvent(event);
}

LoadCoreWindow::LoadCoreWindow(QWidget *parent) :
   QMainWindow(parent)
   ,m_layout()
   ,m_table(new LoadCoreTableWidget())
   ,m_statusLabel(new QLabel())
{
   QHBoxLayout             *hbox = new QHBoxLayout();
   QPushButton *customCoreButton = new QPushButton(
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_LOAD_CUSTOM_CORE));

   connect(customCoreButton, SIGNAL(clicked()), this,
         SLOT(onLoadCustomCoreClicked()));
   connect(m_table, SIGNAL(enterPressed()), this, SLOT(onCoreEnterPressed()));
   connect(m_table, SIGNAL(cellDoubleClicked(int,int)), this,
         SLOT(onCellDoubleClicked(int,int)));

   setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE));

   setCentralWidget(new QWidget());

   centralWidget()->setLayout(&m_layout);

   hbox->addWidget(customCoreButton);
   hbox->addItem(new QSpacerItem(width(),
            20, QSizePolicy::Expanding, QSizePolicy::Minimum));

   m_layout.addWidget(m_table);
   m_layout.addLayout(hbox);

   statusBar()->addPermanentWidget(m_statusLabel);
}

void LoadCoreWindow::closeEvent(QCloseEvent *event)
{
   emit windowClosed();
   QWidget::closeEvent(event);
}

void LoadCoreWindow::keyPressEvent(QKeyEvent *event)
{
   int key = event->key();
   if (key == Qt::Key_Escape)
   {
      event->accept();
      close();
   }
   else
      QMainWindow::keyPressEvent(event);
}

void LoadCoreWindow::setStatusLabel(QString label)
{
   m_statusLabel->setText(label);
}

void LoadCoreWindow::onCellDoubleClicked(int, int)
{
   onCoreEnterPressed();
}

void LoadCoreWindow::loadCore(const char *path)
{
   QProgressDialog progress(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_LOADING_CORE), QString(), 0, 0, this);
   progress.setWindowTitle(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE));
   progress.setMinimumDuration(0);
   progress.setValue(progress.minimum());
   progress.show();

   /* Because core loading will block, we need to go ahead and
    * process pending events that would allow the progress dialog
    * to fully show its contents before actually starting the
    * core loading process. Must call processEvents() twice. */
   qApp->processEvents();
   qApp->processEvents();

#ifdef HAVE_DYNAMIC
   path_set(RARCH_PATH_CORE, path);

   command_event(CMD_EVENT_CORE_INFO_DEINIT, NULL);
   command_event(CMD_EVENT_CORE_INFO_INIT, NULL);

   core_info_init_current_core();

   if (!command_event(CMD_EVENT_LOAD_CORE, NULL))
   {
      QMessageBox::critical(this, msg_hash_to_str(MSG_ERROR),
            msg_hash_to_str(MSG_FAILED_TO_OPEN_LIBRETRO_CORE));
      return;
   }

   setProperty("last_launch_with_index", -1);

   emit coreLoaded();
#endif
}

void LoadCoreWindow::onCoreEnterPressed()
{
   QTableWidgetItem *selectedCoreItem =
      m_table->item(m_table->currentRow(), CORE_NAME_COLUMN);
   QVariantHash                  hash = selectedCoreItem->data(
         Qt::UserRole).toHash();
   QString                       path = hash["path"].toString();

   loadCore(path.toUtf8().constData());
}

void LoadCoreWindow::onLoadCustomCoreClicked()
{
   QString path;
   QByteArray pathArray;
   char filters[128];
   const char *pathData          = NULL;
   settings_t *settings          = config_get_ptr();
   const char *path_dir_libretro = settings->paths.directory_libretro;
   size_t _len  = strlcpy(filters, "Cores (*.", sizeof(filters));
   _len += frontend_driver_get_core_extension(filters + _len, sizeof(filters) - _len);
   strlcpy(filters + _len, ");;All Files (*.*)", sizeof(filters) - _len);

   path                          = QFileDialog::getOpenFileName(
         this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE),
         path_dir_libretro, filters, NULL);

   if (path.isEmpty())
      return;

   pathArray.append(path.toUtf8());
   pathData                      = pathArray.constData();

   loadCore(pathData);
}

void LoadCoreWindow::initCoreList(const QStringList &extensionFilters)
{
   int j;
   unsigned i;
   QStringList horizontal_header_labels;
   core_info_list_t *cores = NULL;
   QScreen *desktop = qApp->primaryScreen();
   QRect desktopRect       = desktop->availableGeometry();

   horizontal_header_labels << msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NAME);
   horizontal_header_labels << msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE_VERSION);

   core_info_get_list(&cores);

   m_table->clear();
   m_table->setColumnCount(0);
   m_table->setRowCount(0);
   m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
   m_table->setSelectionMode(QAbstractItemView::SingleSelection);
   m_table->setSortingEnabled(false);
   m_table->setColumnCount(2);
   m_table->setHorizontalHeaderLabels(horizontal_header_labels);

   if (cores)
   {
      m_table->setRowCount(cores->count);

      for (i = 0; i < cores->count; i++)
      {
         QVariantHash hash;
         core_info_t              *core = core_info_get(cores, i);
         QTableWidgetItem    *name_item = NULL;
         QTableWidgetItem *version_item = new QTableWidgetItem(core->display_version);
         const char               *name = core->display_name;

         if (!name || !*name)
            name                        = path_basename(core->path);

         name_item                      = new QTableWidgetItem(name);

         hash["path"]                   = QByteArray(core->path);
         hash["extensions"]             = QString(core->supported_extensions).split('|');

         name_item->setData(Qt::UserRole, hash);
         name_item->setFlags(name_item->flags() & ~Qt::ItemIsEditable);
         version_item->setFlags(version_item->flags() & ~Qt::ItemIsEditable);

         m_table->setItem(i, CORE_NAME_COLUMN, name_item);
         m_table->setItem(i, CORE_VERSION_COLUMN, version_item);
      }
   }

   if (!extensionFilters.isEmpty())
   {
      QVector<int> rowsToHide;

      for (j = 0; j < m_table->rowCount(); j++)
      {
         int k;
         QVariantHash hash;
         QStringList extensions;
         bool             found = false;
         QTableWidgetItem *item = m_table->item(j, CORE_NAME_COLUMN);

         if (!item)
            continue;

         hash       = item->data(Qt::UserRole).toHash();
         extensions = hash["extensions"].toStringList();

         if (!extensions.isEmpty())
         {
            for (k = 0; k < extensions.size(); k++)
            {
               QString ext = extensions.at(k).toLower();

               if (extensionFilters.contains(ext, Qt::CaseInsensitive))
               {
                  found = true;
                  break;
               }
            }

            if (!found)
               rowsToHide.append(j);
         }
      }

      if (rowsToHide.size() != m_table->rowCount())
      {
         int i;
         for (i = 0; i < rowsToHide.count() && rowsToHide.count() > 0; i++)
         {
            const int &row = rowsToHide.at(i);
            m_table->setRowHidden(row, true);
         }
      }
   }

   m_table->setSortingEnabled(true);
   m_table->resizeColumnsToContents();
   m_table->sortByColumn(0, Qt::AscendingOrder);
   m_table->selectRow(0);
   m_table->setAlternatingRowColors(true);

   resize(((desktopRect.width()) < (contentsMargins().left()
            + m_table->horizontalHeader()->length()
            + contentsMargins().right()) ? (desktopRect.width()) : (contentsMargins().left()
            + m_table->horizontalHeader()->length()
            + contentsMargins().right())), height());
}
