#include "options.h"

DriversCategory::DriversCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS);
   setCategoryIcon("menu_drivers");
}

QVector<OptionsPage*> DriversCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new DriversPage(this);

   return pages;
}

DriversPage::DriversPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS);
}

QWidget *DriversPage::widget()
{
   QWidget *widget    = new QWidget;
   FormLayout *layout = new FormLayout;

   layout->add(MENU_ENUM_LABEL_INPUT_DRIVER);
   layout->add(MENU_ENUM_LABEL_JOYPAD_DRIVER);
   layout->add(MENU_ENUM_LABEL_VIDEO_DRIVER);
   layout->add(MENU_ENUM_LABEL_AUDIO_DRIVER);
   layout->add(MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER);
   layout->add(MENU_ENUM_LABEL_CAMERA_DRIVER);
   layout->add(MENU_ENUM_LABEL_LOCATION_DRIVER);
   layout->add(MENU_ENUM_LABEL_MENU_DRIVER);
   layout->add(MENU_ENUM_LABEL_RECORD_DRIVER);
   layout->add(MENU_ENUM_LABEL_MIDI_DRIVER);

   widget->setLayout(layout);

   return widget;
}
