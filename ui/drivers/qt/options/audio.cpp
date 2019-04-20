#include <QApplication>

#include "options.h"

AudioCategory::AudioCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS);
   setCategoryIcon("menu_audio");
}

QVector<OptionsPage*> AudioCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new AudioPage(this);
   pages << new MenuSoundsPage(this);

   return pages;
}

AudioPage::AudioPage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *AudioPage::widget()
{
   QWidget *widget               = new QWidget;
   QVBoxLayout *layout           = new QVBoxLayout;
   SettingsGroup *outputGroup    = new SettingsGroup("Output");
   SettingsGroup *resamplerGroup = new SettingsGroup("Resampler");
   SettingsGroup *syncGroup      = new SettingsGroup(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_AUDIO_SYNC));
   SettingsGroup *dspGroup       = new SettingsGroup("DSP plugin");
   SettingsGroup *volumeGroup    = new SettingsGroup("Volume");
   QHBoxLayout *volumeLayout     = new QHBoxLayout();

   outputGroup->add(MENU_ENUM_LABEL_AUDIO_ENABLE);
   outputGroup->add(MENU_ENUM_LABEL_AUDIO_DRIVER);
   outputGroup->add(MENU_ENUM_LABEL_AUDIO_DEVICE);
   outputGroup->add(MENU_ENUM_LABEL_AUDIO_LATENCY);

   resamplerGroup->add(MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER);
   resamplerGroup->add(MENU_ENUM_LABEL_AUDIO_RESAMPLER_QUALITY);
   resamplerGroup->add(MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE);

   syncGroup->add(MENU_ENUM_LABEL_AUDIO_SYNC);
   syncGroup->add(MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW);
   syncGroup->add(MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA);

   dspGroup->add(MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN);

   volumeLayout->addWidget(new CheckableIcon(MENU_ENUM_LABEL_AUDIO_MUTE, qApp->style()->standardIcon(QStyle::SP_MediaVolumeMuted)));
   volumeLayout->addLayout(new FloatSliderAndSpinBox(MENU_ENUM_LABEL_AUDIO_VOLUME));
   volumeGroup->addRow(volumeLayout);

   layout->addWidget(outputGroup);
   layout->addWidget(resamplerGroup);
   layout->addWidget(syncGroup);
   layout->addWidget(dspGroup);
   layout->addWidget(volumeGroup);

   layout->addStretch();

   widget->setLayout(layout);

   return widget;
}

MenuSoundsPage::MenuSoundsPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_MENU_SOUNDS);
}

QWidget *MenuSoundsPage::widget()
{
   return create_widget(DISPLAYLIST_MENU_SOUNDS_LIST);
}
