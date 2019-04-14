#include "options.h"

LatencyCategory::LatencyCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS);
   setCategoryIcon("menu_latency");
}

QVector<OptionsPage*> LatencyCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new LatencyPage(this);

   return pages;
}

LatencyPage::LatencyPage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *LatencyPage::widget()
{
   QWidget *widget = new QWidget;

   FormLayout *layout = new FormLayout;

   CheckableSettingsGroup *runAheadGpuSync = new CheckableSettingsGroup(MENU_ENUM_LABEL_RUN_AHEAD_ENABLED);

   {
      rarch_setting_t *hardSyncSetting = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_HARD_SYNC);

      if (hardSyncSetting)
      {
         CheckableSettingsGroup *hardSyncGroup = new CheckableSettingsGroup(hardSyncSetting);

         hardSyncGroup->addUIntSpinBox(MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES);

         layout->addRow(hardSyncGroup);
      }
   }

   layout->addUIntSpinBox(MENU_ENUM_LABEL_VIDEO_FRAME_DELAY);
   layout->addUIntSpinBox(MENU_ENUM_LABEL_AUDIO_LATENCY);
   layout->addUIntComboBox(MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR);

   runAheadGpuSync->addUIntComboBox(MENU_ENUM_LABEL_RUN_AHEAD_FRAMES);
   runAheadGpuSync->addCheckBox(MENU_ENUM_LABEL_RUN_AHEAD_SECONDARY_INSTANCE);
   runAheadGpuSync->addCheckBox(MENU_ENUM_LABEL_RUN_AHEAD_HIDE_WARNINGS);
   layout->addRow(runAheadGpuSync);

   widget->setLayout(layout);

   return widget;
}
