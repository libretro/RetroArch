#include "options.h"

AchievementsCategory::AchievementsCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS);
   setCategoryIcon("menu_achievements");
}

QVector<OptionsPage*> AchievementsCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new AchievementsPage(this);

   return pages;
}

AchievementsPage::AchievementsPage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *AchievementsPage::widget()
{
   QWidget               *widget     = new QWidget;
   QVBoxLayout           *layout     = new QVBoxLayout;
   enum msg_hash_enums check_setting = MENU_ENUM_LABEL_CHEEVOS_ENABLE;
   CheckableSettingsGroup *group     = new CheckableSettingsGroup(check_setting);

   {
      unsigned i;
      file_list_t *list = (file_list_t*)calloc(1, sizeof(*list));
      menu_displaylist_build_list(list, DISPLAYLIST_RETRO_ACHIEVEMENTS_SETTINGS_LIST, true);

      for (i = 0; i < list->size; i++)
      {
         menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
            file_list_get_actiondata_at_offset(list, i);

         if (cbs->enum_idx == check_setting)
            continue;

         group->add(cbs->enum_idx);
      }

      file_list_free(list);
   }

   layout->addWidget(group);

   layout->addStretch();

   widget->setLayout(layout);

   return widget;
}
