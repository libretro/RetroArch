#include "options.h"

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
   QVBoxLayout                  *layout = new QVBoxLayout;

   CheckableSettingsGroup *overlayGroup = new CheckableSettingsGroup(MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE);
   CheckableSettingsGroup  *inputsGroup = new CheckableSettingsGroup(MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS);

   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_AUTOLOAD_PREFERRED);
   overlayGroup->add(MENU_ENUM_LABEL_INPUT_OVERLAY_AUTO_ROTATE);
   overlayGroup->add(MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU);
   overlayGroup->add(MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED);
   overlayGroup->add(MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR);

   inputsGroup->add(MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT);

   overlayGroup->addRow(inputsGroup);

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

   return widget;
}
