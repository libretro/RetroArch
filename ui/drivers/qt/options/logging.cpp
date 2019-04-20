#include "options.h"

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
