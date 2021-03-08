#include <QSettings>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QMessageBox>
#include <QPainter>
#include <QScrollArea>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QToolButton>
#include <QPushButton>
#include <QMenu>
#include <QFileDialog>
#include <QTimer>
#include <QColor>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QMainWindow>
#include <QApplication>
#include <QColorDialog>
#include <QLabel>

#ifdef HAVE_MENU
#include <QBitmap>
#include <QStackedLayout>
#include <QScrollBar>

#include "options/options.h"
#endif

#include "coreoptionsdialog.h"
#include "viewoptionsdialog.h"
#include "coreinfodialog.h"
#include "playlistentrydialog.h"
#include "../ui_qt.h"

#ifndef CXX_BUILD
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include "../../../config.h"
#endif

#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <file/file_path.h>

#ifdef HAVE_MENU
#include "../../../menu/menu_driver.h"
#endif

#include "../../../command.h"
#include "../../../configuration.h"
#include "../../../core_info.h"
#include "../../../file_path_special.h"
#include "../../../msg_hash.h"
#include "../../../core_option_manager.h"
#include "../../../file_path_special.h"
#include "../../../paths.h"
#include "../../../retroarch.h"

#ifndef CXX_BUILD
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

PlaylistEntryDialog::PlaylistEntryDialog(MainWindow *mainwindow, QWidget *parent) :
   QDialog(parent)
   ,m_mainwindow(mainwindow)
   ,m_settings(mainwindow->settings())
   ,m_nameLineEdit(new QLineEdit(this))
   ,m_pathLineEdit(new QLineEdit(this))
   ,m_extensionsLineEdit(new QLineEdit(this))
   ,m_coreComboBox(new QComboBox(this))
   ,m_databaseComboBox(new QComboBox(this))
   ,m_extensionArchiveCheckBox(new QCheckBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_FILTER_INSIDE_ARCHIVES), this))
{
   QFormLayout *form                = new QFormLayout();
   QDialogButtonBox *buttonBox      = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
   QVBoxLayout *databaseVBoxLayout  = new QVBoxLayout();
   QHBoxLayout *pathHBoxLayout      = new QHBoxLayout();
   QHBoxLayout *extensionHBoxLayout = new QHBoxLayout();
   QLabel *databaseLabel            = new QLabel(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FOR_THUMBNAILS), this);
   QToolButton *pathPushButton      = new QToolButton(this);

   pathPushButton->setText(QStringLiteral("..."));

   pathHBoxLayout->addWidget(m_pathLineEdit);
   pathHBoxLayout->addWidget(pathPushButton);

   databaseVBoxLayout->addWidget(m_databaseComboBox);
   databaseVBoxLayout->addWidget(databaseLabel);

   extensionHBoxLayout->addWidget(m_extensionsLineEdit);
   extensionHBoxLayout->addWidget(m_extensionArchiveCheckBox);

   m_extensionsLineEdit->setPlaceholderText(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS_PLACEHOLDER));

   /* Ensure placeholder text is completely visible. */
   m_extensionsLineEdit->setMinimumWidth(QFontMetrics(m_extensionsLineEdit->font()).boundingRect(m_extensionsLineEdit->placeholderText()).width() + m_extensionsLineEdit->frameSize().width());

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
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS), extensionHBoxLayout);

   qobject_cast<QVBoxLayout*>(layout())->addLayout(form);
   layout()->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));
   layout()->addWidget(buttonBox);

   connect(pathPushButton, SIGNAL(clicked()), this, SLOT(onPathClicked()));
}

bool PlaylistEntryDialog::filterInArchive()
{
   return m_extensionArchiveCheckBox->isChecked();
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
   unsigned i, j;
   core_info_list_t *core_info_list = NULL;

   m_nameLineEdit->clear();
   m_pathLineEdit->clear();
   m_coreComboBox->clear();
   m_databaseComboBox->clear();

   m_coreComboBox->addItem(
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK));
   m_databaseComboBox->addItem(
           QString("<") 
         + msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE) 
         + ">",
         QFileInfo(m_mainwindow->getCurrentPlaylistPath()).fileName().remove(".lpl"));

   core_info_get_list(&core_info_list);

   if (core_info_list && core_info_list->count > 0)
   {
      QVector<QHash<QString, QString> > allCores;
      QStringList allDatabases;

      for (i = 0; i < core_info_list->count; i++)
      {
         QString ui_display_name;
         QHash<QString, QString> hash;
         const core_info_t *core = &core_info_list->list[i];
         QStringList databases   = string_split_to_qt(QString(core->databases), '|');

         hash["core_name"]         = core->core_name;
         hash["core_display_name"] = core->display_name;
         hash["core_path"]         = core->path;
         hash["core_databases"]    = core->databases;

         ui_display_name           = hash.value("core_name");

         if (ui_display_name.isEmpty())
            ui_display_name        = hash.value("core_display_name");
         if (ui_display_name.isEmpty())
            ui_display_name        = QFileInfo(
                  hash.value("core_path")).fileName();

         if (ui_display_name.isEmpty())
            continue;

         hash["ui_display_name"] = ui_display_name;

         for (j = 0; static_cast<int>(j) < databases.count(); j++)
         {
            QString database = databases.at(static_cast<int>(j));

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

      for (j = 0; static_cast<int>(j) < allCores.count(); j++)
      {
         const QHash<QString, QString> &hash = allCores.at(static_cast<int>(j));

         m_coreComboBox->addItem(hash.value("ui_display_name"), QVariant::fromValue(hash));
      }

      for (j = 0; static_cast<int>(j) < allDatabases.count(); j++)
      {
         QString database = allDatabases.at(static_cast<int>(j));
         m_databaseComboBox->addItem(database, database);
      }
   }
}

bool PlaylistEntryDialog::nameFieldEnabled()
{
   return m_nameLineEdit->isEnabled();
}

void PlaylistEntryDialog::setEntryValues(
      const QHash<QString, QString> &contentHash)
{
   QString db;
   QString coreName = contentHash.value("core_name");
   int foundDB = 0;
   int i       = 0;

   loadPlaylistOptions();

   if (contentHash.isEmpty())
   {
      m_nameLineEdit->setText(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE));
      m_pathLineEdit->setText(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE));
      m_nameLineEdit->setEnabled(false);
      m_pathLineEdit->setEnabled(false);
   }
   else
   {
      m_nameLineEdit->setText(contentHash.value("label"));
      m_pathLineEdit->setText(contentHash.value("path"));
      m_nameLineEdit->setEnabled(true);
      m_pathLineEdit->setEnabled(true);
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

const QStringList PlaylistEntryDialog::getSelectedExtensions()
{
   QStringList list;
   QString text = m_extensionsLineEdit->text();

   /* Otherwise it would create a QStringList with a single blank entry... */
   if (!text.isEmpty())
      list   = string_split_to_qt(text, ' ');
   return list;
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
   int      row  = 0;
   int row_count = m_formLayout->rowCount();
   int       i   = 0;
   QVector<QHash<QString, QString> > info_list 
                 = m_mainwindow->getCoreInfo();

   if (row_count > 0)
   {
      for (row = 0; row < row_count; row++)
      {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
         /* removeRow() and takeRow() was only added in 5.8! */
         m_formLayout->removeRow(0);
#else
         /* something is buggy here... 
          * sometimes items appear duplicated, and other times not */
         QLayoutItem *item = m_formLayout->itemAt(0);
         QWidget        *w = NULL;

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

   if (info_list.count() == 0)
      return;

   for (i = 0; i < info_list.count(); i++)
   {
      const QHash<QString, QString> &line = info_list.at(i);
      QLabel                       *label = new QLabel(line.value("key"));
      CoreInfoLabel                *value = new CoreInfoLabel(line.value("value"));
      QString                  labelStyle = line.value("label_style");
      QString                  valueStyle = line.value("value_style");

      if (!labelStyle.isEmpty())
         label->setStyleSheet(labelStyle);

      if (!valueStyle.isEmpty())
         value->setStyleSheet(valueStyle);

      m_formLayout->addRow(label, value);
   }

   show();
}

#ifdef HAVE_MENU
QPixmap getColorizedPixmap(const QPixmap& oldPixmap, const QColor& color)
{
   QPixmap pixmap = oldPixmap;
   QBitmap   mask = pixmap.createMaskFromColor(Qt::transparent, Qt::MaskInColor);
   pixmap.fill(color);
   pixmap.setMask(mask);
   return pixmap;
}

QColor getLabelColor(const QString& objectName)
{
   QLabel dummyColor;
   dummyColor.setObjectName(objectName);
   dummyColor.ensurePolished();
   return dummyColor.palette().color(QPalette::Foreground);
}

/* stolen from Qt Creator */
class SmartScrollArea : public QScrollArea
{
public:
   explicit SmartScrollArea(QWidget *parent) :
      QScrollArea(parent)
   {
      setFrameStyle(QFrame::NoFrame | QFrame::Plain);
      viewport()->setAutoFillBackground(false);
      setWidgetResizable(true);
   }
private:
   void resizeEvent(QResizeEvent *event) final
   {
      QWidget *inner = widget();

      if (inner)
      {
         int              fw = frameWidth() * 2;
         QSize     innerSize = event->size() - QSize(fw, fw);
         QSize innerSizeHint = inner->minimumSizeHint();

         /* Widget wants to be bigger than available space */
         if (innerSizeHint.height() > innerSize.height())
         { 
            innerSize.setWidth(innerSize.width() - scrollBarWidth());
            innerSize.setHeight(innerSizeHint.height());
         }
         inner->resize(innerSize);
      }
      QScrollArea::resizeEvent(event);
   }

   QSize minimumSizeHint() const final
   {
      static const int max_min_width  = 250;
      static const int max_min_height = 250;
      QWidget *inner                  = widget();

      if (inner)
      {
         int        fw                = frameWidth() * 2;
         QSize minSize                = inner->minimumSizeHint();

         minSize                     += QSize(fw, fw);
         minSize                     += QSize(scrollBarWidth(), 0);
         minSize.setWidth(qMin(minSize.width(), max_min_width));
         minSize.setHeight(qMin(minSize.height(), max_min_height));
         return minSize;
      }
      return QSize(0, 0);
   }

   bool event(QEvent *event) final
   {
      if (event->type() == QEvent::LayoutRequest)
         updateGeometry();
      return QScrollArea::event(event);
   }

   int scrollBarWidth() const
   {
      SmartScrollArea *that = const_cast<SmartScrollArea *>(this);
      QWidgetList list = that->scrollBarWidgets(Qt::AlignRight);
      if (list.isEmpty())
         return 0;
      return list.first()->sizeHint().width();
   }
};

ViewOptionsDialog::ViewOptionsDialog(MainWindow *mainwindow, QWidget *parent) :
   QDialog(mainwindow)
   ,m_optionsList(new QListWidget(this))
   ,m_optionsStack(new QStackedLayout)
{
   int width;
   QGridLayout        *layout = new QGridLayout(this);
   QLabel      *m_headerLabel = new QLabel(this);
   /* Header label with large font and a bit of spacing 
    * (align with group boxes) */
   QFont      headerLabelFont = m_headerLabel->font();
   const int        pointSize = headerLabelFont.pointSize();
   QHBoxLayout *headerHLayout = new QHBoxLayout;
   const int       leftMargin = QApplication::style()->pixelMetric(QStyle::PM_LayoutLeftMargin);

   m_optionsStack->setMargin(0);

   headerLabelFont.setBold(true);

   /* Paranoia: Should a font be set in pixels... */
   if (pointSize > 0)
      headerLabelFont.setPointSize(pointSize + 2);

   m_headerLabel->setFont(headerLabelFont);

   headerHLayout->addSpacerItem(new QSpacerItem(leftMargin, 0, QSizePolicy::Fixed, QSizePolicy::Ignored));
   headerHLayout->addWidget(m_headerLabel);

   addCategory(new DriversCategory(this));
   addCategory(new VideoCategory(this));
   addCategory(new AudioCategory(this));
   addCategory(new InputCategory(this));
   addCategory(new LatencyCategory(this));
   addCategory(new CoreCategory(this));
   addCategory(new ConfigurationCategory(this));
   addCategory(new SavingCategory(this));
   addCategory(new LoggingCategory(this));
   addCategory(new FrameThrottleCategory(this));
   addCategory(new RecordingCategory(this));
   addCategory(new OnscreenDisplayCategory(this));
   addCategory(new UserInterfaceCategory(mainwindow, this));
   addCategory(new AIServiceCategory(this));
   addCategory(new AchievementsCategory(this));
   addCategory(new NetworkCategory(this));
   addCategory(new PlaylistsCategory(this));
   addCategory(new UserCategory(this));
   addCategory(new DirectoryCategory(this));

   width  = 
        m_optionsList->sizeHintForColumn(0) 
      + m_optionsList->frameWidth() * 2 
      + 5;
   width += m_optionsList->verticalScrollBar()->sizeHint().width();

   m_optionsList->setMaximumWidth(width);
   m_optionsList->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);

   setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE));

   layout->addWidget(m_optionsList, 0, 0, 2, 1);
   layout->addLayout(headerHLayout, 0, 1);
   layout->addLayout(m_optionsStack, 1, 1);

   connect(m_optionsList, SIGNAL(currentRowChanged(int)), m_optionsStack, SLOT(setCurrentIndex(int)));
   connect(m_optionsList, SIGNAL(currentTextChanged(const QString&)), m_headerLabel, SLOT(setText(const QString&)));

   connect(this, SIGNAL(rejected()), this, SLOT(onRejected()));
}

QIcon getIcon(OptionsCategory *category)
{
   settings_t *settings        = config_get_ptr();
   const char *path_dir_assets = settings->paths.directory_assets;
   QPixmap pixmap              = QPixmap(QString(path_dir_assets) 
         + "/xmb/monochrome/png/" 
         + category->categoryIconName() 
         + ".png");
   return QIcon(getColorizedPixmap(pixmap, getLabelColor("iconColor")));
}

void ViewOptionsDialog::addCategory(OptionsCategory *category)
{
   QTabWidget *tabWidget = new QTabWidget();

   m_categoryList.append(category);

   for (OptionsPage* page : category->pages())
   {
      SmartScrollArea *scrollArea = new SmartScrollArea(this);
      QWidget             *widget = page->widget();

      scrollArea->setWidget(widget);
      widget->setAutoFillBackground(false);
      tabWidget->addTab(scrollArea, page->displayName());
   }

#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
   tabWidget->setTabBarAutoHide(true);
#else
   /* TODO remove the tabBar's space */
   if (tabWidget->count() < 2)
      tabWidget->tabBar()->hide();
#endif
   m_optionsList->addItem(
         new QListWidgetItem(getIcon(category),
            category->displayName()));
   m_optionsStack->addWidget(tabWidget);
}

void ViewOptionsDialog::repaintIcons()
{
   unsigned i;

   for (i = 0; i < m_categoryList.size(); i++)
      m_optionsList->item(i)->setIcon(getIcon(m_categoryList.at(i)));
}
#else
ViewOptionsDialog::ViewOptionsDialog(MainWindow *mainwindow, QWidget *parent) :
   QDialog(mainwindow)
   , m_viewOptionsWidget(new ViewOptionsWidget(mainwindow))
{
   QVBoxLayout         *layout = new QVBoxLayout;
   QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

   connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
   connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

   connect(this, SIGNAL(accepted()), m_viewOptionsWidget, SLOT(onAccepted()));
   connect(this, SIGNAL(rejected()), m_viewOptionsWidget, SLOT(onRejected()));

   setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE));

   layout->setContentsMargins(0, 0, 0, 0);

   layout->addWidget(m_viewOptionsWidget);
   layout->addWidget(buttonBox);

   setLayout(layout);
}
#endif

void ViewOptionsDialog::showDialog()
{
#ifndef HAVE_MENU
   m_viewOptionsWidget->loadViewOptions();
#else
   unsigned i;
   for (i = 0; i < m_categoryList.size(); i++)
      m_categoryList.at(i)->load();
#endif
   show();
   activateWindow();
}

void ViewOptionsDialog::hideDialog()
{
   reject();
}

void ViewOptionsDialog::onRejected()
{
#ifdef HAVE_MENU
   int i;
   for (i = 0; i < m_categoryList.size(); i++)
      m_categoryList.at(i)->apply();
#endif
}

ViewOptionsWidget::ViewOptionsWidget(MainWindow *mainwindow, QWidget *parent) :
   QWidget(parent)
   ,m_mainwindow(mainwindow)
   ,m_settings(mainwindow->settings())
   ,m_saveGeometryCheckBox(new QCheckBox(this))
   ,m_saveDockPositionsCheckBox(new QCheckBox(this))
   ,m_saveLastTabCheckBox(new QCheckBox(this))
   ,m_showHiddenFilesCheckBox(new QCheckBox(this))
   ,m_themeComboBox(new QComboBox(this))
   ,m_thumbnailCacheSpinBox(new QSpinBox(this))
   ,m_thumbnailDropSizeSpinBox(new QSpinBox(this))
   ,m_startupPlaylistComboBox(new QComboBox(this))
   ,m_highlightColorPushButton(new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CHOOSE), this))
   ,m_highlightColor()
   ,m_highlightColorLabel(new QLabel(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR), this))
   ,m_customThemePath()
   ,m_suggestLoadedCoreFirstCheckBox(new QCheckBox(this))
   /* ,m_allPlaylistsListMaxCountSpinBox(new QSpinBox(this)) */
   /* ,m_allPlaylistsGridMaxCountSpinBox(new QSpinBox(this)) */
{
   QVBoxLayout *layout = new QVBoxLayout;
   QFormLayout *form   = new QFormLayout;

   m_themeComboBox->addItem(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT), MainWindow::THEME_SYSTEM_DEFAULT);
   m_themeComboBox->addItem(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_DARK), MainWindow::THEME_DARK);
   m_themeComboBox->addItem(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM), MainWindow::THEME_CUSTOM);

   m_thumbnailCacheSpinBox->setSuffix(" MB");
   m_thumbnailCacheSpinBox->setRange(0, 99999);
   
   m_thumbnailDropSizeSpinBox->setSuffix(" px");
   m_thumbnailDropSizeSpinBox->setRange(0, 99999);

   /* m_allPlaylistsListMaxCountSpinBox->setRange(0, 99999); */
   /* m_allPlaylistsGridMaxCountSpinBox->setRange(0, 99999); */

   form->setFormAlignment(Qt::AlignCenter);
   form->setLabelAlignment(Qt::AlignCenter);

   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY), m_saveGeometryCheckBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS), m_saveDockPositionsCheckBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB), m_saveLastTabCheckBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES), m_showHiddenFilesCheckBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST), m_suggestLoadedCoreFirstCheckBox);
#if 0
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT), m_allPlaylistsListMaxCountSpinBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT), m_allPlaylistsGridMaxCountSpinBox);
#endif
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_STARTUP_PLAYLIST), m_startupPlaylistComboBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT), m_thumbnailCacheSpinBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_DROP_SIZE_LIMIT), m_thumbnailDropSizeSpinBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME), m_themeComboBox);
   form->addRow(m_highlightColorLabel, m_highlightColorPushButton);

   layout->addLayout(form);

   layout->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));

   setLayout(layout);

   loadViewOptions();

   connect(m_themeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onThemeComboBoxIndexChanged(int)));
   connect(m_highlightColorPushButton, SIGNAL(clicked()), this, SLOT(onHighlightColorChoose()));
}

void ViewOptionsWidget::onThemeComboBoxIndexChanged(int)
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

void ViewOptionsWidget::onHighlightColorChoose()
{
   QPixmap highlightPixmap(m_highlightColorPushButton->iconSize());
   QColor currentHighlightColor = m_settings->value("highlight_color",
         QApplication::palette().highlight().color()).value<QColor>();
   QColor newHighlightColor     = QColorDialog::getColor(
         currentHighlightColor, this,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR));

   if (newHighlightColor.isValid())
   {
      MainWindow::Theme theme = static_cast<MainWindow::Theme>(
            m_themeComboBox->currentData(Qt::UserRole).toInt());

      m_highlightColor = newHighlightColor;
      m_settings->setValue("highlight_color", m_highlightColor);
      highlightPixmap.fill(m_highlightColor);
      m_highlightColorPushButton->setIcon(highlightPixmap);
      m_mainwindow->setTheme(theme);
   }
}

void ViewOptionsWidget::loadViewOptions()
{
   int i;
   int themeIndex    = 0;
   int playlistIndex = 0;
   QColor highlightColor                       = 
      m_settings->value("highlight_color",
            QApplication::palette().highlight().color()).value<QColor>();
   QPixmap highlightPixmap(m_highlightColorPushButton->iconSize());
   QVector<QPair<QString, QString> > playlists = m_mainwindow->getPlaylists();
   QString initialPlaylist = m_settings->value("initial_playlist",
         m_mainwindow->getSpecialPlaylistPath(
            SPECIAL_PLAYLIST_HISTORY)).toString();

   m_saveGeometryCheckBox->setChecked(m_settings->value("save_geometry", false).toBool());
   m_saveDockPositionsCheckBox->setChecked(m_settings->value("save_dock_positions", false).toBool());
   m_saveLastTabCheckBox->setChecked(m_settings->value("save_last_tab", false).toBool());
   m_showHiddenFilesCheckBox->setChecked(m_settings->value("show_hidden_files", true).toBool());
   m_suggestLoadedCoreFirstCheckBox->setChecked(m_settings->value("suggest_loaded_core_first", false).toBool());
#if 0
   m_allPlaylistsListMaxCountSpinBox->setValue(m_settings->value("all_playlists_list_max_count", 0).toInt());
   m_allPlaylistsGridMaxCountSpinBox->setValue(m_settings->value("all_playlists_grid_max_count", 5000).toInt());
#endif
   m_thumbnailCacheSpinBox->setValue(m_settings->value("thumbnail_cache_limit", 512).toInt());
   m_thumbnailDropSizeSpinBox->setValue(m_settings->value("thumbnail_max_size", 0).toInt());

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

   m_startupPlaylistComboBox->clear();

   for (i = 0; i < playlists.count(); i++)
   {
      const QPair<QString, QString> &pair = playlists.at(i);

      m_startupPlaylistComboBox->addItem(pair.first, pair.second);
   }

   playlistIndex = m_startupPlaylistComboBox->findData(
         initialPlaylist, Qt::UserRole, Qt::MatchFixedString);

   if (playlistIndex >= 0)
      m_startupPlaylistComboBox->setCurrentIndex(playlistIndex);
}

void ViewOptionsWidget::showOrHideHighlightColor()
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

void ViewOptionsWidget::saveViewOptions()
{
   m_settings->setValue("save_geometry", m_saveGeometryCheckBox->isChecked());
   m_settings->setValue("save_dock_positions", m_saveDockPositionsCheckBox->isChecked());
   m_settings->setValue("save_last_tab", m_saveLastTabCheckBox->isChecked());
   m_settings->setValue("theme", m_mainwindow->getThemeString(static_cast<MainWindow::Theme>(m_themeComboBox->currentData(Qt::UserRole).toInt())));
   m_settings->setValue("show_hidden_files", m_showHiddenFilesCheckBox->isChecked());
   m_settings->setValue("highlight_color", m_highlightColor);
   m_settings->setValue("suggest_loaded_core_first", m_suggestLoadedCoreFirstCheckBox->isChecked());
#if 0
   m_settings->setValue("all_playlists_list_max_count", m_allPlaylistsListMaxCountSpinBox->value());
   m_settings->setValue("all_playlists_grid_max_count", m_allPlaylistsGridMaxCountSpinBox->value());
#endif
   m_settings->setValue("initial_playlist", m_startupPlaylistComboBox->currentData(Qt::UserRole).toString());
   m_settings->setValue("thumbnail_cache_limit", m_thumbnailCacheSpinBox->value());
   m_settings->setValue("thumbnail_max_size", m_thumbnailDropSizeSpinBox->value());

   if (!m_mainwindow->customThemeString().isEmpty())
      m_settings->setValue("custom_theme", m_customThemePath);

#if 0
   m_mainwindow->setAllPlaylistsListMaxCount(m_allPlaylistsListMaxCountSpinBox->value());
   m_mainwindow->setAllPlaylistsGridMaxCount(m_allPlaylistsGridMaxCountSpinBox->value());
#endif
   m_mainwindow->setThumbnailCacheLimit(m_thumbnailCacheSpinBox->value());
}

void ViewOptionsWidget::onAccepted()
{
   saveViewOptions();
}

void ViewOptionsWidget::onRejected()
{
   loadViewOptions();
}

CoreOptionsDialog::CoreOptionsDialog(QWidget *parent) :
   QDialog(parent)
   ,m_layout()
   ,m_scrollArea()
{
   setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS));
   setObjectName("coreOptionsDialog");

   resize(720, 480);

   QTimer::singleShot(0, this, SLOT(clearLayout()));
}

CoreOptionsDialog::~CoreOptionsDialog()
{
}

void CoreOptionsDialog::resizeEvent(QResizeEvent *event)
{
   QDialog::resizeEvent(event);

   if (!m_scrollArea)
      return;

   m_scrollArea->resize(event->size());

   emit resized(event->size());
}

void CoreOptionsDialog::closeEvent(QCloseEvent *event)
{
   QDialog::closeEvent(event);

   emit closed();
}

void CoreOptionsDialog::paintEvent(QPaintEvent *event)
{
   QStyleOption o;
   QPainter p;
   o.initFrom(this);
   p.begin(this);
   style()->drawPrimitive(
      QStyle::PE_Widget, &o, &p, this);
   p.end();

   QDialog::paintEvent(event);
}

void CoreOptionsDialog::clearLayout()
{
   QWidget *widget = NULL;

   if (m_scrollArea)
   {
      foreach (QObject *obj, children())
      {
         obj->deleteLater();
      }
   }

   m_layout = new QVBoxLayout();

   widget = new QWidget();
   widget->setLayout(m_layout);
   widget->setObjectName("coreOptionsWidget");

   m_scrollArea = new QScrollArea();

   m_scrollArea->setParent(this);
   m_scrollArea->setWidgetResizable(true);
   m_scrollArea->setWidget(widget);
   m_scrollArea->setObjectName("coreOptionsScrollArea");
   m_scrollArea->show();
}

void CoreOptionsDialog::reload()
{
   buildLayout();
}

void CoreOptionsDialog::onSaveGameSpecificOptions()
{
#ifdef HAVE_MENU
   bool refresh = false;
#endif

   if (!core_options_create_override(true))
      QMessageBox::critical(this, msg_hash_to_str(MSG_ERROR), msg_hash_to_str(MSG_ERROR_SAVING_CORE_OPTIONS_FILE));

#ifdef HAVE_MENU
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
#endif
}

void CoreOptionsDialog::onSaveFolderSpecificOptions()
{
#ifdef HAVE_MENU
   bool refresh = false;
#endif

   if (!core_options_create_override(false))
      QMessageBox::critical(this, msg_hash_to_str(MSG_ERROR), msg_hash_to_str(MSG_ERROR_SAVING_CORE_OPTIONS_FILE));

#ifdef HAVE_MENU
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
#endif
}

void CoreOptionsDialog::onCoreOptionComboBoxCurrentIndexChanged(int index)
{
   unsigned i, k;
   QString key, val;
   size_t          opts = 0;
   QComboBox *combo_box = qobject_cast<QComboBox*>(sender());

   if (!combo_box)
      return;

   key = combo_box->itemData(index, Qt::UserRole).toString();
   val = combo_box->itemText(index);

   if (rarch_ctl(RARCH_CTL_HAS_CORE_OPTIONS, NULL))
   {
      rarch_ctl(RARCH_CTL_GET_CORE_OPTION_SIZE, &opts);

      if (opts)
      {
         core_option_manager_t *coreopts = NULL;

         rarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

         if (coreopts)
         {
            for (i = 0; i < opts; i++)
            {
               QString optKey;
               struct core_option *option = static_cast<struct core_option*>(&coreopts->opts[i]);

               if (!option)
                  continue;

               optKey = option->key;

               if (key == optKey)
               {
                  for (k = 0; k < option->vals->size; k++)
                  {
                     QString str = option->vals->elems[k].data;

                     if (!str.isEmpty() && str == val)
                        core_option_manager_set_val(coreopts, i, k);
                  }
               }
            }
         }
      }
   }
}

void CoreOptionsDialog::buildLayout()
{
   unsigned j, k;
   size_t opts = 0;
   QFormLayout    *form  = NULL;
   settings_t *settings  = config_get_ptr();
   bool has_core_options = rarch_ctl(RARCH_CTL_HAS_CORE_OPTIONS, NULL);

   clearLayout();

   if (has_core_options)
   {
      rarch_ctl(RARCH_CTL_GET_CORE_OPTION_SIZE, &opts);

      if (opts)
      {
         core_option_manager_t *coreopts = NULL;

         form = new QFormLayout();

         if (settings->bools.game_specific_options)
         {
            QString contentLabel;
            QString label;
            rarch_system_info_t *system = runloop_get_system_info();

            /* TODO/FIXME - why have this check here? system is not used */
            if (system)
               contentLabel = QFileInfo(path_get(RARCH_PATH_BASENAME)).completeBaseName();

            if (!contentLabel.isEmpty())
            {
               if (!rarch_ctl(RARCH_CTL_IS_GAME_OPTIONS_ACTIVE, NULL))
                  label = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE);
               else
                  label = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE);

               if (!label.isEmpty())
               {
                  QHBoxLayout *gameOptionsLayout = new QHBoxLayout();
                  QPushButton *button = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SAVE), this);

                  connect(button, SIGNAL(clicked()), this, SLOT(onSaveGameSpecificOptions()));

                  gameOptionsLayout->addWidget(new QLabel(contentLabel, this));
                  gameOptionsLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Preferred));
                  gameOptionsLayout->addWidget(button);

                  form->addRow(label, gameOptionsLayout);
               }
            }
         }

         rarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

         if (coreopts)
         {
            QToolButton *resetAllButton = new QToolButton(this);

            resetAllButton->setDefaultAction(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_RESET_ALL), this));
            connect(resetAllButton, SIGNAL(clicked()), this, SLOT(onCoreOptionResetAllClicked()));

            for (j = 0; j < opts; j++)
            {
               QString desc               = 
                  core_option_manager_get_desc(coreopts, j);
               QString val                = 
                  core_option_manager_get_val(coreopts, j);
               QComboBox *combo_box       = NULL;
               QLabel *descLabel          = NULL;
               QHBoxLayout *comboLayout   = NULL;
               QToolButton *resetButton   = NULL;
               struct core_option *option = NULL;

               if (desc.isEmpty() || !coreopts->opts)
                  continue;

               option = static_cast<struct core_option*>(&coreopts->opts[j]);

               if (!option->vals || option->vals->size == 0)
                  continue;

               comboLayout = new QHBoxLayout();
               descLabel   = new QLabel(desc, this);
               combo_box   = new QComboBox(this);
               combo_box->setObjectName("coreOptionComboBox");
               resetButton = new QToolButton(this);
               resetButton->setObjectName("resetButton");
               resetButton->setDefaultAction(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_RESET), this));
               resetButton->setProperty("comboBox",
                     QVariant::fromValue(combo_box));

               connect(resetButton, SIGNAL(clicked()),
                     this, SLOT(onCoreOptionResetClicked()));

               if (!string_is_empty(option->info))
               {
                  char *new_info = strdup(option->info);
                  word_wrap(new_info, new_info, 50, true, 0);
                  descLabel->setToolTip(new_info);
                  combo_box->setToolTip(new_info);
                  free(new_info);
               }

               for (k = 0; k < option->vals->size; k++)
                  combo_box->addItem(option->vals->elems[k].data, option->key);

               combo_box->setCurrentText(val);
               combo_box->setProperty("default_index",
                     static_cast<unsigned>(option->default_index));

               /* Only connect the signal after setting the default item */
               connect(combo_box, SIGNAL(currentIndexChanged(int)),
                     this,
                     SLOT(onCoreOptionComboBoxCurrentIndexChanged(int)));

               comboLayout->addWidget(combo_box);
               comboLayout->addWidget(resetButton);

               form->addRow(descLabel, comboLayout);
            }

            form->addRow(resetAllButton, new QWidget(this));

            m_layout->addLayout(form);
         }
      }
   }

   if (!opts)
   {
      QLabel *noParamsLabel = new QLabel(msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE), this);
      noParamsLabel->setAlignment(Qt::AlignCenter);

      m_layout->addWidget(noParamsLabel);
   }

   m_layout->addItem(new QSpacerItem(20, 20,
            QSizePolicy::Minimum, QSizePolicy::Expanding));

   resize(width() + 1, height());
   show();
   resize(width() - 1, height());
}

void CoreOptionsDialog::onCoreOptionResetClicked()
{
   bool ok              = false;
   QToolButton *button  = qobject_cast<QToolButton*>(sender());
   QComboBox *combo_box = NULL;
   int default_index    = 0;

   if (!button)
      return;

   combo_box = qobject_cast<QComboBox*>(button->property("comboBox").value<QComboBox*>());

   if (!combo_box)
      return;

   default_index = combo_box->property("default_index").toInt(&ok);

   if (!ok)
      return;

   if (default_index >= 0 && default_index < combo_box->count())
      combo_box->setCurrentIndex(default_index);
}

void CoreOptionsDialog::onCoreOptionResetAllClicked()
{
   int i;
   QList<QComboBox*> combo_boxes = findChildren<QComboBox*>("coreOptionComboBox");

   for (i = 0; i < combo_boxes.count(); i++)
   {
      int   default_index  = 0;
      bool             ok  = false;
      QComboBox *combo_box = combo_boxes.at(i);

      if (!combo_box)
         continue;

      default_index        = combo_box->property("default_index").toInt(&ok);

      if (!ok)
         continue;

      if (default_index >= 0 && default_index < combo_box->count())
         combo_box->setCurrentIndex(default_index);
   }
}
