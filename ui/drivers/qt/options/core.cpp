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
   QWidget *widget    = new QWidget;
   FormLayout *layout = new FormLayout;

   layout->add(MENU_ENUM_LABEL_VIDEO_SHARED_CONTEXT);
   layout->add(MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN);
   layout->add(MENU_ENUM_LABEL_CHECK_FOR_MISSING_FIRMWARE);
   layout->add(MENU_ENUM_LABEL_VIDEO_ALLOW_ROTATE);

   widget->setLayout(layout);

   return widget;
}
