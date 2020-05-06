#include <QApplication>
#include <QLabel>

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
   QWidget *widget                = new QWidget;
   QVBoxLayout *layout            = new QVBoxLayout;
   SettingsGroup *outputGroup     = new SettingsGroup("Output");
   SettingsGroup *resamplerGroup  = new SettingsGroup("Resampler");
   SettingsGroup *syncGroup       = new SettingsGroup(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_AUDIO_SYNC));
   SettingsGroup *dspGroup        = new SettingsGroup("DSP plugin");
   SettingsGroup *volumeGroup     = new SettingsGroup("Volume");
   QGridLayout *volumeLayout      = new QGridLayout();

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

   volumeLayout->addWidget(new QLabel(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME), widget),
         1, 1);
   volumeLayout->addWidget(new CheckableIcon(MENU_ENUM_LABEL_AUDIO_MUTE, qApp->style()->standardIcon(QStyle::SP_MediaVolumeMuted)),
         1, 2);
   volumeLayout->addLayout(new FloatSliderAndSpinBox(MENU_ENUM_LABEL_AUDIO_VOLUME),
         1, 3, 1, 1);

   volumeLayout->addWidget(new QLabel(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME), widget),
         2, 1);
   volumeLayout->addWidget(new CheckableIcon(MENU_ENUM_LABEL_AUDIO_MIXER_MUTE, qApp->style()->standardIcon(QStyle::SP_MediaVolumeMuted)),
         2, 2);
   volumeLayout->addLayout(new FloatSliderAndSpinBox(MENU_ENUM_LABEL_AUDIO_MIXER_VOLUME),
         2, 3, 1, 1);

   volumeGroup->addRow(volumeLayout);
   volumeGroup->add(MENU_ENUM_LABEL_AUDIO_FASTFORWARD_MUTE);

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
