#include "options.h"

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
   unsigned i;
   QWidget             *widget = new QWidget;
   FormLayout          *layout = new FormLayout;
   file_list_t           *list = (file_list_t*)calloc(1, sizeof(*list));
   unsigned           count    = menu_displaylist_build_list(
         list, DISPLAYLIST_FRAME_THROTTLE_SETTINGS_LIST);

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

RewindPage::RewindPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS);
}

QWidget *RewindPage::widget()
{
   unsigned i;
   QWidget             *widget = new QWidget;
   FormLayout          *layout = new FormLayout;
   file_list_t           *list = (file_list_t*)calloc(1, sizeof(*list));
   unsigned           count    = menu_displaylist_build_list(
         list, DISPLAYLIST_REWIND_SETTINGS_LIST);

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
