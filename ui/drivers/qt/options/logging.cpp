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
   QWidget *widget    = new QWidget;
   FormLayout *layout = new FormLayout;

   layout->add(MENU_ENUM_LABEL_LOG_VERBOSITY);
   layout->add(MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL);
   layout->add(MENU_ENUM_LABEL_PERFCNT_ENABLE);

   widget->setLayout(layout);

   return widget;
}
