#include <QCloseEvent>
#include <QResizeEvent>
#include <QMessageBox>
#include <QPainter>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QFileDialog>
#include <QTimer>

#include "coreoptionsdialog.h"
#include "../ui_qt.h"

#ifndef CXX_BUILD
extern "C" {
#endif

#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include "../../../command.h"
#include "../../../configuration.h"
#include "../../../msg_hash.h"
#include "../../../retroarch.h"
#include "../../../paths.h"
#include "../../../file_path_special.h"
#include "../../../managers/core_option_manager.h"

#ifndef CXX_BUILD
}
#endif

CoreOptionsDialog::CoreOptionsDialog(QWidget *parent) :
   QDialog(parent)
   ,m_layout()
   ,m_scrollArea()
{
   setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS));
   setObjectName("coreOptionsDialog");

   resize(720, 480);

   QTimer::singleShot(0, this, SLOT(clearLayout()));
}

CoreOptionsDialog::~CoreOptionsDialog()
{
}

void CoreOptionsDialog::resizeEvent(QResizeEvent *event)
{
   QDialog::resizeEvent(event);

   if (!m_scrollArea)
      return;

   m_scrollArea->resize(event->size());

   emit resized(event->size());
}

void CoreOptionsDialog::closeEvent(QCloseEvent *event)
{
   QDialog::closeEvent(event);

   emit closed();
}

void CoreOptionsDialog::paintEvent(QPaintEvent *event)
{
   QStyleOption o;
   QPainter p;
   o.initFrom(this);
   p.begin(this);
   style()->drawPrimitive(
      QStyle::PE_Widget, &o, &p, this);
   p.end();

   QDialog::paintEvent(event);
}

void CoreOptionsDialog::clearLayout()
{
   QWidget *widget = NULL;

   if (m_scrollArea)
   {
      foreach (QObject *obj, children())
      {
         obj->deleteLater();
      }
   }

   m_layout = new QVBoxLayout();

   widget = new QWidget();
   widget->setLayout(m_layout);
   widget->setObjectName("coreOptionsWidget");

   m_scrollArea = new QScrollArea();

   m_scrollArea->setParent(this);
   m_scrollArea->setWidgetResizable(true);
   m_scrollArea->setWidget(widget);
   m_scrollArea->setObjectName("coreOptionsScrollArea");
   m_scrollArea->show();
}

void CoreOptionsDialog::reload()
{
   buildLayout();
}

void CoreOptionsDialog::onSaveGameSpecificOptions()
{
   if (!create_folder_and_core_options())
      QMessageBox::critical(this, msg_hash_to_str(MSG_ERROR), msg_hash_to_str(MSG_ERROR_SAVING_CORE_OPTIONS_FILE));
}

void CoreOptionsDialog::onCoreOptionComboBoxCurrentIndexChanged(int index)
{
   unsigned i, k;
   QString key, val;
   size_t          opts = 0;
   QComboBox *combo_box = qobject_cast<QComboBox*>(sender());

   if (!combo_box)
      return;

   key = combo_box->itemData(index, Qt::UserRole).toString();
   val = combo_box->itemText(index);

   if (rarch_ctl(RARCH_CTL_HAS_CORE_OPTIONS, NULL))
   {
      rarch_ctl(RARCH_CTL_GET_CORE_OPTION_SIZE, &opts);

      if (opts)
      {
         core_option_manager_t *coreopts = NULL;

         rarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

         if (coreopts)
         {
            for (i = 0; i < opts; i++)
            {
               QString optKey;
               struct core_option *option = static_cast<struct core_option*>(&coreopts->opts[i]);

               if (!option)
                  continue;

               optKey = option->key;

               if (key == optKey)
               {
                  for (k = 0; k < option->vals->size; k++)
                  {
                     QString str = option->vals->elems[k].data;

                     if (!str.isEmpty() && str == val)
                        core_option_manager_set_val(coreopts, i, k);
                  }
               }
            }
         }
      }
   }
}

void CoreOptionsDialog::buildLayout()
{
   unsigned j, k;
   size_t opts = 0;
   QFormLayout    *form  = NULL;
   settings_t *settings  = config_get_ptr();
   bool has_core_options = rarch_ctl(RARCH_CTL_HAS_CORE_OPTIONS, NULL);

   clearLayout();

   if (has_core_options)
   {
      rarch_ctl(RARCH_CTL_GET_CORE_OPTION_SIZE, &opts);

      if (opts)
      {
         core_option_manager_t *coreopts = NULL;

         form = new QFormLayout();

         if (settings->bools.game_specific_options)
         {
            QString contentLabel;
            QString label;
            rarch_system_info_t *system = runloop_get_system_info();

            /* TODO/FIXME - why have this check here? system is not used */
            if (system)
               contentLabel = QFileInfo(path_get(RARCH_PATH_BASENAME)).completeBaseName();

            if (!contentLabel.isEmpty())
            {
               if (!rarch_ctl(RARCH_CTL_IS_GAME_OPTIONS_ACTIVE, NULL))
                  label = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE);
               else
                  label = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE);

               if (!label.isEmpty())
               {
                  QHBoxLayout *gameOptionsLayout = new QHBoxLayout();
                  QPushButton *button = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SAVE), this);

                  connect(button, SIGNAL(clicked()), this, SLOT(onSaveGameSpecificOptions()));

                  gameOptionsLayout->addWidget(new QLabel(contentLabel, this));
                  gameOptionsLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Preferred));
                  gameOptionsLayout->addWidget(button);

                  form->addRow(label, gameOptionsLayout);
               }
            }
         }

         rarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

         if (coreopts)
         {
            QToolButton *resetAllButton = new QToolButton(this);

            resetAllButton->setDefaultAction(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_RESET_ALL), this));
            connect(resetAllButton, SIGNAL(clicked()), this, SLOT(onCoreOptionResetAllClicked()));

            for (j = 0; j < opts; j++)
            {
               QString desc               = 
                  core_option_manager_get_desc(coreopts, j);
               QString val                = 
                  core_option_manager_get_val(coreopts, j);
               QComboBox *combo_box       = NULL;
               QLabel *descLabel          = NULL;
               QHBoxLayout *comboLayout   = NULL;
               QToolButton *resetButton   = NULL;
               struct core_option *option = NULL;

               if (desc.isEmpty() || !coreopts->opts)
                  continue;

               option = static_cast<struct core_option*>(&coreopts->opts[j]);

               if (!option->vals || option->vals->size == 0)
                  continue;

               comboLayout = new QHBoxLayout();
               descLabel   = new QLabel(desc, this);
               combo_box   = new QComboBox(this);
               combo_box->setObjectName("coreOptionComboBox");
               resetButton = new QToolButton(this);
               resetButton->setObjectName("resetButton");
               resetButton->setDefaultAction(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_RESET), this));
               resetButton->setProperty("comboBox",
                     QVariant::fromValue(combo_box));

               connect(resetButton, SIGNAL(clicked()),
                     this, SLOT(onCoreOptionResetClicked()));

               if (!string_is_empty(option->info))
               {
                  char *new_info = strdup(option->info);
                  word_wrap(new_info, new_info, 50, true, 0);
                  descLabel->setToolTip(new_info);
                  combo_box->setToolTip(new_info);
                  free(new_info);
               }

               for (k = 0; k < option->vals->size; k++)
                  combo_box->addItem(option->vals->elems[k].data, option->key);

               combo_box->setCurrentText(val);
               combo_box->setProperty("default_index",
                     static_cast<unsigned>(option->default_index));

               /* Only connect the signal after setting the default item */
               connect(combo_box, SIGNAL(currentIndexChanged(int)),
                     this,
                     SLOT(onCoreOptionComboBoxCurrentIndexChanged(int)));

               comboLayout->addWidget(combo_box);
               comboLayout->addWidget(resetButton);

               form->addRow(descLabel, comboLayout);
            }

            form->addRow(resetAllButton, new QWidget(this));

            m_layout->addLayout(form);
         }
      }
   }

   if (!opts)
   {
      QLabel *noParamsLabel = new QLabel(msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE), this);
      noParamsLabel->setAlignment(Qt::AlignCenter);

      m_layout->addWidget(noParamsLabel);
   }

   m_layout->addItem(new QSpacerItem(20, 20,
            QSizePolicy::Minimum, QSizePolicy::Expanding));

   resize(width() + 1, height());
   show();
   resize(width() - 1, height());
}

void CoreOptionsDialog::onCoreOptionResetClicked()
{
   bool ok              = false;
   QToolButton *button  = qobject_cast<QToolButton*>(sender());
   QComboBox *combo_box = NULL;
   int default_index    = 0;

   if (!button)
      return;

   combo_box = qobject_cast<QComboBox*>(button->property("comboBox").value<QComboBox*>());

   if (!combo_box)
      return;

   default_index = combo_box->property("default_index").toInt(&ok);

   if (!ok)
      return;

   if (default_index >= 0 && default_index < combo_box->count())
      combo_box->setCurrentIndex(default_index);
}

void CoreOptionsDialog::onCoreOptionResetAllClicked()
{
   int i;
   QList<QComboBox*> combo_boxes = findChildren<QComboBox*>("coreOptionComboBox");

   for (i = 0; i < combo_boxes.count(); i++)
   {
      int   default_index  = 0;
      bool             ok  = false;
      QComboBox *combo_box = combo_boxes.at(i);

      if (!combo_box)
         continue;

      default_index        = combo_box->property("default_index").toInt(&ok);

      if (!ok)
         continue;

      if (default_index >= 0 && default_index < combo_box->count())
         combo_box->setCurrentIndex(default_index);
   }
}
