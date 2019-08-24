#include "options.h"

/* DRIVERS */

DriversCategory::DriversCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS);
   setCategoryIcon("menu_drivers");
}

QVector<OptionsPage*> DriversCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new DriversPage(this);

   return pages;
}

DriversPage::DriversPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS);
}

QWidget *DriversPage::widget()
{
   return create_widget(DISPLAYLIST_DRIVER_SETTINGS_LIST);
}

/* DIRECTORY */

DirectoryCategory::DirectoryCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS);
   setCategoryIcon("folder");
}

QVector<OptionsPage*> DirectoryCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new DirectoryPage(this);

   return pages;
}

DirectoryPage::DirectoryPage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *DirectoryPage::widget()
{
   return create_widget(DISPLAYLIST_DIRECTORY_SETTINGS_LIST);
}

/* CONFIGURATION */

ConfigurationCategory::ConfigurationCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS);
   setCategoryIcon("setting");
}

QVector<OptionsPage*> ConfigurationCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new ConfigurationPage(this);

   return pages;
}

ConfigurationPage::ConfigurationPage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *ConfigurationPage::widget()
{
   return create_widget(DISPLAYLIST_CONFIGURATION_SETTINGS_LIST);
}

/* CORE */

CoreCategory::CoreCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_CORE_SETTINGS);
   setCategoryIcon("core-options");
}

QVector<OptionsPage*> CoreCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new CorePage(this);

   return pages;
}

CorePage::CorePage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *CorePage::widget()
{
   return create_widget(DISPLAYLIST_CORE_SETTINGS_LIST);
}

/* LOGGING */

LoggingCategory::LoggingCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS);
   setCategoryIcon("menu_log");
}

QVector<OptionsPage*> LoggingCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new LoggingPage(this);

   return pages;
}

LoggingPage::LoggingPage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *LoggingPage::widget()
{
   return create_widget(DISPLAYLIST_LOGGING_SETTINGS_LIST);
}

/* AI SERVICE */

AIServiceCategory::AIServiceCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS);
   setCategoryIcon("menu_drivers");
}

QVector<OptionsPage*> AIServiceCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new AIServicePage(this);

   return pages;
}

AIServicePage::AIServicePage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS);
}

QWidget *AIServicePage::widget()
{
   return create_widget(DISPLAYLIST_AI_SERVICE_SETTINGS_LIST);
}

/* FRAME THROTTLE */

FrameThrottleCategory::FrameThrottleCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS);
   setCategoryIcon("menu_frameskip");
}

QVector<OptionsPage*> FrameThrottleCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new FrameThrottlePage(this);
   pages << new RewindPage(this);

   return pages;
}

FrameThrottlePage::FrameThrottlePage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS);
}

QWidget *FrameThrottlePage::widget()
{
   return create_widget(DISPLAYLIST_FRAME_THROTTLE_SETTINGS_LIST);
}

RewindPage::RewindPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS);
}

QWidget *RewindPage::widget()
{
   return create_widget(DISPLAYLIST_REWIND_SETTINGS_LIST);
}
