#include <string/stdstring.h>

#include "options.h"
#include "../viewoptionsdialog.h"
#include "../../verbosity.h"

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
   QWidget                     * widget = new QWidget;
   QVBoxLayout                  *layout = new QVBoxLayout;
   SettingsGroup             *menuGroup = new SettingsGroup("Menu");
   SettingsGroup            *inputGroup = new SettingsGroup("Input");
   SettingsGroup             *miscGroup = new SettingsGroup("Miscellaneous");
   CheckableSettingsGroup *desktopGroup = new CheckableSettingsGroup(MENU_ENUM_LABEL_DESKTOP_MENU_ENABLE);
   rarch_setting_t           *kioskMode = menu_setting_find_enum(MENU_ENUM_LABEL_MENU_ENABLE_KIOSK_MODE);

   menuGroup->add(MENU_ENUM_LABEL_SHOW_ADVANCED_SETTINGS);
   menuGroup->add(MENU_ENUM_LABEL_MENU_WIDGETS_ENABLE);

   /* only on XMB and Ozone*/
   if (kioskMode)
   {
      CheckableSettingsGroup *kioskGroup = new CheckableSettingsGroup(kioskMode, widget);

      kioskGroup->add(MENU_ENUM_LABEL_MENU_KIOSK_MODE_PASSWORD);

      menuGroup->addRow(kioskGroup);
   }

   menuGroup->add(MENU_ENUM_LABEL_NAVIGATION_WRAPAROUND);
   menuGroup->add(MENU_ENUM_LABEL_PAUSE_LIBRETRO);

   inputGroup->add(MENU_ENUM_LABEL_MOUSE_ENABLE);
   inputGroup->add(MENU_ENUM_LABEL_POINTER_ENABLE);

   menuGroup->addRow(inputGroup);
   menuGroup->add(MENU_ENUM_LABEL_THREADED_DATA_RUNLOOP_ENABLE);

   miscGroup->add(MENU_ENUM_LABEL_PAUSE_NONACTIVE);
   miscGroup->add(MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION);

   menuGroup->add(MENU_ENUM_LABEL_UI_COMPANION_ENABLE);
   menuGroup->add(MENU_ENUM_LABEL_UI_COMPANION_START_ON_BOOT);
   menuGroup->add(MENU_ENUM_LABEL_UI_MENUBAR_ENABLE);

   /* layout->add(MENU_ENUM_LABEL_DESKTOP_MENU_ENABLE); */
   desktopGroup->add(MENU_ENUM_LABEL_UI_COMPANION_TOGGLE);

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
   QWidget           * widget = new QWidget();
   QHBoxLayout *mainLayout    = new QHBoxLayout;
   FormLayout *leftLayout     = new FormLayout;
   QVBoxLayout *rightLayout   = new QVBoxLayout;
   SettingsGroup *quickMenu   = new SettingsGroup("Quick Menu");
   QuickMenuPage *quickPage   = new QuickMenuPage(this);
   SettingsGroup *mainMenu    = new SettingsGroup("Main Menu");
   SettingsGroup *settings    = new SettingsGroup("Settings");
   SettingsGroup *tabs        = new SettingsGroup("Tabs");
   SettingsGroup *status      = new SettingsGroup("Status");
   SettingsGroup *startScreen = new SettingsGroup("StartScreen");

   {
      unsigned i;
      unsigned tabs_begin   = 0;
      unsigned status_begin = 0;
      file_list_t *list     = (file_list_t*)calloc(1, sizeof(*list));
      menu_displaylist_build_list(list, DISPLAYLIST_MENU_VIEWS_SETTINGS_LIST, true);

      for (i = 0; i < list->size; i++)
      {
         menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
            file_list_get_actiondata_at_offset(list, i);

         if (cbs->enum_idx == MENU_ENUM_LABEL_CONTENT_SHOW_SETTINGS)
         {
            tabs_begin = i;
            break;
         }

         mainMenu->add(cbs->enum_idx);
      }

      for (i = tabs_begin; i < list->size; i++)
      {
         menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
            file_list_get_actiondata_at_offset(list, i);

         if (cbs->enum_idx == MENU_ENUM_LABEL_TIMEDATE_ENABLE)
         {
            status_begin = i;
            break;
         }

         tabs->add(cbs->enum_idx);
      }

      for (i = status_begin; i < list->size; i++)
      {
         menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
            file_list_get_actiondata_at_offset(list, i);

         if (cbs->enum_idx == MENU_ENUM_LABEL_MENU_SHOW_SUBLABELS)
         {
            break;
         }

         status->add(cbs->enum_idx);
      }

      file_list_free(list);
   }

   {
      unsigned i;
      file_list_t *list = (file_list_t*)calloc(1, sizeof(*list));
      menu_displaylist_build_list(list, DISPLAYLIST_SETTINGS_VIEWS_SETTINGS_LIST, true);

      for (i = 0; i < list->size; i++)
      {
         menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
            file_list_get_actiondata_at_offset(list, i);

         settings->add(cbs->enum_idx);
      }

      file_list_free(list);
   }

   startScreen->add(MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN);

   quickMenu->layout()->setContentsMargins(0, 0, 0, 0);
   quickMenu->addRow(quickPage->widget());

   leftLayout->addRow(mainMenu);
   leftLayout->addRow(settings);
   leftLayout->addRow(tabs);
   leftLayout->addRow(startScreen);
   leftLayout->add(MENU_ENUM_LABEL_MENU_SHOW_SUBLABELS);

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
   return create_widget(DISPLAYLIST_QUICK_MENU_VIEWS_SETTINGS_LIST);
}

AppearancePage::AppearancePage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_MENU_SETTINGS);
}

QWidget *AppearancePage::widget()
{
   unsigned i;
   QWidget            * widget = new QWidget;
   FormLayout          *layout = new FormLayout;
   file_list_t           *list = (file_list_t*)calloc(1, sizeof(*list));

   menu_displaylist_build_list(
         list, DISPLAYLIST_MENU_SETTINGS_LIST, true);

   /* TODO/FIXME - we haven't yet figured out how to 
    * put a radio button setting next to another radio 
    * button on the same row */

   for (i = 0; i < list->size; i++)
   {
      menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
         file_list_get_actiondata_at_offset(list, i);

      switch (cbs->enum_idx)
      {
         /* TODO/FIXME - this is a dirty hack - if we 
          * detect this setting, we instead replace it with a
          * color button and ignore the other two font color 
          * settings since they are already covered by this one
          * color button */
         case MENU_ENUM_LABEL_MENU_FONT_COLOR_RED:
            layout->addUIntColorButton("Menu Font Color: ",
                  MENU_ENUM_LABEL_MENU_FONT_COLOR_RED,
                  MENU_ENUM_LABEL_MENU_FONT_COLOR_GREEN,
                  MENU_ENUM_LABEL_MENU_FONT_COLOR_BLUE);
            break;
         case MENU_ENUM_LABEL_MENU_FONT_COLOR_GREEN:
         case MENU_ENUM_LABEL_MENU_FONT_COLOR_BLUE:
            break;
         default:
            layout->add(cbs->enum_idx);
            break;
      }
   }

   file_list_free(list);

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
