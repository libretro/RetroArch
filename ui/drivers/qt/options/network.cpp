#include <QButtonGroup>

#include "options.h"

#ifndef CXX_BUILD
extern "C" {
#endif

#include <string/stdstring.h>

#include "../../network/netplay/netplay.h"

#ifndef CXX_BUILD
}
#endif

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
   checksLayout->add(MENU_ENUM_LABEL_NETWORK_ON_DEMAND_THUMBNAILS);

   serverForm->add(MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS);
   serverForm->add(MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT);
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
