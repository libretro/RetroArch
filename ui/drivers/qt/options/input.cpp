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

   {
      unsigned i;
      file_list_t *list     = (file_list_t*)calloc(1, sizeof(*list));
      menu_displaylist_build_list(list, DISPLAYLIST_INPUT_SETTINGS_LIST, true);

      for (i = 0; i < list->size; i++)
      {
         menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
            file_list_get_actiondata_at_offset(list, i);

         if (cbs->enum_idx == MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS)
            break;

         layout->add(cbs->enum_idx);
      }

      file_list_free(list);
   }

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
   file_list_t *list       = (file_list_t*)calloc(1, sizeof(*list));

   menu_displaylist_build_list(list, DISPLAYLIST_INPUT_HOTKEY_BINDS_LIST, true);

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
