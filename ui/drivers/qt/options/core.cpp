#include "options.h"

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
