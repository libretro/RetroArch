#include "options.h"

PlaylistsCategory::PlaylistsCategory(QWidget *parent) :
   OptionsCategory(parent)
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
   layout->add(MENU_ENUM_LABEL_PLAYLIST_ENTRY_REMOVE);
   layout->add(MENU_ENUM_LABEL_PLAYLIST_USE_OLD_FORMAT);

   widget->setLayout(layout);

   return widget;
}
