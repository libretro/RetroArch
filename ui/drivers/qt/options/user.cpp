#include "options.h"

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

   layout->addStretch();

   widget->setLayout(layout);

   return widget;
}
