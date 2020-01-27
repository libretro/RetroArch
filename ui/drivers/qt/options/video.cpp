#include "options.h"

#ifndef CXX_BUILD
extern "C" {
#endif

#include "../../../../gfx/video_display_server.h"
#include "../../../../retroarch.h"

#ifndef CXX_BUILD
}
#endif

VideoCategory::VideoCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS);
   setCategoryIcon("menu_video");
}

QVector<OptionsPage*> VideoCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new VideoPage(this);
   pages << new CrtSwitchresPage(this);

   return pages;
}

VideoPage::VideoPage(QObject *parent) :
   OptionsPage(parent)
   ,m_resolutionCombo(new QComboBox())
{
}

QWidget *VideoPage::widget()
{
   QWidget               *widget       = new QWidget;

   QVBoxLayout        *layout          = new QVBoxLayout;

   SettingsGroup *outputGroup          = new SettingsGroup("Output");
   SettingsGroup *aspectGroup          = new SettingsGroup("Scaling");

   SettingsGroup *fullscreenGroup      = new SettingsGroup("Fullscreen Mode");
   SettingsGroup *windowedGroup        = new SettingsGroup("Windowed Mode");

   QHBoxLayout *fullcreenSizeLayout    = new QHBoxLayout;
   FormLayout *leftFullscreenSizeForm  = new FormLayout;
   FormLayout *rightFullscreenSizeForm = new FormLayout;

   QHBoxLayout *windowedSizeLayout     = new QHBoxLayout;
   FormLayout *leftWindowedSizeForm    = new FormLayout;
   FormLayout *rightWindowedSizeForm   = new FormLayout;

   SettingsGroup *syncGroup            = new SettingsGroup("Synchronization");
   CheckableSettingsGroup *vSyncGroup  = new CheckableSettingsGroup(MENU_ENUM_LABEL_VIDEO_VSYNC);

   QHBoxLayout *outputScalingLayout    = new QHBoxLayout;
   QHBoxLayout *modeLayout             = new QHBoxLayout;
   QHBoxLayout *syncMiscLayout         = new QHBoxLayout;

   SettingsGroup *miscGroup            = new SettingsGroup("Miscellaneous");
   SettingsGroup *filterGroup          = new SettingsGroup("Video Filter");

   unsigned i, size                    = 0;
   struct video_display_config *list   = (struct video_display_config*) video_display_server_get_resolution_list(&size);

   if (list)
   {
      for (i = 0; i < size; i++)
      {
         char val_d[256], str[256];
         snprintf(str, sizeof(str), "%dx%d (%d Hz)", list[i].width, list[i].height, list[i].refreshrate);
         snprintf(val_d, sizeof(val_d), "%d", i);

         m_resolutionCombo->addItem(str);

         if (list[i].current)
            m_resolutionCombo->setCurrentIndex(i);
      }

      free(list);
   }

   outputGroup->add(MENU_ENUM_LABEL_VIDEO_DRIVER);
   outputGroup->add(MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX);
   outputGroup->add(MENU_ENUM_LABEL_VIDEO_ROTATION);
   outputGroup->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION), m_resolutionCombo);
   outputGroup->add(MENU_ENUM_LABEL_VIDEO_FORCE_SRGB_DISABLE);

   fullscreenGroup->add(MENU_ENUM_LABEL_VIDEO_FULLSCREEN);
   fullscreenGroup->add(MENU_ENUM_LABEL_VIDEO_WINDOWED_FULLSCREEN);

   leftFullscreenSizeForm->addRow("Width:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_FULLSCREEN_X));
   rightFullscreenSizeForm->addRow("Height:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_FULLSCREEN_Y));

   fullcreenSizeLayout->addLayout(leftFullscreenSizeForm);
   fullcreenSizeLayout->addLayout(rightFullscreenSizeForm);

   fullscreenGroup->addRow(fullcreenSizeLayout);

   aspectGroup->add(MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER);
   aspectGroup->addRow(new AspectRatioGroup("Aspect Ratio"));

   leftWindowedSizeForm->addRow("Scale:", new FloatSpinBox(MENU_ENUM_LABEL_VIDEO_SCALE));
   leftWindowedSizeForm->addRow("Width:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_WINDOW_WIDTH));

   rightWindowedSizeForm->addRow("Opacity:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_WINDOW_OPACITY));
   rightWindowedSizeForm->addRow("Height:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_WINDOW_HEIGHT));

   windowedSizeLayout->addLayout(leftWindowedSizeForm);
   windowedSizeLayout->addLayout(rightWindowedSizeForm);

   windowedGroup->addRow(windowedSizeLayout);

   windowedGroup->add(MENU_ENUM_LABEL_VIDEO_WINDOW_SHOW_DECORATIONS);
   windowedGroup->add(MENU_ENUM_LABEL_VIDEO_WINDOW_SAVE_POSITION);

   vSyncGroup->add(MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL);
   vSyncGroup->add(MENU_ENUM_LABEL_VIDEO_ADAPTIVE_VSYNC);
   vSyncGroup->add(MENU_ENUM_LABEL_VIDEO_FRAME_DELAY);
   syncGroup->addRow(vSyncGroup);

   rarch_setting_t *hardSyncSetting = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_HARD_SYNC);

   if (hardSyncSetting)
   {
      CheckableSettingsGroup *hardSyncGroup = new CheckableSettingsGroup(hardSyncSetting);

      hardSyncGroup->add(MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES);

      syncGroup->addRow(hardSyncGroup);
   }

   syncGroup->add(MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES);

   miscGroup->add(MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE);
   miscGroup->add(MENU_ENUM_LABEL_VIDEO_THREADED);
   miscGroup->add(MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION);
   miscGroup->add(MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT);
   miscGroup->add(MENU_ENUM_LABEL_VIDEO_SMOOTH);
   miscGroup->add(MENU_ENUM_LABEL_VIDEO_SHADER_DELAY);

   syncMiscLayout->addWidget(syncGroup);
   syncMiscLayout->addWidget(miscGroup);

   filterGroup->add(MENU_ENUM_LABEL_VIDEO_FILTER);

   modeLayout->addWidget(fullscreenGroup);
   modeLayout->addWidget(windowedGroup);

   aspectGroup->add(MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN);

   outputScalingLayout->addWidget(outputGroup);
   outputScalingLayout->addWidget(aspectGroup);

   layout->addLayout(outputScalingLayout);
   layout->addLayout(modeLayout);
   layout->addLayout(syncMiscLayout);
   layout->addWidget(filterGroup);

   layout->addStretch();

   connect(m_resolutionCombo, SIGNAL(currentIndexChanged(const QString&)), this, SLOT(onResolutionComboIndexChanged(const QString&)));

   widget->setLayout(layout);

   return widget;
}

AspectRatioGroup::AspectRatioGroup(const QString &title, QWidget *parent) :
   SettingsGroup(title, parent)
   ,m_radioButton(new AspectRatioRadioButton(ASPECT_RATIO_4_3, ASPECT_RATIO_32_9))
   ,m_comboBox(new UIntComboBox(MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX, ASPECT_RATIO_4_3, ASPECT_RATIO_32_9))
{
   QHBoxLayout *aspectLayout   = new QHBoxLayout;
   QHBoxLayout *preset         = new QHBoxLayout;
   QHBoxLayout *custom         = new QHBoxLayout;
   QVBoxLayout *customRadio    = new QVBoxLayout;
   QHBoxLayout *config         = new QHBoxLayout;
   QHBoxLayout *aspectL        = new QHBoxLayout;
   FormLayout *leftAspectForm  = new FormLayout;
   FormLayout *rightAspectForm = new FormLayout;
   FormLayout *leftAspect      = new FormLayout;
   FormLayout *rightAspect     = new FormLayout;

   leftAspectForm->addRow("X Pos.:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_X));
   leftAspectForm->addRow("Width:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH));
   rightAspectForm->addRow("Y Pos.:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_Y));
   rightAspectForm->addRow("Height:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT));

   aspectLayout->addLayout(leftAspectForm);
   aspectLayout->addLayout(rightAspectForm);

   preset->addWidget(m_radioButton);
   preset->addWidget(m_comboBox);
   preset->setStretch(1, 1);

   customRadio->addWidget(new UIntRadioButton(MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX, ASPECT_RATIO_CUSTOM), Qt::AlignTop);
   customRadio->addStretch();

   custom->addLayout(customRadio);
   custom->addLayout(aspectLayout);
   custom->addStretch();

   config->addWidget(new UIntRadioButton(MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX, ASPECT_RATIO_CONFIG));
   config->addWidget(new FloatSpinBox(MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO));
   config->setStretch(1, 1);
   config->setSizeConstraint(QLayout::SetMinimumSize);

   leftAspect->addRow(new UIntRadioButton(MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX, ASPECT_RATIO_CORE));
   leftAspect->addRow(preset);

   rightAspect->addRow(config);
   rightAspect->addRow(new UIntRadioButton(MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX, ASPECT_RATIO_SQUARE));

   aspectL->addLayout(leftAspect);
   aspectL->addStretch();
   aspectL->addSpacing(30);
   aspectL->addLayout(rightAspect);

   addRow(aspectL);
   addRow(custom);

   connect(m_radioButton, SIGNAL(clicked(bool)), this, SLOT(onAspectRadioClicked(bool)));
}

void AspectRatioGroup::paintEvent(QPaintEvent *event)
{
   unsigned value = config_get_ptr()->uints.video_aspect_ratio_idx;

   if (ASPECT_RATIO_4_3 >= value || value <= ASPECT_RATIO_32_9)
   {
      m_comboBox->blockSignals(false);
      m_radioButton->setChecked(true);
   }
   else
      m_comboBox->blockSignals(true);

   SettingsGroup::paintEvent(event);
}

void AspectRatioGroup::onAspectRadioToggled(bool checked)
{
   if (checked)
      m_comboBox->currentIndexChanged(m_comboBox->currentIndex());
   else
      m_comboBox->blockSignals(true);
}

void AspectRatioGroup::onAspectRadioClicked(bool checked)
{
   m_comboBox->blockSignals(false);
   m_comboBox->currentIndexChanged(m_comboBox->currentIndex());
   setChecked(true);
}

CrtSwitchresPage::CrtSwitchresPage(QObject *parent) :
   OptionsPage(parent)
   ,m_crtSuperResolutionCombo(new QComboBox())
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS);
}

QWidget *CrtSwitchresPage::widget()
{
   QWidget *widget    = new QWidget;
   FormLayout *layout = new FormLayout;

   m_crtSuperResolutionCombo->addItem(msg_hash_to_str(MSG_NATIVE), 0);
   m_crtSuperResolutionCombo->addItem("1920", 1920);
   m_crtSuperResolutionCombo->addItem("2560", 2560);
   m_crtSuperResolutionCombo->addItem("3840", 3840);

   layout->add(MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION);
   layout->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER), m_crtSuperResolutionCombo);
   layout->add(MENU_ENUM_LABEL_CRT_SWITCH_X_AXIS_CENTERING);
   layout->add(MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE);

   connect(m_crtSuperResolutionCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onCrtSuperResolutionComboIndexChanged(int)));

   widget->setLayout(layout);

   return widget;
}

void VideoPage::onResolutionComboIndexChanged(const QString &text)
{
   const char *path     = text.toUtf8().constData();
   action_cb_push_dropdown_item_resolution(path,
         NULL, 0, 0, 0);
}

void CrtSwitchresPage::onCrtSuperResolutionComboIndexChanged(int index)
{
   Q_UNUSED(index)
   config_get_ptr()->uints.crt_switch_resolution_super = m_crtSuperResolutionCombo->currentData().value<unsigned>();
}

AspectRatioRadioButton::AspectRatioRadioButton(unsigned min, unsigned max, QWidget *parent) :
   QRadioButton(parent)
   ,m_min(min)
   ,m_max(max)
{
}
