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
   QWidget                         *widget = new QWidget;
   FormLayout                      *layout = new FormLayout;
   CheckableSettingsGroup *runAheadGpuSync = new CheckableSettingsGroup(MENU_ENUM_LABEL_RUN_AHEAD_ENABLED);

   rarch_setting_t *hardSyncSetting        = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_HARD_SYNC);

   if (hardSyncSetting)
   {
      CheckableSettingsGroup *hardSyncGroup = new CheckableSettingsGroup(hardSyncSetting);

      hardSyncGroup->add(MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES);

      layout->addRow(hardSyncGroup);
   }

   layout->add(MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES);

   layout->add(MENU_ENUM_LABEL_VIDEO_FRAME_DELAY);
   layout->add(MENU_ENUM_LABEL_AUDIO_LATENCY);
   layout->add(MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR);

   runAheadGpuSync->add(MENU_ENUM_LABEL_RUN_AHEAD_FRAMES);
   runAheadGpuSync->add(MENU_ENUM_LABEL_RUN_AHEAD_SECONDARY_INSTANCE);
   runAheadGpuSync->add(MENU_ENUM_LABEL_RUN_AHEAD_HIDE_WARNINGS);
   layout->addRow(runAheadGpuSync);

   widget->setLayout(layout);

   return widget;
}
