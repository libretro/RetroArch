#include <QCloseEvent>
#include <QResizeEvent>
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

#include "shaderparamsdialog.h"
#include "../ui_qt.h"

extern "C" {
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include "../../../command.h"
#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../paths.h"
#ifdef HAVE_MENU
#include "../../../menu/menu_shader.h"
#endif
}

enum
{
   SHADER_PRESET_SAVE_CORE = 0,
   SHADER_PRESET_SAVE_GAME,
   SHADER_PRESET_SAVE_PARENT,
   SHADER_PRESET_SAVE_NORMAL
};

ShaderParamsDialog::ShaderParamsDialog(QWidget *parent) :
   QDialog(parent)
   ,m_layout(NULL)
{
   QScrollArea *scrollArea = NULL;
   QWidget *widget = NULL;

   setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS));
   setObjectName("shaderParamsDialog");

   m_layout = new QVBoxLayout();

   widget = new QWidget();
   widget->setLayout(m_layout);
   widget->setObjectName("shaderParamsWidget");
   scrollArea = new QScrollArea(this);
   scrollArea->setWidgetResizable(true);
   scrollArea->setWidget(widget);
   scrollArea->setObjectName("shaderParamsScrollArea");

   setProperty("scrollArea", QVariant::fromValue(scrollArea));
}

ShaderParamsDialog::~ShaderParamsDialog()
{
}

void ShaderParamsDialog::resizeEvent(QResizeEvent *event)
{
   QVariant scrollAreaVariant = property("scrollArea");
   QScrollArea *scrollArea = NULL;

   QDialog::resizeEvent(event);

   if (!scrollAreaVariant.isValid())
      return;

   scrollArea = scrollAreaVariant.value<QScrollArea*>();

   if (!scrollArea)
      return;

   scrollArea->resize(event->size());
}

void ShaderParamsDialog::closeEvent(QCloseEvent *event)
{
   QDialog::closeEvent(event);

   emit closed();
}

QString ShaderParamsDialog::getFilterLabel(unsigned filter)
{
   QString filterString;

   switch (filter)
   {
      case 0:
         filterString = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DONT_CARE);
         break;
      case 1:
         filterString = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LINEAR);
         break;
      case 2:
         filterString = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NEAREST);
         break;
      default:
         break;
   }

   return filterString;
}

void ShaderParamsDialog::clearLayout(QLayout *layout)
{
   QLayoutItem *child = NULL;

   while (layout->count() && ((child = layout->takeAt(0)) != 0))
   {
      QWidget *widget = child->widget();
      QLayout *childLayout = child->layout();

      if (widget)
      {
         QLayout *widgetLayout = widget->layout();

         if (widgetLayout)
            clearLayout(widgetLayout);

         /* deleteLater() doesn't work right for some reason here,
          * so just disconnect any signals in case there are pending events,
          * and delete the widget immediately.
          */
         widget->disconnect();
         delete widget;
      }

      if (childLayout)
         clearLayout(childLayout);

      delete child;
   }
}

void ShaderParamsDialog::getShaders(struct video_shader **menu_shader, struct video_shader **video_shader)
{
   video_shader_ctx_t shader_info = {0};
#ifdef HAVE_MENU
   struct video_shader *shader = menu_shader_get();

   if (menu_shader)
   {
      if (shader)
      {
         *menu_shader = shader;
      }
      else
      {
         *menu_shader = NULL;
      }
   }

   if (video_shader)
   {
      if (shader)
      {
         *video_shader = shader_info.data;
      }
      else
      {
         *video_shader = NULL;
      }
   }
#endif

   if (video_shader)
   {
      if (!video_shader_driver_get_current_shader(&shader_info))
      {
         *video_shader = NULL;
         return;
      }

      if (!shader_info.data || shader_info.data->num_parameters > GFX_MAX_PARAMETERS)
      {
         *video_shader = NULL;
         return;
      }

      if (shader_info.data)
      {
         *video_shader = shader_info.data;
      }
      else
      {
         *video_shader = NULL;
      }
   }
}

void ShaderParamsDialog::onFilterComboBoxIndexChanged(int)
{
   QComboBox *comboBox = qobject_cast<QComboBox*>(sender());
   QVariant passVariant;
   int pass = 0;
   bool ok = false;
   struct video_shader *menu_shader = NULL;
   struct video_shader *video_shader = NULL;

   getShaders(&menu_shader, &video_shader);

   if (!comboBox)
      return;

   passVariant = comboBox->property("pass");

   if (!passVariant.isValid())
      return;

   pass = passVariant.toInt(&ok);

   if (!ok)
      return;

   if (menu_shader && pass >= 0 && pass < static_cast<int>(menu_shader->passes))
   {
      QVariant data = comboBox->currentData();

      if (data.isValid())
      {
         unsigned filter = data.toUInt(&ok);

         if (ok)
         {
            if (menu_shader)
               menu_shader->pass[pass].filter = filter;
            if (video_shader)
               video_shader->pass[pass].filter = filter;

            command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
         }
      }
   }
}

void ShaderParamsDialog::onScaleComboBoxIndexChanged(int)
{
   QComboBox *comboBox = qobject_cast<QComboBox*>(sender());
   QVariant passVariant;
   int pass = 0;
   bool ok = false;
   struct video_shader *menu_shader = NULL;
   struct video_shader *video_shader = NULL;

   getShaders(&menu_shader, &video_shader);

   if (!comboBox)
      return;

   passVariant = comboBox->property("pass");

   if (!passVariant.isValid())
      return;

   pass = passVariant.toInt(&ok);

   if (!ok)
      return;

   if (menu_shader && pass >= 0 && pass < static_cast<int>(menu_shader->passes))
   {
      QVariant data = comboBox->currentData();

      if (data.isValid())
      {
         unsigned scale = data.toUInt(&ok);

         if (ok)
         {
            if (menu_shader)
            {
               menu_shader->pass[pass].fbo.scale_x = scale;
               menu_shader->pass[pass].fbo.scale_y = scale;
               menu_shader->pass[pass].fbo.valid = scale;
            }

            if (video_shader)
            {
               video_shader->pass[pass].fbo.scale_x = scale;
               video_shader->pass[pass].fbo.scale_y = scale;
               video_shader->pass[pass].fbo.valid = scale;
            }

            command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
         }
      }
   }
}

void ShaderParamsDialog::onShaderPassMoveDownClicked()
{
   QPushButton *button = qobject_cast<QPushButton*>(sender());
   QVariant passVariant;
   struct video_shader *menu_shader = NULL;
   struct video_shader *video_shader = NULL;
   int pass = 0;
   bool ok = false;

   getShaders(&menu_shader, &video_shader);

   if (!button)
      return;

   passVariant = button->property("pass");

   if (!passVariant.isValid())
      return;

   pass = passVariant.toInt(&ok);

   if (!ok)
      return;

   if (pass < 0)
      return;

   if (video_shader)
   {
      if (pass >= static_cast<int>(video_shader->passes) - 1)
         return;

      std::swap(video_shader->pass[pass], video_shader->pass[pass + 1]);
   }

   if (menu_shader)
   {
      if (pass >= static_cast<int>(menu_shader->passes) - 1)
         return;

      std::swap(menu_shader->pass[pass], menu_shader->pass[pass + 1]);
   }

   command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
}

void ShaderParamsDialog::onShaderPassMoveUpClicked()
{
   QPushButton *button = qobject_cast<QPushButton*>(sender());
   QVariant passVariant;
   struct video_shader *menu_shader = NULL;
   struct video_shader *video_shader = NULL;
   int pass = 0;
   bool ok = false;

   getShaders(&menu_shader, &video_shader);

   if (!button)
      return;

   passVariant = button->property("pass");

   if (!passVariant.isValid())
      return;

   pass = passVariant.toInt(&ok);

   if (!ok)
      return;

   if (pass <= 0)
      return;

   if (video_shader)
   {
      if (pass > static_cast<int>(video_shader->passes) - 1)
         return;

      std::swap(video_shader->pass[pass - 1], video_shader->pass[pass]);
   }

   if (menu_shader)
   {
      if (pass > static_cast<int>(menu_shader->passes) - 1)
         return;

      std::swap(menu_shader->pass[pass - 1], menu_shader->pass[pass]);
   }

   command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
}

void ShaderParamsDialog::onShaderLoadPresetClicked()
{
#ifdef HAVE_MENU
   QString path;
   QByteArray pathArray;
   struct video_shader *menu_shader = NULL;
   struct video_shader *video_shader = NULL;
   const char *pathData = NULL;
   settings_t *settings = config_get_ptr();
   enum rarch_shader_type type = RARCH_SHADER_NONE;

   if (!settings)
      return;

   getShaders(&menu_shader, &video_shader);

   if (!menu_shader)
      return;

   path = QFileDialog::getOpenFileName(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET), settings->paths.directory_video_shader);

   if (path.isEmpty())
      return;

   pathArray = path.toUtf8();
   pathData = pathArray.constData();

   type = video_shader_parse_type(pathData, RARCH_SHADER_NONE);

   menu_shader_manager_set_preset(menu_shader, type, pathData);
#endif
}

void ShaderParamsDialog::onShaderAddPassClicked()
{
#ifdef HAVE_MENU
   QString path;
   QByteArray pathArray;
   struct video_shader *menu_shader = NULL;
   struct video_shader *video_shader = NULL;
   struct video_shader_pass *shader_pass = NULL;
   const char *pathData = NULL;
   settings_t *settings = config_get_ptr();

   if (!settings)
      return;

   getShaders(&menu_shader, &video_shader);

   if (!menu_shader)
      return;

   path = QFileDialog::getOpenFileName(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET), settings->paths.directory_video_shader);

   if (path.isEmpty())
      return;

   pathArray = path.toUtf8();
   pathData = pathArray.constData();

   if (menu_shader->passes < GFX_MAX_SHADERS)
      menu_shader_manager_increment_amount_passes();
   else
      return;

   shader_pass = &menu_shader->pass[menu_shader->passes - 1];

   if (!shader_pass)
      return;

   strlcpy(shader_pass->source.path, pathData, sizeof(shader_pass->source.path));

   video_shader_resolve_parameters(NULL, menu_shader);

   command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
#endif
}

void ShaderParamsDialog::onShaderSavePresetAsClicked()
{
#ifdef HAVE_MENU
   settings_t *settings = config_get_ptr();
   QString path;
   QByteArray pathArray;
   const char *pathData = NULL;

   path = QFileDialog::getSaveFileName(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS), settings->paths.directory_video_shader);

   if (path.isEmpty())
      return;

   pathArray = path.toUtf8();
   pathData = pathArray.constData();

   saveShaderPreset(pathData, SHADER_PRESET_SAVE_NORMAL);
#endif
}

void ShaderParamsDialog::saveShaderPreset(const char *path, unsigned action_type)
{
   char directory[PATH_MAX_LENGTH];
   char file[PATH_MAX_LENGTH];
   char tmp[PATH_MAX_LENGTH];
   settings_t *settings            = config_get_ptr();
   const char *core_name           = NULL;
   rarch_system_info_t *info       = runloop_get_system_info();

   directory[0] = file[0] = tmp[0] = '\0';

   if (info)
      core_name           = info->info.library_name;

   if (!string_is_empty(core_name))
   {
      fill_pathname_join(
            tmp,
            settings->paths.directory_video_shader,
            "presets",
            sizeof(tmp));
      fill_pathname_join(
            directory,
            tmp,
            core_name,
            sizeof(directory));
   }

   if (!filestream_exists(directory))
       path_mkdir(directory);

   switch (action_type)
   {
      case SHADER_PRESET_SAVE_CORE:
         if (!string_is_empty(core_name))
            fill_pathname_join(file, directory, core_name, sizeof(file));
         break;
      case SHADER_PRESET_SAVE_GAME:
      {
         const char *game_name = path_basename(path_get(RARCH_PATH_BASENAME));
         fill_pathname_join(file, directory, game_name, sizeof(file));
         break;
      }
      case SHADER_PRESET_SAVE_PARENT:
      {
         fill_pathname_parent_dir_name(tmp, path_get(RARCH_PATH_BASENAME), sizeof(tmp));
         fill_pathname_join(file, directory, tmp, sizeof(file));
         break;
      }
      case SHADER_PRESET_SAVE_NORMAL:
      default:
         if (!string_is_empty(path))
            strlcpy(file, path, sizeof(file));
         break;
   }

   if (menu_shader_manager_save_preset(file, false, true))
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_SHADER_PRESET_SAVED_SUCCESSFULLY),
            1, 100, true);
   else
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_ERROR_SAVING_SHADER_PRESET),
            1, 100, true);
}

void ShaderParamsDialog::onShaderSaveCorePresetClicked()
{
   saveShaderPreset(NULL, SHADER_PRESET_SAVE_CORE);
}

void ShaderParamsDialog::onShaderSaveParentPresetClicked()
{
   saveShaderPreset(NULL, SHADER_PRESET_SAVE_PARENT);
}

void ShaderParamsDialog::onShaderSaveGamePresetClicked()
{
   saveShaderPreset(NULL, SHADER_PRESET_SAVE_GAME);
}

void ShaderParamsDialog::onShaderClearAllPassesClicked()
{
#ifdef HAVE_MENU
   struct video_shader *menu_shader = NULL;
   struct video_shader *video_shader = NULL;

   getShaders(&menu_shader, &video_shader);

   if (!menu_shader)
      return;

   while (menu_shader->passes > 0)
      menu_shader_manager_decrement_amount_passes();

   onShaderApplyClicked();
#endif
}

void ShaderParamsDialog::onShaderRemovePassClicked()
{
#ifdef HAVE_MENU
   QAction *action = qobject_cast<QAction*>(sender());
   QVariant passVariant;
   struct video_shader *menu_shader = NULL;
   struct video_shader *video_shader = NULL;
   int pass = 0;
   int i;
   bool ok = false;

   getShaders(&menu_shader, &video_shader);

   if (!menu_shader || menu_shader->passes == 0 || !action)
      return;

   passVariant = action->data();

   if (!passVariant.isValid())
      return;

   pass = passVariant.toInt(&ok);

   if (!ok)
      return;

   if (pass < 0 || pass > static_cast<int>(menu_shader->passes))
      return;

   /* move selected pass to the bottom */
   for (i = pass; i < static_cast<int>(menu_shader->passes) - 1; i++)
   {
      std::swap(menu_shader->pass[i], menu_shader->pass[i + 1]);
   }

   menu_shader_manager_decrement_amount_passes();

   onShaderApplyClicked();
#endif
}

void ShaderParamsDialog::onShaderApplyClicked()
{
   command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
}

void ShaderParamsDialog::reload()
{
   QPushButton *loadButton = NULL;
   QPushButton *saveButton = NULL;
   QPushButton *removeButton = NULL;
   QPushButton *applyButton = NULL;
   QHBoxLayout *topButtonLayout = NULL;
   QMenu *loadMenu = NULL;
   QMenu *saveMenu = NULL;
   QMenu *removeMenu = NULL;
   struct video_shader *menu_shader = NULL;
   struct video_shader *video_shader = NULL;
   const char *shader_path = NULL;
   int i;
   unsigned j;
   bool hasPasses = false;

   getShaders(&menu_shader, &video_shader);

   /* NOTE: For some reason, menu_shader_get() returns a COPY of what get_current_shader() gives us.
    * And if you want to be able to change shader settings/parameters from both the raster menu and
    * Qt at the same time... you must change BOTH or one will overwrite the other.
    */

   if ((video_shader && video_shader->passes == 0) || !video_shader)
   {
      setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS));
      goto end;
   }

   clearLayout(m_layout);

   /* Only check video_shader for the path, menu_shader seems stale... e.g. if you remove all the shader passes,
    * it still has the old path in it, but video_shader does not
    */
   if (!string_is_empty(video_shader->path))
   {
      shader_path = video_shader->path;
      setWindowTitle(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER)) + ": " + QFileInfo(shader_path).fileName());
   }
   else
      setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS));

   loadButton = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_LOAD), this);
   saveButton = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SAVE), this);
   removeButton = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_REMOVE), this);
   applyButton = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_APPLY), this);

   loadMenu = new QMenu(loadButton);
   loadMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET), this, SLOT(onShaderLoadPresetClicked()));
   loadMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SHADER_ADD_PASS), this, SLOT(onShaderAddPassClicked()));

   loadButton->setMenu(loadMenu);

   saveMenu = new QMenu(saveButton);
   saveMenu->addAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS)) + "...", this, SLOT(onShaderSavePresetAsClicked()));
   saveMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE), this, SLOT(onShaderSaveCorePresetClicked()));
   saveMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT), this, SLOT(onShaderSaveParentPresetClicked()));
   saveMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME), this, SLOT(onShaderSaveGamePresetClicked()));

   saveButton->setMenu(saveMenu);

   removeMenu = new QMenu(removeButton);

   /* When there are no passes, at least on first startup, it seems video_shader erroneously shows 1 pass, with an empty source file.
    * So we use menu_shader instead for that.
    */
   if (menu_shader)
   {
      for (i = 0; i < static_cast<int>(menu_shader->passes); i++)
      {
         QFileInfo fileInfo(menu_shader->pass[i].source.path);
         QString shaderBasename = fileInfo.completeBaseName();
         QAction *action = removeMenu->addAction(shaderBasename, this, SLOT(onShaderRemovePassClicked()));

         action->setData(i);
      }
   }

   removeMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SHADER_CLEAR_ALL_PASSES), this, SLOT(onShaderClearAllPassesClicked()));

   removeButton->setMenu(removeMenu);

   connect(applyButton, SIGNAL(clicked()), this, SLOT(onShaderApplyClicked()));

   topButtonLayout = new QHBoxLayout();
   topButtonLayout->addWidget(loadButton);
   topButtonLayout->addWidget(saveButton);
   topButtonLayout->addWidget(removeButton);
   topButtonLayout->addWidget(applyButton);

   m_layout->addLayout(topButtonLayout);

   /* NOTE: We assume that parameters are always grouped in order by the pass number, e.g., all parameters for pass 0 come first, then params for pass 1, etc. */
   for (i = 0; i < static_cast<int>(video_shader->passes); i++)
   {
      QFormLayout *form = NULL;
      QGroupBox *groupBox = NULL;
      QFileInfo fileInfo(video_shader->pass[i].source.path);
      QString shaderBasename = fileInfo.completeBaseName();
      QHBoxLayout *filterScaleHBoxLayout = NULL;
      QComboBox *filterComboBox = new QComboBox(this);
      QComboBox *scaleComboBox = new QComboBox(this);
      QToolButton *moveDownButton = NULL;
      QToolButton *moveUpButton = NULL;
      unsigned j = 0;

      /* Sometimes video_shader shows 1 pass with no source file, when there are really 0 passes. */
      if (shaderBasename.isEmpty())
         continue;

      hasPasses = true;

      filterComboBox->setProperty("pass", i);
      scaleComboBox->setProperty("pass", i);

      moveDownButton = new QToolButton(this);
      moveDownButton->setText("↓");
      moveDownButton->setProperty("pass", i);

      moveUpButton = new QToolButton(this);
      moveUpButton->setText("↑");
      moveUpButton->setProperty("pass", i);

      /* Can't move down if we're already at the bottom. */
      if (i < static_cast<int>(video_shader->passes) - 1)
         connect(moveDownButton, SIGNAL(clicked()), this, SLOT(onShaderPassMoveDownClicked()));
      else
         moveDownButton->setDisabled(true);

      /* Can't move up if we're already at the top. */
      if (i > 0)
         connect(moveUpButton, SIGNAL(clicked()), this, SLOT(onShaderPassMoveUpClicked()));
      else
         moveUpButton->setDisabled(true);

      for (;;)
      {
         QString filterLabel = getFilterLabel(j);

         if (filterLabel.isEmpty())
            break;

         if (j == 0)
            filterLabel = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DONT_CARE);

         filterComboBox->addItem(filterLabel, j);

         j++;
      }

      for (j = 0; j < 7; j++)
      {
         QString label;

         if (j == 0)
            label = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DONT_CARE);
         else
            label = QString::number(j) + "x";

         scaleComboBox->addItem(label, j);
      }

      filterComboBox->setCurrentIndex(static_cast<int>(video_shader->pass[i].filter));
      scaleComboBox->setCurrentIndex(static_cast<int>(video_shader->pass[i].fbo.scale_x));

      /* connect the signals only after the initial index is set */
      connect(filterComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onFilterComboBoxIndexChanged(int)));
      connect(scaleComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onScaleComboBoxIndexChanged(int)));

      form = new QFormLayout();
      groupBox = new QGroupBox(shaderBasename);
      groupBox->setLayout(form);

      m_layout->addWidget(groupBox);

      filterScaleHBoxLayout = new QHBoxLayout();
      filterScaleHBoxLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Preferred));
      filterScaleHBoxLayout->addWidget(new QLabel(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FILTER)) + ":", this));
      filterScaleHBoxLayout->addWidget(filterComboBox);
      filterScaleHBoxLayout->addWidget(new QLabel(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCALE)) + ":", this));
      filterScaleHBoxLayout->addWidget(scaleComboBox);
      filterScaleHBoxLayout->addSpacerItem(new QSpacerItem(20, 0, QSizePolicy::Preferred, QSizePolicy::Preferred));

      if (moveUpButton)
         filterScaleHBoxLayout->addWidget(moveUpButton);

      if (moveDownButton)
         filterScaleHBoxLayout->addWidget(moveDownButton);

      form->addRow("", filterScaleHBoxLayout);

      for (j = 0; j < video_shader->num_parameters; j++)
      {
         struct video_shader_parameter *param = &video_shader->parameters[j];

         if (param->pass != i)
            continue;

         addShaderParam(param, j, form);
      }
   }

   if (!hasPasses)
   {
      QLabel *noParamsLabel = new QLabel(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SHADER_NO_PASSES), this);
      noParamsLabel->setAlignment(Qt::AlignCenter);

      m_layout->addWidget(noParamsLabel);
   }

   m_layout->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));

end:
   resize(720, 480);
   show();
}

void ShaderParamsDialog::addShaderParam(struct video_shader_parameter *param, int parameter, QFormLayout *form)
{
   QString desc = param->desc;

   if ((param->minimum == 0.0)
         && (param->maximum
            == (param->minimum
               + param->step)))
   {
      /* option is basically a bool, so use a checkbox */
      QCheckBox *checkBox = new QCheckBox(this);
      checkBox->setChecked(param->current == param->maximum ? true : false);
      checkBox->setProperty("param", parameter);

      connect(checkBox, SIGNAL(clicked()), this, SLOT(onShaderParamCheckBoxClicked()));

      form->addRow(desc, checkBox);
   }
   else
   {
      QDoubleSpinBox *doubleSpinBox = NULL;
      QSpinBox *spinBox = NULL;
      QHBoxLayout *box = new QHBoxLayout();
      QSlider *slider = new QSlider(Qt::Horizontal, this);
      double value = MainWindow::lerp(param->minimum, param->maximum, 0, 100, param->current);
      double intpart = 0;
      bool stepIsFractional = modf(param->step, &intpart);

      slider->setRange(0, 100);
      slider->setSingleStep(1);
      slider->setValue(value);
      slider->setProperty("param", parameter);

      connect(slider, SIGNAL(valueChanged(int)), this, SLOT(onShaderParamSliderValueChanged(int)));

      box->addWidget(slider);

      if (stepIsFractional)
      {
         doubleSpinBox = new QDoubleSpinBox(this);
         doubleSpinBox->setRange(param->minimum, param->maximum);
         doubleSpinBox->setSingleStep(param->step);
         doubleSpinBox->setValue(param->current);
         doubleSpinBox->setProperty("slider", QVariant::fromValue(slider));
         slider->setProperty("doubleSpinBox", QVariant::fromValue(doubleSpinBox));

         connect(doubleSpinBox, SIGNAL(valueChanged(double)), this, SLOT(onShaderParamDoubleSpinBoxValueChanged(double)));

         box->addWidget(doubleSpinBox);
      }
      else
      {
         spinBox = new QSpinBox(this);
         spinBox->setRange(param->minimum, param->maximum);
         spinBox->setSingleStep(param->step);
         spinBox->setValue(param->current);
         spinBox->setProperty("slider", QVariant::fromValue(slider));
         slider->setProperty("spinBox", QVariant::fromValue(spinBox));

         connect(spinBox, SIGNAL(valueChanged(int)), this, SLOT(onShaderParamSpinBoxValueChanged(int)));

         box->addWidget(spinBox);
      }

      form->addRow(desc, box);
   }
}

void ShaderParamsDialog::onShaderParamCheckBoxClicked()
{
   QCheckBox *checkBox = qobject_cast<QCheckBox*>(sender());
   QVariant paramVariant;
   struct video_shader *menu_shader = NULL;
   struct video_shader *video_shader = NULL;

   getShaders(&menu_shader, &video_shader);

   if (!checkBox)
      return;

   if (menu_shader && menu_shader->passes == 0)
      return;

   paramVariant = checkBox->property("param");

   if (paramVariant.isValid())
   {
      bool ok = false;
      int parameter = paramVariant.toInt(&ok);

      if (!ok)
         return;

      if (menu_shader)
      {
         struct video_shader_parameter *param = NULL;

         param = &menu_shader->parameters[parameter];

         if (param)
            param->current = (checkBox->isChecked() ? param->maximum : param->minimum);
      }

      if (video_shader)
      {
         struct video_shader_parameter *param = NULL;

         param = &video_shader->parameters[parameter];

         if (param)
            param->current = (checkBox->isChecked() ? param->maximum : param->minimum);
      }
   }
}

void ShaderParamsDialog::onShaderParamSliderValueChanged(int)
{
   QVariant spinBoxVariant;
   QVariant paramVariant;
   QSlider *slider = qobject_cast<QSlider*>(sender());
   struct video_shader *menu_shader = NULL;
   struct video_shader *video_shader = NULL;
   double newValue = 0.0;

   getShaders(&menu_shader, &video_shader);

   if (!slider)
      return;

   spinBoxVariant = slider->property("spinBox");
   paramVariant = slider->property("param");

   if (paramVariant.isValid())
   {
      bool ok = false;
      int parameter = paramVariant.toInt(&ok);

      if (ok)
      {
         if (menu_shader)
         {
            struct video_shader_parameter *param = &menu_shader->parameters[parameter];

            newValue = MainWindow::lerp(0, 100, param->minimum, param->maximum, slider->value());
            param->current = newValue;
         }

         if (video_shader)
         {
            struct video_shader_parameter *param = &video_shader->parameters[parameter];

            newValue = MainWindow::lerp(0, 100, param->minimum, param->maximum, slider->value());
            param->current = newValue;
         }
      }
   }

   if (spinBoxVariant.isValid())
   {
      QSpinBox *spinBox = spinBoxVariant.value<QSpinBox*>();

      if (!spinBox)
         return;

      spinBox->blockSignals(true);
      spinBox->setValue(newValue);
      spinBox->blockSignals(false);
   }
   else
   {
      QVariant doubleSpinBoxVariant = slider->property("doubleSpinBox");
      QDoubleSpinBox *doubleSpinBox = doubleSpinBoxVariant.value<QDoubleSpinBox*>();

      if (!doubleSpinBox)
         return;

      doubleSpinBox->blockSignals(true);
      doubleSpinBox->setValue(newValue);
      doubleSpinBox->blockSignals(false);
   }
}

void ShaderParamsDialog::onShaderParamSpinBoxValueChanged(int value)
{
   QSpinBox *spinBox = qobject_cast<QSpinBox*>(sender());
   QVariant sliderVariant;
   QVariant paramVariant;
   QSlider *slider = NULL;
   struct video_shader *menu_shader = NULL;
   struct video_shader *video_shader = NULL;

   getShaders(&menu_shader, &video_shader);

   if (!spinBox)
      return;

   sliderVariant = spinBox->property("slider");

   if (!sliderVariant.isValid())
      return;

   slider = sliderVariant.value<QSlider*>();

   if (!slider)
      return;

   paramVariant = slider->property("param");

   if (paramVariant.isValid())
   {
      bool ok = false;
      int parameter = paramVariant.toInt(&ok);

      if (ok)
      {
         double newValue = 0.0;

         if (menu_shader)
         {
            struct video_shader_parameter *param = &menu_shader->parameters[parameter];

            param->current = value;
            newValue = MainWindow::lerp(param->minimum, param->maximum, 0, 100, param->current);
            slider->blockSignals(true);
            slider->setValue(newValue);
            slider->blockSignals(false);
         }

         if (video_shader)
         {
            struct video_shader_parameter *param = &video_shader->parameters[parameter];

            param->current = value;
            newValue = MainWindow::lerp(param->minimum, param->maximum, 0, 100, param->current);
            slider->blockSignals(true);
            slider->setValue(newValue);
            slider->blockSignals(false);
         }
      }
   }
}

void ShaderParamsDialog::onShaderParamDoubleSpinBoxValueChanged(double value)
{
   QDoubleSpinBox *doubleSpinBox = qobject_cast<QDoubleSpinBox*>(sender());
   QVariant sliderVariant;
   QVariant paramVariant;
   QSlider *slider = NULL;
   struct video_shader *menu_shader = NULL;
   struct video_shader *video_shader = NULL;

   getShaders(&menu_shader, &video_shader);

   if (!doubleSpinBox)
      return;

   sliderVariant = doubleSpinBox->property("slider");

   if (!sliderVariant.isValid())
      return;

   slider = sliderVariant.value<QSlider*>();

   if (!slider)
      return;

   paramVariant = slider->property("param");

   if (paramVariant.isValid())
   {
      bool ok = false;
      int parameter = paramVariant.toInt(&ok);

      if (ok)
      {
         double newValue = 0.0;

         if (menu_shader)
         {
            struct video_shader_parameter *param = &menu_shader->parameters[parameter];

            param->current = value;
            newValue = MainWindow::lerp(param->minimum, param->maximum, 0, 100, param->current);
            slider->blockSignals(true);
            slider->setValue(newValue);
            slider->blockSignals(false);
         }

         if (video_shader)
         {
            struct video_shader_parameter *param = &video_shader->parameters[parameter];

            param->current = value;
            newValue = MainWindow::lerp(param->minimum, param->maximum, 0, 100, param->current);
            slider->blockSignals(true);
            slider->setValue(newValue);
            slider->blockSignals(false);
         }
      }
   }
}
