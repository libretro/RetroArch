#include "options.h"
#include "../viewoptionsdialog.h"

UserInterfaceCategory::UserInterfaceCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS);
   setCategoryIcon("menu_ui");
}

UserInterfaceCategory::UserInterfaceCategory(MainWindow *mainwindow, QWidget *parent) :
   OptionsCategory(parent)
   ,m_mainwindow(mainwindow)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS);
   setCategoryIcon("menu_ui");

   m_pages << new UserInterfacePage(this);
   m_pages << new ViewsPage(this);
   /* pages << new QuickMenuPage(parent); */
   m_pages << new AppearancePage(this);
   m_pages << new DesktopMenuPage(m_mainwindow, this);
}

QVector<OptionsPage*> UserInterfaceCategory::pages()
{
   return m_pages;
}

UserInterfacePage::UserInterfacePage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *UserInterfacePage::widget()
{
   QWidget * widget = new QWidget;

   QVBoxLayout *layout = new QVBoxLayout;

   SettingsGroup *menuGroup = new SettingsGroup("Menu");
   SettingsGroup *inputGroup = new SettingsGroup("Input");
   SettingsGroup *miscGroup = new SettingsGroup("Miscelaneous");
   CheckableSettingsGroup *desktopGroup = new CheckableSettingsGroup(MENU_ENUM_LABEL_DESKTOP_MENU_ENABLE);

   menuGroup->addCheckBox(MENU_ENUM_LABEL_SHOW_ADVANCED_SETTINGS);

   {
      rarch_setting_t *kioskMode = menu_setting_find_enum(MENU_ENUM_LABEL_MENU_ENABLE_KIOSK_MODE);

      /* only on xmb and ozone*/
      if (kioskMode)
      {
         CheckableSettingsGroup *kioskGroup = new CheckableSettingsGroup(kioskMode, widget);

         kioskGroup->addPasswordLineEdit(MENU_ENUM_LABEL_MENU_KIOSK_MODE_PASSWORD);

         menuGroup->addRow(kioskGroup);
      }

   }

   menuGroup->addCheckBox(MENU_ENUM_LABEL_NAVIGATION_WRAPAROUND);
   menuGroup->addCheckBox(MENU_ENUM_LABEL_PAUSE_LIBRETRO);

   inputGroup->addCheckBox(MENU_ENUM_LABEL_MOUSE_ENABLE);
   inputGroup->addCheckBox(MENU_ENUM_LABEL_POINTER_ENABLE);

   menuGroup->addRow(inputGroup);
   menuGroup->addCheckBox(MENU_ENUM_LABEL_THREADED_DATA_RUNLOOP_ENABLE);

   miscGroup->addCheckBox(MENU_ENUM_LABEL_PAUSE_NONACTIVE);
   miscGroup->addCheckBox(MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION);

   menuGroup->addCheckBox(MENU_ENUM_LABEL_UI_COMPANION_ENABLE);
   menuGroup->addCheckBox(MENU_ENUM_LABEL_UI_COMPANION_START_ON_BOOT);
   menuGroup->addCheckBox(MENU_ENUM_LABEL_UI_MENUBAR_ENABLE);

   /* layout->addCheckBox(MENU_ENUM_LABEL_DESKTOP_MENU_ENABLE); */
   desktopGroup->addCheckBox(MENU_ENUM_LABEL_UI_COMPANION_TOGGLE);

   layout->addWidget(menuGroup);
   layout->addWidget(miscGroup);
   layout->addWidget(desktopGroup);
   layout->addStretch();

   widget->setLayout(layout);

   return widget;
}

ViewsPage::ViewsPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS);
}

QWidget *ViewsPage::widget()
{
   QWidget * widget = new QWidget();
   QHBoxLayout *mainLayout = new QHBoxLayout;
   FormLayout *leftLayout = new FormLayout;
   QVBoxLayout *rightLayout = new QVBoxLayout;
   SettingsGroup *quickMenu = new SettingsGroup("Quick Menu");
   QuickMenuPage *quickPage = new QuickMenuPage(this);
   SettingsGroup *mainMenu = new SettingsGroup("Main Menu");
   SettingsGroup *tabs = new SettingsGroup("Tabs");
   SettingsGroup *status = new SettingsGroup("Status");
   SettingsGroup *startScreen = new SettingsGroup("StartScreen");

   mainMenu->addCheckBox(MENU_ENUM_LABEL_MENU_SHOW_LOAD_CORE);
   mainMenu->addCheckBox(MENU_ENUM_LABEL_MENU_SHOW_LOAD_CONTENT);
   /* mainMenu->addCheckBox(MENU_ENUM_LABEL_SHOW_WIMP); */
   mainMenu->addCheckBox(MENU_ENUM_LABEL_MENU_SHOW_ONLINE_UPDATER);
   mainMenu->addCheckBox(MENU_ENUM_LABEL_MENU_SHOW_CORE_UPDATER);
   mainMenu->addCheckBox(MENU_ENUM_LABEL_MENU_SHOW_INFORMATION);
   mainMenu->addCheckBox(MENU_ENUM_LABEL_MENU_SHOW_CONFIGURATIONS);
   mainMenu->addCheckBox(MENU_ENUM_LABEL_MENU_SHOW_HELP);
   mainMenu->addCheckBox(MENU_ENUM_LABEL_MENU_SHOW_QUIT_RETROARCH);
   mainMenu->addCheckBox(MENU_ENUM_LABEL_MENU_SHOW_REBOOT);
   mainMenu->addCheckBox(MENU_ENUM_LABEL_MENU_SHOW_SHUTDOWN);

   tabs->addCheckBox(MENU_ENUM_LABEL_CONTENT_SHOW_SETTINGS);
   tabs->addPasswordLineEdit(MENU_ENUM_LABEL_CONTENT_SHOW_SETTINGS_PASSWORD);
   tabs->addCheckBox(MENU_ENUM_LABEL_CONTENT_SHOW_FAVORITES);
   tabs->addCheckBox(MENU_ENUM_LABEL_CONTENT_SHOW_HISTORY);
   tabs->addCheckBox(MENU_ENUM_LABEL_CONTENT_SHOW_IMAGES);
   tabs->addCheckBox(MENU_ENUM_LABEL_CONTENT_SHOW_MUSIC);
   tabs->addCheckBox(MENU_ENUM_LABEL_CONTENT_SHOW_VIDEO);
   tabs->addCheckBox(MENU_ENUM_LABEL_CONTENT_SHOW_NETPLAY);
   tabs->addCheckBox(MENU_ENUM_LABEL_CONTENT_SHOW_ADD);
   tabs->addCheckBox(MENU_ENUM_LABEL_CONTENT_SHOW_PLAYLISTS);

   status->addCheckBox(MENU_ENUM_LABEL_TIMEDATE_ENABLE);
   status->addUIntComboBox(MENU_ENUM_LABEL_TIMEDATE_STYLE);
   status->addCheckBox(MENU_ENUM_LABEL_BATTERY_LEVEL_ENABLE);
   status->addCheckBox(MENU_ENUM_LABEL_CORE_ENABLE);

   startScreen->addCheckBox(MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN);

   quickMenu->layout()->setContentsMargins(0, 0, 0, 0);
   quickMenu->addRow(quickPage->widget());

   leftLayout->addRow(mainMenu);
   leftLayout->addRow(tabs);
   leftLayout->addRow(startScreen);

   rightLayout->addWidget(quickMenu);
   rightLayout->addWidget(status);
   rightLayout->addStretch();

   mainLayout->addLayout(leftLayout);
   mainLayout->addLayout(rightLayout);

   widget->setLayout(mainLayout);

   return widget;
}

QuickMenuPage::QuickMenuPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS);
}

QWidget *QuickMenuPage::widget()
{
   QWidget * widget = new QWidget;
   FormLayout *layout = new FormLayout;

   layout->addCheckBox(MENU_ENUM_LABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT);
   layout->addCheckBox(MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE);
   layout->addCheckBox(MENU_ENUM_LABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE);
   layout->addCheckBox(MENU_ENUM_LABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES);
   layout->addCheckBox(MENU_ENUM_LABEL_QUICK_MENU_SHOW_START_RECORDING);
   layout->addCheckBox(MENU_ENUM_LABEL_QUICK_MENU_SHOW_START_STREAMING);
   layout->addCheckBox(MENU_ENUM_LABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION);
   layout->addCheckBox(MENU_ENUM_LABEL_QUICK_MENU_SHOW_OPTIONS);
   layout->addCheckBox(MENU_ENUM_LABEL_QUICK_MENU_SHOW_CONTROLS);
   layout->addCheckBox(MENU_ENUM_LABEL_QUICK_MENU_SHOW_CHEATS);
   layout->addCheckBox(MENU_ENUM_LABEL_QUICK_MENU_SHOW_SHADERS);
   layout->addCheckBox(MENU_ENUM_LABEL_CONTENT_SHOW_REWIND);
   layout->addCheckBox(MENU_ENUM_LABEL_CONTENT_SHOW_LATENCY);
   layout->addCheckBox(MENU_ENUM_LABEL_CONTENT_SHOW_OVERLAYS);
   layout->addCheckBox(MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES);
   layout->addCheckBox(MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES);
   layout->addCheckBox(MENU_ENUM_LABEL_QUICK_MENU_SHOW_INFORMATION);

   widget->setLayout(layout);

   return widget;
}

AppearancePage::AppearancePage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_MENU_SETTINGS);
}

QWidget *AppearancePage::widget()
{
   QWidget * widget = new QWidget;
   FormLayout *layout = new FormLayout;

   layout->addFileSelector(MENU_ENUM_LABEL_MENU_WALLPAPER);
   layout->addCheckBox(MENU_ENUM_LABEL_DYNAMIC_WALLPAPER);
   layout->addFloatSliderAndSpinBox(MENU_ENUM_LABEL_MENU_WALLPAPER_OPACITY);
   layout->addFloatSliderAndSpinBox(MENU_ENUM_LABEL_MENU_FRAMEBUFFER_OPACITY);
   layout->addCheckBox(MENU_ENUM_LABEL_MENU_HORIZONTAL_ANIMATION);
   layout->addCheckBox(MENU_ENUM_LABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE);
   layout->addCheckBox(MENU_ENUM_LABEL_MENU_RGUI_BORDER_FILLER_ENABLE);
   layout->addCheckBox(MENU_ENUM_LABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE);
   layout->addCheckBox(MENU_ENUM_LABEL_MENU_RGUI_FULL_WIDTH_LAYOUT);
   layout->addCheckBox(MENU_ENUM_LABEL_MENU_LINEAR_FILTER);
   layout->addUIntComboBox(MENU_ENUM_LABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL);
   layout->addUIntComboBox(MENU_ENUM_LABEL_MENU_RGUI_ASPECT_RATIO_LOCK);
   layout->addUIntComboBox(MENU_ENUM_LABEL_RGUI_MENU_COLOR_THEME);
   layout->addFileSelector(MENU_ENUM_LABEL_RGUI_MENU_THEME_PRESET);
   layout->addCheckBox(MENU_ENUM_LABEL_DPI_OVERRIDE_ENABLE);
   layout->addUIntSpinBox(MENU_ENUM_LABEL_DPI_OVERRIDE_VALUE);
   layout->addUIntSpinBox(MENU_ENUM_LABEL_XMB_ALPHA_FACTOR);
   layout->addUIntSpinBox(MENU_ENUM_LABEL_XMB_SCALE_FACTOR);
   layout->addFontSelector(MENU_ENUM_LABEL_XMB_FONT);
   layout->addUIntColorButton("Menu Font Color: ",
      MENU_ENUM_LABEL_MENU_FONT_COLOR_RED,
      MENU_ENUM_LABEL_MENU_FONT_COLOR_GREEN,
      MENU_ENUM_LABEL_MENU_FONT_COLOR_BLUE);
   layout->addUIntComboBox(MENU_ENUM_LABEL_XMB_LAYOUT);
   layout->addUIntComboBox(MENU_ENUM_LABEL_XMB_THEME);
   layout->addCheckBox(MENU_ENUM_LABEL_XMB_SHADOWS_ENABLE);
   layout->addUIntComboBox(MENU_ENUM_LABEL_XMB_RIBBON_ENABLE);
   layout->addUIntComboBox(MENU_ENUM_LABEL_XMB_MENU_COLOR_THEME);
   layout->addUIntComboBox(MENU_ENUM_LABEL_OZONE_MENU_COLOR_THEME);
   layout->addCheckBox(MENU_ENUM_LABEL_MATERIALUI_ICONS_ENABLE);
   layout->addUIntComboBox(MENU_ENUM_LABEL_MATERIALUI_MENU_COLOR_THEME);
   layout->addFloatSliderAndSpinBox(MENU_ENUM_LABEL_MATERIALUI_MENU_HEADER_OPACITY);
   layout->addFloatSliderAndSpinBox(MENU_ENUM_LABEL_MATERIALUI_MENU_FOOTER_OPACITY);
   layout->addCheckBox(MENU_ENUM_LABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME);

   {
      rarch_setting_t *thumbnails = menu_setting_find_enum(MENU_ENUM_LABEL_THUMBNAILS);

      if (thumbnails)
      {
         QHBoxLayout *thumbsLayout = new QHBoxLayout;

         rarch_setting_t *leftThumbnails = menu_setting_find_enum(MENU_ENUM_LABEL_LEFT_THUMBNAILS);

         thumbsLayout->addWidget(new UIntRadioButtons(thumbnails));

         if (leftThumbnails)
            thumbsLayout->addWidget(new UIntRadioButtons(leftThumbnails));

         layout->addRow(thumbsLayout);
      }
   }

   layout->addCheckBox(MENU_ENUM_LABEL_XMB_VERTICAL_THUMBNAILS);
   layout->addUIntRadioButtons(MENU_ENUM_LABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER);
   layout->addUIntRadioButtons(MENU_ENUM_LABEL_MENU_TICKER_TYPE);
   layout->addFloatSpinBox(MENU_ENUM_LABEL_MENU_TICKER_SPEED);

   widget->setLayout(layout);

   return widget;
}

DesktopMenuPage::DesktopMenuPage(MainWindow *mainwindow, QObject *parent) :
   OptionsPage(parent)
   ,m_widget(new ViewOptionsWidget(mainwindow))
{
   setDisplayName("Desktop Menu");
}

void DesktopMenuPage::apply()
{
   m_widget->saveViewOptions();
}

void DesktopMenuPage::load()
{
   m_widget->loadViewOptions();
}

QWidget *DesktopMenuPage::widget()
{
   return m_widget;
}
