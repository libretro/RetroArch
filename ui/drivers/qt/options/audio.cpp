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
   QWidget *widget = new QWidget;

   QVBoxLayout *layout = new QVBoxLayout;

   SettingsGroup *outputGroup = new SettingsGroup("Output");
   SettingsGroup *resamplerGroup = new SettingsGroup("Resampler");
   SettingsGroup *syncGroup = new SettingsGroup(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_SYNC));
   SettingsGroup *dspGroup = new SettingsGroup("DSP plugin");
   SettingsGroup *volumeGroup = new SettingsGroup("Volume");

   QHBoxLayout *volumeLayout = new QHBoxLayout();

   outputGroup->addCheckBox(MENU_ENUM_LABEL_AUDIO_ENABLE);
   outputGroup->addStringComboBox(MENU_ENUM_LABEL_AUDIO_DRIVER);
   outputGroup->addStringLineEdit(MENU_ENUM_LABEL_AUDIO_DEVICE);
   outputGroup->addUIntSpinBox(MENU_ENUM_LABEL_AUDIO_LATENCY);

   resamplerGroup->addStringComboBox(MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER);
   resamplerGroup->addUIntComboBox(MENU_ENUM_LABEL_AUDIO_RESAMPLER_QUALITY);
   resamplerGroup->addUIntSpinBox(MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE);

   syncGroup->addCheckBox(MENU_ENUM_LABEL_AUDIO_SYNC);
   syncGroup->addFloatSpinBox(MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW);
   syncGroup->addFloatSpinBox(MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA);

   dspGroup->addFileSelector(MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN);

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
   QWidget *widget = new QWidget();

   FormLayout *layout = new FormLayout;

   layout->addCheckBox(MENU_ENUM_LABEL_AUDIO_ENABLE_MENU);
   layout->addCheckBox(MENU_ENUM_LABEL_MENU_SOUND_OK);
   layout->addCheckBox(MENU_ENUM_LABEL_MENU_SOUND_CANCEL);
   layout->addCheckBox(MENU_ENUM_LABEL_MENU_SOUND_NOTICE);
   layout->addCheckBox(MENU_ENUM_LABEL_MENU_SOUND_BGM);

   widget->setLayout(layout);

   return widget;
}
