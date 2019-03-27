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
   QWidget *widget = new QWidget;

   FormLayout *layout = new FormLayout;

   SettingsGroup *savesGroup = new SettingsGroup("Saves");
   SettingsGroup *savestatesGroup = new SettingsGroup("Savestates");
   CheckableSettingsGroup *autoSavestatesGroup = new CheckableSettingsGroup(MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE);
   SettingsGroup *saveRamGroup = new SettingsGroup("SaveRAM");
   SettingsGroup *systemFilesDirGroup = new SettingsGroup("System Files");
   SettingsGroup *screenshotsDirGroup = new SettingsGroup("Screenshots");

   savesGroup->addCheckBox(MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE);
   savesGroup->addCheckBox(MENU_ENUM_LABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE);

   savestatesGroup->addCheckBox(MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX);

   autoSavestatesGroup->addCheckBox(MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD);

   savestatesGroup->addRow(autoSavestatesGroup);
   savestatesGroup->addCheckBox(MENU_ENUM_LABEL_SAVESTATE_THUMBNAIL_ENABLE);
   savestatesGroup->addCheckBox(MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE);
   savestatesGroup->addCheckBox(MENU_ENUM_LABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE);

   saveRamGroup->addCheckBox(MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE);
   saveRamGroup->addUIntSpinBox(MENU_ENUM_LABEL_AUTOSAVE_INTERVAL);

   systemFilesDirGroup->addCheckBox(MENU_ENUM_LABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE);

   screenshotsDirGroup->addCheckBox(MENU_ENUM_LABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE);

   layout->addRow(savesGroup);
   layout->addRow(savestatesGroup);
   layout->addRow(saveRamGroup);
   layout->addRow(systemFilesDirGroup);
   layout->addRow(screenshotsDirGroup);

   widget->setLayout(layout);

   return widget;
}
