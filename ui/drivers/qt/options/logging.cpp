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
   unsigned i;
   QWidget          *widget    = new QWidget;
   FormLayout          *layout = new FormLayout;
   file_list_t           *list = (file_list_t*)calloc(1, sizeof(*list));
   unsigned           count    = menu_displaylist_build_list(
         list, DISPLAYLIST_LOGGING_SETTINGS_LIST);

   for (i = 0; i < list->size; i++)
   {
      menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
         file_list_get_actiondata_at_offset(list, i);

      layout->add(cbs->enum_idx);
   }

   file_list_free(list);

   widget->setLayout(layout);

   return widget;
}
