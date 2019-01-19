#include <QSettings>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QColor>
#include <QLabel>
#include <QSpinBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QApplication>
#include <QColorDialog>

#include "viewoptionsdialog.h"
#include "../ui_qt.h"

#ifndef CXX_BUILD
extern "C" {
#endif

#include "../../../msg_hash.h"

#ifndef CXX_BUILD
}
#endif

ViewOptionsDialog::ViewOptionsDialog(MainWindow *mainwindow, QWidget *parent) :
   QDialog(mainwindow)
   ,m_mainwindow(mainwindow)
   ,m_settings(mainwindow->settings())
   ,m_saveGeometryCheckBox(new QCheckBox(this))
   ,m_saveDockPositionsCheckBox(new QCheckBox(this))
   ,m_saveLastTabCheckBox(new QCheckBox(this))
   ,m_showHiddenFilesCheckBox(new QCheckBox(this))
   ,m_themeComboBox(new QComboBox(this))
   ,m_thumbnailComboBox(new QComboBox(this))
   ,m_thumbnailCacheSpinBox(new QSpinBox(this))
   ,m_startupPlaylistComboBox(new QComboBox(this))
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

   m_thumbnailComboBox->addItem(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART), THUMBNAIL_TYPE_BOXART);
   m_thumbnailComboBox->addItem(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT), THUMBNAIL_TYPE_SCREENSHOT);
   m_thumbnailComboBox->addItem(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN), THUMBNAIL_TYPE_TITLE_SCREEN);

   m_thumbnailCacheSpinBox->setSuffix(" MB");
   m_thumbnailCacheSpinBox->setRange(0, 99999);

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
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_STARTUP_PLAYLIST), m_startupPlaylistComboBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_TYPE), m_thumbnailComboBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT), m_thumbnailCacheSpinBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME), m_themeComboBox);
   form->addRow(m_highlightColorLabel, m_highlightColorPushButton);

   qobject_cast<QVBoxLayout*>(layout())->addLayout(form);
   layout()->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));
   layout()->addWidget(buttonBox);

   loadViewOptions();

   connect(m_themeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onThemeComboBoxIndexChanged(int)));
   connect(m_thumbnailComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onThumbnailComboBoxIndexChanged(int)));
   connect(m_highlightColorPushButton, SIGNAL(clicked()), this, SLOT(onHighlightColorChoose()));
}

void ViewOptionsDialog::onThumbnailComboBoxIndexChanged(int index)
{
   ThumbnailType type = static_cast<ThumbnailType>(m_thumbnailComboBox->currentData().value<int>());
   m_mainwindow->setCurrentThumbnailType(type);
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
   QVector<QPair<QString, QString> > playlists = m_mainwindow->getPlaylists();
   QString initialPlaylist = m_settings->value("initial_playlist", m_mainwindow->getSpecialPlaylistPath(SPECIAL_PLAYLIST_HISTORY)).toString();
   int themeIndex = 0;
   int thumbnailIndex = 0;
   int playlistIndex = 0;
   int i;

   m_saveGeometryCheckBox->setChecked(m_settings->value("save_geometry", false).toBool());
   m_saveDockPositionsCheckBox->setChecked(m_settings->value("save_dock_positions", false).toBool());
   m_saveLastTabCheckBox->setChecked(m_settings->value("save_last_tab", false).toBool());
   m_showHiddenFilesCheckBox->setChecked(m_settings->value("show_hidden_files", true).toBool());
   m_suggestLoadedCoreFirstCheckBox->setChecked(m_settings->value("suggest_loaded_core_first", false).toBool());
   m_allPlaylistsListMaxCountSpinBox->setValue(m_settings->value("all_playlists_list_max_count", 0).toInt());
   m_allPlaylistsGridMaxCountSpinBox->setValue(m_settings->value("all_playlists_grid_max_count", 5000).toInt());
   m_thumbnailCacheSpinBox->setValue(m_settings->value("thumbnail_cache_limit", 512).toInt());

   themeIndex = m_themeComboBox->findData(m_mainwindow->getThemeFromString(m_settings->value("theme", "default").toString()));

   if (m_themeComboBox->count() > themeIndex)
      m_themeComboBox->setCurrentIndex(themeIndex);

   thumbnailIndex = m_thumbnailComboBox->findData(m_mainwindow->getThumbnailTypeFromString(m_settings->value("icon_view_thumbnail_type", "boxart").toString()));

   if (m_thumbnailComboBox->count() > thumbnailIndex)
      m_thumbnailComboBox->setCurrentIndex(thumbnailIndex);

   if (highlightColor.isValid())
   {
      m_highlightColor = highlightColor;
      highlightPixmap.fill(m_highlightColor);
      m_highlightColorPushButton->setIcon(highlightPixmap);
   }

   showOrHideHighlightColor();

   m_startupPlaylistComboBox->clear();

   for (i = 0; i < playlists.count(); i++)
   {
      const QPair<QString, QString> &pair = playlists.at(i);

      m_startupPlaylistComboBox->addItem(pair.first, pair.second);
   }

   playlistIndex = m_startupPlaylistComboBox->findData(initialPlaylist, Qt::UserRole, Qt::MatchFixedString);

   if (playlistIndex >= 0)
      m_startupPlaylistComboBox->setCurrentIndex(playlistIndex);
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
   m_settings->setValue("initial_playlist", m_startupPlaylistComboBox->currentData(Qt::UserRole).toString());
   m_settings->setValue("icon_view_thumbnail_type", m_mainwindow->getCurrentThumbnailTypeString());
   m_settings->setValue("thumbnail_cache_limit", m_thumbnailCacheSpinBox->value());

   if (!m_mainwindow->customThemeString().isEmpty())
      m_settings->setValue("custom_theme", m_customThemePath);

   m_mainwindow->setAllPlaylistsListMaxCount(m_allPlaylistsListMaxCountSpinBox->value());
   m_mainwindow->setAllPlaylistsGridMaxCount(m_allPlaylistsGridMaxCountSpinBox->value());
   m_mainwindow->setThumbnailCacheLimit(m_thumbnailCacheSpinBox->value());
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
   setWindowFlags(windowFlags() | Qt::Tool);
   show();
   activateWindow();
}

void ViewOptionsDialog::hideDialog()
{
   reject();
}
