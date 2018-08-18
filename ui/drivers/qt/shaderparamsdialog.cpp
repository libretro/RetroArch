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

#include "shaderparamsdialog.h"
#include "../ui_qt.h"

extern "C" {
#include <string/stdstring.h>
#include "../../../command.h"
#ifdef HAVE_MENU
#include "../../../menu/menu_shader.h"
#endif
}

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

void ShaderParamsDialog::reload()
{
   struct video_shader *menu_shader = NULL;
   struct video_shader *video_shader = NULL;
   const char *shader_path = NULL;
   int i;
   unsigned j;

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
      setWindowTitle(QFileInfo(shader_path).completeBaseName());
   }
   else
      setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS));

   /* NOTE: We assume that parameters are always grouped in order by the pass number, e.g., all parameters for pass 0 come first, then params for pass 1, etc. */
   for (i = 0; i < static_cast<int>(video_shader->passes); i++)
   {
      QFormLayout *form = NULL;
      QGroupBox *groupBox = NULL;
      QFileInfo fileInfo(video_shader->pass[i].source.path);
      QString shaderBasename = fileInfo.completeBaseName();
      QHBoxLayout *filterScaleHBoxLayout = NULL;
      QComboBox *filterComboBox = new QComboBox();
      QComboBox *scaleComboBox = new QComboBox();
      unsigned j = 0;

      filterComboBox->setProperty("pass", i);
      scaleComboBox->setProperty("pass", i);

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
      filterScaleHBoxLayout->addWidget(new QLabel(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FILTER)));
      filterScaleHBoxLayout->addWidget(filterComboBox);
      filterScaleHBoxLayout->addWidget(new QLabel(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCALE)));
      filterScaleHBoxLayout->addWidget(scaleComboBox);

      form->addRow("", filterScaleHBoxLayout);

      for (j = 0; j < video_shader->num_parameters; j++)
      {
         struct video_shader_parameter *param = &video_shader->parameters[j];

         if (param->pass != i)
            continue;

         addShaderParam(param, j, form);
      }
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
