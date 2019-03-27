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
   QWidget *widget = new QWidget;

   FormLayout *layout = new FormLayout;

   layout->addDirectorySelector(MENU_ENUM_LABEL_CORE_ASSETS_DIRECTORY);
   layout->addDirectorySelector(MENU_ENUM_LABEL_ASSETS_DIRECTORY);
   layout->addDirectorySelector(MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY);
   layout->addDirectorySelector(MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY);
   layout->addDirectorySelector(MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY);
   layout->addDirectorySelector(MENU_ENUM_LABEL_RGUI_CONFIG_DIRECTORY);
   layout->addDirectorySelector(MENU_ENUM_LABEL_LIBRETRO_DIR_PATH);
   layout->addDirectorySelector(MENU_ENUM_LABEL_LIBRETRO_INFO_PATH);
   layout->addDirectorySelector(MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY);
   layout->addDirectorySelector(MENU_ENUM_LABEL_CURSOR_DIRECTORY);
   layout->addDirectorySelector(MENU_ENUM_LABEL_CHEAT_DATABASE_PATH);
   layout->addDirectorySelector(MENU_ENUM_LABEL_VIDEO_FILTER_DIR);
   layout->addDirectorySelector(MENU_ENUM_LABEL_AUDIO_FILTER_DIR);
   layout->addDirectorySelector(MENU_ENUM_LABEL_VIDEO_SHADER_DIR);
   layout->addDirectorySelector(MENU_ENUM_LABEL_RECORDING_OUTPUT_DIRECTORY);
   layout->addDirectorySelector(MENU_ENUM_LABEL_RECORDING_CONFIG_DIRECTORY);
   layout->addDirectorySelector(MENU_ENUM_LABEL_OVERLAY_DIRECTORY);
   layout->addDirectorySelector(MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY);
   layout->addDirectorySelector(MENU_ENUM_LABEL_JOYPAD_AUTOCONFIG_DIR);
   layout->addDirectorySelector(MENU_ENUM_LABEL_INPUT_REMAPPING_DIRECTORY);
   layout->addDirectorySelector(MENU_ENUM_LABEL_PLAYLIST_DIRECTORY);
   layout->addDirectorySelector(MENU_ENUM_LABEL_SAVEFILE_DIRECTORY);
   layout->addDirectorySelector(MENU_ENUM_LABEL_SAVESTATE_DIRECTORY);
   layout->addDirectorySelector(MENU_ENUM_LABEL_CACHE_DIRECTORY);

   widget->setLayout(layout);

   return widget;
}
