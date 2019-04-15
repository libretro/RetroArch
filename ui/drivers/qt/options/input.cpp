#include <QStackedWidget>

#include "options.h"

#ifndef CXX_BUILD
extern "C" {
#endif

#include "../../input/input_driver.h"

#ifndef CXX_BUILD
}
#endif

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
   QWidget *widget    = new QWidget;
   FormLayout *layout = new FormLayout;

   layout->add(MENU_ENUM_LABEL_INPUT_MAX_USERS);
   layout->add(MENU_ENUM_LABEL_INPUT_UNIFIED_MENU_CONTROLS);
   layout->add(MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR);
   layout->add(MENU_ENUM_LABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO);
   layout->add(MENU_ENUM_LABEL_MENU_INPUT_SWAP_OK_CANCEL);
   layout->add(MENU_ENUM_LABEL_INPUT_ALL_USERS_CONTROL_MENU);
   layout->add(MENU_ENUM_LABEL_INPUT_REMAP_BINDS_ENABLE);
   layout->add(MENU_ENUM_LABEL_INPUT_AUTODETECT_ENABLE);
   layout->add(MENU_ENUM_LABEL_INPUT_BUTTON_AXIS_THRESHOLD);
   layout->add(MENU_ENUM_LABEL_INPUT_ANALOG_DEADZONE);
   layout->add(MENU_ENUM_LABEL_INPUT_ANALOG_SENSITIVITY);
   layout->add(MENU_ENUM_LABEL_INPUT_BIND_TIMEOUT);
   layout->add(MENU_ENUM_LABEL_INPUT_BIND_HOLD);
   layout->add(MENU_ENUM_LABEL_INPUT_TURBO_PERIOD);
   layout->add(MENU_ENUM_LABEL_INPUT_DUTY_CYCLE);

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
   unsigned count          = 0;
   unsigned half           = 40 / 2; /* TODO unhardcode */
   QWidget *widget         = new QWidget;
   QHBoxLayout *layout     = new QHBoxLayout;
   FormLayout *leftLayout  = new FormLayout;
   FormLayout *rightLayout = new FormLayout;

   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      enum msg_hash_enums lbl = (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN + i);

      if (count < half)
      {
         if (leftLayout->add(lbl))
            count++;
      }
      else
         rightLayout->add(lbl);
   }

   layout->addLayout(leftLayout);
   layout->addSpacing(50);
   layout->addLayout(rightLayout);

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
   unsigned max_users    = *(input_driver_get_uint(INPUT_ACTION_MAX_USERS));
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

         input_config_get_bind_string(descriptor,
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
