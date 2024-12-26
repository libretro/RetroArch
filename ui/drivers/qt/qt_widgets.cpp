#include <QDragEnterEvent>
#include <QDropEvent>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QSpinBox>
#include <QKeyEvent>
#include <QFormLayout>
#include <QFileInfo>
#include <QFileDialog>
#include <QGroupBox>
#include <QButtonGroup>
#include <QPaintEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>
#include <QMimeData>
#include <QAction>
#include <QMenu>

#include "qt_widgets.h"
#include "qt_dialogs.h"
#include "../ui_qt.h"

#ifndef CXX_BUILD
extern "C" {
#endif

#include <math.h>

#ifdef HAVE_CONFIG_H
#include "../../../config.h"
#endif

#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#include "../../../configuration.h"
#include "../../../file_path_special.h"
#include "../../../msg_hash.h"

#ifndef CXX_BUILD
}
#endif

#ifdef HAVE_MENU
static const QRegularExpression decimalsRegex("%.(\\d)f");

static inline void add_sublabel_and_whats_this(
      QWidget *widget, rarch_setting_t *setting)
{
   struct menu_file_list_cbs cbs = {};
   char tmp[512];
   tmp[0] = '\0';

   cbs.enum_idx = setting->enum_idx;

   menu_cbs_init_bind_sublabel(&cbs, NULL, NULL, 0, setting->type, setting->size);

   cbs.action_sublabel(0, 0, 0, 0, 0, tmp, sizeof(tmp));

   widget->setToolTip(tmp);

   msg_hash_get_help_enum(setting->enum_idx, tmp, sizeof(tmp));

   if (!string_is_equal(tmp, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE)))
      widget->setWhatsThis(tmp);
}

static inline QString sanitize_ampersand(QString input)
{
   return input.replace("&", "&&");
}

static inline QString form_label(rarch_setting_t *setting)
{
   return QString(sanitize_ampersand(setting->short_description)) + ":";
}

FormLayout::FormLayout(QWidget *parent) :
   QFormLayout(parent)
{
}

void FormLayout::addCheckBox(rarch_setting_t *setting)
{
   if (setting && setting->short_description)
      addRow(new CheckBox(setting));
}

void FormLayout::addUIntRadioButtons(rarch_setting_t *setting)
{
   if (setting && setting->short_description)
      addRow(new UIntRadioButtons(setting));
}

void FormLayout::addUIntComboBox(rarch_setting_t *setting)
{
   if (setting && setting->short_description)
      addRow(form_label(setting), new UIntComboBox(setting));
}

void FormLayout::addStringComboBox(rarch_setting_t *setting)
{
   if (setting && setting->short_description)
      addRow(form_label(setting), new StringComboBox(setting));
}

void FormLayout::addStringLineEdit(rarch_setting_t *setting)
{
   if (setting && setting->short_description)
      addRow(form_label(setting), new StringLineEdit(setting));
}

void FormLayout::addPasswordLineEdit(rarch_setting_t *setting)
{
   if (setting && setting->short_description)
      addRow(form_label(setting), new PasswordLineEdit(setting));
}

void FormLayout::addUIntSpinBox(rarch_setting_t *setting)
{
   if (setting && setting->short_description)
      addRow(form_label(setting), new UIntSpinBox(setting));
}

void FormLayout::addSizeSpinBox(rarch_setting_t *setting, unsigned scale)
{
   if (setting && setting->short_description)
      addRow(form_label(setting), new SizeSpinBox(setting, scale));
}

void FormLayout::addFloatSpinBox(rarch_setting_t *setting)
{
   if (setting && setting->short_description)
      addRow(form_label(setting), new FloatSpinBox(setting));
}

void FormLayout::addDirectorySelector(rarch_setting_t *setting)
{
   if (setting && setting->short_description)
      addRow(form_label(setting), new DirectorySelector(setting));
}

void FormLayout::addFileSelector(rarch_setting_t *setting)
{
   if (setting && setting->short_description)
      addRow(form_label(setting), new FileSelector(setting));
}

void FormLayout::addFloatSliderAndSpinBox(rarch_setting_t *setting)
{
   if (setting && setting->short_description)
      addRow(form_label(setting), new FloatSliderAndSpinBox(setting));
}

void FormLayout::addUIntColorButton(const QString &title, msg_hash_enums r, msg_hash_enums g, msg_hash_enums b)
{
   rarch_setting_t   *red = menu_setting_find_enum(r);
   rarch_setting_t *green = menu_setting_find_enum(g);
   rarch_setting_t  *blue = menu_setting_find_enum(b);

   if (red && green && blue)
      addRow(title, new UIntColorButton(red, green, blue));
}

bool FormLayout::addBindButton(rarch_setting_t *setting)
{
   if (!setting || !setting->short_description)
      return false;

   addRow(QString(setting->short_description), new BindButton(setting));

   return true;
}

SettingsGroup::SettingsGroup(const QString &title, QWidget *parent) :
   QGroupBox(title, parent)
   ,m_layout(new FormLayout(this))
{
}

SettingsGroup::SettingsGroup(QWidget *parent) :
   QGroupBox(parent)
   ,m_layout(new FormLayout(this))
{
}

void SettingsGroup::addWidget(QWidget *widget)
{
   m_layout->addWidget(widget);
}

void SettingsGroup::addRow(QString label, QWidget *widget)
{
   m_layout->addRow(label, widget);
}

void SettingsGroup::addRow(QWidget *widget)
{
   m_layout->addRow(widget);
}

void SettingsGroup::addRow(QLayout *layout)
{
   m_layout->addRow(layout);
}

void SettingsGroup::addRow(QString label, QLayout *layout)
{
   m_layout->addRow(label, layout);
}

void SettingsGroup::addCheckBox(rarch_setting_t *setting)
{
   m_layout->addCheckBox(setting);
}

void SettingsGroup::addDirectorySelector(rarch_setting_t *setting)
{
   m_layout->addDirectorySelector(setting);
}

void SettingsGroup::addFileSelector(rarch_setting_t *setting)
{
   m_layout->addFileSelector(setting);
}

void SettingsGroup::addStringLineEdit(rarch_setting_t *setting)
{
   m_layout->addStringLineEdit(setting);
}

void SettingsGroup::addPasswordLineEdit(rarch_setting_t *setting)
{
   m_layout->addPasswordLineEdit(setting);
}

void SettingsGroup::addStringComboBox(rarch_setting_t *setting)
{
   m_layout->addStringComboBox(setting);
}

void SettingsGroup::addUIntComboBox(rarch_setting_t *setting)
{
   m_layout->addUIntComboBox(setting);
}

void SettingsGroup::addUIntRadioButtons(rarch_setting_t *setting)
{
   m_layout->addUIntRadioButtons(setting);
}

void SettingsGroup::addUIntSpinBox(rarch_setting_t *setting)
{
   m_layout->addUIntSpinBox(setting);
}

void SettingsGroup::addFloatSpinBox(rarch_setting_t *setting)
{
   m_layout->addFloatSpinBox(setting);
}

void SettingsGroup::addFloatSliderAndSpinBox(rarch_setting_t *setting)
{
   m_layout->addFloatSliderAndSpinBox(setting);
}

void SettingsGroup::addUIntColorButton(const QString &title, msg_hash_enums r, msg_hash_enums g, msg_hash_enums b)
{
   m_layout->addUIntColorButton(title, r, g, b);
}

void SettingsGroup::addBindButton(rarch_setting_t *setting)
{
   m_layout->addBindButton(setting);
}

CheckBox::CheckBox(rarch_setting_t *setting, QWidget *parent) :
   QCheckBox(sanitize_ampersand(setting->short_description), parent)
   ,m_setting(setting)
   ,m_value(setting->value.target.boolean)
{
   QAbstractButton::setChecked(*m_value);

   /* TODO/FIXME only one of these should be necessary */
   connect(this, SIGNAL(toggled(bool)), this, SLOT(onClicked(bool)));
   connect(this, SIGNAL(clicked(bool)), this, SLOT(onClicked(bool)));

   add_sublabel_and_whats_this(this, m_setting);
}

CheckBox::CheckBox(msg_hash_enums enum_idx, QWidget *parent) :
   CheckBox(menu_setting_find_enum(enum_idx), parent)
{
}

void CheckBox::onClicked(bool checked)
{
   *m_value = checked;

   setting_generic_handle_change(m_setting);
}

void CheckBox::paintEvent(QPaintEvent *event)
{
   if (*m_value != QAbstractButton::isChecked())
   {
      blockSignals(true);

      QAbstractButton::setChecked(*m_value);

      blockSignals(false);
   }

   QCheckBox::paintEvent(event);
}

CheckableSettingsGroup::CheckableSettingsGroup(rarch_setting_t *setting, QWidget *parent) :
   SettingsGroup(parent)
{
   if (setting && setting->short_description)
   {
      m_setting = setting;
      m_value   = setting->value.target.boolean;

      setTitle(setting->short_description);

      setCheckable(true);
      setChecked(*m_value);

      connect(this, SIGNAL(clicked(bool)), this, SLOT(onClicked(bool)));

      add_sublabel_and_whats_this(this, m_setting);
   }
}

CheckableSettingsGroup::CheckableSettingsGroup(msg_hash_enums enum_idx, QWidget *parent) :
   CheckableSettingsGroup(menu_setting_find_enum(enum_idx), parent)
{
}

void CheckableSettingsGroup::onClicked(bool checked)
{
   *m_value = checked;

   setting_generic_handle_change(m_setting);
}

void CheckableSettingsGroup::paintEvent(QPaintEvent *event)
{
   if (*m_value != isChecked())
   {
      blockSignals(true);

      setChecked(*m_value);

      blockSignals(false);
   }

   QGroupBox::paintEvent(event);
}

CheckableIcon::CheckableIcon(msg_hash_enums enum_idx, const QIcon &icon, QWidget *parent) :
   CheckableIcon(menu_setting_find_enum(enum_idx), icon, parent)
{
}

CheckableIcon::CheckableIcon(rarch_setting_t *setting, const QIcon &icon, QWidget *parent) :
   QToolButton(parent)
   ,m_setting(setting)
   ,m_value(setting->value.target.boolean)
{
   setIcon(icon);

   setCheckable(true);

   QAbstractButton::setChecked(*m_value);

   connect(this, SIGNAL(toggled(bool)), this, SLOT(onToggled(bool)));

   add_sublabel_and_whats_this(this, m_setting);
}

void CheckableIcon::onToggled(bool checked)
{
   *m_value = QAbstractButton::isChecked();

   setting_generic_handle_change(m_setting);

   QAbstractButton::setChecked(checked);
}

void CheckableIcon::paintEvent(QPaintEvent *event)
{
   if (QAbstractButton::isChecked() != *m_value)
   {
      blockSignals(true);

      QAbstractButton::setChecked(*m_value);

      blockSignals(false);
   }
   QToolButton::paintEvent(event);
}

StringLineEdit::StringLineEdit(rarch_setting_t *setting, QWidget *parent) :
   QLineEdit(setting->value.target.string)
   ,m_setting(setting)
   ,m_value(setting->value.target.string)
{
   connect(this, SIGNAL(editingFinished()), this, SLOT(onEditingFinished()));

   add_sublabel_and_whats_this(this, m_setting);
}

void StringLineEdit::onEditingFinished()
{
   strlcpy(m_value, text().toUtf8().data(), m_setting->size);

   setting_generic_handle_change(m_setting);

   setModified(false);
}

void StringLineEdit::paintEvent(QPaintEvent *event)
{
   if (!isModified() && m_value != text())
   {
      setText(m_value);
      setModified(false);
   }

   QLineEdit::paintEvent(event);
}

PasswordLineEdit::PasswordLineEdit(rarch_setting_t *setting, QWidget *parent) :
   StringLineEdit(setting)
{
   setEchoMode(QLineEdit::Password);
}

StringComboBox::StringComboBox(rarch_setting_t *setting, QWidget *parent) :
   QComboBox(parent)
   ,m_setting(setting)
   ,m_value(setting->value.target.string)
{
   addItems(string_split_to_qt(QString(setting->values), '|'));

   connect(this, SIGNAL(currentTextChanged(const QString&)), this, SLOT(onCurrentTextChanged(const QString&)));

   add_sublabel_and_whats_this(this, m_setting);
}

void StringComboBox::onCurrentTextChanged(const QString &text)
{
   strlcpy(m_value, text.toUtf8().data(), sizeof(m_value));

   setting_generic_handle_change(m_setting);
}

void StringComboBox::paintEvent(QPaintEvent *event)
{
   setCurrentText(m_value);
   QComboBox::paintEvent(event);
}

UIntComboBox::UIntComboBox(rarch_setting_t *setting, QWidget *parent) :
   QComboBox(parent)
   ,m_setting(setting)
   ,m_value(setting->value.target.unsigned_integer)
{
   float min = (setting->flags & SD_FLAG_ENFORCE_MINRANGE) ? setting->min : 0.00f;
   float max = (setting->flags & SD_FLAG_ENFORCE_MAXRANGE) ? setting->max : 999.00f;

   populate(min, max);

   connect(this, SIGNAL(currentIndexChanged(int)), this, SLOT(onCurrentIndexChanged(int)));

   add_sublabel_and_whats_this(this, m_setting);
}

UIntComboBox::UIntComboBox(rarch_setting_t *setting, double min, double max, QWidget *parent) :
   QComboBox(parent)
   ,m_setting(setting)
   ,m_value(setting->value.target.unsigned_integer)
{
   populate(min, max);

   connect(this, SIGNAL(currentIndexChanged(int)), this, SLOT(onCurrentIndexChanged(int)));

   add_sublabel_and_whats_this(this, m_setting);
}

void UIntComboBox::populate(double min, double max)
{
   float i;
   unsigned orig_value = *m_setting->value.target.unsigned_integer;
   float          step = m_setting->step;
   bool  checked_found = false;
   unsigned      count = 0;

   if (m_setting->get_string_representation)
   {
      for (i = min; i <= max; i += step)
      {
         char val_s[NAME_MAX_LENGTH];
         unsigned val = (unsigned)i;

         *m_setting->value.target.unsigned_integer = val;

         m_setting->get_string_representation(m_setting, val_s, sizeof(val_s));

         m_hash[i] = QString(val_s);

         addItem(m_hash[i], i);

         if (!checked_found && val == orig_value)
         {
            setCurrentIndex(count);
            checked_found = true;
         }
         count++;
      }

      *m_setting->value.target.unsigned_integer = orig_value;
   }
}

UIntComboBox::UIntComboBox(msg_hash_enums enum_idx, QWidget *parent) :
   UIntComboBox(menu_setting_find_enum(enum_idx), parent)
{
}

UIntComboBox::UIntComboBox(msg_hash_enums enum_idx, double min, double max, QWidget *parent) :
   UIntComboBox(menu_setting_find_enum(enum_idx), min, max, parent)
{
}

void UIntComboBox::onCurrentIndexChanged(int index)
{
   Q_UNUSED(index);

   *m_value = currentData().toUInt();

   setting_generic_handle_change(m_setting);
}

void UIntComboBox::paintEvent(QPaintEvent *event)
{
   setCurrentText(m_hash.value(*m_value));
   QComboBox::paintEvent(event);
}

UIntSpinBox::UIntSpinBox(rarch_setting_t *setting, QWidget *parent) :
   QSpinBox(parent)
   ,m_setting(setting)
   ,m_value(setting->value.target.unsigned_integer)
{
   setMinimum((setting->flags & SD_FLAG_ENFORCE_MINRANGE) ? setting->min : 0.00f);
   setMaximum((setting->flags & SD_FLAG_ENFORCE_MAXRANGE) ? setting->max : INT_MAX);

   setSingleStep(setting->step);

   connect(this, SIGNAL(valueChanged(int)), this, SLOT(onValueChanged(int)));

   add_sublabel_and_whats_this(this, m_setting);
}

UIntSpinBox::UIntSpinBox(msg_hash_enums enum_idx, QWidget *parent) :
    UIntSpinBox(menu_setting_find_enum(enum_idx), parent)
{
}

void UIntSpinBox::onValueChanged(int value)
{
   *m_value = value;
   setting_generic_handle_change(m_setting);
}

void UIntSpinBox::paintEvent(QPaintEvent *event)
{
   if ((unsigned)value() != *m_value)
   {
      blockSignals(true);

      setValue(*m_value);

      blockSignals(false);
   }

   QSpinBox::paintEvent(event);
}

SizeSpinBox::SizeSpinBox(rarch_setting_t *setting, unsigned scale, QWidget *parent) :
   QSpinBox(parent)
   ,m_setting(setting)
   ,m_value(setting->value.target.sizet)
   ,m_scale(scale)
{
   setMinimum((setting->flags & SD_FLAG_ENFORCE_MINRANGE) ? setting->min / m_scale : 0.00f);
   setMaximum((setting->flags & SD_FLAG_ENFORCE_MAXRANGE) ? setting->max / m_scale : INT_MAX);

   setSingleStep(setting->step / m_scale);

   setValue(*m_value / m_scale);

   connect(this, SIGNAL(valueChanged(int)), this, SLOT(onValueChanged(int)));

   add_sublabel_and_whats_this(this, m_setting);
}

SizeSpinBox::SizeSpinBox(msg_hash_enums enum_idx, unsigned scale, QWidget *parent) :
   SizeSpinBox(menu_setting_find_enum(enum_idx), scale, parent)
{
}

void SizeSpinBox::onValueChanged(int value)
{
   *m_value = value * m_scale;
   setting_generic_handle_change(m_setting);
}

void SizeSpinBox::paintEvent(QPaintEvent *event)
{
   if ((value() * m_scale) != *m_value)
   {
      blockSignals(true);

      setValue(*m_value / m_scale);

      blockSignals(false);
   }

   QSpinBox::paintEvent(event);
}

UIntRadioButton::UIntRadioButton(msg_hash_enums enum_idx, unsigned value, QWidget *parent) :
   QRadioButton(parent)
   ,m_setting(menu_setting_find_enum(enum_idx))
   ,m_target(m_setting->value.target.unsigned_integer)
   ,m_value(value)
{
   char val_s[NAME_MAX_LENGTH];
   unsigned orig_value = *m_setting->value.target.unsigned_integer;

   *m_setting->value.target.unsigned_integer = value;

   m_setting->get_string_representation(m_setting, val_s, sizeof(val_s));

   *m_setting->value.target.unsigned_integer = orig_value;

   setText(val_s);

   if (m_value == orig_value)
      setChecked(true);

   connect(this, SIGNAL(clicked(bool)), this, SLOT(onClicked(bool)));
}

UIntRadioButton::UIntRadioButton(const QString &text, rarch_setting_t *setting, unsigned value, QWidget *parent) :
   QRadioButton(text, parent)
   ,m_setting(setting)
   ,m_target(setting->value.target.unsigned_integer)
   ,m_value(value)
{
   connect(this, SIGNAL(clicked(bool)), this, SLOT(onClicked(bool)));
}

void UIntRadioButton::onClicked(bool)
{
   *m_target = m_value;
   setting_generic_handle_change(m_setting);
}

void UIntRadioButton::paintEvent(QPaintEvent *event)
{
   if (*m_target == m_value)
      setChecked(true);
   else
      setChecked(false);

   QRadioButton::paintEvent(event);
}

UIntRadioButtons::UIntRadioButtons(rarch_setting_t *setting, QWidget *parent) :
   QGroupBox(setting->short_description, parent)
   ,m_setting(setting)
   ,m_value(setting->value.target.unsigned_integer)
   ,m_buttonGroup(new QButtonGroup(this))
{
   QVBoxLayout *layout = new QVBoxLayout(this);
   /* from menu_displaylist */
   float i;
   unsigned orig_value = *setting->value.target.unsigned_integer;
   float          step = setting->step;
   float           min = (setting->flags & SD_FLAG_ENFORCE_MINRANGE) ? setting->min : 0.00f;
   float           max = (setting->flags & SD_FLAG_ENFORCE_MAXRANGE) ? setting->max : UINT_MAX;
   bool  checked_found = false;

   if (setting->get_string_representation)
   {
      for (i = min; i <= max; i += step)
      {
         char val_s[NAME_MAX_LENGTH];

         *setting->value.target.unsigned_integer = i;

         setting->get_string_representation(setting, val_s, sizeof(val_s));

         QRadioButton *button = new QRadioButton(QString(val_s), this);

         m_buttonGroup->addButton(button, i);

         layout->addWidget(button);

         if (!checked_found && i == orig_value)
         {
            button->setChecked(true);
            checked_found = true;
         }
      }

      *setting->value.target.unsigned_integer = orig_value;
   }
   add_sublabel_and_whats_this(this, m_setting);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
   connect(m_buttonGroup, &QButtonGroup::idClicked, this, &UIntRadioButtons::onButtonClicked);
#else
   connect(m_buttonGroup, SIGNAL(buttonClicked(int)), this, SLOT(onButtonClicked(int)));
#endif
}

UIntRadioButtons::UIntRadioButtons(msg_hash_enums enum_idx, QWidget *parent) :
    UIntRadioButtons(menu_setting_find_enum(enum_idx), parent)
{
}

void UIntRadioButtons::onButtonClicked(int id)
{
   *m_value = id;

   setting_generic_handle_change(m_setting);
}

IntSpinBox::IntSpinBox(rarch_setting_t *setting, QWidget *parent) :
   QSpinBox(parent)
   ,m_setting(setting)
   ,m_value(setting->value.target.integer)
{
   setMinimum((setting->flags & SD_FLAG_ENFORCE_MINRANGE) ? setting->min : INT_MIN);
   setMaximum((setting->flags & SD_FLAG_ENFORCE_MAXRANGE) ? setting->max : INT_MAX);

   setSingleStep(setting->step);

   connect(this, SIGNAL(valueChanged(int)), this, SLOT(onValueChanged(int)));

   add_sublabel_and_whats_this(this, m_setting);
}

void IntSpinBox::onValueChanged(int value)
{
   *m_value = value;
   setting_generic_handle_change(m_setting);
}

void IntSpinBox::paintEvent(QPaintEvent *event)
{
   if (value() != *m_value)
   {
      blockSignals(true);
      setValue(*m_value);
      blockSignals(false);
   }

   QSpinBox::paintEvent(event);
}

FloatSpinBox::FloatSpinBox(msg_hash_enums enum_idx, QWidget *parent) :
    FloatSpinBox(menu_setting_find_enum(enum_idx), parent)
{
}

const QRegularExpression FloatSpinBox::DECIMALS_REGEX = QRegularExpression("%.(\\d)f");

FloatSpinBox::FloatSpinBox(rarch_setting_t *setting, QWidget *parent) :
   QDoubleSpinBox(parent)
   ,m_setting(setting)
   ,m_value(setting->value.target.fraction)
{
   QRegularExpressionMatch match = DECIMALS_REGEX.match(setting->rounding_fraction);

   if (match.hasMatch())
      setDecimals(match.captured(1).toInt());

   setMinimum((setting->flags & SD_FLAG_ENFORCE_MINRANGE) ? setting->min : 0.00f);
   setMaximum((setting->flags & SD_FLAG_ENFORCE_MAXRANGE) ? setting->max :
         999.00f);

   setValue(*m_value);
   setSingleStep(setting->step);

   connect(this, SIGNAL(valueChanged(double)), this, SLOT(onValueChanged(double)));
   add_sublabel_and_whats_this(this, m_setting);
}

void FloatSpinBox::onValueChanged(double value)
{
   *m_value = (float)value;

   setting_generic_handle_change(m_setting);
}

void FloatSpinBox::paintEvent(QPaintEvent *event)
{
   if ((float)value() != *m_value)
   {
      blockSignals(true);

      if (*m_value < minimum() || *m_value > maximum())
         *m_value = m_setting->default_value.fraction;

      setValue(*m_value);

      blockSignals(false);
   }

   QDoubleSpinBox::paintEvent(event);
}

PathButton::PathButton(rarch_setting_t *setting, QWidget *parent) :
   QPushButton("Browse...", parent)
   ,m_setting(setting)
   ,m_value(setting->value.target.string)
   ,m_dir(setting->default_value.string)
{
   connect(this, SIGNAL(clicked(bool)), this, SLOT(onClicked(bool)));

   add_sublabel_and_whats_this(this, m_setting);
}

QString PathButton::currentPath()
{
   QString current(m_setting->value.target.string);
   if (current.isEmpty())
      current = m_setting->default_value.string;
   return current;
}

void DirectoryButton::onClicked(bool)
{
   QString dir = QFileDialog::getExistingDirectory(
      this,
      "Choose " + QString(m_setting->short_description) + " Directory",
      currentPath());

   if (!dir.isNull())
   {
      strlcpy(m_setting->value.target.string, QDir::toNativeSeparators(dir).toUtf8().data(), m_setting->size);

      setting_generic_handle_change(m_setting);
   }

   emit changed();
}

void FileButton::onClicked(bool)
{
   QString file = QFileDialog::getOpenFileName(
      this,
      "Choose File",
      currentPath(),
      QString(m_setting->short_description) + " (*." + QString(m_setting->values) + ")");

   if (!file.isNull())
   {
      strlcpy(m_setting->value.target.string, QDir::toNativeSeparators(file).toUtf8().data(), m_setting->size);

      setting_generic_handle_change(m_setting);
   }

   emit changed();
}

DirectorySelector::DirectorySelector(rarch_setting_t *setting, QWidget *parent) :
   QHBoxLayout(parent)
   ,m_lineEdit(new StringLineEdit(setting))
   ,m_button(new DirectoryButton(setting))
{
   addWidget(m_lineEdit);
   addWidget(m_button);

   connect(m_button, SIGNAL(changed()), m_lineEdit, SLOT(update()));
}

FileSelector::FileSelector(rarch_setting_t *setting, QWidget *parent) :
   QHBoxLayout(parent)
   ,m_lineEdit(new StringLineEdit(setting))
   ,m_button(new FileButton(setting))

{
   addWidget(m_lineEdit);
   addWidget(m_button);

   connect(m_button, SIGNAL(changed()), m_lineEdit, SLOT(update()));
}

FloatSlider::FloatSlider(rarch_setting_t *setting, QWidget *parent) :
   QSlider(Qt::Horizontal, parent)
   ,m_setting(setting)
   ,m_value(setting->value.target.fraction)
   ,m_decimalsRegEx("%.(\\d)f")
{
   QRegularExpressionMatch match = m_decimalsRegEx.match(setting->rounding_fraction);

   if (match.hasMatch())
      m_precision = pow(10, match.captured(1).toInt());
   else
      m_precision = 10 * 10 * 10;

   setMinimum((setting->flags & SD_FLAG_ENFORCE_MINRANGE) ? setting->min *
         m_precision : 0.00f * m_precision);
   setMaximum((setting->flags & SD_FLAG_ENFORCE_MAXRANGE) ? setting->max *
         m_precision : 999.00f * m_precision);

   setSingleStep(setting->step * m_precision);

   setValue(*m_value * m_precision);

   connect(this, SIGNAL(valueChanged(int)), this, SLOT(onValueChanged(int)));

   add_sublabel_and_whats_this(this, m_setting);
}

void FloatSlider::onValueChanged(int value)
{
   *m_value = (float)value / m_precision;

   setting_generic_handle_change(m_setting);
}

void FloatSlider::paintEvent(QPaintEvent *event)
{
   if (value() / m_precision != *m_value)
   {
      blockSignals(true);

      setValue(*m_value * m_precision);

      blockSignals(false);
   }

   QSlider::paintEvent(event);
}

FloatSliderAndSpinBox::FloatSliderAndSpinBox(msg_hash_enums enum_idx, QWidget *parent) :
   FloatSliderAndSpinBox(menu_setting_find_enum(enum_idx), parent)
{
}

FloatSliderAndSpinBox::FloatSliderAndSpinBox(rarch_setting_t *setting, QWidget *parent) :
   QHBoxLayout(parent)
   ,m_slider(new FloatSlider(setting))
   ,m_spinBox(new FloatSpinBox(setting))
{
   addWidget(m_slider);
   addWidget(m_spinBox);

   connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(onSliderValueChanged(int)));
   connect(m_spinBox, SIGNAL(valueChanged(double)), this, SLOT(onSpinBoxValueChanged(double)));
}

void FloatSliderAndSpinBox::onSliderValueChanged(int value)
{
   Q_UNUSED(value);

   m_spinBox->update();
}

void FloatSliderAndSpinBox::onSpinBoxValueChanged(double value)
{
   Q_UNUSED(value);

   m_slider->update();
}

BindButton::BindButton(rarch_setting_t *setting, QWidget *parent) :
   QPushButton(parent)
   ,m_setting(setting)
{
   char val_s[NAME_MAX_LENGTH];

   setting->get_string_representation(setting, val_s, sizeof(val_s));

   setText(val_s);

   connect(this, SIGNAL(clicked(bool)), this, SLOT(onClicked(bool)));
}

BindButton::BindButton(msg_hash_enums enum_idx, QWidget *parent) :
   BindButton(menu_setting_find_enum(enum_idx), parent)
{
}

void BindButton::onClicked(bool checked)
{
   Q_UNUSED(checked);

   m_setting->action_ok(m_setting, 0, false);
}

ColorButton::ColorButton(rarch_setting_t *red, rarch_setting_t *green, rarch_setting_t *blue, QWidget *parent) :
   QToolButton(parent)
   ,m_red(red)
   ,m_green(green)
   ,m_blue(blue)
   ,m_dialog(new QColorDialog(this))
{
   setMinimumSize(64, 0);

   m_dialog->setOptions(QColorDialog::NoButtons);

   connect(this, SIGNAL(clicked()), m_dialog, SLOT(open()));
   connect(m_dialog, SIGNAL(currentColorChanged(const QColor&)), this, SLOT(onColorChanged(const QColor&)));
}

ColorButton::ColorButton(msg_hash_enums red, msg_hash_enums green, msg_hash_enums blue, QWidget *parent) :
   ColorButton(menu_setting_find_enum(red), menu_setting_find_enum(green), menu_setting_find_enum(blue), parent)
{
}

/* Stolen from Qt Creator's QtColorButton */
void ColorButton::paintEvent(QPaintEvent *event)
{
   const int pixSize = 10;
   const int    corr = 5;
   const QColor frameColor1(0, 0, 0, 26);
   const QColor frameColor2(0, 0, 0, 51);

   QToolButton::paintEvent(event);
   if (!isEnabled())
      return;

   QBrush br(color());

   QPainter p(this);
   QRect r = rect().adjusted(corr, corr, -corr, -corr);
   p.setBrushOrigin((r.width() % pixSize + pixSize) / 2 + corr, (r.height() % pixSize + pixSize) / 2 + corr);
   p.fillRect(r, br);

   p.setPen(frameColor1);
   p.drawRect(r.adjusted(1, 1, -2, -2));
   p.setPen(frameColor2);
   p.drawRect(r.adjusted(0, 0, -1, -1));
}

UIntColorButton::UIntColorButton(msg_hash_enums red, msg_hash_enums green, msg_hash_enums blue, QWidget *parent) :
   ColorButton(red, green, blue, parent)
{
}

UIntColorButton::UIntColorButton(rarch_setting_t *red, rarch_setting_t *green, rarch_setting_t *blue, QWidget *parent) :
   ColorButton(red, green, blue, parent)
{
}

void UIntColorButton::onColorChanged(const QColor &color)
{
   if (color.isValid())
   {
      *m_red->value.target.unsigned_integer   = color.red();
      *m_green->value.target.unsigned_integer = color.green();
      *m_blue->value.target.unsigned_integer  = color.blue();
   }
}

QColor UIntColorButton::color()
{
   return QColor(
      *m_red->value.target.unsigned_integer,
      *m_green->value.target.unsigned_integer,
      *m_blue->value.target.unsigned_integer);
}

FloatColorButton::FloatColorButton(msg_hash_enums red, msg_hash_enums green, msg_hash_enums blue, QWidget *parent) :
   ColorButton(red, green, blue, parent)
{
}

void FloatColorButton::onColorChanged(const QColor &color)
{
   if (color.isValid())
   {
      *m_red->value.target.fraction   = color.red() / 255.0f;
      *m_green->value.target.fraction = color.green() / 255.0f;
      *m_blue->value.target.fraction  = color.blue() / 255.0f;
   }
}

QColor FloatColorButton::color()
{
   return QColor(
      *m_red->value.target.fraction * 255,
      *m_green->value.target.fraction * 255,
      *m_blue->value.target.fraction * 255);
}
#endif

FileDropWidget::FileDropWidget(QWidget *parent) :
   QStackedWidget(parent)
{
   setAcceptDrops(true);
}

void FileDropWidget::paintEvent(QPaintEvent *event)
{
   QStyleOption o;
   QPainter p;
   o.initFrom(this);
   p.begin(this);
   style()->drawPrimitive(
      QStyle::PE_Widget, &o, &p, this);
   p.end();

   QWidget::paintEvent(event);
}

void FileDropWidget::keyPressEvent(QKeyEvent *event)
{
   if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
   {
      event->accept();
      emit enterPressed();
   }
   else if (event->key() == Qt::Key_Delete)
   {
      event->accept();
      emit deletePressed();
   }
   else
      QWidget::keyPressEvent(event);
}

void FileDropWidget::dragEnterEvent(QDragEnterEvent *event)
{
   const QMimeData *data = event->mimeData();

   if (data->hasUrls())
      event->acceptProposedAction();
}

/* Workaround for QTBUG-72844. Without it, you can't drop on this if you first drag over another widget that doesn't accept drops. */
void FileDropWidget::dragMoveEvent(QDragMoveEvent *event)
{
   event->acceptProposedAction();
}

void FileDropWidget::dropEvent(QDropEvent *event)
{
   const QMimeData *data = event->mimeData();

   if (data->hasUrls())
   {
      QList<QUrl> urls = data->urls();
      QStringList files;
      int i;

      for (i = 0; i < urls.count(); i++)
      {
         QString path(urls.at(i).toLocalFile());

         files.append(path);
      }

      emit filesDropped(files);
   }
}

void MainWindow::onFileDropWidgetContextMenuRequested(const QPoint &pos)
{
   QScopedPointer<QMenu> menu;
   QScopedPointer<QAction> downloadThumbnailAction;
   QScopedPointer<QAction> addEntryAction;
   QScopedPointer<QAction> addFilesAction;
   QScopedPointer<QAction> addFolderAction;
   QScopedPointer<QAction> editAction;
   QScopedPointer<QAction> deleteAction;
   QPointer<QAction> selectedAction;
   QPoint                    cursorPos = QCursor::pos();
   QHash<QString, QString> contentHash = getCurrentContentHash();
   bool                specialPlaylist = currentPlaylistIsSpecial();
   bool                    allPlaylist = currentPlaylistIsAll();
   bool                   actionsAdded = false;

   menu.reset(new QMenu(this));

   if (!specialPlaylist)
   {
      downloadThumbnailAction.reset(new QAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_THUMBNAIL)), this));
      menu->addAction(downloadThumbnailAction.data());
   }

   if (!allPlaylist)
   {
      addEntryAction.reset(new QAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ADD_ENTRY)), this));
      menu->addAction(addEntryAction.data());

      addFilesAction.reset(new QAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ADD_FILES)), this));
      menu->addAction(addFilesAction.data());

      addFolderAction.reset(new QAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ADD_FOLDER)), this));
      menu->addAction(addFolderAction.data());

      editAction.reset(new QAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_EDIT)), this));
      deleteAction.reset(new QAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_DELETE)), this));

      if (!contentHash.isEmpty())
      {
         menu->addAction(editAction.data());
         menu->addAction(deleteAction.data());
      }

      actionsAdded = true;
   }

   if (actionsAdded)
      selectedAction = menu->exec(cursorPos);

   if (!selectedAction)
      return;

   if (!specialPlaylist)
   {
      if (selectedAction == downloadThumbnailAction.data())
      {
         QHash<QString, QString> hash = getCurrentContentHash();
         QString system = QFileInfo(getCurrentPlaylistPath()).completeBaseName();
         QString title  = hash.value("label");

         if (!title.isEmpty())
         {
            if (m_pendingThumbnailDownloadTypes.isEmpty())
            {
               m_pendingThumbnailDownloadTypes.append(THUMBNAIL_BOXART);
               m_pendingThumbnailDownloadTypes.append(THUMBNAIL_SCREENSHOT);
               m_pendingThumbnailDownloadTypes.append(THUMBNAIL_TITLE);
               downloadThumbnail(system, title);
            }
            else
            {
               showMessageBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALREADY_IN_PROGRESS), MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
            }
         }
      }
   }

   if (!allPlaylist)
   {
      if (selectedAction == addFilesAction.data())
      {
         QStringList filePaths = QFileDialog::getOpenFileNames(
               this,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SELECT_FILES));

         if (!filePaths.isEmpty())
            addFilesToPlaylist(filePaths);
      }
      else if (selectedAction == addEntryAction.data())
         addFilesToPlaylist(QStringList());
      else if (selectedAction == addFolderAction.data())
      {
         QString dirPath = QFileDialog::getExistingDirectory(
               this,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SELECT_FOLDER),
               QString(), QFileDialog::ShowDirsOnly);

         if (!dirPath.isEmpty())
            addFilesToPlaylist(QStringList() << dirPath);
      }
      else if (selectedAction == editAction.data())
      {
         QString selectedDatabase;
         QString selectedName;
         QString selectedPath;
         QHash<QString, QString> selectedCore;
         PlaylistEntryDialog *playlistDialog = playlistEntryDialog();
         QString         currentPlaylistPath = getCurrentPlaylistPath();

         if (!playlistDialog->showDialog(contentHash))
            return;

         selectedName     = m_playlistEntryDialog->getSelectedName();
         selectedPath     = m_playlistEntryDialog->getSelectedPath();
         selectedCore     = m_playlistEntryDialog->getSelectedCore();
         selectedDatabase = m_playlistEntryDialog->getSelectedDatabase();

         if (selectedCore.isEmpty())
         {
            selectedCore["core_name"] = "DETECT";
            selectedCore["core_path"] = "DETECT";
         }

         if (selectedDatabase.isEmpty())
            selectedDatabase = QFileInfo(currentPlaylistPath).fileName().remove(".lpl");

         contentHash["label"]     = selectedName;
         contentHash["path"]      = selectedPath;
         contentHash["core_name"] = selectedCore.value("core_name");
         contentHash["core_path"] = selectedCore.value("core_path");
         contentHash["db_name"]   = selectedDatabase;

         if (!updateCurrentPlaylistEntry(contentHash))
         {
            showMessageBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY), MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
            return;
         }
      }
      else if (selectedAction == deleteAction.data())
         deleteCurrentPlaylistItem();
   }
}
