#include <QCloseEvent>
#include <QSettings>
#include <QResizeEvent>
#include <QMessageBox>
#include <QPainter>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QFileInfo>
#include <QFileDialog>
#include <QTimer>
#include <QMainWindow>
#include <QApplication>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QColor>
#include <QColorDialog>
#include <QLabel>
#include <QBitmap>
#include <QStackedLayout>
#include <QScrollBar>

#include "qt_dialogs.h"
#include "../ui_qt.h"

#ifndef CXX_BUILD
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include "../../../config.h"
#endif

#include <math.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <file/file_path.h>

#ifdef HAVE_MENU
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_entries.h"
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#include "../../../menu/menu_shader.h"
#endif
#endif

#include "../../../command.h"
#include "../../../core_info.h"
#include "../../../core_option_manager.h"
#include "../../../configuration.h"
#include "../../../file_path_special.h"
#include "../../../msg_hash.h"
#include "../../../paths.h"
#include "../../../retroarch.h"

#ifndef CXX_BUILD
}
#endif

#include "qt_options.h"

#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#pragma warning(disable:4566)
#endif

static inline bool comp_string_lower(const QString &lhs, const QString &rhs)
{
   return lhs.toLower() < rhs.toLower();
}

static inline bool comp_hash_ui_display_name_key_lower(const QHash<QString, QString> &lhs, const QHash<QString, QString> &rhs)
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
   size_t list_size = (size_t)m_categoryList.size();

   for (i = 0; i < list_size; i++)
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
#ifdef HAVE_MENU
   unsigned i;
   size_t list_size = (size_t)m_categoryList.size();
   for (i = 0; i < list_size; i++)
      m_categoryList.at(i)->load();
#else
   m_viewOptionsWidget->loadViewOptions();
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
   unsigned i;
   size_t list_size = (size_t)m_categoryList.size();
   for (i = 0; i < list_size; i++)
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

   if (retroarch_ctl(RARCH_CTL_HAS_CORE_OPTIONS, NULL))
   {
      retroarch_ctl(RARCH_CTL_GET_CORE_OPTION_SIZE, &opts);

      if (opts)
      {
         core_option_manager_t *coreopts = NULL;

         retroarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

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
                        core_option_manager_set_val(coreopts, i, k, true);
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
   bool has_core_options = retroarch_ctl(RARCH_CTL_HAS_CORE_OPTIONS, NULL);

   clearLayout();

   if (has_core_options)
   {
      retroarch_ctl(RARCH_CTL_GET_CORE_OPTION_SIZE, &opts);

      if (opts)
      {
         core_option_manager_t *coreopts = NULL;

         form = new QFormLayout();

         if (settings->bools.game_specific_options)
         {
            QString contentLabel;
            QString label;
            rarch_system_info_t *system = &runloop_state_get_ptr()->system;

            /* TODO/FIXME - why have this check here? system is not used */
            if (system)
               contentLabel = QFileInfo(path_get(RARCH_PATH_BASENAME)).completeBaseName();

            if (!contentLabel.isEmpty())
            {
               uint32_t flags = runloop_get_flags();
               if (flags & RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE)
                  label = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE);
               else
                  label = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE);

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

         retroarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

         if (coreopts)
         {
            QToolButton *resetAllButton = new QToolButton(this);

            resetAllButton->setDefaultAction(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_RESET_ALL), this));
            connect(resetAllButton, SIGNAL(clicked()), this, SLOT(onCoreOptionResetAllClicked()));

            for (j = 0; j < opts; j++)
            {
               QString desc               = 
                  core_option_manager_get_desc(coreopts, j, false);
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
                  char *new_info;
                  size_t option_info_len = strlen(option->info);
                  size_t new_info_len    = option_info_len + 1;

                  if (!(new_info = (char *)malloc(new_info_len)))
                     return;
                  new_info[0] = '\0';

                  word_wrap(new_info, new_info_len, option->info,
                        option_info_len, 50, 100, 0);
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

#if defined(HAVE_MENU)
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
enum
{
   QT_SHADER_PRESET_GLOBAL = 0,
   QT_SHADER_PRESET_CORE,
   QT_SHADER_PRESET_PARENT,
   QT_SHADER_PRESET_GAME,
   QT_SHADER_PRESET_NORMAL
};

ShaderPass::ShaderPass(struct video_shader_pass *passToCopy) :
   pass(NULL)
{
   if (passToCopy)
   {
      pass = (struct video_shader_pass*)calloc(1, sizeof(*pass));
      memcpy(pass, passToCopy, sizeof(*pass));
   }
}

ShaderPass::~ShaderPass()
{
   if (pass)
      free(pass);
}

ShaderPass& ShaderPass::operator=(const ShaderPass &other)
{
   if (this != &other && other.pass)
   {
      pass = (struct video_shader_pass*)calloc(1, sizeof(*pass));
      memcpy(pass, other.pass, sizeof(*pass));
   }

   return *this;
}

ShaderParamsDialog::ShaderParamsDialog(QWidget *parent) :
   QDialog(parent)
   ,m_layout()
   ,m_scrollArea()
{
   setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS));
   setObjectName("shaderParamsDialog");

   resize(720, 480);

   QTimer::singleShot(0, this, SLOT(clearLayout()));
}

ShaderParamsDialog::~ShaderParamsDialog()
{
}

void ShaderParamsDialog::resizeEvent(QResizeEvent *event)
{
   QDialog::resizeEvent(event);

   if (!m_scrollArea)
      return;

   m_scrollArea->resize(event->size());

   emit resized(event->size());
}

void ShaderParamsDialog::closeEvent(QCloseEvent *event)
{
   QDialog::closeEvent(event);

   emit closed();
}

void ShaderParamsDialog::paintEvent(QPaintEvent *event)
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

QString ShaderParamsDialog::getFilterLabel(unsigned filter)
{
   QString filterString;

   switch (filter)
   {
      case 0:
         filterString = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DONT_CARE);
         break;
      case 1:
         filterString = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LINEAR);
         break;
      case 2:
         filterString = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NEAREST);
         break;
      default:
         break;
   }

   return filterString;
}

void ShaderParamsDialog::clearLayout()
{
   QWidget *widget = NULL;

   if (m_scrollArea)
   {
      foreach (QObject *obj, children())
         delete obj;
   }

   m_layout = new QVBoxLayout();

   widget   = new QWidget();
   widget->setLayout(m_layout);
   widget->setObjectName("shaderParamsWidget");

   m_scrollArea = new QScrollArea();

   m_scrollArea->setParent(this);
   m_scrollArea->setWidgetResizable(true);
   m_scrollArea->setWidget(widget);
   m_scrollArea->setObjectName("shaderParamsScrollArea");
   m_scrollArea->show();
}

void ShaderParamsDialog::getShaders(struct video_shader **menu_shader, struct video_shader **video_shader)
{
   video_shader_ctx_t shader_info = {0};
   struct video_shader    *shader = menu_shader_get();

   if (menu_shader)
   {
      if (shader)
         *menu_shader = shader;
      else
         *menu_shader = NULL;
   }

   if (video_shader)
   {
      if (shader)
         *video_shader = shader_info.data;
      else
         *video_shader = NULL;
   }

   if (video_shader)
   {
      if (!video_shader_driver_get_current_shader(&shader_info))
      {
         *video_shader = NULL;
         return;
      }

      if (!shader_info.data || shader_info.data->num_parameters > GFX_MAX_PARAMETERS)
      {
         *video_shader = NULL;
         return;
      }

      if (shader_info.data)
         *video_shader = shader_info.data;
      else
         *video_shader = NULL;
   }
}

void ShaderParamsDialog::onFilterComboBoxIndexChanged(int)
{
   QVariant passVariant;
   QComboBox               *comboBox = qobject_cast<QComboBox*>(sender());
   int pass                          = 0;
   bool ok                           = false;
   struct video_shader *menu_shader  = NULL;
   struct video_shader *video_shader = NULL;

   getShaders(&menu_shader, &video_shader);

   if (!comboBox)
      return;

   passVariant = comboBox->property("pass");

   if (!passVariant.isValid())
      return;

   pass = passVariant.toInt(&ok);

   if (!ok)
      return;

   if (     menu_shader 
         && (pass >= 0)
         && (pass < static_cast<int>(menu_shader->passes)))
   {
      QVariant data = comboBox->currentData();

      if (data.isValid())
      {
         unsigned filter = data.toUInt(&ok);

         if (ok)
         {
            if (menu_shader)
               menu_shader->pass[pass].filter = filter;
            if (video_shader)
               video_shader->pass[pass].filter = filter;

            video_shader->flags |= SHDR_FLAG_MODIFIED;

            command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
         }
      }
   }
}

void ShaderParamsDialog::onScaleComboBoxIndexChanged(int)
{
   QVariant passVariant;
   QComboBox *comboBox               = qobject_cast<QComboBox*>(sender());
   int pass                          = 0;
   bool ok                           = false;
   struct video_shader *menu_shader  = NULL;
   struct video_shader *video_shader = NULL;

   getShaders(&menu_shader, &video_shader);

   if (!comboBox)
      return;

   passVariant = comboBox->property("pass");

   if (!passVariant.isValid())
      return;

   pass = passVariant.toInt(&ok);

   if (!ok)
      return;

   if (menu_shader && pass >= 0 && pass < static_cast<int>(menu_shader->passes))
   {
      QVariant data = comboBox->currentData();

      if (data.isValid())
      {
         unsigned scale = data.toUInt(&ok);

         if (ok)
         {
            if (menu_shader)
            {
               menu_shader->pass[pass].fbo.scale_x   = scale;
               menu_shader->pass[pass].fbo.scale_y   = scale;
               if (scale)
                  menu_shader->pass[pass].fbo.flags |=  FBO_SCALE_FLAG_VALID;
               else
                  menu_shader->pass[pass].fbo.flags &= ~FBO_SCALE_FLAG_VALID;
            }

            if (video_shader)
            {
               video_shader->pass[pass].fbo.scale_x   = scale;
               video_shader->pass[pass].fbo.scale_y   = scale;
               if (scale)
                  video_shader->pass[pass].fbo.flags |=  FBO_SCALE_FLAG_VALID;
               else
                  video_shader->pass[pass].fbo.flags &= ~FBO_SCALE_FLAG_VALID;
            }

            video_shader->flags |= SHDR_FLAG_MODIFIED;

            command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
         }
      }
   }
}

void ShaderParamsDialog::onShaderPassMoveDownClicked()
{
   QVariant passVariant;
   bool ok                           = false;
   struct video_shader *menu_shader  = NULL;
   struct video_shader *video_shader = NULL;
   QToolButton *button               = qobject_cast<QToolButton*>(sender());
   int pass                          = 0;

   getShaders(&menu_shader, &video_shader);

   if (!button)
      return;

   passVariant                       = button->property("pass");

   if (!passVariant.isValid())
      return;

   pass = passVariant.toInt(&ok);

   if (!ok || pass < 0)
      return;

   if (video_shader)
   {
      ShaderPass tempPass;
      int i;

      if (pass >= static_cast<int>(video_shader->passes) - 1)
         return;

      for (i = 0; i < static_cast<int>(video_shader->num_parameters); i++)
      {
         struct video_shader_parameter *param = &video_shader->parameters[i];

         if (param->pass == pass)
            param->pass += 1;
         else if (param->pass == pass + 1)
            param->pass -= 1;
      }

      tempPass = ShaderPass(&video_shader->pass[pass]);
      memcpy(&video_shader->pass[pass], &video_shader->pass[pass + 1], sizeof(struct video_shader_pass));
      memcpy(&video_shader->pass[pass + 1], tempPass.pass, sizeof(struct video_shader_pass));
   }

   if (menu_shader)
   {
      ShaderPass tempPass;
      int i;

      if (pass >= static_cast<int>(menu_shader->passes) - 1)
         return;

      for (i = 0; i < static_cast<int>(menu_shader->num_parameters); i++)
      {
         struct video_shader_parameter *param = &menu_shader->parameters[i];

         if (param->pass == pass)
            param->pass += 1;
         else if (param->pass == pass + 1)
            param->pass -= 1;
      }

      tempPass = ShaderPass(&menu_shader->pass[pass]);
      memcpy(&menu_shader->pass[pass], &menu_shader->pass[pass + 1], sizeof(struct video_shader_pass));
      memcpy(&menu_shader->pass[pass + 1], tempPass.pass, sizeof(struct video_shader_pass));
   }

   menu_shader->flags |= SHDR_FLAG_MODIFIED;

   reload();
}

void ShaderParamsDialog::onShaderPassMoveUpClicked()
{
   QVariant passVariant;
   int pass                          = 0;
   bool ok                           = false;
   struct video_shader *menu_shader  = NULL;
   struct video_shader *video_shader = NULL;
   QToolButton *button               = qobject_cast<QToolButton*>(sender());

   getShaders(&menu_shader, &video_shader);

   if (!button)
      return;

   passVariant = button->property("pass");

   if (!passVariant.isValid())
      return;

   pass = passVariant.toInt(&ok);

   if (!ok || pass <= 0)
      return;

   if (video_shader)
   {
      ShaderPass tempPass;
      int i;

      if (pass > static_cast<int>(video_shader->passes) - 1)
         return;

      for (i = 0; i < static_cast<int>(video_shader->num_parameters); i++)
      {
         struct video_shader_parameter *param = &video_shader->parameters[i];

         if (param->pass == pass)
            param->pass -= 1;
         else if (param->pass == pass - 1)
            param->pass += 1;
      }

      tempPass = ShaderPass(&video_shader->pass[pass - 1]);
      memcpy(&video_shader->pass[pass - 1], &video_shader->pass[pass], sizeof(struct video_shader_pass));
      memcpy(&video_shader->pass[pass], tempPass.pass, sizeof(struct video_shader_pass));
   }

   if (menu_shader)
   {
      ShaderPass tempPass;
      int i;

      if (pass > static_cast<int>(menu_shader->passes) - 1)
         return;

      for (i = 0; i < static_cast<int>(menu_shader->num_parameters); i++)
      {
         struct video_shader_parameter *param = &menu_shader->parameters[i];

         if (param->pass == pass)
            param->pass -= 1;
         else if (param->pass == pass - 1)
            param->pass += 1;
      }

      tempPass = ShaderPass(&menu_shader->pass[pass - 1]);
      memcpy(&menu_shader->pass[pass - 1], &menu_shader->pass[pass], sizeof(struct video_shader_pass));
      memcpy(&menu_shader->pass[pass], tempPass.pass, sizeof(struct video_shader_pass));
   }

   menu_shader->flags |= SHDR_FLAG_MODIFIED;

   reload();
}

void ShaderParamsDialog::onShaderLoadPresetClicked()
{
   QString path, filter;
   QByteArray pathArray;
   struct video_shader *menu_shader  = NULL;
   struct video_shader *video_shader = NULL;
   const char *pathData              = NULL;
   enum rarch_shader_type type       = RARCH_SHADER_NONE;
#if !defined(HAVE_MENU)
   settings_t *settings              = config_get_ptr();
   const char *shader_preset_dir     = settings->paths.directory_video_shader;
#else
   const char *shader_preset_dir     = NULL;

   menu_driver_get_last_shader_preset_path(&shader_preset_dir, NULL);
#endif

   getShaders(&menu_shader, &video_shader);

   if (!menu_shader)
      return;

   filter.append("Shader Preset (");

   /* NOTE: Maybe we should have a way to get a list 
    * of all shader types instead of hard-coding this? */
   if (video_shader_is_supported(RARCH_SHADER_CG))
   {
      filter.append(QLatin1Literal(" *"));
      filter.append(".cgp");
   }

   if (video_shader_is_supported(RARCH_SHADER_GLSL))
   {
      filter.append(QLatin1Literal(" *"));
      filter.append(".glslp");
   }

   if (video_shader_is_supported(RARCH_SHADER_SLANG))
   {
      filter.append(QLatin1Literal(" *"));
      filter.append(".slangp");
   }

   filter.append(")");
   path       = QFileDialog::getOpenFileName(
         this,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET),
         shader_preset_dir, filter);

   if (path.isEmpty())
      return;

   pathArray  = path.toUtf8();
   pathData   = pathArray.constData();
   type       = video_shader_parse_type(pathData);

#if defined(HAVE_MENU)
   /* Cache selected shader parent directory */
   menu_driver_set_last_shader_preset_path(pathData);
#endif

   menu_shader_manager_set_preset(menu_shader, type, pathData, true);
}

void ShaderParamsDialog::onShaderResetPass(int pass)
{
   unsigned i;
   struct video_shader *menu_shader  = NULL;
   struct video_shader *video_shader = NULL;

   getShaders(&menu_shader, &video_shader);

   if (menu_shader)
   {
      for (i = 0; i < menu_shader->num_parameters; i++)
      {
         struct video_shader_parameter *param = &menu_shader->parameters[i];

         /* if pass < 0, reset all params,
          * otherwise only reset the selected pass */
         if (pass >= 0 && param->pass != pass)
            continue;

         param->current = param->initial;
      }
   }

   if (video_shader)
   {
      for (i = 0; i < video_shader->num_parameters; i++)
      {
         struct video_shader_parameter *param = &video_shader->parameters[i];

         /* if pass < 0, reset all params,
          * otherwise only reset the selected pass */
         if (pass >= 0 && param->pass != pass)
            continue;

         param->current = param->initial;
      }

      video_shader->flags |= SHDR_FLAG_MODIFIED;
   }

   reload();
}

void ShaderParamsDialog::onShaderResetParameter(QString parameter)
{
   struct video_shader *menu_shader  = NULL;
   struct video_shader *video_shader = NULL;

   getShaders(&menu_shader, &video_shader);

   if (menu_shader)
   {
      int i;
      struct video_shader_parameter *param = NULL;

      for (i = 0; i < static_cast<int>(menu_shader->num_parameters); i++)
      {
         QString id = menu_shader->parameters[i].id;

         if (id == parameter)
            param = &menu_shader->parameters[i];
      }

      if (param)
         param->current = param->initial;
   }

   if (video_shader)
   {
      int i;
      struct video_shader_parameter *param = NULL;

      for (i = 0; i < static_cast<int>(video_shader->num_parameters); i++)
      {
         QString id = video_shader->parameters[i].id;

         if (id == parameter)
            param = &video_shader->parameters[i];
      }

      if (param)
         param->current = param->initial;

      video_shader->flags |= SHDR_FLAG_MODIFIED;
   }

   reload();
}

void ShaderParamsDialog::onShaderResetAllPasses()
{
   onShaderResetPass(-1);
}

void ShaderParamsDialog::onShaderAddPassClicked()
{
   QString path, filter;
   QByteArray pathArray;
   struct video_shader *menu_shader      = NULL;
   struct video_shader *video_shader     = NULL;
   struct video_shader_pass *shader_pass = NULL;
   const char *pathData                  = NULL;
#if !defined(HAVE_MENU)
   settings_t *settings                  = config_get_ptr();
   const char *shader_pass_dir           = settings->paths.directory_video_shader;
#else
   const char *shader_pass_dir           = NULL;

   menu_driver_get_last_shader_pass_path(&shader_pass_dir, NULL);
#endif

   getShaders(&menu_shader, &video_shader);

   if (!menu_shader)
      return;

   filter.append("Shader (");

   /* NOTE: Maybe we should have a way to get a list 
    * of all shader types instead of hard-coding this? */
   if (video_shader_is_supported(RARCH_SHADER_CG))
      filter.append(QLatin1Literal(" *.cg"));

   if (video_shader_is_supported(RARCH_SHADER_GLSL))
      filter.append(QLatin1Literal(" *.glsl"));

   if (video_shader_is_supported(RARCH_SHADER_SLANG))
      filter.append(QLatin1Literal(" *.slang"));

   filter.append(")");

   path = QFileDialog::getOpenFileName(
         this,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET),
         shader_pass_dir, filter);

   if (path.isEmpty())
      return;

   /* Qt uses '/' as a directory separator regardless
    * of host platform. Have to convert to native separators,
    * or video_shader_resolve_parameters() will fail on
    * non-Linux platforms */
   path      = QDir::toNativeSeparators(path);

   pathArray = path.toUtf8();
   pathData  = pathArray.constData();

   if (menu_shader->passes < GFX_MAX_SHADERS)
      menu_shader->passes++;
   else
      return;

   menu_shader->flags   |= SHDR_FLAG_MODIFIED;
   shader_pass           = &menu_shader->pass[menu_shader->passes - 1];

   if (!shader_pass)
      return;

   strlcpy(shader_pass->source.path,
         pathData,
         sizeof(shader_pass->source.path));

#if defined(HAVE_MENU)
   /* Cache selected shader parent directory */
   menu_driver_set_last_shader_pass_path(pathData);
#endif

   video_shader_resolve_parameters(menu_shader);

   command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
}

void ShaderParamsDialog::onShaderSavePresetAsClicked()
{
   QByteArray pathArray;
   const char *pathData              = NULL;
   settings_t *settings              = config_get_ptr();
   const char *path_dir_video_shader = settings->paths.directory_video_shader;
   QString path                      = QFileDialog::getSaveFileName(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS), path_dir_video_shader);

   if (path.isEmpty())
      return;

   pathArray                         = path.toUtf8();
   pathData                          = pathArray.constData();

   operateShaderPreset(true, pathData, QT_SHADER_PRESET_NORMAL);
}

/** save or remove shader preset */
void ShaderParamsDialog::operateShaderPreset(bool save, const char *path, unsigned action_type)
{
   bool ret;
   enum auto_shader_type preset_type;
   settings_t              *settings = config_get_ptr();
   const char *path_dir_video_shader = settings->paths.directory_video_shader;
   const char *path_dir_menu_config  = settings->paths.directory_menu_config;

   switch (action_type)
   {
      case QT_SHADER_PRESET_GLOBAL:
         preset_type = SHADER_PRESET_GLOBAL;
         break;
      case QT_SHADER_PRESET_CORE:
         preset_type = SHADER_PRESET_CORE;
         break;
      case QT_SHADER_PRESET_PARENT:
         preset_type = SHADER_PRESET_PARENT;
         break;
      case QT_SHADER_PRESET_GAME:
         preset_type = SHADER_PRESET_GAME;
         break;
      case QT_SHADER_PRESET_NORMAL:
         break;
      default:
         return;
   }

   if (save)
   {
      if (action_type == QT_SHADER_PRESET_NORMAL)
         ret = menu_shader_manager_save_preset(
               menu_shader_get(),
               path,
               path_dir_video_shader,
               path_dir_menu_config,
               true);
      else
         ret = menu_shader_manager_save_auto_preset(
               menu_shader_get(),
               preset_type,
               path_dir_video_shader,
               path_dir_menu_config,
               true);

      if (ret)
         runloop_msg_queue_push(
               msg_hash_to_str(MSG_SHADER_PRESET_SAVED_SUCCESSFULLY),
               1, 100, true, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT,
               MESSAGE_QUEUE_CATEGORY_INFO
               );
      else
         runloop_msg_queue_push(
               msg_hash_to_str(MSG_ERROR_SAVING_SHADER_PRESET),
               1, 100, true, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT,
               MESSAGE_QUEUE_CATEGORY_ERROR
               );
   }
   else
   {
      if (action_type != QT_SHADER_PRESET_NORMAL &&
            menu_shader_manager_remove_auto_preset(preset_type,
               path_dir_video_shader,
               path_dir_menu_config))
      {
#ifdef HAVE_MENU
         bool refresh = false;
#endif

         runloop_msg_queue_push(
               msg_hash_to_str(MSG_SHADER_PRESET_REMOVED_SUCCESSFULLY),
               1, 100, true, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT,
               MESSAGE_QUEUE_CATEGORY_INFO
               );

#ifdef HAVE_MENU
         menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
#endif
      }
      else
         runloop_msg_queue_push(
               msg_hash_to_str(MSG_ERROR_REMOVING_SHADER_PRESET),
               1, 100, true, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT,
               MESSAGE_QUEUE_CATEGORY_ERROR
               );
   }
}

void ShaderParamsDialog::onShaderSaveGlobalPresetClicked()
{
   operateShaderPreset(true, NULL, QT_SHADER_PRESET_GLOBAL);
}

void ShaderParamsDialog::onShaderSaveCorePresetClicked()
{
   operateShaderPreset(true, NULL, QT_SHADER_PRESET_CORE);
}

void ShaderParamsDialog::onShaderSaveParentPresetClicked()
{
   operateShaderPreset(true, NULL, QT_SHADER_PRESET_PARENT);
}

void ShaderParamsDialog::onShaderSaveGamePresetClicked()
{
   operateShaderPreset(true, NULL, QT_SHADER_PRESET_GAME);
}

void ShaderParamsDialog::onShaderRemoveGlobalPresetClicked()
{
   operateShaderPreset(false, NULL, QT_SHADER_PRESET_GLOBAL);
}

void ShaderParamsDialog::onShaderRemoveCorePresetClicked()
{
   operateShaderPreset(false, NULL, QT_SHADER_PRESET_CORE);
}

void ShaderParamsDialog::onShaderRemoveParentPresetClicked()
{
   operateShaderPreset(false, NULL, QT_SHADER_PRESET_PARENT);
}

void ShaderParamsDialog::onShaderRemoveGamePresetClicked()
{
   operateShaderPreset(false, NULL, QT_SHADER_PRESET_GAME);
}

void ShaderParamsDialog::onShaderRemoveAllPassesClicked()
{
   struct video_shader *menu_shader  = NULL;
   struct video_shader *video_shader = NULL;

   getShaders(&menu_shader, &video_shader);

   if (!menu_shader)
      return;

   menu_shader->passes   = 0;
   menu_shader->flags   |= SHDR_FLAG_MODIFIED;

   onShaderApplyClicked();
}

void ShaderParamsDialog::onShaderRemovePassClicked()
{
   QVariant passVariant;
   QAction                   *action = qobject_cast<QAction*>(sender());
   int pass                          = 0;
   bool ok                           = false;

   if (!action)
      return;
   passVariant = action->data();

   if (!passVariant.isValid())
      return;

   pass = passVariant.toInt(&ok);

   if (!ok)
      return;

   onShaderRemovePass(pass);
}

void ShaderParamsDialog::onShaderRemovePass(int pass)
{
   int i;
   struct video_shader *menu_shader  = NULL;
   struct video_shader *video_shader = NULL;

   getShaders(&menu_shader, &video_shader);

   if (!menu_shader || menu_shader->passes == 0)
      return;

   if (pass < 0 || pass > static_cast<int>(menu_shader->passes))
      return;

   /* move selected pass to the bottom */
   for (i = pass; i < static_cast<int>(menu_shader->passes) - 1; i++)
      std::swap(menu_shader->pass[i], menu_shader->pass[i + 1]);

   menu_shader->passes--;

   menu_shader->flags   |= SHDR_FLAG_MODIFIED;

   onShaderApplyClicked();
}

void ShaderParamsDialog::onShaderApplyClicked()
{
   command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
}

void ShaderParamsDialog::updateRemovePresetButtonsState()
{
   settings_t      *settings         = config_get_ptr();
   const char *path_dir_video_shader = settings->paths.directory_video_shader;
   const char *path_dir_menu_config  = settings->paths.directory_menu_config;

   if (removeGlobalPresetAction)
      removeGlobalPresetAction->setEnabled(
            menu_shader_manager_auto_preset_exists(
               SHADER_PRESET_GLOBAL,
               path_dir_video_shader,
               path_dir_menu_config 
               ));
   if (removeCorePresetAction)
      removeCorePresetAction->setEnabled(
            menu_shader_manager_auto_preset_exists(
               SHADER_PRESET_CORE,
               path_dir_video_shader,
               path_dir_menu_config 
               ));
   if (removeParentPresetAction)
      removeParentPresetAction->setEnabled(
            menu_shader_manager_auto_preset_exists(
               SHADER_PRESET_PARENT,
               path_dir_video_shader,
               path_dir_menu_config 
               ));
   if (removeGamePresetAction)
      removeGamePresetAction->setEnabled(
            menu_shader_manager_auto_preset_exists(
               SHADER_PRESET_GAME,
               path_dir_video_shader,
               path_dir_menu_config 
               ));
}

void ShaderParamsDialog::reload()
{
   buildLayout();
}

void ShaderParamsDialog::buildLayout()
{
   unsigned i;
   bool hasPasses                           = false;
#if defined(HAVE_MENU)
   CheckableSettingsGroup *topSettingsGroup = NULL;
#endif
   QPushButton *loadButton                  = NULL;
   QPushButton *saveButton                  = NULL;
   QPushButton *removeButton                = NULL;
   QPushButton *removePassButton            = NULL;
   QPushButton *applyButton                 = NULL;
   QHBoxLayout *topButtonLayout             = NULL;
   QMenu *loadMenu                          = NULL;
   QMenu *saveMenu                          = NULL;
   QMenu *removeMenu                        = NULL;
   QMenu *removePassMenu                    = NULL;
   struct video_shader *menu_shader         = NULL;
   struct video_shader *video_shader        = NULL;
   struct video_shader *avail_shader        = NULL;
   const char *shader_path                  = NULL;

   getShaders(&menu_shader, &video_shader);

   /* NOTE: For some reason, menu_shader_get() returns a COPY 
    * of what get_current_shader() gives us.
    * And if you want to be able to change shader settings/parameters 
    * from both the raster menu and
    * Qt at the same time... you must change BOTH or one will 
    * overwrite the other.
    *
    * AND, during a context reset, video_shader will be NULL 
    * but not menu_shader, so don't totally bail
    * just because video_shader is NULL.
    *
    * Someone please fix this mess.
    */

   if (video_shader)
   {
      avail_shader = video_shader;

      if (video_shader->passes == 0)
         setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS));
   }
   /* Normally we'd only use video_shader, 
    * but the Vulkan driver returns a NULL shader when there
    * are zero passes, so just fall back to menu_shader.
    */
   else if (menu_shader)
   {
      avail_shader = menu_shader;

      if (menu_shader->passes == 0)
         setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS));
   }
   else
   {
      setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS));

      /* no shader is available yet, just keep retrying until it is */
      QTimer::singleShot(0, this, SLOT(buildLayout()));
      return;
   }

   clearLayout();

   /* Only check video_shader for the path, menu_shader seems stale... 
    * e.g. if you remove all the shader passes,
    * it still has the old path in it, but video_shader does not
    */
   if (video_shader)
   {
      if (!string_is_empty(video_shader->path))
      {
         shader_path = video_shader->path;
         setWindowTitle(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER)) + ": " + QFileInfo(shader_path).fileName());
      }
   }
   else if (menu_shader)
   {
      if (!string_is_empty(menu_shader->path))
      {
         shader_path = menu_shader->path;
         setWindowTitle(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER)) + ": " + QFileInfo(shader_path).fileName());
      }
   }
   else
      setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS));

   loadButton       = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_LOAD), this);
   saveButton       = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SAVE), this);
   removeButton     = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_REMOVE), this);
   removePassButton = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_REMOVE_PASSES), this);
   applyButton      = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_APPLY), this);

   loadMenu         = new QMenu(loadButton);

   loadMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET), this, SLOT(onShaderLoadPresetClicked()));
   loadMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SHADER_ADD_PASS), this, SLOT(onShaderAddPassClicked()));

   loadButton->setMenu(loadMenu);

   saveMenu         = new QMenu(saveButton);
   saveMenu->addAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS)) + "...", this, SLOT(onShaderSavePresetAsClicked()));
   saveMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GLOBAL), this, SLOT(onShaderSaveGlobalPresetClicked()));
   saveMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE), this, SLOT(onShaderSaveCorePresetClicked()));
   saveMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT), this, SLOT(onShaderSaveParentPresetClicked()));
   saveMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME), this, SLOT(onShaderSaveGamePresetClicked()));

   saveButton->setMenu(saveMenu);

   removeMenu = new QMenu(removeButton);
   removeGlobalPresetAction = removeMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL), this, SLOT(onShaderRemoveGlobalPresetClicked()));
   removeCorePresetAction   = removeMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE),   this, SLOT(onShaderRemoveCorePresetClicked()));
   removeParentPresetAction = removeMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT), this, SLOT(onShaderRemoveParentPresetClicked()));
   removeGamePresetAction   = removeMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME),   this, SLOT(onShaderRemoveGamePresetClicked()));

   removeButton->setMenu(removeMenu);

   connect(removeMenu, SIGNAL(aboutToShow()), this, SLOT(updateRemovePresetButtonsState()));

   removePassMenu = new QMenu(removeButton);

   /* When there are no passes, at least on first startup, it seems video_shader erroneously shows 1 pass, with an empty source file.
    * So we use menu_shader instead for that.
    */
   if (menu_shader)
   {
      for (i = 0; i < menu_shader->passes; i++)
      {
         QFileInfo fileInfo(menu_shader->pass[i].source.path);
         QString shaderBasename = fileInfo.completeBaseName();
         QAction        *action = removePassMenu->addAction(shaderBasename, this, SLOT(onShaderRemovePassClicked()));

         action->setData(i);
      }
   }

   removePassMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SHADER_CLEAR_ALL_PASSES), this, SLOT(onShaderRemoveAllPassesClicked()));

   removePassButton->setMenu(removePassMenu);

   connect(applyButton, SIGNAL(clicked()), this, SLOT(onShaderApplyClicked()));

#if defined(HAVE_MENU)
   topSettingsGroup = new CheckableSettingsGroup(MENU_ENUM_LABEL_VIDEO_SHADERS_ENABLE);
   topSettingsGroup->add(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_REFERENCE);
   topSettingsGroup->add(MENU_ENUM_LABEL_SHADER_WATCH_FOR_CHANGES);
   topSettingsGroup->add(MENU_ENUM_LABEL_VIDEO_SHADER_REMEMBER_LAST_DIR);
#endif

   topButtonLayout = new QHBoxLayout();
   topButtonLayout->addWidget(loadButton);
   topButtonLayout->addWidget(saveButton);
   topButtonLayout->addWidget(removeButton);
   topButtonLayout->addWidget(removePassButton);
   topButtonLayout->addWidget(applyButton);

#if defined(HAVE_MENU)
   m_layout->addWidget(topSettingsGroup);
#endif
   m_layout->addLayout(topButtonLayout);

   /* NOTE: We assume that parameters are always grouped in order by the pass number, e.g., all parameters for pass 0 come first, then params for pass 1, etc. */
   for (i = 0; avail_shader && i < avail_shader->passes; i++)
   {
      QFormLayout                  *form = NULL;
      QGroupBox                *groupBox = NULL;
      QFileInfo fileInfo(avail_shader->pass[i].source.path);
      QString             shaderBasename = fileInfo.completeBaseName();
      QHBoxLayout *filterScaleHBoxLayout = NULL;
      QComboBox          *filterComboBox = new QComboBox(this);
      QComboBox           *scaleComboBox = new QComboBox(this);
      QToolButton        *moveDownButton = NULL;
      QToolButton          *moveUpButton = NULL;
      unsigned                         j = 0;

      /* Sometimes video_shader shows 1 pass with no source file, when there are really 0 passes. */
      if (shaderBasename.isEmpty())
         continue;

      hasPasses                          = true;

      filterComboBox->setProperty("pass", i);
      scaleComboBox->setProperty("pass", i);

      moveDownButton                     = new QToolButton(this);
      moveDownButton->setText("↓");
      moveDownButton->setProperty("pass", i);

      moveUpButton                       = new QToolButton(this);
      moveUpButton->setText("↑");
      moveUpButton->setProperty("pass", i);

      /* Can't move down if we're already at the bottom. */
      if (i < avail_shader->passes - 1)
         connect(moveDownButton, SIGNAL(clicked()),
               this, SLOT(onShaderPassMoveDownClicked()));
      else
         moveDownButton->setDisabled(true);

      /* Can't move up if we're already at the top. */
      if (i > 0)
         connect(moveUpButton, SIGNAL(clicked()),
               this, SLOT(onShaderPassMoveUpClicked()));
      else
         moveUpButton->setDisabled(true);

      for (;;)
      {
         QString filterLabel = getFilterLabel(j);

         if (filterLabel.isEmpty())
            break;

         if (j == 0)
            filterLabel = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DONT_CARE);

         filterComboBox->addItem(filterLabel, j);

         j++;
      }

      for (j = 0; j < 7; j++)
      {
         QString label;

         if (j == 0)
            label = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DONT_CARE);
         else
            label = QString::number(j) + "x";

         scaleComboBox->addItem(label, j);
      }

      filterComboBox->setCurrentIndex(static_cast<int>(avail_shader->pass[i].filter));
      scaleComboBox->setCurrentIndex(static_cast<int>(avail_shader->pass[i].fbo.scale_x));

      /* connect the signals only after the initial index is set */
      connect(filterComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onFilterComboBoxIndexChanged(int)));
      connect(scaleComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onScaleComboBoxIndexChanged(int)));

      form     = new QFormLayout();
      groupBox = new QGroupBox(shaderBasename);
      groupBox->setLayout(form);
      groupBox->setProperty("pass", i);
      groupBox->setContextMenuPolicy(Qt::CustomContextMenu);

      connect(groupBox, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onGroupBoxContextMenuRequested(const QPoint&)));

      m_layout->addWidget(groupBox);

      filterScaleHBoxLayout = new QHBoxLayout();
      filterScaleHBoxLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Preferred));
      filterScaleHBoxLayout->addWidget(new QLabel(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FILTER)) + ":", this));
      filterScaleHBoxLayout->addWidget(filterComboBox);
      filterScaleHBoxLayout->addWidget(new QLabel(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCALE)) + ":", this));
      filterScaleHBoxLayout->addWidget(scaleComboBox);
      filterScaleHBoxLayout->addSpacerItem(new QSpacerItem(20, 0, QSizePolicy::Preferred, QSizePolicy::Preferred));

      if (moveUpButton)
         filterScaleHBoxLayout->addWidget(moveUpButton);

      if (moveDownButton)
         filterScaleHBoxLayout->addWidget(moveDownButton);

      form->addRow("", filterScaleHBoxLayout);

      for (j = 0; j < avail_shader->num_parameters; j++)
      {
         struct video_shader_parameter *param = &avail_shader->parameters[j];

         if (param->pass != static_cast<int>(i))
            continue;

         addShaderParam(param, form);
      }
   }

   if (!hasPasses)
   {
      QLabel *noParamsLabel = new QLabel(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SHADER_NO_PASSES), this);
      noParamsLabel->setAlignment(Qt::AlignCenter);

      m_layout->addWidget(noParamsLabel);
   }

   m_layout->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));

   /* Why is this required?? The layout is corrupt without both resizes. */
   resize(width() + 1, height());
   show();
   resize(width() - 1, height());
}

void ShaderParamsDialog::onParameterLabelContextMenuRequested(const QPoint&)
{
   QVariant paramVariant;
   QString parameter;
   QPointer<QAction> action;
   QList<QAction*> actions;
   QScopedPointer<QAction> resetParamAction;
   QLabel *label = qobject_cast<QLabel*>(sender());

   if (!label)
      return;

   paramVariant  = label->property("parameter");

   if (!paramVariant.isValid())
      return;

   parameter     = paramVariant.toString();

   resetParamAction.reset(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_RESET_PARAMETER), 0));

   actions.append(resetParamAction.data());

   action        = QMenu::exec(actions, QCursor::pos(), NULL, label);

   if (!action)
      return;

   if (action == resetParamAction.data())
      onShaderResetParameter(parameter);
}

void ShaderParamsDialog::onGroupBoxContextMenuRequested(const QPoint&)
{
   QPointer<QAction> action;
   QList<QAction*> actions;
   QScopedPointer<QAction> resetPassAction;
   QScopedPointer<QAction> resetAllPassesAction;
   QVariant passVariant;
   int pass            = 0;
   bool ok             = false;
   QGroupBox *groupBox = qobject_cast<QGroupBox*>(sender());

   if (!groupBox)
      return;

   passVariant         = groupBox->property("pass");

   if (!passVariant.isValid())
      return;

   pass = passVariant.toInt(&ok);

   if (!ok)
      return;

   resetPassAction.reset(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_RESET_PASS), 0));
   resetAllPassesAction.reset(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_RESET_ALL_PASSES), 0));

   actions.append(resetPassAction.data());
   actions.append(resetAllPassesAction.data());

   action              = QMenu::exec(actions, QCursor::pos(), NULL, groupBox);

   if (!action)
      return;

   if (action == resetPassAction.data())
      onShaderResetPass(pass);
   else if (action == resetAllPassesAction.data())
      onShaderResetAllPasses();
}

void ShaderParamsDialog::addShaderParam(struct video_shader_parameter *param, QFormLayout *form)
{
   QString      desc = param->desc;
   QString parameter = param->id;
   QLabel     *label = new QLabel(desc);

   label->setProperty("parameter", parameter);
   label->setContextMenuPolicy(Qt::CustomContextMenu);

   connect(label, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onParameterLabelContextMenuRequested(const QPoint&)));

   if ((param->minimum == 0.0)
         && (param->maximum
            == (param->minimum
               + param->step)))
   {
      /* option is basically a bool, so use a checkbox */
      QCheckBox *checkBox = new QCheckBox(this);
      checkBox->setChecked(param->current == param->maximum ? true : false);
      checkBox->setProperty("param", parameter);

      connect(checkBox, SIGNAL(clicked()), this, SLOT(onShaderParamCheckBoxClicked()));

      form->addRow(label, checkBox);
   }
   else
   {
      QDoubleSpinBox *doubleSpinBox = NULL;
      QSpinBox *spinBox             = NULL;
      QHBoxLayout *box              = new QHBoxLayout();
      QSlider *slider               = new QSlider(Qt::Horizontal, this);
      double value                  = lerp(
            param->minimum, param->maximum, 0, 100, param->current);
      double intpart                = 0;
      bool stepIsFractional         = modf(param->step, &intpart);

      slider->setRange(0, 100);
      slider->setSingleStep(1);
      slider->setValue(value);
      slider->setProperty("param", parameter);

      connect(slider, SIGNAL(valueChanged(int)),
            this, SLOT(onShaderParamSliderValueChanged(int)));

      box->addWidget(slider);

      if (stepIsFractional)
      {
         doubleSpinBox = new QDoubleSpinBox(this);
         doubleSpinBox->setRange(param->minimum, param->maximum);
         doubleSpinBox->setSingleStep(param->step);
         doubleSpinBox->setValue(param->current);
         doubleSpinBox->setProperty("slider", QVariant::fromValue(slider));
         slider->setProperty("doubleSpinBox", QVariant::fromValue(doubleSpinBox));

         connect(doubleSpinBox, SIGNAL(valueChanged(double)), this, SLOT(onShaderParamDoubleSpinBoxValueChanged(double)));

         box->addWidget(doubleSpinBox);
      }
      else
      {
         spinBox = new QSpinBox(this);
         spinBox->setRange(param->minimum, param->maximum);
         spinBox->setSingleStep(param->step);
         spinBox->setValue(param->current);
         spinBox->setProperty("slider", QVariant::fromValue(slider));
         slider->setProperty("spinBox", QVariant::fromValue(spinBox));

         connect(spinBox, SIGNAL(valueChanged(int)), this, SLOT(onShaderParamSpinBoxValueChanged(int)));

         box->addWidget(spinBox);
      }

      form->addRow(label, box);
   }
}

void ShaderParamsDialog::onShaderParamCheckBoxClicked()
{
   QVariant paramVariant;
   QCheckBox *checkBox               = qobject_cast<QCheckBox*>(sender());
   struct video_shader *menu_shader  = NULL;
   struct video_shader *video_shader = NULL;

   getShaders(&menu_shader, &video_shader);

   if (!checkBox)
      return;

   if (menu_shader && menu_shader->passes == 0)
      return;

   paramVariant = checkBox->property("param");

   if (paramVariant.isValid())
   {
      QString parameter = paramVariant.toString();

      if (menu_shader)
      {
         int i;
         struct video_shader_parameter *param = NULL;

         for (i = 0; i < static_cast<int>(menu_shader->num_parameters); i++)
         {
            QString id = menu_shader->parameters[i].id;

            if (id == parameter)
               param = &menu_shader->parameters[i];
         }

         if (param)
            param->current = (checkBox->isChecked() ? param->maximum : param->minimum);
      }

      if (video_shader)
      {
         int i;
         struct video_shader_parameter *param = NULL;

         for (i = 0; i < static_cast<int>(video_shader->num_parameters); i++)
         {
            QString id = video_shader->parameters[i].id;

            if (id == parameter)
               param = &video_shader->parameters[i];
         }

         if (param)
            param->current = (checkBox->isChecked() ? param->maximum : param->minimum);
      }

      video_shader->flags   |= SHDR_FLAG_MODIFIED;
   }
}

void ShaderParamsDialog::onShaderParamSliderValueChanged(int)
{
   QVariant spinBoxVariant;
   QVariant paramVariant;
   QSlider *slider = qobject_cast<QSlider*>(sender());
   struct video_shader *menu_shader  = NULL;
   struct video_shader *video_shader = NULL;
   double                   newValue = 0.0;

   getShaders(&menu_shader, &video_shader);

   if (!slider)
      return;

   spinBoxVariant = slider->property("spinBox");
   paramVariant   = slider->property("param");

   if (paramVariant.isValid())
   {
      QString parameter = paramVariant.toString();

      if (menu_shader)
      {
         int i;
         struct video_shader_parameter *param = NULL;

         for (i = 0; i < static_cast<int>(menu_shader->num_parameters); i++)
         {
            QString id = menu_shader->parameters[i].id;

            if (id == parameter)
               param = &menu_shader->parameters[i];
         }

         if (param)
         {
            newValue = lerp(0, 100, param->minimum, param->maximum, slider->value());
            newValue = round(newValue / param->step) * param->step;
            param->current = newValue;
         }
      }

      if (video_shader)
      {
         int i;
         struct video_shader_parameter *param = NULL;

         for (i = 0; i < static_cast<int>(video_shader->num_parameters); i++)
         {
            QString id = video_shader->parameters[i].id;

            if (id == parameter)
               param = &video_shader->parameters[i];
         }

         if (param)
         {
            newValue = lerp(0, 100, param->minimum, param->maximum, slider->value());
            newValue = round(newValue / param->step) * param->step;
            param->current = newValue;
         }

         video_shader->flags   |= SHDR_FLAG_MODIFIED;
      }

   }

   if (spinBoxVariant.isValid())
   {
      QSpinBox *spinBox = spinBoxVariant.value<QSpinBox*>();

      if (!spinBox)
         return;

      spinBox->blockSignals(true);
      spinBox->setValue(newValue);
      spinBox->blockSignals(false);
   }
   else
   {
      QVariant doubleSpinBoxVariant = slider->property("doubleSpinBox");
      QDoubleSpinBox *doubleSpinBox = doubleSpinBoxVariant.value<QDoubleSpinBox*>();

      if (!doubleSpinBox)
         return;

      doubleSpinBox->blockSignals(true);
      doubleSpinBox->setValue(newValue);
      doubleSpinBox->blockSignals(false);
   }
}

void ShaderParamsDialog::onShaderParamSpinBoxValueChanged(int value)
{
   QVariant sliderVariant;
   QVariant paramVariant;
   QSpinBox                 *spinBox = qobject_cast<QSpinBox*>(sender());
   QSlider                   *slider = NULL;
   struct video_shader  *menu_shader = NULL;
   struct video_shader *video_shader = NULL;

   getShaders(&menu_shader, &video_shader);

   if (!spinBox)
      return;

   sliderVariant = spinBox->property("slider");

   if (!sliderVariant.isValid())
      return;

   slider = sliderVariant.value<QSlider*>();

   if (!slider)
      return;

   paramVariant = slider->property("param");

   if (paramVariant.isValid())
   {
      QString parameter = paramVariant.toString();
      double   newValue = 0.0;

      if (menu_shader)
      {
         int i;
         struct video_shader_parameter *param = NULL;

         for (i = 0; i < static_cast<int>(menu_shader->num_parameters); i++)
         {
            QString id = menu_shader->parameters[i].id;

            if (id == parameter)
               param = &menu_shader->parameters[i];
         }

         if (param)
         {
            param->current = value;
            newValue       = lerp(
                  param->minimum, param->maximum, 0, 100, param->current);
            slider->blockSignals(true);
            slider->setValue(newValue);
            slider->blockSignals(false);
         }
      }

      if (video_shader)
      {
         int i;
         struct video_shader_parameter *param = NULL;

         for (i = 0; i < static_cast<int>(video_shader->num_parameters); i++)
         {
            QString id = video_shader->parameters[i].id;

            if (id == parameter)
               param = &video_shader->parameters[i];
         }

         if (param)
         {
            param->current = value;
            newValue       = lerp(
                  param->minimum, param->maximum, 0, 100, param->current);
            slider->blockSignals(true);
            slider->setValue(newValue);
            slider->blockSignals(false);
         }

         video_shader->flags   |= SHDR_FLAG_MODIFIED;
      }
   }
}

void ShaderParamsDialog::onShaderParamDoubleSpinBoxValueChanged(double value)
{
   QVariant sliderVariant;
   QVariant paramVariant;
   QSlider                   *slider = NULL;
   QDoubleSpinBox     *doubleSpinBox = qobject_cast<QDoubleSpinBox*>(sender());
   struct video_shader  *menu_shader = NULL;
   struct video_shader *video_shader = NULL;

   getShaders(&menu_shader, &video_shader);

   if (!doubleSpinBox)
      return;

   sliderVariant = doubleSpinBox->property("slider");

   if (!sliderVariant.isValid())
      return;

   slider = sliderVariant.value<QSlider*>();

   if (!slider)
      return;

   paramVariant = slider->property("param");

   if (paramVariant.isValid())
   {
      QString parameter = paramVariant.toString();
      double newValue   = 0.0;

      if (menu_shader)
      {
         int i;
         struct video_shader_parameter *param = NULL;

         for (i = 0; i < static_cast<int>(menu_shader->num_parameters); i++)
         {
            QString id = menu_shader->parameters[i].id;

            if (id == parameter)
               param = &menu_shader->parameters[i];
         }

         if (param)
         {
            param->current = value;
            newValue       = lerp(
                  param->minimum, param->maximum, 0, 100, param->current);
            slider->blockSignals(true);
            slider->setValue(newValue);
            slider->blockSignals(false);
         }
      }

      if (video_shader)
      {
         int i;
         struct video_shader_parameter *param = NULL;

         for (i = 0; i < static_cast<int>(video_shader->num_parameters); i++)
         {
            QString id = video_shader->parameters[i].id;

            if (id == parameter)
               param = &video_shader->parameters[i];
         }

         if (param)
         {
            param->current = value;
            newValue       = lerp(
                  param->minimum, param->maximum, 0, 100, param->current);
            slider->blockSignals(true);
            slider->setValue(newValue);
            slider->blockSignals(false);
         }

         video_shader->flags   |= SHDR_FLAG_MODIFIED;
      }
   }
}
#endif
#endif
