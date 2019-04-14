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
   QWidget *widget = new QWidget;

   QVBoxLayout *layout = new QVBoxLayout;

   CheckableSettingsGroup *group = new CheckableSettingsGroup(MENU_ENUM_LABEL_CHEEVOS_ENABLE);

   group->addStringLineEdit(MENU_ENUM_LABEL_CHEEVOS_USERNAME);
   group->addPasswordLineEdit(MENU_ENUM_LABEL_CHEEVOS_PASSWORD);
   group->addCheckBox(MENU_ENUM_LABEL_CHEEVOS_HARDCORE_MODE_ENABLE);
   group->addCheckBox(MENU_ENUM_LABEL_CHEEVOS_LEADERBOARDS_ENABLE);
   group->addCheckBox(MENU_ENUM_LABEL_CHEEVOS_BADGES_ENABLE);
   group->addCheckBox(MENU_ENUM_LABEL_CHEEVOS_TEST_UNOFFICIAL);
   group->addCheckBox(MENU_ENUM_LABEL_CHEEVOS_VERBOSE_ENABLE);
   group->addCheckBox(MENU_ENUM_LABEL_CHEEVOS_AUTO_SCREENSHOT);

   layout->addWidget(group);

   layout->addStretch();

   widget->setLayout(layout);

   return widget;
}
