#include "options.h"

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
