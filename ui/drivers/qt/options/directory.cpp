#include "options.h"

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
   QWidget *widget    = new QWidget;
   FormLayout *layout = new FormLayout;

   layout->add(MENU_ENUM_LABEL_CORE_ASSETS_DIRECTORY);
   layout->add(MENU_ENUM_LABEL_ASSETS_DIRECTORY);
   layout->add(MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY);
   layout->add(MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY);
   layout->add(MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY);
   layout->add(MENU_ENUM_LABEL_RGUI_CONFIG_DIRECTORY);
   layout->add(MENU_ENUM_LABEL_LIBRETRO_DIR_PATH);
   layout->add(MENU_ENUM_LABEL_LIBRETRO_INFO_PATH);
   layout->add(MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY);
   layout->add(MENU_ENUM_LABEL_CURSOR_DIRECTORY);
   layout->add(MENU_ENUM_LABEL_CHEAT_DATABASE_PATH);
   layout->add(MENU_ENUM_LABEL_VIDEO_FILTER_DIR);
   layout->add(MENU_ENUM_LABEL_AUDIO_FILTER_DIR);
   layout->add(MENU_ENUM_LABEL_VIDEO_SHADER_DIR);
   layout->add(MENU_ENUM_LABEL_RECORDING_OUTPUT_DIRECTORY);
   layout->add(MENU_ENUM_LABEL_RECORDING_CONFIG_DIRECTORY);
   layout->add(MENU_ENUM_LABEL_OVERLAY_DIRECTORY);
   layout->add(MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY);
   layout->add(MENU_ENUM_LABEL_JOYPAD_AUTOCONFIG_DIR);
   layout->add(MENU_ENUM_LABEL_INPUT_REMAPPING_DIRECTORY);
   layout->add(MENU_ENUM_LABEL_PLAYLIST_DIRECTORY);
   layout->add(MENU_ENUM_LABEL_SAVEFILE_DIRECTORY);
   layout->add(MENU_ENUM_LABEL_SAVESTATE_DIRECTORY);
   layout->add(MENU_ENUM_LABEL_CACHE_DIRECTORY);

   widget->setLayout(layout);

   return widget;
}
