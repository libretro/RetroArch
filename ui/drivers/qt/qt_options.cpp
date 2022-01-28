#include <QApplication>
#include <QLabel>
#include <QStackedWidget>
#include <QButtonGroup>

#include "qt_options.h"
#include "viewoptionsdialog.h"

#ifndef CXX_BUILD
extern "C" {
#endif

#include <string/stdstring.h>

#include "../../../gfx/video_display_server.h"
#include "../../../input/input_driver.h"
#include "../../../input/input_remapping.h"
#include "../../../network/netplay/netplay.h"
#include "../../../retroarch.h"

#include "../../../verbosity.h"

#ifndef CXX_BUILD
}
#endif

AchievementsCategory::AchievementsCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS);
   setCategoryIcon("menu_achievements");
}

QVector<OptionsPage*> AchievementsCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new AchievementsPage(this);

   return pages;
}

AchievementsPage::AchievementsPage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *AchievementsPage::widget()
{
   unsigned i;
   QWidget               *widget     = new QWidget;
   QVBoxLayout           *layout     = new QVBoxLayout;
   enum msg_hash_enums check_setting = MENU_ENUM_LABEL_CHEEVOS_ENABLE;
   CheckableSettingsGroup *group     = new CheckableSettingsGroup(check_setting);
   settings_t *settings              = config_get_ptr();
   file_list_t *list = (file_list_t*)calloc(1, sizeof(*list));
   menu_displaylist_build_list(list, settings,
         DISPLAYLIST_RETRO_ACHIEVEMENTS_SETTINGS_LIST, true);

   for (i = 0; i < list->size; i++)
   {
      menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
         file_list_get_actiondata_at_offset(list, i);

      if (cbs->enum_idx == check_setting)
         continue;

      group->add(cbs->enum_idx);
   }

   file_list_free(list);

   layout->addWidget(group);

   layout->addStretch();

   widget->setLayout(layout);

   return widget;
}

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

InputCategory::InputCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS);
   setCategoryIcon("core-input-remapping-options");
}

QVector<OptionsPage*> InputCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new InputPage(this);
   pages << new HotkeyBindsPage(this);

   return pages;
}

InputPage::InputPage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *InputPage::widget()
{
   unsigned i;
   QWidget *widget       = new QWidget;
   FormLayout *layout    = new FormLayout;
   settings_t *settings  = config_get_ptr();
   file_list_t *list     = (file_list_t*)calloc(1, sizeof(*list));

   menu_displaylist_build_list(list, settings,
         DISPLAYLIST_INPUT_SETTINGS_LIST, true);

   for (i = 0; i < list->size; i++)
   {
      menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
         file_list_get_actiondata_at_offset(list, i);

      if (cbs->enum_idx == MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS)
         break;

      layout->add(cbs->enum_idx);
   }

   file_list_free(list);

   widget->setLayout(layout);

   return widget;
}

HotkeyBindsPage::HotkeyBindsPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS);
}

QWidget *HotkeyBindsPage::widget()
{
   unsigned i;
   QWidget *widget         = new QWidget;
   QHBoxLayout *layout     = new QHBoxLayout;
   FormLayout *mainLayout  = new FormLayout;
   settings_t *settings    = config_get_ptr();
   file_list_t *list       = (file_list_t*)calloc(1, sizeof(*list));

   menu_displaylist_build_list(list, settings,
         DISPLAYLIST_INPUT_HOTKEY_BINDS_LIST, true);

   for (i = 0; i < list->size; i++)
   {
      menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
         file_list_get_actiondata_at_offset(list, i);

      mainLayout->add(cbs->enum_idx);
   }

   file_list_free(list);

   layout->addLayout(mainLayout);

   widget->setLayout(layout);

   return widget;
}

UserBindsPage::UserBindsPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName("User Binds");
}

QWidget *UserBindsPage::widget()
{
   unsigned p, retro_id;
   settings_t *settings      = config_get_ptr();
   unsigned max_users    = settings->uints.input_max_users;
   QWidget *widget       = new QWidget;
   QGridLayout *layout   = new QGridLayout;
   QComboBox *userCombo  = new QComboBox;
   QStackedWidget *stack = new QStackedWidget;

   for (p = 0; p < max_users; p++)
   {
      userCombo->addItem(QString::number(p));

      QWidget *uWidget = new QWidget();
      FormLayout *form = new FormLayout();

      for (retro_id = 0; retro_id < RARCH_FIRST_CUSTOM_BIND + 20; retro_id++)
      {
         char descriptor[300];
         const struct retro_keybind *keybind   = 
            &input_config_binds[p][retro_id];
         const struct retro_keybind *auto_bind = 
            (const struct retro_keybind*)
            input_config_get_bind_auto(p, retro_id);

         input_config_get_bind_string(settings, descriptor,
            keybind, auto_bind, sizeof(descriptor));

         const struct retro_keybind *keyptr =
            &input_config_binds[p][retro_id];

         QString label = msg_hash_to_str(keyptr->enum_idx);

         form->addRow(QString(msg_hash_to_str(keyptr->enum_idx)),
               new QPushButton(QString(descriptor)));
      }

      uWidget->setLayout(form);

      stack->addWidget(uWidget);
   }

   connect(userCombo, SIGNAL(activated(int)),
         stack, SLOT(setCurrentIndex(int)));

   layout->addWidget(userCombo, 0, 0);
   layout->addWidget(stack, 1, 0);

   widget->setLayout(layout);

   return widget;
}

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

NetworkCategory::NetworkCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS);
   setCategoryIcon("menu_network");
}

QVector<OptionsPage*> NetworkCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new NetplayPage(this);
   pages << new UpdaterPage(this);

   return pages;
}

NetplayPage::NetplayPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_NETPLAY);
}

QWidget *NetplayPage::widget()
{
   QWidget *widget            = new QWidget;
   QGridLayout *layout        = new QGridLayout;
   FormLayout *checksLayout   = new FormLayout;
   QGroupBox *serverGroup     = new QGroupBox("Server");
   SettingsGroup *syncGroup   = new SettingsGroup("Synchronization");
   SettingsGroup *slaveGroup  = new SettingsGroup("Slave-Mode");
   SettingsGroup *inputGroup  = new SettingsGroup("Input Sharing");
   SettingsGroup *deviceGroup = new SettingsGroup("Device Request");
   FormLayout *serverForm     = new FormLayout;
   QHBoxLayout *serverLayout  = new QHBoxLayout;
   QVBoxLayout *mainLayout    = new QVBoxLayout;
   QGridLayout *requestGrid   = new QGridLayout;
   unsigned i                 = 0;
   unsigned row               = 0;
   unsigned column            = 0;

   checksLayout->add(MENU_ENUM_LABEL_NETPLAY_PUBLIC_ANNOUNCE);
   checksLayout->add(MENU_ENUM_LABEL_NETPLAY_START_AS_SPECTATOR);
   checksLayout->add(MENU_ENUM_LABEL_NETPLAY_FADE_CHAT);
   checksLayout->add(MENU_ENUM_LABEL_NETPLAY_ALLOW_PAUSING);
   checksLayout->add(MENU_ENUM_LABEL_NETWORK_ON_DEMAND_THUMBNAILS);

   serverForm->add(MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS);
   serverForm->add(MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT);
   serverForm->add(MENU_ENUM_LABEL_NETPLAY_MAX_CONNECTIONS);
   serverForm->add(MENU_ENUM_LABEL_NETPLAY_MAX_PING);
   serverForm->add(MENU_ENUM_LABEL_NETPLAY_PASSWORD);
   serverForm->add(MENU_ENUM_LABEL_NETPLAY_SPECTATE_PASSWORD);
   serverForm->add(MENU_ENUM_LABEL_NETPLAY_NAT_TRAVERSAL);

   serverLayout->addWidget(createMitmServerGroup());
   serverLayout->addSpacing(30);
   serverLayout->addLayout(serverForm);

   serverGroup->setLayout(serverLayout);

   slaveGroup->add(MENU_ENUM_LABEL_NETPLAY_ALLOW_SLAVES);
   slaveGroup->add(MENU_ENUM_LABEL_NETPLAY_REQUIRE_SLAVES);

   syncGroup->add(MENU_ENUM_LABEL_NETPLAY_STATELESS_MODE);
   syncGroup->add(MENU_ENUM_LABEL_NETPLAY_CHECK_FRAMES);
   syncGroup->add(MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN);
   syncGroup->add(MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE);

   inputGroup->add(MENU_ENUM_LABEL_NETPLAY_SHARE_DIGITAL);
   inputGroup->add(MENU_ENUM_LABEL_NETPLAY_SHARE_ANALOG);

   for (i = 0; i < MAX_USERS; i++)
   {
      if (column % 4 == 0)
      {
         column = 0;
         row++;
      }
      requestGrid->addWidget(new CheckBox((enum msg_hash_enums)(MENU_ENUM_LABEL_NETPLAY_REQUEST_DEVICE_1 + i)), row, column);
      column++;
   }

   deviceGroup->addRow(requestGrid);

   layout->addLayout(checksLayout, 0, 0, 1, 2);
   layout->addWidget(serverGroup, 1, 0, 1, 2);
   layout->addWidget(slaveGroup, 2, 0, 1, 1);
   layout->addWidget(syncGroup, 2, 1, 2, 1);
   layout->addWidget(inputGroup, 3, 0, 1, 1);
   layout->addWidget(deviceGroup, 4, 0, 1, 2);

   mainLayout->addLayout(layout);

   mainLayout->addStretch();

   widget->setLayout(mainLayout);

   return widget;
}

QGroupBox *NetplayPage::createMitmServerGroup()
{
   unsigned i;
   CheckableSettingsGroup *groupBox = new CheckableSettingsGroup(
         MENU_ENUM_LABEL_NETPLAY_USE_MITM_SERVER);
   QButtonGroup *buttonGroup        = new QButtonGroup(this);
   unsigned list_len                = ARRAY_SIZE(netplay_mitm_server_list);
   rarch_setting_t *setting         = menu_setting_find_enum(
         MENU_ENUM_LABEL_NETPLAY_MITM_SERVER);

   if (!setting)
      return nullptr;

   for (i = 0; i < list_len; i++)
   {
      QRadioButton *radioButton = new QRadioButton(
            netplay_mitm_server_list[i].description);

      /* find the currently selected server in the list */
      if (string_is_equal(setting->value.target.string,
               netplay_mitm_server_list[i].name))
         radioButton->setChecked(true);

      buttonGroup->addButton(radioButton, i);

      groupBox->addRow(radioButton);
   }

   groupBox->add(MENU_ENUM_LABEL_NETPLAY_CUSTOM_MITM_SERVER);

   connect(buttonGroup, SIGNAL(buttonClicked(int)),
         this, SLOT(onRadioButtonClicked(int)));

   return groupBox;
}

void NetplayPage::onRadioButtonClicked(int id)
{
   rarch_setting_t *setting = 
      menu_setting_find_enum(MENU_ENUM_LABEL_NETPLAY_MITM_SERVER);

   if (!setting)
      return;

   strlcpy(setting->value.target.string,
         netplay_mitm_server_list[id].name, setting->size);
}

UpdaterPage::UpdaterPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS);
}

QWidget *UpdaterPage::widget()
{
   return create_widget(DISPLAYLIST_UPDATER_SETTINGS_LIST);
}

OnscreenDisplayCategory::OnscreenDisplayCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS);
   setCategoryIcon("menu_osd");
}

QVector<OptionsPage*> OnscreenDisplayCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new OverlayPage(this);
   pages << new NotificationsPage(this);

   return pages;
}

NotificationsPage::NotificationsPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS);
}

QWidget *NotificationsPage::widget()
{
   QWidget                            *widget = new QWidget;
   QVBoxLayout                        *layout = new QVBoxLayout;
   CheckableSettingsGroup *notificationsGroup = new CheckableSettingsGroup(MENU_ENUM_LABEL_VIDEO_FONT_ENABLE);
   CheckableSettingsGroup            *bgGroup = new CheckableSettingsGroup(MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE);

   notificationsGroup->add(MENU_ENUM_LABEL_FPS_SHOW);
   notificationsGroup->add(MENU_ENUM_LABEL_FPS_UPDATE_INTERVAL);
   notificationsGroup->add(MENU_ENUM_LABEL_FRAMECOUNT_SHOW);
   notificationsGroup->add(MENU_ENUM_LABEL_MEMORY_SHOW);
   notificationsGroup->add(MENU_ENUM_LABEL_MEMORY_UPDATE_INTERVAL);
   notificationsGroup->add(MENU_ENUM_LABEL_STATISTICS_SHOW);
   notificationsGroup->add(MENU_ENUM_LABEL_NETPLAY_PING_SHOW);
   notificationsGroup->add(MENU_ENUM_LABEL_VIDEO_FONT_PATH);
   notificationsGroup->add(MENU_ENUM_LABEL_VIDEO_FONT_SIZE);
   notificationsGroup->add(MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_X);
   notificationsGroup->add(MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_Y);
   notificationsGroup->addRow("Notification Color: ", new FloatColorButton(
      MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_RED,
      MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_GREEN,
      MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_BLUE));

   bgGroup->addRow("Notification Background Color: ", new UIntColorButton(
      MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_RED,
      MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_GREEN,
      MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_BLUE));
   bgGroup->add(MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY);

   notificationsGroup->addRow(bgGroup);

   notificationsGroup->add(MENU_ENUM_LABEL_MENU_WIDGETS_ENABLE);
   notificationsGroup->add(MENU_ENUM_LABEL_MENU_WIDGET_SCALE_AUTO);
   notificationsGroup->add(MENU_ENUM_LABEL_MENU_WIDGET_SCALE_FACTOR);
#if !(defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))
   notificationsGroup->add(MENU_ENUM_LABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED);
#endif
   notificationsGroup->add(MENU_ENUM_LABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_AUTOCONFIG);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_REMAP_LOAD);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_FAST_FORWARD);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_CHEATS_APPLIED);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_SCREENSHOT);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_REFRESH_RATE);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_NETPLAY_EXTRA);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE);

   layout->addWidget(notificationsGroup);

   layout->addStretch();

   widget->setLayout(layout);

   return widget;
}

OverlayPage::OverlayPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS);
}

QWidget *OverlayPage::widget()
{
   QWidget                      *widget = new QWidget;
#if defined(HAVE_OVERLAY)
   QVBoxLayout                  *layout = new QVBoxLayout;

   CheckableSettingsGroup *overlayGroup = new CheckableSettingsGroup(MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE);

   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_AUTOLOAD_PREFERRED);
   overlayGroup->add(MENU_ENUM_LABEL_INPUT_OVERLAY_AUTO_ROTATE);
   overlayGroup->add(MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU);
   overlayGroup->add(MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED);
   overlayGroup->add(MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR);

   overlayGroup->add(MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_INPUTS);
   overlayGroup->add(MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_INPUTS_PORT);

   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_PRESET);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_OPACITY);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_SCALE_LANDSCAPE);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_ASPECT_ADJUST_LANDSCAPE);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_X_SEPARATION_LANDSCAPE);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_Y_SEPARATION_LANDSCAPE);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_X_OFFSET_LANDSCAPE);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_Y_OFFSET_LANDSCAPE);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_SCALE_PORTRAIT);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_ASPECT_ADJUST_PORTRAIT);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_X_SEPARATION_PORTRAIT);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_X_OFFSET_PORTRAIT);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_Y_OFFSET_PORTRAIT);

   layout->addWidget(overlayGroup);

   layout->addStretch();

   widget->setLayout(layout);
#endif

   return widget;
}

PlaylistsCategory::PlaylistsCategory(QWidget *parent) : OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS);
   setCategoryIcon("menu_playlist");
}

QVector<OptionsPage*> PlaylistsCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new PlaylistsPage(this);

   return pages;
}

PlaylistsPage::PlaylistsPage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *PlaylistsPage::widget()
{
   QWidget *widget                 = new QWidget;
   FormLayout *layout              = new FormLayout;
   CheckableSettingsGroup *history = new CheckableSettingsGroup(MENU_ENUM_LABEL_HISTORY_LIST_ENABLE);

   history->add(MENU_ENUM_LABEL_CONTENT_HISTORY_SIZE);

   layout->addRow(history);

   layout->add(MENU_ENUM_LABEL_PLAYLIST_ENTRY_RENAME);
   /*layout->add(MENU_ENUM_LABEL_PLAYLIST_ENTRY_REMOVE); TOFIX */
   layout->add(MENU_ENUM_LABEL_PLAYLIST_SORT_ALPHABETICAL);
   layout->add(MENU_ENUM_LABEL_PLAYLIST_USE_OLD_FORMAT);
   layout->add(MENU_ENUM_LABEL_PLAYLIST_COMPRESSION);
   layout->add(MENU_ENUM_LABEL_PLAYLIST_SHOW_SUBLABELS);
   layout->add(MENU_ENUM_LABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH);
   layout->add(MENU_ENUM_LABEL_SCAN_WITHOUT_CORE_MATCH);

   widget->setLayout(layout);

   return widget;
}

RecordingCategory::RecordingCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS);
   setCategoryIcon("menu_record");
}

QVector<OptionsPage*> RecordingCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new RecordingPage(this);

   return pages;
}

RecordingPage::RecordingPage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *RecordingPage::widget()
{
   QWidget              * widget = new QWidget;
   QVBoxLayout           *layout = new QVBoxLayout;
   SettingsGroup *recordingGroup = new SettingsGroup("Recording");
   SettingsGroup *streamingGroup = new SettingsGroup("Streaming");
   QHBoxLayout               *hl = new QHBoxLayout;

   recordingGroup->add(MENU_ENUM_LABEL_VIDEO_RECORD_QUALITY);
   recordingGroup->add(MENU_ENUM_LABEL_RECORD_CONFIG);
   recordingGroup->add(MENU_ENUM_LABEL_VIDEO_RECORD_THREADS);
   recordingGroup->add(MENU_ENUM_LABEL_RECORDING_OUTPUT_DIRECTORY);
   recordingGroup->add(MENU_ENUM_LABEL_VIDEO_POST_FILTER_RECORD);
   recordingGroup->add(MENU_ENUM_LABEL_VIDEO_GPU_RECORD);

   hl->addWidget(new UIntRadioButtons(MENU_ENUM_LABEL_STREAMING_MODE));
   hl->addWidget(new UIntRadioButtons(MENU_ENUM_LABEL_VIDEO_STREAM_QUALITY));

   streamingGroup->addRow(hl);

   streamingGroup->add(MENU_ENUM_LABEL_STREAM_CONFIG);
   streamingGroup->add(MENU_ENUM_LABEL_STREAMING_TITLE);
   streamingGroup->add(MENU_ENUM_LABEL_STREAMING_URL);
   streamingGroup->add(MENU_ENUM_LABEL_UDP_STREAM_PORT);

   layout->addWidget(recordingGroup);
   layout->addWidget(streamingGroup);

   layout->addStretch();

   widget->setLayout(layout);

   return widget;
}

SavingCategory::SavingCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS);
   setCategoryIcon("menu_saving");
}

QVector<OptionsPage*> SavingCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new SavingPage(this);

   return pages;
}

SavingPage::SavingPage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *SavingPage::widget()
{
   QWidget                             *widget = new QWidget;
   FormLayout                          *layout = new FormLayout;
   SettingsGroup                   *savesGroup = new SettingsGroup("Saves");
   SettingsGroup              *savestatesGroup = new SettingsGroup("Savestates");
   CheckableSettingsGroup *autoSavestatesGroup = new CheckableSettingsGroup(MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE);
   SettingsGroup                 *saveRamGroup = new SettingsGroup("SaveRAM");
   SettingsGroup          *systemFilesDirGroup = new SettingsGroup("System Files");
   SettingsGroup          *screenshotsDirGroup = new SettingsGroup("Screenshots");
   SettingsGroup          *runtimeLogGroup     = new SettingsGroup("Runtime Log");

   savesGroup->add(MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE);
   savesGroup->add(MENU_ENUM_LABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE);
   savesGroup->add(MENU_ENUM_LABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE);

   savestatesGroup->add(MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX);

   autoSavestatesGroup->add(MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD);

   savestatesGroup->addRow(autoSavestatesGroup);
   savestatesGroup->add(MENU_ENUM_LABEL_SAVESTATE_THUMBNAIL_ENABLE);
   savestatesGroup->add(MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE);
   savestatesGroup->add(MENU_ENUM_LABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE);
   savestatesGroup->add(MENU_ENUM_LABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE);
   savestatesGroup->add(MENU_ENUM_LABEL_SAVESTATE_FILE_COMPRESSION);

   saveRamGroup->add(MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE);
   saveRamGroup->add(MENU_ENUM_LABEL_AUTOSAVE_INTERVAL);
   saveRamGroup->add(MENU_ENUM_LABEL_SAVE_FILE_COMPRESSION);

   systemFilesDirGroup->add(MENU_ENUM_LABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE);

   screenshotsDirGroup->add(MENU_ENUM_LABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE);
   screenshotsDirGroup->add(MENU_ENUM_LABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE);

   runtimeLogGroup->add(MENU_ENUM_LABEL_CONTENT_RUNTIME_LOG);
   runtimeLogGroup->add(MENU_ENUM_LABEL_CONTENT_RUNTIME_LOG_AGGREGATE);

   layout->addRow(savesGroup);
   layout->addRow(savestatesGroup);
   layout->addRow(saveRamGroup);
   layout->addRow(systemFilesDirGroup);
   layout->addRow(screenshotsDirGroup);
   layout->addRow(runtimeLogGroup);

   widget->setLayout(layout);

   return widget;
}

UserInterfaceCategory::UserInterfaceCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS);
   setCategoryIcon("menu_ui");
}

UserInterfaceCategory::UserInterfaceCategory(MainWindow *mainwindow, QWidget *parent) :
   OptionsCategory(parent)
   ,m_mainwindow(mainwindow)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS);
   setCategoryIcon("menu_ui");

   m_pages << new UserInterfacePage(this);
   m_pages << new ViewsPage(this);
   m_pages << new AppearancePage(this);
   m_pages << new DesktopMenuPage(m_mainwindow, this);
}

QVector<OptionsPage*> UserInterfaceCategory::pages()
{
   return m_pages;
}

UserInterfacePage::UserInterfacePage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *UserInterfacePage::widget()
{
   QWidget                     * widget = new QWidget;
   QVBoxLayout                  *layout = new QVBoxLayout;
   SettingsGroup             *menuGroup = new SettingsGroup("Menu");
   SettingsGroup            *inputGroup = new SettingsGroup("Input");
   SettingsGroup             *miscGroup = new SettingsGroup("Miscellaneous");
   CheckableSettingsGroup *desktopGroup = new CheckableSettingsGroup(MENU_ENUM_LABEL_DESKTOP_MENU_ENABLE);
   rarch_setting_t           *kioskMode = menu_setting_find_enum(MENU_ENUM_LABEL_MENU_ENABLE_KIOSK_MODE);

   menuGroup->add(MENU_ENUM_LABEL_SHOW_ADVANCED_SETTINGS);

   /* only on XMB and Ozone*/
   if (kioskMode)
   {
      CheckableSettingsGroup *kioskGroup = new CheckableSettingsGroup(kioskMode, widget);

      kioskGroup->add(MENU_ENUM_LABEL_MENU_KIOSK_MODE_PASSWORD);

      menuGroup->addRow(kioskGroup);
   }

   menuGroup->add(MENU_ENUM_LABEL_NAVIGATION_WRAPAROUND);
   menuGroup->add(MENU_ENUM_LABEL_PAUSE_LIBRETRO);

   inputGroup->add(MENU_ENUM_LABEL_MOUSE_ENABLE);
   inputGroup->add(MENU_ENUM_LABEL_POINTER_ENABLE);

   menuGroup->addRow(inputGroup);
   menuGroup->add(MENU_ENUM_LABEL_THREADED_DATA_RUNLOOP_ENABLE);

   miscGroup->add(MENU_ENUM_LABEL_PAUSE_NONACTIVE);
   miscGroup->add(MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION);

   menuGroup->add(MENU_ENUM_LABEL_UI_COMPANION_ENABLE);
   menuGroup->add(MENU_ENUM_LABEL_UI_COMPANION_START_ON_BOOT);
   menuGroup->add(MENU_ENUM_LABEL_UI_MENUBAR_ENABLE);
   menuGroup->add(MENU_ENUM_LABEL_MENU_SCROLL_FAST);

   /* layout->add(MENU_ENUM_LABEL_DESKTOP_MENU_ENABLE); */
   desktopGroup->add(MENU_ENUM_LABEL_UI_COMPANION_TOGGLE);

   layout->addWidget(menuGroup);
   layout->addWidget(miscGroup);
   layout->addWidget(desktopGroup);
   layout->addStretch();

   widget->setLayout(layout);

   return widget;
}

ViewsPage::ViewsPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS);
}

QWidget *ViewsPage::widget()
{
   unsigned i;
   QWidget           * widget = new QWidget();
   QHBoxLayout *mainLayout    = new QHBoxLayout;
   FormLayout *leftLayout     = new FormLayout;
   QVBoxLayout *rightLayout   = new QVBoxLayout;
   SettingsGroup *quickMenu   = new SettingsGroup("Quick Menu");
   QuickMenuPage *quickPage   = new QuickMenuPage(this);
   SettingsGroup *mainMenu    = new SettingsGroup("Main Menu");
   SettingsGroup *settings    = new SettingsGroup("Settings");
   SettingsGroup *tabs        = new SettingsGroup("Tabs");
   SettingsGroup *status      = new SettingsGroup("Status");
   SettingsGroup *startScreen = new SettingsGroup("StartScreen");
   settings_t *_settings      = config_get_ptr();
   unsigned tabs_begin        = 0;
   unsigned status_begin      = 0;
   file_list_t *list          = (file_list_t*)calloc(1, sizeof(*list));

   {
      rarch_setting_t *kiosk_mode = NULL;
      menu_displaylist_build_list(list, _settings,
            DISPLAYLIST_MENU_VIEWS_SETTINGS_LIST, true);
      kiosk_mode                  = menu_setting_find_enum(
            MENU_ENUM_LABEL_MENU_ENABLE_KIOSK_MODE);

      for (i = 0; i < list->size; i++)
      {
         menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
            file_list_get_actiondata_at_offset(list, i);

         if (cbs->enum_idx == (kiosk_mode 
                  ? MENU_ENUM_LABEL_CONTENT_SHOW_SETTINGS 
                  : MENU_ENUM_LABEL_CONTENT_SHOW_EXPLORE))
         {
            tabs_begin = i;
            break;
         }

         mainMenu->add(cbs->enum_idx);
      }

      for (i = tabs_begin; i < list->size; i++)
      {
         menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
            file_list_get_actiondata_at_offset(list, i);

         if (cbs->enum_idx == MENU_ENUM_LABEL_TIMEDATE_ENABLE)
         {
            status_begin = i;
            break;
         }

         tabs->add(cbs->enum_idx);
      }

      for (i = status_begin; i < list->size; i++)
      {
         menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
            file_list_get_actiondata_at_offset(list, i);

         if (cbs->enum_idx == MENU_ENUM_LABEL_MENU_SHOW_SUBLABELS)
            break;

         status->add(cbs->enum_idx);
      }

      file_list_free(list);
   }

   {
      unsigned i;
      file_list_t *list = (file_list_t*)calloc(1, sizeof(*list));
      menu_displaylist_build_list(list, _settings,
            DISPLAYLIST_SETTINGS_VIEWS_SETTINGS_LIST, true);

      for (i = 0; i < list->size; i++)
      {
         menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
            file_list_get_actiondata_at_offset(list, i);

         settings->add(cbs->enum_idx);
      }

      file_list_free(list);
   }

   startScreen->add(MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN);

   quickMenu->layout()->setContentsMargins(0, 0, 0, 0);
   quickMenu->addRow(quickPage->widget());

   leftLayout->addRow(mainMenu);
   leftLayout->addRow(settings);
   leftLayout->addRow(startScreen);
   leftLayout->add(MENU_ENUM_LABEL_MENU_SHOW_SUBLABELS);

   rightLayout->addWidget(tabs);
   rightLayout->addWidget(quickMenu);
   rightLayout->addWidget(status);
   rightLayout->addStretch();

   mainLayout->addLayout(leftLayout);
   mainLayout->addLayout(rightLayout);

   widget->setLayout(mainLayout);

   return widget;
}

QuickMenuPage::QuickMenuPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS);
}

QWidget *QuickMenuPage::widget()
{
   return create_widget(DISPLAYLIST_QUICK_MENU_VIEWS_SETTINGS_LIST);
}

AppearancePage::AppearancePage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_MENU_SETTINGS);
}

QWidget *AppearancePage::widget()
{
   unsigned i;
   QWidget            * widget = new QWidget;
   FormLayout          *layout = new FormLayout;
   file_list_t           *list = (file_list_t*)calloc(1, sizeof(*list));
   settings_t *settings        = config_get_ptr();

   menu_displaylist_build_list(
         list, settings, DISPLAYLIST_MENU_SETTINGS_LIST, true);

   /* TODO/FIXME - we haven't yet figured out how to 
    * put a radio button setting next to another radio 
    * button on the same row */

   for (i = 0; i < list->size; i++)
   {
      menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
         file_list_get_actiondata_at_offset(list, i);

      switch (cbs->enum_idx)
      {
         /* TODO/FIXME - this is a dirty hack - if we 
          * detect this setting, we instead replace it with a
          * color button and ignore the other two font color 
          * settings since they are already covered by this one
          * color button */
         case MENU_ENUM_LABEL_MENU_FONT_COLOR_RED:
            layout->addUIntColorButton("Menu Font Color: ",
                  MENU_ENUM_LABEL_MENU_FONT_COLOR_RED,
                  MENU_ENUM_LABEL_MENU_FONT_COLOR_GREEN,
                  MENU_ENUM_LABEL_MENU_FONT_COLOR_BLUE);
            break;
         case MENU_ENUM_LABEL_MENU_FONT_COLOR_GREEN:
         case MENU_ENUM_LABEL_MENU_FONT_COLOR_BLUE:
            break;
         default:
            layout->add(cbs->enum_idx);
            break;
      }
   }

   file_list_free(list);

   widget->setLayout(layout);

   return widget;
}

DesktopMenuPage::DesktopMenuPage(MainWindow *mainwindow, QObject *parent) :
   OptionsPage(parent)
   ,m_widget(new ViewOptionsWidget(mainwindow))
{
   setDisplayName("Desktop Menu");
}

void DesktopMenuPage::apply()
{
   m_widget->saveViewOptions();
}

void DesktopMenuPage::load()
{
   m_widget->loadViewOptions();
}

QWidget *DesktopMenuPage::widget()
{
   return m_widget;
}

UserCategory::UserCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_USER_SETTINGS);
   setCategoryIcon("menu_user");
}

QVector<OptionsPage*> UserCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new UserPage(this);
   pages << new AccountsPage(this);

   return pages;
}

UserPage::UserPage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *UserPage::widget()
{
   QWidget *widget    = new QWidget;
   FormLayout *layout = new FormLayout;

   layout->add(MENU_ENUM_LABEL_NETPLAY_NICKNAME);
   layout->add(MENU_ENUM_LABEL_USER_LANGUAGE);

   widget->setLayout(layout);

   return widget;
}

AccountsPage::AccountsPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST);
}

QWidget *AccountsPage::widget()
{
   QWidget *widget             = new QWidget;
   QVBoxLayout *layout         = new QVBoxLayout;
   SettingsGroup *youtubeGroup = new SettingsGroup(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACCOUNTS_YOUTUBE));
   SettingsGroup *twitchGroup  = new SettingsGroup(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACCOUNTS_TWITCH));
   SettingsGroup *facebookGroup = new SettingsGroup(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACCOUNTS_FACEBOOK));
#ifdef HAVE_CHEEVOS
   SettingsGroup *cheevosGroup = new SettingsGroup(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS));

   cheevosGroup->add(MENU_ENUM_LABEL_CHEEVOS_USERNAME);
   cheevosGroup->add(MENU_ENUM_LABEL_CHEEVOS_PASSWORD);

   layout->addWidget(cheevosGroup);
#endif

   youtubeGroup->add(MENU_ENUM_LABEL_YOUTUBE_STREAM_KEY);

   layout->addWidget(youtubeGroup);

   twitchGroup->add(MENU_ENUM_LABEL_TWITCH_STREAM_KEY);

   layout->addWidget(twitchGroup);

   facebookGroup->add(MENU_ENUM_LABEL_FACEBOOK_STREAM_KEY);

   layout->addWidget(facebookGroup);

   layout->addStretch();

   widget->setLayout(layout);

   return widget;
}

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
   QHBoxLayout *windowedCustomSizeLayout   = new QHBoxLayout;
   FormLayout *leftWindowedCustomSizeForm  = new FormLayout;
   FormLayout *rightWindowedCustomSizeForm = new FormLayout;
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   CheckableSettingsGroup *savePosGroup    = new CheckableSettingsGroup(MENU_ENUM_LABEL_VIDEO_WINDOW_SAVE_POSITION);
#else
   CheckableSettingsGroup *savePosGroup    = new CheckableSettingsGroup(MENU_ENUM_LABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE);
#endif

   SettingsGroup *hdrGroup             = new SettingsGroup("HDR");
   QHBoxLayout *hdrLayout              = new QHBoxLayout;

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
   leftWindowedSizeForm->addRow("Max Width:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX));

   rightWindowedSizeForm->addRow("Opacity:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_WINDOW_OPACITY));
   rightWindowedSizeForm->addRow("Max Height:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX));

   windowedSizeLayout->addLayout(leftWindowedSizeForm);
   windowedSizeLayout->addLayout(rightWindowedSizeForm);

   windowedGroup->addRow(windowedSizeLayout);

   leftWindowedCustomSizeForm->addRow("Width:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_WINDOW_WIDTH));
   rightWindowedCustomSizeForm->addRow("Height:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_WINDOW_HEIGHT));

   windowedCustomSizeLayout->addLayout(leftWindowedCustomSizeForm);
   windowedCustomSizeLayout->addLayout(rightWindowedCustomSizeForm);

   savePosGroup->addRow(windowedCustomSizeLayout);
   windowedGroup->addRow(savePosGroup);

   windowedGroup->add(MENU_ENUM_LABEL_VIDEO_WINDOW_SHOW_DECORATIONS);

   hdrGroup->add(MENU_ENUM_LABEL_VIDEO_HDR_ENABLE);
   hdrGroup->add(MENU_ENUM_LABEL_VIDEO_HDR_MAX_NITS);
   hdrGroup->add(MENU_ENUM_LABEL_VIDEO_HDR_PAPER_WHITE_NITS);
   hdrGroup->add(MENU_ENUM_LABEL_VIDEO_HDR_CONTRAST);
   hdrGroup->add(MENU_ENUM_LABEL_VIDEO_HDR_EXPAND_GAMUT);

   vSyncGroup->add(MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL);
   vSyncGroup->add(MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION);
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
   miscGroup->add(MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT);
   miscGroup->add(MENU_ENUM_LABEL_VIDEO_SMOOTH);
   miscGroup->add(MENU_ENUM_LABEL_VIDEO_CTX_SCALING);
   miscGroup->add(MENU_ENUM_LABEL_VIDEO_SHADER_DELAY);

   hdrLayout->addWidget(hdrGroup);

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
   layout->addLayout(hdrLayout);
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
   QHBoxLayout *full           = new QHBoxLayout;
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

   full->addWidget(new UIntRadioButton(MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX, ASPECT_RATIO_FULL));

   leftAspect->addRow(new UIntRadioButton(MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX, ASPECT_RATIO_CORE));
   leftAspect->addRow(preset);

   rightAspect->addRow(config);
   rightAspect->addRow(new UIntRadioButton(MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX, ASPECT_RATIO_SQUARE));

   aspectL->addLayout(leftAspect);
   aspectL->addStretch();
   aspectL->addSpacing(30);
   aspectL->addLayout(rightAspect);

   addRow(aspectL);
   addRow(full);
   addRow(custom);

   connect(m_radioButton, SIGNAL(clicked(bool)), this, SLOT(onAspectRadioClicked(bool)));
}

void AspectRatioGroup::paintEvent(QPaintEvent *event)
{
   settings_t *settings = config_get_ptr();
   unsigned       value = settings->uints.video_aspect_ratio_idx;

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
   settings_t *settings = config_get_ptr();
   Q_UNUSED(index)

   settings->uints.crt_switch_resolution_super = 
   m_crtSuperResolutionCombo->currentData().value<unsigned>();
}

AspectRatioRadioButton::AspectRatioRadioButton(unsigned min, unsigned max, QWidget *parent) :
   QRadioButton(parent)
   ,m_min(min)
   ,m_max(max)
{
}

/* DRIVERS */

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
   return create_widget(DISPLAYLIST_DRIVER_SETTINGS_LIST);
}

/* DIRECTORY */

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

/* CONFIGURATION */

ConfigurationCategory::ConfigurationCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS);
   setCategoryIcon("setting");
}

QVector<OptionsPage*> ConfigurationCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new ConfigurationPage(this);

   return pages;
}

ConfigurationPage::ConfigurationPage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *ConfigurationPage::widget()
{
   return create_widget(DISPLAYLIST_CONFIGURATION_SETTINGS_LIST);
}

/* CORE */

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
   return create_widget(DISPLAYLIST_CORE_SETTINGS_LIST);
}

/* LOGGING */

LoggingCategory::LoggingCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS);
   setCategoryIcon("menu_log");
}

QVector<OptionsPage*> LoggingCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new LoggingPage(this);

   return pages;
}

LoggingPage::LoggingPage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *LoggingPage::widget()
{
   return create_widget(DISPLAYLIST_LOGGING_SETTINGS_LIST);
}

/* AI SERVICE */

AIServiceCategory::AIServiceCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS);
   setCategoryIcon("menu_drivers");
}

QVector<OptionsPage*> AIServiceCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new AIServicePage(this);

   return pages;
}

AIServicePage::AIServicePage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS);
}

QWidget *AIServicePage::widget()
{
   return create_widget(DISPLAYLIST_AI_SERVICE_SETTINGS_LIST);
}

/* FRAME THROTTLE */

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
   return create_widget(DISPLAYLIST_FRAME_THROTTLE_SETTINGS_LIST);
}

RewindPage::RewindPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS);
}

QWidget *RewindPage::widget()
{
   return create_widget(DISPLAYLIST_REWIND_SETTINGS_LIST);
}
