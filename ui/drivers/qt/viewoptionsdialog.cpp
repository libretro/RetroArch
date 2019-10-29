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

#ifdef HAVE_MENU
#include <QBitmap>
#include <QStackedLayout>
#include <QScrollBar>

#include "options/options.h"
#endif

#include "../ui_qt.h"

#ifndef CXX_BUILD
extern "C" {
#endif

#include "../../../msg_hash.h"

#ifndef CXX_BUILD
}
#endif

#ifdef HAVE_MENU

static const int MAX_MIN_WIDTH = 250;
static const int MAX_MIN_HEIGHT = 250;

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
      QWidget *inner = widget();

      if (inner)
      {
         int        fw = frameWidth() * 2;
         QSize minSize = inner->minimumSizeHint();

         minSize += QSize(fw, fw);
         minSize += QSize(scrollBarWidth(), 0);
         minSize.setWidth(qMin(minSize.width(), MAX_MIN_WIDTH));
         minSize.setHeight(qMin(minSize.height(), MAX_MIN_HEIGHT));
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

   width = m_optionsList->sizeHintForColumn(0) + m_optionsList->frameWidth() * 2 + 5;
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

QIcon getIcon(OptionsCategory *category) {
   QPixmap pixmap = QPixmap(QString(config_get_ptr()->paths.directory_assets) + "/xmb/monochrome/png/" + category->categoryIconName() + ".png");
   return QIcon(getColorizedPixmap(pixmap, getLabelColor("iconColor")));
}

void ViewOptionsDialog::addCategory(OptionsCategory *category)
{
   QTabWidget *tabWidget = new QTabWidget();

   m_categoryList.append(category);

   for (OptionsPage* page : category->pages())
   {
      SmartScrollArea *scrollArea = new SmartScrollArea(this);
      QWidget *widget = page->widget();
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
   m_optionsList->addItem(new QListWidgetItem(getIcon(category), category->displayName()));
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
   QFormLayout *form = new QFormLayout;

   m_themeComboBox->addItem(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT), MainWindow::THEME_SYSTEM_DEFAULT);
   m_themeComboBox->addItem(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_DARK), MainWindow::THEME_DARK);
   m_themeComboBox->addItem(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM), MainWindow::THEME_CUSTOM);

   m_thumbnailCacheSpinBox->setSuffix(" MB");
   m_thumbnailCacheSpinBox->setRange(0, 99999);

   /* m_allPlaylistsListMaxCountSpinBox->setRange(0, 99999); */
   /* m_allPlaylistsGridMaxCountSpinBox->setRange(0, 99999); */

   form->setFormAlignment(Qt::AlignCenter);
   form->setLabelAlignment(Qt::AlignCenter);

   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY), m_saveGeometryCheckBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS), m_saveDockPositionsCheckBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB), m_saveLastTabCheckBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES), m_showHiddenFilesCheckBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST), m_suggestLoadedCoreFirstCheckBox);
   /* form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT), m_allPlaylistsListMaxCountSpinBox); */
   /* form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT), m_allPlaylistsGridMaxCountSpinBox); */
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_STARTUP_PLAYLIST), m_startupPlaylistComboBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT), m_thumbnailCacheSpinBox);
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

void ViewOptionsWidget::loadViewOptions()
{
   int i;
   int themeIndex    = 0;
   int playlistIndex = 0;
   QColor highlightColor = m_settings->value("highlight_color", QApplication::palette().highlight().color()).value<QColor>();
   QPixmap highlightPixmap(m_highlightColorPushButton->iconSize());
   QVector<QPair<QString, QString> > playlists = m_mainwindow->getPlaylists();
   QString initialPlaylist = m_settings->value("initial_playlist", m_mainwindow->getSpecialPlaylistPath(SPECIAL_PLAYLIST_HISTORY)).toString();

   m_saveGeometryCheckBox->setChecked(m_settings->value("save_geometry", false).toBool());
   m_saveDockPositionsCheckBox->setChecked(m_settings->value("save_dock_positions", false).toBool());
   m_saveLastTabCheckBox->setChecked(m_settings->value("save_last_tab", false).toBool());
   m_showHiddenFilesCheckBox->setChecked(m_settings->value("show_hidden_files", true).toBool());
   m_suggestLoadedCoreFirstCheckBox->setChecked(m_settings->value("suggest_loaded_core_first", false).toBool());
   /* m_allPlaylistsListMaxCountSpinBox->setValue(m_settings->value("all_playlists_list_max_count", 0).toInt()); */
   /* m_allPlaylistsGridMaxCountSpinBox->setValue(m_settings->value("all_playlists_grid_max_count", 5000).toInt()); */
   m_thumbnailCacheSpinBox->setValue(m_settings->value("thumbnail_cache_limit", 512).toInt());

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

   playlistIndex = m_startupPlaylistComboBox->findData(initialPlaylist, Qt::UserRole, Qt::MatchFixedString);

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
   /* m_settings->setValue("all_playlists_list_max_count", m_allPlaylistsListMaxCountSpinBox->value()); */
   /* m_settings->setValue("all_playlists_grid_max_count", m_allPlaylistsGridMaxCountSpinBox->value()); */
   m_settings->setValue("initial_playlist", m_startupPlaylistComboBox->currentData(Qt::UserRole).toString());
   m_settings->setValue("thumbnail_cache_limit", m_thumbnailCacheSpinBox->value());

   if (!m_mainwindow->customThemeString().isEmpty())
      m_settings->setValue("custom_theme", m_customThemePath);

   /* m_mainwindow->setAllPlaylistsListMaxCount(m_allPlaylistsListMaxCountSpinBox->value()); */
   /* m_mainwindow->setAllPlaylistsGridMaxCount(m_allPlaylistsGridMaxCountSpinBox->value()); */
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
