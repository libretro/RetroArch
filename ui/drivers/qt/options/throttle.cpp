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
   QWidget *widget = new QWidget;

   FormLayout *layout = new FormLayout;

   layout->addFloatSpinBox(MENU_ENUM_LABEL_FASTFORWARD_RATIO);
   layout->addFloatSpinBox(MENU_ENUM_LABEL_SLOWMOTION_RATIO);
   layout->addCheckBox(MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE);
   layout->addCheckBox(MENU_ENUM_LABEL_MENU_THROTTLE_FRAMERATE);

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
   QWidget *widget = new QWidget;

   FormLayout *layout = new FormLayout;

   layout->addCheckBox(MENU_ENUM_LABEL_REWIND_ENABLE);
   layout->addUIntSpinBox(MENU_ENUM_LABEL_REWIND_GRANULARITY);
   layout->addSizeSpinBox(MENU_ENUM_LABEL_REWIND_BUFFER_SIZE);
   layout->addUIntSpinBox(MENU_ENUM_LABEL_REWIND_BUFFER_SIZE_STEP);

   widget->setLayout(layout);

   return widget;
}
