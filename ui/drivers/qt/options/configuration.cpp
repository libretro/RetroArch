#include "options.h"

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
   QWidget *widget = new QWidget;

   FormLayout *layout = new FormLayout;

   layout->addCheckBox(MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT);
   layout->addCheckBox(MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS);
   layout->addCheckBox(MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE);
   layout->addCheckBox(MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE);

   widget->setLayout(layout);

   return widget;
}
