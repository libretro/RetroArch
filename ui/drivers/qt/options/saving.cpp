#include "options.h"

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
   SettingsGroup                   *savesGroup = new SettingsGroup(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_SETTINGS_SUBSECTION_SAVING_SAVES));
   SettingsGroup              *savestatesGroup = new SettingsGroup(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_SETTINGS_SUBSECTION_SAVING_SAVESTATES));
   CheckableSettingsGroup *autoSavestatesGroup = new CheckableSettingsGroup(MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE);
   SettingsGroup                 *saveRamGroup = new SettingsGroup(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_SETTINGS_SUBSECTION_SAVING_SAVERAM));
   SettingsGroup          *systemFilesDirGroup = new SettingsGroup(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_SETTINGS_SUBSECTION_SAVING_SYSTEMFILES));
   SettingsGroup          *screenshotsDirGroup = new SettingsGroup(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_SETTINGS_SUBSECTION_SAVING_SCREENSHOTS));
   SettingsGroup          *runtimeLogGroup     = new SettingsGroup(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_SETTINGS_SUBSECTION_SAVING_RUNTIME_LOG));

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
