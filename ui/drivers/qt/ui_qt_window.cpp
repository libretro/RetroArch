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
#include <QStyle>
#include <QTimer>
#include <QLabel>
#include <QFileSystemModel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTableWidget>
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

#include "../ui_qt.h"
#include "ui_qt_load_core_window.h"
#include "ui_qt_themes.h"
#include "flowlayout.h"

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
#include "../../../menu/menu_driver.h"
#include "../../../tasks/tasks_internal.h"
#include <string/stdstring.h>
#include <encodings/utf.h>
#include <file/file_path.h>
#include <file/archive_file.h>
#include <math.h>
}

#define TIMER_MSEC 1000 /* periodic timer for gathering statistics */
#define STATUS_MSG_THROTTLE_MSEC 250

#ifndef COLLECTION_SIZE
#define COLLECTION_SIZE 99999
#endif

#define GENERIC_FOLDER_ICON "/xmb/dot-art/png/folder.png"
#define ICON_PATH "/xmb/dot-art/png/"
#define THUMBNAIL_BOXART "Named_Boxarts"
#define THUMBNAIL_SCREENSHOT "Named_Snaps"
#define THUMBNAIL_TITLE "Named_Titles"
#define ALL_PLAYLISTS_TOKEN "|||ALL|||"
#define HIRAGANA_START 0x3041U
#define HIRAGANA_END 0x3096U
#define KATAKANA_START 0x30A1U
#define KATAKANA_END 0x30F6U
#define HIRA_KATA_OFFSET (KATAKANA_START - HIRAGANA_START)

static ui_window_qt_t ui_window = {0};

enum CoreSelection
{
   CORE_SELECTION_CURRENT,
   CORE_SELECTION_PLAYLIST_SAVED,
   CORE_SELECTION_PLAYLIST_DEFAULT,
   CORE_SELECTION_ASK,
   CORE_SELECTION_LOAD_CORE
};

static double lerp(double x, double y, double a, double b, double d) {
  return a + (b - a) * ((double)(d - x) / (double)(y - x));
}

/* https://stackoverflow.com/questions/7246622/how-to-create-a-slider-with-a-non-linear-scale */
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

#ifdef HAVE_LIBRETRODB
static void scan_finished_handler(void *task_data, void *user_data, const char *err)
{
   menu_ctx_environment_t menu_environ;
   menu_environ.type = MENU_ENVIRON_RESET_HORIZONTAL_LIST;
   menu_environ.data = NULL;

   (void)task_data;
   (void)user_data;
   (void)err;

   menu_driver_ctl(RARCH_MENU_CTL_ENVIRONMENT, &menu_environ);

   if (!ui_window.qtWindow->settings()->value("scan_finish_confirm", true).toBool())
      return;

   if (!ui_window.qtWindow->showMessageBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED), MainWindow::MSGBOX_TYPE_INFO, Qt::ApplicationModal))
      ui_window.qtWindow->settings()->setValue("scan_finish_confirm", false);
}
#endif

inline static bool comp_string_lower(const QString &lhs, const QString &rhs)
{
   return lhs.toLower() < rhs.toLower();
}

inline static bool comp_hash_ui_display_name_key_lower(const QHash<QString, QString> &lhs, const QHash<QString, QString> &rhs)
{
   return lhs.value("ui_display_name").toLower() < rhs.value("ui_display_name").toLower();
}

inline static bool comp_hash_label_key_lower(const QHash<QString, QString> &lhs, const QHash<QString, QString> &rhs)
{
   return lhs.value("label").toLower() < rhs.value("label").toLower();
}

static void addDirectoryFilesToList(QStringList &list, QDir &dir)
{
   QStringList dirList = dir.entryList(QStringList(), QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
   int i;

   for (i = 0; i < dirList.count(); i++)
   {
      QString path(dir.path() + "/" + dirList.at(i));
      QFileInfo fileInfo(path);

      if (fileInfo.isDir())
      {
         QDir fileInfoDir(path);

         addDirectoryFilesToList(list, fileInfoDir);
         continue;
      }

      if (fileInfo.isFile())
      {
         list.append(fileInfo.absoluteFilePath());
      }
   }
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

FileDropWidget::FileDropWidget(QWidget *parent) :
   QWidget(parent)
{
   setAcceptDrops(true);
}

void FileDropWidget::paintEvent(QPaintEvent *event)
{
   QStyleOption o;
   QPainter p;
   o.initFrom(this);
   p.begin(this);
   style()->drawPrimitive(
      QStyle::PE_Widget, &o, &p, this);
   p.end();

   QWidget::paintEvent(event);
}

void FileDropWidget::keyPressEvent(QKeyEvent *event)
{
   if (event->key() == Qt::Key_Delete)
   {
      event->accept();
      emit deletePressed();
   }
   else
      QWidget::keyPressEvent(event);
}

void FileDropWidget::dragEnterEvent(QDragEnterEvent *event)
{
   const QMimeData *data = event->mimeData();

   if (data->hasUrls())
      event->acceptProposedAction();
}

void FileDropWidget::dropEvent(QDropEvent *event)
{
   const QMimeData *data = event->mimeData();

   if (data->hasUrls())
   {
      QList<QUrl> urls = data->urls();
      QStringList files;
      int i;

      for (i = 0; i < urls.count(); i++)
      {
         QString path(urls.at(i).toLocalFile());

         files.append(path);
      }

      emit filesDropped(files);
   }
}

TableWidget::TableWidget(QWidget *parent) :
   QTableWidget(parent)
{
}

void TableWidget::keyPressEvent(QKeyEvent *event)
{
   if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
   {
      event->accept();
      emit enterPressed();
   }
   else if (event->key() == Qt::Key_Delete)
   {
      event->accept();
      emit deletePressed();
   }
   else
      QTableWidget::keyPressEvent(event);
}

CoreInfoLabel::CoreInfoLabel(QString text, QWidget *parent) :
   QLabel(text, parent)
{
   setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
}

CoreInfoDialog::CoreInfoDialog(MainWindow *mainwindow, QWidget *parent) :
   QDialog(parent)
   ,m_formLayout(new QFormLayout())
   ,m_mainwindow(mainwindow)
{
   QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);

   connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
   connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

   setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFORMATION));

   m_formLayout->setFormAlignment(Qt::AlignCenter);
   m_formLayout->setLabelAlignment(Qt::AlignCenter);

   setLayout(new QVBoxLayout());

   qobject_cast<QVBoxLayout*>(layout())->addLayout(m_formLayout);
   layout()->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));
   layout()->addWidget(buttonBox);
}

void CoreInfoDialog::showCoreInfo()
{
   int row = 0;
   int rowCount = m_formLayout->rowCount();
   int i = 0;
   QVector<QHash<QString, QString> > infoList = m_mainwindow->getCoreInfo();

   if (rowCount > 0)
   {
      for (row = 0; row < rowCount; row++)
      {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
         /* removeRow() and takeRow() was only added in 5.8! */
         m_formLayout->removeRow(0);
#else
         /* something is buggy here... sometimes items appear duplicated, and other times not */
         QLayoutItem *item = m_formLayout->itemAt(0);
         QWidget *w = NULL;

         if (item)
         {
            w = item->widget();

            if (w)
            {
               QWidget *label = m_formLayout->labelForField(w);

               if (label)
                  delete label;

               m_formLayout->removeWidget(w);

               delete w;
            }
         }
#endif
      }
   }

   if (infoList.count() == 0)
      return;

   for (i = 0; i < infoList.count(); i++)
   {
      const QHash<QString, QString> &line = infoList.at(i);
      QLabel *label = new QLabel(line.value("key"));
      CoreInfoLabel *value = new CoreInfoLabel(line.value("value"));
      QString labelStyle = line.value("label_style");
      QString valueStyle = line.value("value_style");

      if (!labelStyle.isEmpty())
         label->setStyleSheet(labelStyle);

      if (!valueStyle.isEmpty())
         value->setStyleSheet(valueStyle);

      m_formLayout->addRow(label, value);
   }

   show();
}

PlaylistEntryDialog::PlaylistEntryDialog(MainWindow *mainwindow, QWidget *parent) :
   QDialog(parent)
   ,m_mainwindow(mainwindow)
   ,m_settings(mainwindow->settings())
   ,m_nameLineEdit(new QLineEdit(this))
   ,m_pathLineEdit(new QLineEdit(this))
   ,m_coreComboBox(new QComboBox(this))
   ,m_databaseComboBox(new QComboBox(this))
{
   QFormLayout *form = new QFormLayout();
   QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
   QVBoxLayout *databaseVBoxLayout = new QVBoxLayout();
   QHBoxLayout *pathHBoxLayout = new QHBoxLayout();
   QLabel *databaseLabel = new QLabel(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FOR_THUMBNAILS), this);
   QToolButton *pathPushButton = new QToolButton(this);

   pathPushButton->setText("...");

   pathHBoxLayout->addWidget(m_pathLineEdit);
   pathHBoxLayout->addWidget(pathPushButton);

   databaseVBoxLayout->addWidget(m_databaseComboBox);
   databaseVBoxLayout->addWidget(databaseLabel);

   setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY));

   form->setFormAlignment(Qt::AlignCenter);
   form->setLabelAlignment(Qt::AlignCenter);

   setLayout(new QVBoxLayout(this));

   connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
   connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

   connect(this, SIGNAL(accepted()), this, SLOT(onAccepted()));
   connect(this, SIGNAL(rejected()), this, SLOT(onRejected()));

   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_NAME), m_nameLineEdit);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_PATH), pathHBoxLayout);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_CORE), m_coreComboBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE), databaseVBoxLayout);

   qobject_cast<QVBoxLayout*>(layout())->addLayout(form);
   layout()->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));
   layout()->addWidget(buttonBox);

   connect(pathPushButton, SIGNAL(clicked()), this, SLOT(onPathClicked()));
}

void PlaylistEntryDialog::onPathClicked()
{
   QString filePath = QFileDialog::getOpenFileName(this);

   if (filePath.isEmpty())
      return;

   m_pathLineEdit->setText(filePath);
}

void PlaylistEntryDialog::loadPlaylistOptions()
{
   core_info_list_t *core_info_list = NULL;
   const core_info_t *core_info = NULL;
   unsigned i = 0;
   int j = 0;

   m_nameLineEdit->clear();
   m_pathLineEdit->clear();
   m_coreComboBox->clear();
   m_databaseComboBox->clear();

   m_coreComboBox->addItem(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK));
   m_databaseComboBox->addItem(QString("<") + msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE) + ">", QFileInfo(m_mainwindow->getCurrentPlaylistPath()).fileName().remove(file_path_str(FILE_PATH_LPL_EXTENSION)));

   core_info_get_list(&core_info_list);

   if (core_info_list && core_info_list->count > 0)
   {
      QVector<QHash<QString, QString> > allCores;
      QStringList allDatabases;

      for (i = 0; i < core_info_list->count; i++)
      {
         const core_info_t *core = &core_info_list->list[i];
         QStringList databases = QString(core->databases).split("|");
         QHash<QString, QString> hash;
         QString ui_display_name;

         hash["core_name"] = core->core_name;
         hash["core_display_name"] = core->display_name;
         hash["core_path"] = core->path;
         hash["core_databases"] = core->databases;

         ui_display_name = hash.value("core_name");

         if (ui_display_name.isEmpty())
            ui_display_name = hash.value("core_display_name");
         if (ui_display_name.isEmpty())
            ui_display_name = QFileInfo(hash.value("core_path")).fileName();
         if (ui_display_name.isEmpty())
            continue;

         hash["ui_display_name"] = ui_display_name;

         for (j = 0; j < databases.count(); j++)
         {
            QString database = databases.at(j);

            if (database.isEmpty())
               continue;

            if (!allDatabases.contains(database))
               allDatabases.append(database);
         }

         if (!allCores.contains(hash))
            allCores.append(hash);
      }

      std::sort(allCores.begin(), allCores.end(), comp_hash_ui_display_name_key_lower);
      std::sort(allDatabases.begin(), allDatabases.end(), comp_string_lower);

      for (j = 0; j < allCores.count(); j++)
      {
         const QHash<QString, QString> &hash = allCores.at(j);

         m_coreComboBox->addItem(hash.value("ui_display_name"), QVariant::fromValue(hash));
      }

      for (j = 0; j < allDatabases.count(); j++)
      {
         QString database = allDatabases.at(j);
         m_databaseComboBox->addItem(database, database);
      }
   }
}

void PlaylistEntryDialog::setEntryValues(const QHash<QString, QString> &contentHash)
{
   QString db;
   QString coreName = contentHash.value("core_name");
   int foundDB = 0;
   int i = 0;

   loadPlaylistOptions();

   if (contentHash.isEmpty())
   {
      m_nameLineEdit->setText(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE));
      m_pathLineEdit->setText(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE));
      m_nameLineEdit->setEnabled(false);
      m_pathLineEdit->setEnabled(false);
   }
   else
   {
      m_nameLineEdit->setEnabled(true);
      m_pathLineEdit->setEnabled(true);
      m_nameLineEdit->setText(contentHash.value("label"));
      m_pathLineEdit->setText(contentHash.value("path"));
   }

   for (i = 0; i < m_coreComboBox->count(); i++)
   {
      const QHash<QString, QString> hash = m_coreComboBox->itemData(i, Qt::UserRole).value<QHash<QString, QString> >();

      if (hash.isEmpty() || coreName.isEmpty())
         continue;

      if (hash.value("core_name") == coreName)
      {
         m_coreComboBox->setCurrentIndex(i);
         break;
      }
   }

   db = contentHash.value("db_name");

   if (!db.isEmpty())
   {
      foundDB = m_databaseComboBox->findText(db);

      if (foundDB >= 0)
         m_databaseComboBox->setCurrentIndex(foundDB);
   }
}

const QHash<QString, QString> PlaylistEntryDialog::getSelectedCore()
{
   return m_coreComboBox->currentData(Qt::UserRole).value<QHash<QString, QString> >();
}

const QString PlaylistEntryDialog::getSelectedName()
{
   return m_nameLineEdit->text();
}

const QString PlaylistEntryDialog::getSelectedPath()
{
   return m_pathLineEdit->text();
}

const QString PlaylistEntryDialog::getSelectedDatabase()
{
   return m_databaseComboBox->currentData(Qt::UserRole).toString();
}

void PlaylistEntryDialog::onAccepted()
{
}

void PlaylistEntryDialog::onRejected()
{
}

bool PlaylistEntryDialog::showDialog(const QHash<QString, QString> &hash)
{
   loadPlaylistOptions();
   setEntryValues(hash);

   if (exec() == QDialog::Accepted)
      return true;

   return false;
}

void PlaylistEntryDialog::hideDialog()
{
   reject();
}

ViewOptionsDialog::ViewOptionsDialog(MainWindow *mainwindow, QWidget *parent) :
   QDialog(parent)
   ,m_mainwindow(mainwindow)
   ,m_settings(mainwindow->settings())
   ,m_saveGeometryCheckBox(new QCheckBox(this))
   ,m_saveDockPositionsCheckBox(new QCheckBox(this))
   ,m_saveLastTabCheckBox(new QCheckBox(this))
   ,m_showHiddenFilesCheckBox(new QCheckBox(this))
   ,m_themeComboBox(new QComboBox(this))
   ,m_highlightColorPushButton(new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CHOOSE), this))
   ,m_highlightColor()
   ,m_highlightColorLabel(new QLabel(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR), this))
   ,m_customThemePath()
   ,m_suggestLoadedCoreFirstCheckBox(new QCheckBox(this))
   ,m_allPlaylistsListMaxCountSpinBox(new QSpinBox(this))
   ,m_allPlaylistsGridMaxCountSpinBox(new QSpinBox(this))
{
   QFormLayout *form = new QFormLayout();
   QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

   setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE));

   m_themeComboBox->addItem(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT), MainWindow::THEME_SYSTEM_DEFAULT);
   m_themeComboBox->addItem(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_DARK), MainWindow::THEME_DARK);
   m_themeComboBox->addItem(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM), MainWindow::THEME_CUSTOM);

   m_allPlaylistsListMaxCountSpinBox->setRange(0, 99999);
   m_allPlaylistsGridMaxCountSpinBox->setRange(0, 99999);

   form->setFormAlignment(Qt::AlignCenter);
   form->setLabelAlignment(Qt::AlignCenter);

   setLayout(new QVBoxLayout(this));

   connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
   connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

   connect(this, SIGNAL(accepted()), this, SLOT(onAccepted()));
   connect(this, SIGNAL(rejected()), this, SLOT(onRejected()));

   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY), m_saveGeometryCheckBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS), m_saveDockPositionsCheckBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB), m_saveLastTabCheckBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES), m_showHiddenFilesCheckBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST), m_suggestLoadedCoreFirstCheckBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT), m_allPlaylistsListMaxCountSpinBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT), m_allPlaylistsGridMaxCountSpinBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME), m_themeComboBox);
   form->addRow(m_highlightColorLabel, m_highlightColorPushButton);

   qobject_cast<QVBoxLayout*>(layout())->addLayout(form);
   layout()->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));
   layout()->addWidget(buttonBox);

   loadViewOptions();

   connect(m_themeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onThemeComboBoxIndexChanged(int)));
   connect(m_highlightColorPushButton, SIGNAL(clicked()), this, SLOT(onHighlightColorChoose()));
}

void ViewOptionsDialog::onThemeComboBoxIndexChanged(int)
{
   MainWindow::Theme theme = static_cast<MainWindow::Theme>(m_themeComboBox->currentData(Qt::UserRole).toInt());

   if (theme == MainWindow::THEME_CUSTOM)
   {
      QString filePath = QFileDialog::getOpenFileName(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SELECT_THEME));

      if (filePath.isEmpty())
      {
         int oldThemeIndex = m_themeComboBox->findData(m_mainwindow->getThemeFromString(m_settings->value("theme", "default").toString()));

         if (m_themeComboBox->count() > oldThemeIndex)
         {
            disconnect(m_themeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onThemeComboBoxIndexChanged(int)));
            m_themeComboBox->setCurrentIndex(oldThemeIndex);
            connect(m_themeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onThemeComboBoxIndexChanged(int)));
         }
      }
      else
      {
         m_customThemePath = filePath;

         if (m_mainwindow->setCustomThemeFile(filePath))
            m_mainwindow->setTheme(theme);
      }
   }
   else
      m_mainwindow->setTheme(theme);

   showOrHideHighlightColor();
}

void ViewOptionsDialog::onHighlightColorChoose()
{
   QPixmap highlightPixmap(m_highlightColorPushButton->iconSize());
   QColor currentHighlightColor = m_settings->value("highlight_color", QApplication::palette().highlight().color()).value<QColor>();
   QColor newHighlightColor = QColorDialog::getColor(currentHighlightColor, this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR));

   if (newHighlightColor.isValid())
   {
      MainWindow::Theme theme = static_cast<MainWindow::Theme>(m_themeComboBox->currentData(Qt::UserRole).toInt());

      m_highlightColor = newHighlightColor;
      m_settings->setValue("highlight_color", m_highlightColor);
      highlightPixmap.fill(m_highlightColor);
      m_highlightColorPushButton->setIcon(highlightPixmap);
      m_mainwindow->setTheme(theme);
   }
}

void ViewOptionsDialog::loadViewOptions()
{
   QColor highlightColor = m_settings->value("highlight_color", QApplication::palette().highlight().color()).value<QColor>();
   QPixmap highlightPixmap(m_highlightColorPushButton->iconSize());
   int themeIndex = 0;

   m_saveGeometryCheckBox->setChecked(m_settings->value("save_geometry", false).toBool());
   m_saveDockPositionsCheckBox->setChecked(m_settings->value("save_dock_positions", false).toBool());
   m_saveLastTabCheckBox->setChecked(m_settings->value("save_last_tab", false).toBool());
   m_showHiddenFilesCheckBox->setChecked(m_settings->value("show_hidden_files", true).toBool());
   m_suggestLoadedCoreFirstCheckBox->setChecked(m_settings->value("suggest_loaded_core_first", false).toBool());
   m_allPlaylistsListMaxCountSpinBox->setValue(m_settings->value("all_playlists_list_max_count", 0).toInt());
   m_allPlaylistsGridMaxCountSpinBox->setValue(m_settings->value("all_playlists_grid_max_count", 5000).toInt());

   themeIndex = m_themeComboBox->findData(m_mainwindow->getThemeFromString(m_settings->value("theme", "default").toString()));

   if (m_themeComboBox->count() > themeIndex)
      m_themeComboBox->setCurrentIndex(themeIndex);

   if (highlightColor.isValid())
   {
      m_highlightColor = highlightColor;
      highlightPixmap.fill(m_highlightColor);
      m_highlightColorPushButton->setIcon(highlightPixmap);
   }

   showOrHideHighlightColor();
}

void ViewOptionsDialog::showOrHideHighlightColor()
{
   if (m_mainwindow->theme() == MainWindow::THEME_DARK)
   {
      m_highlightColorLabel->show();
      m_highlightColorPushButton->show();
   }
   else
   {
      m_highlightColorLabel->hide();
      m_highlightColorPushButton->hide();
   }
}

void ViewOptionsDialog::saveViewOptions()
{
   m_settings->setValue("save_geometry", m_saveGeometryCheckBox->isChecked());
   m_settings->setValue("save_dock_positions", m_saveDockPositionsCheckBox->isChecked());
   m_settings->setValue("save_last_tab", m_saveLastTabCheckBox->isChecked());
   m_settings->setValue("theme", m_mainwindow->getThemeString(static_cast<MainWindow::Theme>(m_themeComboBox->currentData(Qt::UserRole).toInt())));
   m_settings->setValue("show_hidden_files", m_showHiddenFilesCheckBox->isChecked());
   m_settings->setValue("highlight_color", m_highlightColor);
   m_settings->setValue("suggest_loaded_core_first", m_suggestLoadedCoreFirstCheckBox->isChecked());
   m_settings->setValue("all_playlists_list_max_count", m_allPlaylistsListMaxCountSpinBox->value());
   m_settings->setValue("all_playlists_grid_max_count", m_allPlaylistsGridMaxCountSpinBox->value());

   if (!m_mainwindow->customThemeString().isEmpty())
      m_settings->setValue("custom_theme", m_customThemePath);

   m_mainwindow->setAllPlaylistsListMaxCount(m_allPlaylistsListMaxCountSpinBox->value());
   m_mainwindow->setAllPlaylistsGridMaxCount(m_allPlaylistsGridMaxCountSpinBox->value());
}

void ViewOptionsDialog::onAccepted()
{
   MainWindow::Theme newTheme = static_cast<MainWindow::Theme>(m_themeComboBox->currentData(Qt::UserRole).toInt());

   m_mainwindow->setTheme(newTheme);

   saveViewOptions();
}

void ViewOptionsDialog::onRejected()
{
   loadViewOptions();
}

void ViewOptionsDialog::showDialog()
{
   loadViewOptions();
   show();
}

void ViewOptionsDialog::hideDialog()
{
   reject();
}

CoreInfoWidget::CoreInfoWidget(CoreInfoLabel *label, QWidget *parent) :
   QWidget(parent)
   ,m_label(label)
   ,m_scrollArea(new QScrollArea(this))
{
   //m_scrollArea->setFrameShape(QFrame::NoFrame);
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
   ,m_listWidget(new QListWidget(this))
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
   int i = 0;

   qRegisterMetaType<QPointer<ThumbnailWidget> >("ThumbnailWidget");

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

   /* make sure these use an auto connection so it will be queued if called from a different thread (some facilities in RA log messages from other threads) */
   connect(this, SIGNAL(gotLogMessage(const QString&)), this, SLOT(onGotLogMessage(const QString&)), Qt::AutoConnection);
   connect(this, SIGNAL(gotStatusMessage(QString,unsigned,unsigned,bool)), this, SLOT(onGotStatusMessage(QString,unsigned,unsigned,bool)), Qt::AutoConnection);
   connect(this, SIGNAL(gotReloadPlaylists()), this, SLOT(onGotReloadPlaylists()));

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

void MainWindow::onPlaylistFilesDropped(QStringList files)
{
   addFilesToPlaylist(files);
}

/* Takes a list of files and folders and adds them to the currently selected playlist. Folders will have their contents added recursively. */
void MainWindow::addFilesToPlaylist(QStringList files)
{
   QStringList list;
   QString currentPlaylistPath;
   QListWidgetItem *currentItem = m_listWidget->currentItem();
   QByteArray currentPlaylistArray;
   QScopedPointer<QProgressDialog> dialog(NULL);
   PlaylistEntryDialog *playlistDialog = playlistEntryDialog();
   QHash<QString, QString> selectedCore;
   QHash<QString, QString> itemToAdd;
   QString selectedDatabase;
   QString selectedName;
   QString selectedPath;
   const char *currentPlaylistData = NULL;
   playlist_t *playlist = NULL;
   int i;

   if (files.count() == 1)
   {
      QString path = files.at(0);
      QFileInfo info(path);

      if (info.isFile())
      {
         itemToAdd["label"] = info.completeBaseName();
         itemToAdd["path"] = path;
      }
   }

   if (currentItem)
   {
      currentPlaylistPath = currentItem->data(Qt::UserRole).toString();

      if (!currentPlaylistPath.isEmpty())
      {
         currentPlaylistArray = currentPlaylistPath.toUtf8();
         currentPlaylistData = currentPlaylistArray.constData();
      }
   }

   if (currentPlaylistPath == ALL_PLAYLISTS_TOKEN)
   {
      ui_window.qtWindow->showMessageBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CANNOT_ADD_TO_ALL_PLAYLISTS), MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
      return;
   }

   /* a blank itemToAdd means there will be multiple */
   if (!playlistDialog->showDialog(itemToAdd))
      return;

   selectedName = m_playlistEntryDialog->getSelectedName();
   selectedPath = m_playlistEntryDialog->getSelectedPath();
   selectedCore = m_playlistEntryDialog->getSelectedCore();
   selectedDatabase = m_playlistEntryDialog->getSelectedDatabase();

   if (selectedDatabase.isEmpty())
      selectedDatabase = QFileInfo(currentPlaylistPath).fileName();
   else
      selectedDatabase += file_path_str(FILE_PATH_LPL_EXTENSION);

   dialog.reset(new QProgressDialog(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_GATHERING_LIST_OF_FILES), "Cancel", 0, 0, this));
   dialog->setWindowModality(Qt::ApplicationModal);

   for (i = 0; i < files.count(); i++)
   {
      QString path(files.at(i));
      QFileInfo fileInfo(path);

      if (dialog->wasCanceled())
         return;

      if (i % 25 == 0)
         qApp->processEvents();

      if (fileInfo.isDir())
      {
         QDir dir(path);
         addDirectoryFilesToList(list, dir);
         continue;
      }

      if (fileInfo.isFile())
      {
         list.append(fileInfo.absoluteFilePath());
      }
   }

   dialog->setLabelText(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ADDING_FILES_TO_PLAYLIST));
   dialog->setMaximum(list.count());

   playlist = playlist_init(currentPlaylistData, COLLECTION_SIZE);

   for (i = 0; i < list.count(); i++)
   {
      QString fileName = list.at(i);
      QFileInfo fileInfo;
      QByteArray fileBaseNameArray;
      QByteArray pathArray;
      QByteArray corePathArray;
      QByteArray coreNameArray;
      QByteArray databaseArray;
      const char *pathData = NULL;
      const char *fileNameNoExten = NULL;
      const char *corePathData = NULL;
      const char *coreNameData = NULL;
      const char *databaseData = NULL;

      if (dialog->wasCanceled())
      {
         playlist_free(playlist);
         return;
      }

      if (fileName.isEmpty())
         continue;

      fileInfo = fileName;

      if (files.count() == 1 && list.count() == 1 && i == 0)
      {
         fileBaseNameArray = selectedName.toUtf8();
         pathArray = QDir::toNativeSeparators(selectedPath).toUtf8();
      }
      else
      {
         fileBaseNameArray = fileInfo.completeBaseName().toUtf8();
         pathArray = QDir::toNativeSeparators(fileName).toUtf8();
      }

      fileNameNoExten = fileBaseNameArray.constData();

      /* a modal QProgressDialog calls processEvents() automatically in setValue() */
      dialog->setValue(i + 1);

      pathData = pathArray.constData();

      if (selectedCore.isEmpty())
      {
         corePathData = "DETECT";
         coreNameData = "DETECT";
      }
      else
      {
         corePathArray = QDir::toNativeSeparators(selectedCore.value("core_path")).toUtf8();
         coreNameArray = selectedCore.value("core_name").toUtf8();
         corePathData = corePathArray.constData();
         coreNameData = coreNameArray.constData();
      }

      databaseArray = selectedDatabase.toUtf8();
      databaseData = databaseArray.constData();

      if (path_is_compressed_file(pathData))
      {
         struct string_list *list = file_archive_get_file_list(pathData, NULL);

         if (list)
         {
            if (list->size == 1)
            {
               /* assume archives with one file should have that file loaded directly */
               pathArray = QDir::toNativeSeparators(QString(pathData) + "#" + list->elems[0].data).toUtf8();
               pathData = pathArray.constData();
            }

            string_list_free(list);
         }
      }

      playlist_push(playlist, pathData, fileNameNoExten,
            corePathData, coreNameData, "00000000|crc", databaseData);
   }

   playlist_write_file(playlist);
   playlist_free(playlist);

   reloadPlaylists();
}

void MainWindow::onGridItemClicked()
{
   QHash<QString, QString> hash;
   ThumbnailWidget *w = static_cast<ThumbnailWidget*>(sender());

   if (!w)
      return;

   if (m_currentGridWidget)
   {
      m_currentGridWidget->setObjectName("thumbnailWidget");
      //m_currentGridWidget->setFrameStyle(QFrame::Plain);
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

inline void MainWindow::calcGridItemSize(GridItem *item, int zoomValue)
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

   if (!ui_window.qtWindow->settings()->value("show_welcome_screen", true).toBool())
      return;

   if (!ui_window.qtWindow->showMessageBox(welcomeText, MainWindow::MSGBOX_TYPE_INFO, Qt::ApplicationModal))
      ui_window.qtWindow->settings()->setValue("show_welcome_screen", false);

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

bool MainWindow::showMessageBox(QString msg, MessageBoxType msgType, Qt::WindowModality modality, bool showDontAsk)
{
   QPointer<QMessageBox> msgBoxPtr;
   QMessageBox *msgBox = NULL;
   QCheckBox *checkBox = NULL;

   msgBoxPtr = new QMessageBox(this);
   msgBox = msgBoxPtr.data();

   msgBox->setWindowModality(modality);
   msgBox->setTextFormat(Qt::RichText);

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
      case MSGBOX_TYPE_QUESTION:
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

   if (checkBox && checkBox->isChecked())
      return false;

   if (msgBox->result() == QMessageBox::Cancel)
      return false;

   return true;
}

bool MainWindow::updateCurrentPlaylistEntry(const QHash<QString, QString> &contentHash)
{
   QString playlistPath = getCurrentPlaylistPath();
   QString path;
   QString label;
   QString corePath;
   QString coreName;
   QString dbName;
   QString crc32;
   QByteArray playlistPathArray;
   QByteArray pathArray;
   QByteArray labelArray;
   QByteArray corePathArray;
   QByteArray coreNameArray;
   QByteArray dbNameArray;
   QByteArray crc32Array;
   const char *playlistPathData = NULL;
   const char *pathData = NULL;
   const char *labelData = NULL;
   const char *corePathData = NULL;
   const char *coreNameData = NULL;
   const char *dbNameData = NULL;
   const char *crc32Data = NULL;
   playlist_t *playlist = NULL;
   unsigned index = 0;
   bool ok = false;

   if (playlistPath.isEmpty() || contentHash.isEmpty() || !contentHash.contains("index"))
      return false;

   index = contentHash.value("index").toUInt(&ok);

   if (!ok)
      return false;

   path = contentHash.value("path");
   label = contentHash.value("label");
   coreName = contentHash.value("core_name");
   corePath = contentHash.value("core_path");
   dbName = contentHash.value("db_name");
   crc32 = contentHash.value("crc32");

   if (path.isEmpty() ||
       label.isEmpty() ||
       coreName.isEmpty() ||
       corePath.isEmpty() ||
       dbName.isEmpty() ||
       crc32.isEmpty()
      )
      return false;

   playlistPathArray = playlistPath.toUtf8();
   pathArray = QDir::toNativeSeparators(path).toUtf8();
   labelArray = label.toUtf8();
   coreNameArray = coreName.toUtf8();
   corePathArray = QDir::toNativeSeparators(corePath).toUtf8();
   dbNameArray = (dbName + file_path_str(FILE_PATH_LPL_EXTENSION)).toUtf8();
   crc32Array = crc32.toUtf8();

   playlistPathData = playlistPathArray.constData();
   pathData = pathArray.constData();
   labelData = labelArray.constData();
   coreNameData = coreNameArray.constData();
   corePathData = corePathArray.constData();
   dbNameData = dbNameArray.constData();
   crc32Data = crc32Array.constData();

   if (path_is_compressed_file(pathData))
   {
      struct string_list *list = file_archive_get_file_list(pathData, NULL);

      if (list)
      {
         if (list->size == 1)
         {
            /* assume archives with one file should have that file loaded directly */
            pathArray = QDir::toNativeSeparators(QString(pathData) + "#" + list->elems[0].data).toUtf8();
            pathData = pathArray.constData();
         }

         string_list_free(list);
      }
   }

   playlist = playlist_init(playlistPathData, COLLECTION_SIZE);

   playlist_update(playlist, index, pathData, labelData,
         corePathData, coreNameData, crc32Data, dbNameData);
   playlist_write_file(playlist);
   playlist_free(playlist);

   reloadPlaylists();

   return true;
}

void MainWindow::onFileDropWidgetContextMenuRequested(const QPoint &pos)
{
   QScopedPointer<QMenu> menu;
   QScopedPointer<QAction> addFilesAction;
   QScopedPointer<QAction> addFolderAction;
   QScopedPointer<QAction> editAction;
   QScopedPointer<QAction> deleteAction;
   QPointer<QAction> selectedAction;
   QPoint cursorPos = QCursor::pos();
   QHash<QString, QString> contentHash = getCurrentContentHash();

   menu.reset(new QMenu(this));

   addFilesAction.reset(new QAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ADD_FILES)), this));
   addFolderAction.reset(new QAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ADD_FOLDER)), this));
   editAction.reset(new QAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_EDIT)), this));
   deleteAction.reset(new QAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_DELETE)), this));

   menu->addAction(addFilesAction.data());
   menu->addAction(addFolderAction.data());

   if (!contentHash.isEmpty())
   {
      menu->addAction(editAction.data());
      menu->addAction(deleteAction.data());
   }

   selectedAction = menu->exec(cursorPos);

   if (!selectedAction)
      return;

   if (selectedAction == addFilesAction.data())
   {
      QStringList filePaths = QFileDialog::getOpenFileNames(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SELECT_FILES));

      if (!filePaths.isEmpty())
         addFilesToPlaylist(filePaths);
   }
   else if (selectedAction == addFolderAction.data())
   {
      QString dirPath = QFileDialog::getExistingDirectory(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SELECT_FOLDER), QString(), QFileDialog::ShowDirsOnly);

      if (!dirPath.isEmpty())
         addFilesToPlaylist(QStringList() << dirPath);
   }
   else if (selectedAction == editAction.data())
   {
      PlaylistEntryDialog *playlistDialog = playlistEntryDialog();
      QHash<QString, QString> selectedCore;
      QString selectedDatabase;
      QString selectedName;
      QString selectedPath;
      QString currentPlaylistPath = getCurrentPlaylistPath();

      if (!playlistDialog->showDialog(contentHash))
         return;

      selectedName = m_playlistEntryDialog->getSelectedName();
      selectedPath = m_playlistEntryDialog->getSelectedPath();
      selectedCore = m_playlistEntryDialog->getSelectedCore();
      selectedDatabase = m_playlistEntryDialog->getSelectedDatabase();

      if (selectedCore.isEmpty())
      {
         selectedCore["core_name"] = "DETECT";
         selectedCore["core_path"] = "DETECT";
      }

      if (selectedDatabase.isEmpty())
      {
         selectedDatabase = QFileInfo(currentPlaylistPath).fileName().remove(file_path_str(FILE_PATH_LPL_EXTENSION));
      }

      contentHash["label"] = selectedName;
      contentHash["path"] = selectedPath;
      contentHash["core_name"] = selectedCore.value("core_name");
      contentHash["core_path"] = selectedCore.value("core_path");
      contentHash["db_name"] = selectedDatabase;

      if (!updateCurrentPlaylistEntry(contentHash))
      {
         ui_window.qtWindow->showMessageBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY), MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
         return;
      }
   }
   else if (selectedAction == deleteAction.data())
   {
      deleteCurrentPlaylistItem();
   }
}

void MainWindow::onPlaylistWidgetContextMenuRequested(const QPoint&)
{
   settings_t *settings = config_get_ptr();
   QScopedPointer<QMenu> menu;
   QScopedPointer<QMenu> associateMenu;
   QScopedPointer<QMenu> hiddenPlaylistsMenu;
   QScopedPointer<QAction> hideAction;
   QScopedPointer<QAction> newPlaylistAction;
   QScopedPointer<QAction> deletePlaylistAction;
   QPointer<QAction> selectedAction;
   QPoint cursorPos = QCursor::pos();
   QListWidgetItem *selectedItem = m_listWidget->itemAt(m_listWidget->viewport()->mapFromGlobal(cursorPos));
   QDir playlistDir(settings->paths.directory_playlist);
   QString playlistDirAbsPath = playlistDir.absolutePath();
   QString currentPlaylistDirPath;
   QString currentPlaylistPath;
   QString currentPlaylistFileName;
   QFile currentPlaylistFile;
   QByteArray currentPlaylistFileNameArray;
   QFileInfo currentPlaylistFileInfo;
   QMap<QString, const core_info_t*> coreList;
   core_info_list_t *core_info_list = NULL;
   union string_list_elem_attr attr = {0};
   struct string_list *stnames = NULL;
   struct string_list *stcores = NULL;
   unsigned i = 0;
   int j = 0;
   size_t found = 0;
   const char *currentPlaylistFileNameData = NULL;
   char new_playlist_names[PATH_MAX_LENGTH];
   char new_playlist_cores[PATH_MAX_LENGTH];
   bool specialPlaylist = false;
   bool foundHiddenPlaylist = false;

   new_playlist_names[0] = new_playlist_cores[0] = '\0';

   stnames = string_split(settings->arrays.playlist_names, ";");
   stcores = string_split(settings->arrays.playlist_cores, ";");

   if (selectedItem)
   {
      currentPlaylistPath = selectedItem->data(Qt::UserRole).toString();
      currentPlaylistFile.setFileName(currentPlaylistPath);

      currentPlaylistFileInfo = QFileInfo(currentPlaylistPath);
      currentPlaylistFileName = currentPlaylistFileInfo.fileName();
      currentPlaylistDirPath = currentPlaylistFileInfo.absoluteDir().absolutePath();

      currentPlaylistFileNameArray.append(currentPlaylistFileName);
      currentPlaylistFileNameData = currentPlaylistFileNameArray.constData();
   }

   menu.reset(new QMenu(this));
   menu->setObjectName("menu");

   hiddenPlaylistsMenu.reset(new QMenu(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS), this));
   newPlaylistAction.reset(new QAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST)) + "...", this));

   hiddenPlaylistsMenu->setObjectName("hiddenPlaylistsMenu");

   menu->addAction(newPlaylistAction.data());

   if (currentPlaylistFile.exists())
   {
      deletePlaylistAction.reset(new QAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST)) + "...", this));
      menu->addAction(deletePlaylistAction.data());
   }

   if (selectedItem)
   {
      hideAction.reset(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_HIDE), this));
      menu->addAction(hideAction.data());
   }

   for (j = 0; j < m_listWidget->count(); j++)
   {
      QListWidgetItem *item = m_listWidget->item(j);
      bool hidden = m_listWidget->isItemHidden(item);

      if (hidden)
      {
         QAction *action = hiddenPlaylistsMenu->addAction(item->text());
         action->setProperty("row", j);
         action->setProperty("core_path", item->data(Qt::UserRole).toString());
         foundHiddenPlaylist = true;
      }
   }

   if (!foundHiddenPlaylist)
   {
      QAction *action = hiddenPlaylistsMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE));
      action->setProperty("row", -1);
   }

   menu->addMenu(hiddenPlaylistsMenu.data());

   if (currentPlaylistDirPath != playlistDirAbsPath)
   {
      /* special playlists like history etc. can't have an association */
      specialPlaylist = true;
   }

   if (!specialPlaylist)
   {
      associateMenu.reset(new QMenu(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ASSOCIATE_CORE), this));
      associateMenu->setObjectName("associateMenu");

      core_info_get_list(&core_info_list);

      for (i = 0; i < core_info_list->count && core_info_list->count > 0; i++)
      {
         const core_info_t *core = &core_info_list->list[i];
         coreList[core->core_name] = core;
      }

      {
         QMapIterator<QString, const core_info_t*> coreListIterator(coreList);

         while (coreListIterator.hasNext())
         {
            QString key;
            const core_info_t *core = NULL;
            QAction *action = NULL;

            coreListIterator.next();

            key = coreListIterator.key();
            core = coreList.value(key);
            action = associateMenu->addAction(core->core_name);
            action->setProperty("core_path", core->path);
         }
      }

      menu->addMenu(associateMenu.data());
   }

   selectedAction = menu->exec(cursorPos);

   if (!selectedAction)
      goto end;

   if (!specialPlaylist && selectedAction->parent() == associateMenu.data())
   {
      found = string_list_find_elem(stnames, currentPlaylistFileNameData);

      if (found)
         string_list_set(stcores, static_cast<unsigned>(found - 1), selectedAction->property("core_path").toString().toUtf8().constData());
      else
      {
         string_list_append(stnames, currentPlaylistFileNameData, attr);
         string_list_append(stcores, "DETECT", attr);

         found = string_list_find_elem(stnames, currentPlaylistFileNameData);

         if (found)
            string_list_set(stcores, static_cast<unsigned>(found - 1), selectedAction->property("core_path").toString().toUtf8().constData());
      }

      string_list_join_concat(new_playlist_names,
            sizeof(new_playlist_names), stnames, ";");
      string_list_join_concat(new_playlist_cores,
            sizeof(new_playlist_cores), stcores, ";");

      strlcpy(settings->arrays.playlist_names,
            new_playlist_names, sizeof(settings->arrays.playlist_names));
      strlcpy(settings->arrays.playlist_cores,
            new_playlist_cores, sizeof(settings->arrays.playlist_cores));
   }
   else if (selectedAction == deletePlaylistAction.data())
   {
      if (currentPlaylistFile.exists())
      {
         if (ui_window.qtWindow->showMessageBox(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST)).arg(selectedItem->text()), MainWindow::MSGBOX_TYPE_QUESTION, Qt::ApplicationModal, false))
         {
            if (currentPlaylistFile.remove())
               reloadPlaylists();
            else
               ui_window.qtWindow->showMessageBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE), MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
         }
      }
   }
   else if (selectedAction == newPlaylistAction.data())
   {
      QString name = QInputDialog::getText(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST), msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME));
      QString newPlaylistPath = playlistDirAbsPath + "/" + name + file_path_str(FILE_PATH_LPL_EXTENSION);
      QFile file(newPlaylistPath);

      if (file.open(QIODevice::WriteOnly))
         file.close();

      reloadPlaylists();
   }
   else if (selectedAction == hideAction.data())
   {
      int row = m_listWidget->row(selectedItem);

      if (row >= 0)
      {
         QStringList hiddenPlaylists = m_settings->value("hidden_playlists").toStringList();

         if (!hiddenPlaylists.contains(currentPlaylistFileName))
         {
            hiddenPlaylists.append(currentPlaylistFileName);
            m_settings->setValue("hidden_playlists", hiddenPlaylists);
         }

         m_listWidget->setRowHidden(row, true);
      }
   }
   else if (selectedAction->parent() == hiddenPlaylistsMenu.data())
   {
      QVariant rowVariant = selectedAction->property("row");

      if (rowVariant.isValid())
      {
         QStringList hiddenPlaylists = m_settings->value("hidden_playlists").toStringList();
         int row = rowVariant.toInt();

         if (row >= 0)
         {
            QString playlistPath = selectedAction->property("core_path").toString();
            QFileInfo playlistFileInfo(playlistPath);
            QString playlistFileName = playlistFileInfo.fileName();

            if (hiddenPlaylists.contains(playlistFileName))
            {
               hiddenPlaylists.removeOne(playlistFileName);
               m_settings->setValue("hidden_playlists", hiddenPlaylists);
            }

            m_listWidget->setRowHidden(row, false);
         }
      }
   }

   setCoreActions();

end:
   if (stnames)
      string_list_free(stnames);
   if (stcores)
      string_list_free(stcores);
}

void MainWindow::onFileBrowserTreeContextMenuRequested(const QPoint&)
{
#ifdef HAVE_LIBRETRODB
   QPointer<QAction> action;
   QList<QAction*> actions;
   QScopedPointer<QAction> scanAction;
   QDir dir;
   QString currentDirString = m_dirModel->filePath(m_dirTree->currentIndex());
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

void MainWindow::deferReloadPlaylists()
{
   emit gotReloadPlaylists();
}

void MainWindow::onGotReloadPlaylists()
{
   reloadPlaylists();
}

void MainWindow::reloadPlaylists()
{
   QListWidgetItem *allPlaylistsItem = NULL;
   QListWidgetItem *favoritesPlaylistsItem = NULL;
   QListWidgetItem *imagePlaylistsItem = NULL;
   QListWidgetItem *musicPlaylistsItem = NULL;
   QListWidgetItem *videoPlaylistsItem = NULL;
   QListWidgetItem *firstItem = NULL;
   QListWidgetItem *currentItem = NULL;
   settings_t *settings = config_get_ptr();
   QDir playlistDir(settings->paths.directory_playlist);
   QString currentPlaylistPath;
   QStringList hiddenPlaylists = m_settings->value("hidden_playlists").toStringList();
   int i = 0;

   currentItem = m_listWidget->currentItem();

   if (currentItem)
   {
      currentPlaylistPath = currentItem->data(Qt::UserRole).toString();
   }

   getPlaylistFiles();

   m_listWidget->clear();

   allPlaylistsItem = new QListWidgetItem(m_folderIcon, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS));
   allPlaylistsItem->setData(Qt::UserRole, ALL_PLAYLISTS_TOKEN);

   favoritesPlaylistsItem = new QListWidgetItem(m_folderIcon, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES_TAB));
   favoritesPlaylistsItem->setData(Qt::UserRole, settings->paths.path_content_favorites);

   m_historyPlaylistsItem = new QListWidgetItem(m_folderIcon, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HISTORY_TAB));
   m_historyPlaylistsItem->setData(Qt::UserRole, settings->paths.path_content_history);

   imagePlaylistsItem = new QListWidgetItem(m_folderIcon, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_IMAGES_TAB));
   imagePlaylistsItem->setData(Qt::UserRole, settings->paths.path_content_image_history);

   musicPlaylistsItem = new QListWidgetItem(m_folderIcon, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MUSIC_TAB));
   musicPlaylistsItem->setData(Qt::UserRole, settings->paths.path_content_music_history);

   videoPlaylistsItem = new QListWidgetItem(m_folderIcon, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_TAB));
   videoPlaylistsItem->setData(Qt::UserRole, settings->paths.path_content_video_history);

   m_listWidget->addItem(allPlaylistsItem);
   m_listWidget->addItem(favoritesPlaylistsItem);
   m_listWidget->addItem(m_historyPlaylistsItem);
   m_listWidget->addItem(imagePlaylistsItem);
   m_listWidget->addItem(musicPlaylistsItem);
   m_listWidget->addItem(videoPlaylistsItem);

   if (hiddenPlaylists.contains(ALL_PLAYLISTS_TOKEN))
      m_listWidget->setRowHidden(m_listWidget->row(allPlaylistsItem), true);
   if (hiddenPlaylists.contains(QFileInfo(settings->paths.path_content_favorites).fileName()))
      m_listWidget->setRowHidden(m_listWidget->row(favoritesPlaylistsItem), true);
   if (hiddenPlaylists.contains(QFileInfo(settings->paths.path_content_history).fileName()))
      m_listWidget->setRowHidden(m_listWidget->row(m_historyPlaylistsItem), true);
   if (hiddenPlaylists.contains(QFileInfo(settings->paths.path_content_image_history).fileName()))
      m_listWidget->setRowHidden(m_listWidget->row(imagePlaylistsItem), true);
   if (hiddenPlaylists.contains(QFileInfo(settings->paths.path_content_music_history).fileName()))
      m_listWidget->setRowHidden(m_listWidget->row(musicPlaylistsItem), true);
   if (hiddenPlaylists.contains(QFileInfo(settings->paths.path_content_video_history).fileName()))
      m_listWidget->setRowHidden(m_listWidget->row(videoPlaylistsItem), true);

   for (i = 0; i < m_playlistFiles.count(); i++)
   {
      QListWidgetItem *item = NULL;
      const QString &file = m_playlistFiles.at(i);
      QString fileDisplayName = file;
      QString fileName = file;
      bool hasIcon = false;
      QIcon icon;
      QString iconPath;

      fileDisplayName.remove(file_path_str(FILE_PATH_LPL_EXTENSION));

      iconPath = QString(settings->paths.directory_assets) + ICON_PATH + fileDisplayName + ".png";

      hasIcon = QFile::exists(iconPath);

      if (hasIcon)
         icon = QIcon(iconPath);
      else
         icon = m_folderIcon;

      item = new QListWidgetItem(icon, fileDisplayName);
      item->setData(Qt::UserRole, playlistDir.absoluteFilePath(file));

      m_listWidget->addItem(item);

      if (hiddenPlaylists.contains(fileName))
      {
         int row = m_listWidget->row(item);

         if (row >= 0)
            m_listWidget->setRowHidden(row, true);
      }
   }

   if (m_listWidget->count() > 0)
   {
      firstItem = m_listWidget->item(0);

      if (firstItem)
      {
         bool found = false;

         for (i = 0; i < m_listWidget->count(); i++)
         {
            QListWidgetItem *item = m_listWidget->item(i);
            QString path;

            if (item)
            {
               path = item->data(Qt::UserRole).toString();

               if (!currentPlaylistPath.isEmpty() && !path.isEmpty())
               {
                  if (path == currentPlaylistPath)
                  {
                     found = true;
                     m_listWidget->setCurrentItem(item);
                     break;
                  }
               }
            }
         }

         /* the previous playlist must be gone now, just select the first one */
         if (!found)
            m_listWidget->setCurrentItem(firstItem);
      }
   }
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
      QString filePath(dir.absoluteFilePath(fileName));
      QFileInfo fileInfo(filePath);

      hash["path"] = filePath;
      hash["label"] = hash["path"];
      hash["label_noext"] = fileInfo.fileName().remove(QString(".") + fileInfo.completeSuffix());
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
}

QTabWidget* MainWindow::browserAndPlaylistTabWidget()
{
   return m_browserAndPlaylistTabWidget;
}

void MainWindow::onTableWidgetEnterPressed()
{
   onRunClicked();
}

void MainWindow::onTableWidgetDeletePressed()
{
   deleteCurrentPlaylistItem();
}

QString MainWindow::getCurrentPlaylistPath()
{
   QListWidgetItem *playlistItem = m_listWidget->currentItem();
   QHash<QString, QString> contentHash;
   QString playlistPath;

   if (!playlistItem)
      return playlistPath;

   playlistPath = playlistItem->data(Qt::UserRole).toString();

   return playlistPath;
}

QHash<QString, QString> MainWindow::getCurrentContentHash()
{
   QTableWidgetItem *contentItem = m_tableWidget->currentItem();
   QListWidgetItem *playlistItem = m_listWidget->currentItem();
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

void MainWindow::deleteCurrentPlaylistItem()
{
   QString playlistPath = getCurrentPlaylistPath();
   QByteArray playlistArray;
   QHash<QString, QString> contentHash = getCurrentContentHash();
   playlist_t *playlist = NULL;
   const char *playlistData = NULL;
   unsigned index = 0;
   bool ok = false;

   if (playlistPath.isEmpty())
      return;

   if (contentHash.isEmpty())
      return;

   playlistArray = playlistPath.toUtf8();
   playlistData = playlistArray.constData();

   index = contentHash.value("index").toUInt(&ok);

   if (!ok)
      return;

   if (!ui_window.qtWindow->showMessageBox(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST_ITEM)).arg(contentHash["label"]), MainWindow::MSGBOX_TYPE_QUESTION, Qt::ApplicationModal, false))
      return;

   playlist = playlist_init(playlistData, COLLECTION_SIZE);

   playlist_delete_index(playlist, index);
   playlist_write_file(playlist);
   playlist_free(playlist);

   reloadPlaylists();
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

   if (!item)
      return;

   if (viewType == VIEW_TYPE_LIST)
      contentHash = item->data(Qt::UserRole).value<QHash<QString, QString> >();
   else if (viewType == VIEW_TYPE_ICONS)
      contentHash = m_currentGridHash;
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

QVector<QHash<QString, QString> > MainWindow::getPlaylistDefaultCores()
{
   settings_t *settings = config_get_ptr();
   struct string_list *playlists = string_split(settings->arrays.playlist_names, ";");
   struct string_list *cores = string_split(settings->arrays.playlist_cores, ";");
   unsigned i = 0;
   QVector<QHash<QString, QString> > coreList;

   if (!playlists || !cores)
   {
      RARCH_WARN("[Qt]: Could not parse one of playlist_names or playlist_cores\n");
      goto finish;
   }
   else if (playlists->size != cores->size)
   {
      RARCH_WARN("[Qt]: playlist_names array size differs from playlist_cores\n");
      goto finish;
   }

   if (playlists->size == 0)
      goto finish;

   for (i = 0; i < playlists->size; i++)
   {
      const char *playlist = playlists->elems[i].data;
      const char *core = cores->elems[i].data;
      QHash<QString, QString> hash;

      hash["playlist_filename"] = playlist;
      hash["playlist_filename"].remove(file_path_str(FILE_PATH_LPL_EXTENSION));
      hash["core_path"] = core;

      coreList.append(hash);
   }

finish:
   if (playlists)
      string_list_free(playlists);
   if (cores)
      string_list_free(cores);

   return coreList;
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

void MainWindow::getPlaylistFiles()
{
   settings_t *settings = config_get_ptr();
   QDir playlistDir(settings->paths.directory_playlist);

   m_playlistFiles = playlistDir.entryList(QDir::NoDotAndDotDot | QDir::Readable | QDir::Files, QDir::Name);
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

   m_dirTree->setCurrentIndex(m_dirModel->index(dir.absolutePath()));
   /* for some reason, scrollTo only seems to work right when the button is clicked twice (only tested on Linux) */
   m_dirTree->scrollTo(m_dirTree->currentIndex(), QAbstractItemView::PositionAtTop);
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

QListWidget* MainWindow::playlistListWidget()
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

void MainWindow::addPlaylistItemsToGrid(const QStringList &paths, bool add)
{
   QVector<QHash<QString, QString> > items;
   int i;

   if (paths.isEmpty())
      return;

   for (i = 0; i < paths.size(); i++)
   {
      int j;
      QVector<QHash<QString, QString> > vec = getPlaylistItems(paths.at(i));
      /* QVector::append() wasn't added until 5.5, so just do it the old fashioned way */
      for (j = 0; j < vec.size(); j++)
      {
         if (add && m_allPlaylistsGridMaxCount > 0 && items.size() >= m_allPlaylistsGridMaxCount)
            goto finish;

         items.append(vec.at(j));
      }
   }
finish:
   std::sort(items.begin(), items.end(), comp_hash_label_key_lower);

   addPlaylistHashToGrid(items);
}

void MainWindow::addPlaylistHashToGrid(const QVector<QHash<QString, QString> > &items)
{
   QScreen *screen = qApp->primaryScreen();
   QSize screenSize = screen->size();
   QListWidgetItem *currentItem = m_listWidget->currentItem();
   settings_t *settings = config_get_ptr();
   int i = 0;
   int zoomValue = m_zoomSlider->value();

   m_gridProgressBar->setMinimum(0);
   m_gridProgressBar->setMaximum(qMax(0, items.count() - 1));
   m_gridProgressBar->setValue(0);

   for (i = 0; i < items.count(); i++)
   {
      const QHash<QString, QString> &hash = items.at(i);
      QPointer<GridItem> item;
      QPointer<ThumbnailLabel> label;
      QString thumbnailFileNameNoExt;
      QLabel *newLabel = NULL;
      QSize thumbnailWidgetSizeHint(screenSize.width() / 8, screenSize.height() / 8);
      QByteArray extension;
      QString extensionStr;
      QString imagePath;
      int lastIndex = -1;

      if (m_listWidget->currentItem() != currentItem)
      {
         /* user changed the current playlist before we finished loading... abort */
         m_gridProgressWidget->hide();
         break;
      }

      item = new GridItem();

      lastIndex = hash["path"].lastIndexOf('.');

      if (lastIndex >= 0)
      {
         extensionStr = hash["path"].mid(lastIndex + 1);

         if (!extensionStr.isEmpty())
         {
            extension = extensionStr.toLower().toUtf8();
         }
      }

      if (!extension.isEmpty() && m_imageFormats.contains(extension))
      {
         /* use thumbnail widgets to show regular image files */
         imagePath = hash["path"];
      }
      else
      {
         thumbnailFileNameNoExt = hash["label_noext"];
         thumbnailFileNameNoExt.replace(m_fileSanitizerRegex, "_");
         imagePath = QString(settings->paths.directory_thumbnails) + "/" + hash.value("db_name") + "/" + THUMBNAIL_BOXART + "/" + thumbnailFileNameNoExt + ".png";
      }

      item->hash = hash;
      item->widget = new ThumbnailWidget();
      item->widget->setSizeHint(thumbnailWidgetSizeHint);
      item->widget->setFixedSize(item->widget->sizeHint());
      item->widget->setLayout(new QVBoxLayout());
      item->widget->setObjectName("thumbnailWidget");
      item->widget->setProperty("hash", QVariant::fromValue<QHash<QString, QString> >(hash));

      connect(item->widget, SIGNAL(mouseDoubleClicked()), this, SLOT(onGridItemDoubleClicked()));
      connect(item->widget, SIGNAL(mousePressed()), this, SLOT(onGridItemClicked()));

      label = new ThumbnailLabel(item->widget);
      label->setObjectName("thumbnailGridLabel");

      item->label = label;
      item->labelText = hash.value("label");

      newLabel = new QLabel(item->labelText, item->widget);
      newLabel->setObjectName("thumbnailQLabel");
      newLabel->setAlignment(Qt::AlignCenter);
      newLabel->setToolTip(item->labelText);

      calcGridItemSize(item, zoomValue);

      item->widget->layout()->addWidget(label);

      item->widget->layout()->addWidget(newLabel);
      qobject_cast<QVBoxLayout*>(item->widget->layout())->setStretchFactor(label, 1);

      m_gridLayout->addWidgetDeferred(item->widget);
      m_gridItems.append(item);

      loadImageDeferred(item, imagePath);

      if (i % 25 == 0)
         qApp->processEvents();

      m_gridProgressBar->setValue(i);
   }

   /* If there's only one entry, a min/max/value of all zero would make an indeterminate progress bar that never ends... so just hide it when we are done. */
   if (m_gridProgressBar->value() == m_gridProgressBar->maximum())
      m_gridProgressWidget->hide();
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
   m_gridLayoutWidget->resize(m_gridScrollArea->viewport()->size());

   onZoomValueChanged(m_zoomSlider->value());

   onSearchEnterPressed();
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
}

QVector<QHash<QString, QString> > MainWindow::getPlaylistItems(QString pathString)
{
   QByteArray pathArray;
   QVector<QHash<QString, QString> > items;
   const char *pathData = NULL;
   playlist_t *playlist = NULL;
   unsigned playlistSize = 0;
   unsigned i = 0;

   pathArray.append(pathString);
   pathData = pathArray.constData();

   playlist = playlist_init(pathData, COLLECTION_SIZE);
   playlistSize = playlist_get_size(playlist);

   for (i = 0; i < playlistSize; i++)
   {
      const char *path = NULL;
      const char *label = NULL;
      const char *core_path = NULL;
      const char *core_name = NULL;
      const char *crc32 = NULL;
      const char *db_name = NULL;
      QHash<QString, QString> hash;

      playlist_get_index(playlist, i,
                         &path, &label, &core_path,
                         &core_name, &crc32, &db_name);

      if (string_is_empty(path))
         continue;
      else
         hash["path"] = path;

      hash["index"] = QString::number(i);

      if (string_is_empty(label))
      {
         hash["label"] = path;
         hash["label_noext"] = path;
      }
      else
      {
         hash["label"] = label;
         hash["label_noext"] = label;
      }

      if (!string_is_empty(core_path))
         hash["core_path"] = core_path;

      if (!string_is_empty(core_name))
         hash["core_name"] = core_name;

      if (!string_is_empty(crc32))
         hash["crc32"] = crc32;

      if (!string_is_empty(db_name))
      {
         hash["db_name"] = db_name;
         hash["db_name"].remove(file_path_str(FILE_PATH_LPL_EXTENSION));
      }

      items.append(hash);
   }

   playlist_free(playlist);
   playlist = NULL;

   return items;
}

void MainWindow::addPlaylistItemsToTable(const QStringList &paths, bool add)
{
   QVector<QHash<QString, QString> > items;
   int i;

   if (paths.isEmpty())
      return;

   for (i = 0; i < paths.size(); i++)
   {
      int j;
      QVector<QHash<QString, QString> > vec = getPlaylistItems(paths.at(i));
      /* QVector::append() wasn't added until 5.5, so just do it the old fashioned way */
      for (j = 0; j < vec.size(); j++)
      {
         if (add && m_allPlaylistsListMaxCount > 0 && items.size() >= m_allPlaylistsListMaxCount)
            goto finish;

         items.append(vec.at(j));
      }
   }
finish:
   addPlaylistHashToTable(items);
}

void MainWindow::addPlaylistHashToTable(const QVector<QHash<QString, QString> > &items)
{
   int i = 0;
   int oldRowCount = m_tableWidget->rowCount();

   m_tableWidget->setRowCount(oldRowCount + items.count());

   for (i = 0; i < items.count(); i++)
   {
      QTableWidgetItem *labelItem = NULL;
      const QHash<QString, QString> &hash = items.at(i);

      labelItem = new QTableWidgetItem(hash.value("label"));
      labelItem->setData(Qt::UserRole, QVariant::fromValue<QHash<QString, QString> >(hash));
      labelItem->setFlags(labelItem->flags() & ~Qt::ItemIsEditable);

      m_tableWidget->setItem(oldRowCount + i, 0, labelItem);
   }
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

void MainWindow::setAllPlaylistsListMaxCount(int count)
{
   if (count < 1)
      count = 0;

   m_allPlaylistsListMaxCount = count;
}

void MainWindow::setAllPlaylistsGridMaxCount(int count)
{
   if (count < 1)
      count = 0;

   m_allPlaylistsGridMaxCount = count;
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
