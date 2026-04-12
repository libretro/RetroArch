#include <QAction>
#include <QApplication>
#include <QBitmap>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QColor>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLayout>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPaintEvent>
#include <QProgressDialog>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollBar>
#include <QSettings>
#include <QStackedLayout>
#include <QStyle>
#include <QStyleOption>
#include <QTimer>

#include "ui_qt_widgets.h"
#include "ui_qt.h"

#ifndef CXX_BUILD
extern "C" {
#endif

#include <math.h>

#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include <file/archive_file.h>
#include <lists/string_list.h>

#include "../../config.def.h"
#include "../../command.h"
#include "../../core_info.h"
#include "../../core_option_manager.h"
#include "../../file_path_special.h"
#include "../../msg_hash.h"
#include "../../paths.h"
#include "../../tasks/tasks_internal.h"
#include "../../verbosity.h"
#include "../../version.h"
#include "../../gfx/video_display_server.h"
#include "../../input/input_driver.h"
#include "../../input/input_remapping.h"
#include "../../network/netplay/netplay.h"
#include "../../playlist.h"

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#include "../../menu/menu_entries.h"
#include "../../menu/menu_displaylist.h"
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#include "../../menu/menu_shader.h"
#endif
#endif

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

FormLayout::FormLayout(QWidget *parent) : QFormLayout(parent) { }

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

void FormLayout::addUIntColorButton(const QString &title,
	msg_hash_enums r, msg_hash_enums g, msg_hash_enums b)
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
   ,m_layout(new FormLayout(this)) { }

SettingsGroup::SettingsGroup(QWidget *parent) :
   QGroupBox(parent)
   ,m_layout(new FormLayout(this)) { }

void SettingsGroup::addWidget(QWidget *widget) { m_layout->addWidget(widget); }

void SettingsGroup::addRow(QString label, QWidget *widget)
{
   m_layout->addRow(label, widget);
}

void SettingsGroup::addRow(QWidget *widget) { m_layout->addRow(widget); }
void SettingsGroup::addRow(QLayout *layout) { m_layout->addRow(layout); }

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

void SettingsGroup::addUIntColorButton(const QString &title,
	msg_hash_enums r, msg_hash_enums g, msg_hash_enums b)
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
   CheckableSettingsGroup(menu_setting_find_enum(enum_idx), parent) { }

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
   CheckableIcon(menu_setting_find_enum(enum_idx), icon, parent) { }

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

   connect(this, SIGNAL(currentTextChanged(const QString&)), this,
		 SLOT(onCurrentTextChanged(const QString&)));

   add_sublabel_and_whats_this(this, m_setting);
}

void StringComboBox::onCurrentTextChanged(const QString &text)
{
   strlcpy(m_value, text.toUtf8().data(), m_setting->size);

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
   UIntComboBox(menu_setting_find_enum(enum_idx), min, max, parent) { }

void UIntComboBox::onCurrentIndexChanged(int index)
{
   (void)(index);

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
    UIntSpinBox(menu_setting_find_enum(enum_idx), parent) { }

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
   setMinimum((setting->flags & SD_FLAG_ENFORCE_MINRANGE)
		   ? setting->min / m_scale : 0.00f);
   setMaximum((setting->flags & SD_FLAG_ENFORCE_MAXRANGE)
		   ? setting->max / m_scale : INT_MAX);

   setSingleStep(setting->step / m_scale);

   setValue(*m_value / m_scale);

   connect(this, SIGNAL(valueChanged(int)), this, SLOT(onValueChanged(int)));

   add_sublabel_and_whats_this(this, m_setting);
}

SizeSpinBox::SizeSpinBox(msg_hash_enums enum_idx, unsigned scale, QWidget *parent) :
   SizeSpinBox(menu_setting_find_enum(enum_idx), scale, parent) { }

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

UIntRadioButton::UIntRadioButton(const QString &text,
	rarch_setting_t *setting, unsigned value, QWidget *parent) :
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
    UIntRadioButtons(menu_setting_find_enum(enum_idx), parent) { }

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
   (void)(value);

   m_spinBox->update();
}

void FloatSliderAndSpinBox::onSpinBoxValueChanged(double value)
{
   (void)(value);

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
   (void)(checked);

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

UIntColorButton::UIntColorButton(msg_hash_enums red,
   msg_hash_enums green, msg_hash_enums blue, QWidget *parent) :
   ColorButton(red, green, blue, parent) { }

UIntColorButton::UIntColorButton(rarch_setting_t *red,
   rarch_setting_t *green, rarch_setting_t *blue, QWidget *parent) :
   ColorButton(red, green, blue, parent) { }

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

FloatColorButton::FloatColorButton(msg_hash_enums red,
   msg_hash_enums green, msg_hash_enums blue, QWidget *parent) :
   ColorButton(red, green, blue, parent) { }

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
      *m_red->value.target.fraction   * 255,
      *m_green->value.target.fraction * 255,
      *m_blue->value.target.fraction  * 255);
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
   int key = event->key();
   if (   key == Qt::Key_Return
       || key == Qt::Key_Enter)
   {
      event->accept();
      emit enterPressed();
   }
   else if (key == Qt::Key_Delete)
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

/* Workaround for QTBUG-72844. Without it, you can't drop on this if you
 * first drag over another widget that doesn't accept drops. */
void FileDropWidget::dragMoveEvent(QDragMoveEvent *event)
{
   event->acceptProposedAction();
}

void FileDropWidget::dropEvent(QDropEvent *event)
{
   const QMimeData *data = event->mimeData();

   if (data->hasUrls())
   {
      int i;
      QStringList files;
      QList<QUrl> urls = data->urls();

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
      downloadThumbnailAction.reset(new QAction(
               QString(msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_THUMBNAIL)), this));
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
               m_pendingThumbnailDownloadTypes.append(THUMBNAIL_LOGO);
               downloadThumbnail(system, title);
            }
            else
            {
               showMessageBox(msg_hash_to_str(
			MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALREADY_IN_PROGRESS),
			       MainWindow::MSGBOX_TYPE_ERROR,
			       Qt::ApplicationModal, false);
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
         QString dirPath = QFileDialog::getExistingDirectory(this,
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
            showMessageBox(msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY),
                  MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
            return;
         }
      }
      else if (selectedAction == deleteAction.data())
         deleteCurrentPlaylistItem();
   }
}

/* http://www.informit.com/articles/article.aspx?p=1613548 */

ThumbnailDelegate::ThumbnailDelegate(const GridItem &gridItem, QObject* parent) :
   QStyledItemDelegate(parent), m_style(gridItem) { }

void ThumbnailDelegate::paint(QPainter* painter,
      const QStyleOptionViewItem &option, const QModelIndex& index) const
{
   QStyleOptionViewItem opt = option;
   const QWidget    *widget = opt.widget;
   QStyle            *style = widget->style();
   int              padding = m_style.padding;
   int      text_top_margin = 4; /* Qt seemingly reports -4 the actual line height. */
   int          text_height = painter->fontMetrics().height() + padding + padding;
   QRect               rect = opt.rect;
   QRect           adjusted = rect.adjusted(padding, padding, -padding,
         -text_height + text_top_margin);
   QPixmap           pixmap = index.data(
         PlaylistModel::THUMBNAIL).value<QPixmap>();

   painter->save();

   initStyleOption(&opt, index);

   /* draw the background */
   style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, widget);

   /* draw the image */
   if (!pixmap.isNull())
   {
      QPixmap pixmapScaled = pixmap.scaled(adjusted.size(),
            Qt::KeepAspectRatio, Qt::SmoothTransformation);
      style->drawItemPixmap(painter, adjusted,
            Qt::AlignHCenter | m_style.thumbnailVerticalAlignmentFlag,
            pixmapScaled);
   }

   /* draw the text */
   if (!opt.text.isEmpty())
   {
      QPalette::ColorGroup cg = opt.state & QStyle::State_Enabled
         ? QPalette::Normal : QPalette::Disabled;
      QRect textRect          = QRect(rect.x() + padding,
            rect.y() + adjusted.height() - text_top_margin + padding,
            rect.width() - 2 * padding, text_height);
      QString elidedText      = painter->fontMetrics().elidedText(opt.text,
            opt.textElideMode, textRect.width(), Qt::TextShowMnemonic);

      if (cg == QPalette::Normal && !(opt.state & QStyle::State_Active))
         cg = QPalette::Inactive;

      if (opt.state & QStyle::State_Selected)
         painter->setPen(opt.palette.color(cg, QPalette::HighlightedText));
      else
         painter->setPen(opt.palette.color(cg, QPalette::Text));

      painter->setFont(opt.font);
      painter->drawText(textRect, Qt::AlignCenter, elidedText);
   }

   painter->restore();
}

GridView::GridView(QWidget *parent) :
   QAbstractItemView(parent), m_idealHeight(0), m_hashIsDirty(false)
{
   setFocusPolicy(Qt::WheelFocus);
   horizontalScrollBar()->setRange(0, 0);
   verticalScrollBar()->setRange(0, 0);
}

void GridView::setModel(QAbstractItemModel *newModel)
{
   if (model())
      disconnect(model(), SIGNAL(rowsRemoved(QModelIndex, int, int)),
            this, SLOT(rowsRemoved(QModelIndex, int, int)));

   QAbstractItemView::setModel(newModel);

   connect(newModel, SIGNAL(rowsRemoved(QModelIndex, int, int)),
         this, SLOT(rowsRemoved(QModelIndex, int, int)));

   m_hashIsDirty = true;
}

void GridView::setviewMode(ViewMode mode)
{
   m_viewMode = mode;
}

void GridView::calculateRectsIfNecessary() const
{
   int x, y;
   int row, nextX;
   if (!m_hashIsDirty)
      return;

   x                  = m_spacing;
   y                  = m_spacing;
   const int maxWidth = viewport()->width();

   if (m_size + m_spacing * 2 > maxWidth)
   {
      m_rectForRow[0] = QRectF(x, y, m_size, m_size);

      for (row = 1; row < model()->rowCount(); ++row)
      {
         y                 += m_size + m_spacing;
         m_rectForRow[row]  = QRectF(x, y, m_size, m_size);
      }
   }
   else
   {
      switch (m_viewMode)
      {
         case Anchored:
         {
            int columns = (maxWidth - m_spacing) / (m_size + m_spacing);
            if (columns > 0)
            {
               const int actualSpacing = (maxWidth - m_spacing -
                     m_size - (columns - 1) * m_size) / columns;
               for (row = 0; row < model()->rowCount(); ++row)
               {
                  nextX = x + m_size + actualSpacing;
                  if (nextX > maxWidth)
                  {
                     x = m_spacing;
                     y += m_size + m_spacing;
                     nextX = x + m_size + actualSpacing;
                  }
                  m_rectForRow[row] = QRectF(x, y, m_size, m_size);
                  x = nextX;
               }
            }
            break;
         }
         case Centered:
         {
            int columns = (maxWidth - m_spacing) / (m_size + m_spacing);
            if (columns > 0)
            {
               const int actualSpacing = (maxWidth - columns * m_size)
                  / (columns + 1);
               x                       = actualSpacing;

               for (row = 0; row < model()->rowCount(); ++row)
               {
                  nextX = x + m_size + actualSpacing;
                  if (nextX > maxWidth)
                  {
                     x = actualSpacing;
                     y += m_size + m_spacing;
                     nextX = x + m_size + actualSpacing;
                  }
                  m_rectForRow[row] = QRectF(x, y, m_size, m_size);
                  x = nextX;
               }
            }
            break;
         }
         case Simple:
            for (row = 0; row < model()->rowCount(); ++row)
            {
               nextX = x + m_size + m_spacing;
               if (nextX > maxWidth)
               {
                  x = m_spacing;
                  y += m_size + m_spacing;
                  nextX = x + m_size + m_spacing;
               }
               m_rectForRow[row] = QRectF(x, y, m_size, m_size);
               x = nextX;
            }
            break;
         }
   }

   m_idealHeight = y + m_size + m_spacing;
   m_hashIsDirty = false;
   viewport()->update();
}

QRect GridView::visualRect(const QModelIndex &index) const
{
   QRect rect;
   if (index.isValid())
      rect = viewportRectForRow(index.row()).toRect();
   return rect;
}

QRectF GridView::viewportRectForRow(int row) const
{
   QRectF rect;
   calculateRectsIfNecessary();
   rect = m_rectForRow.value(row).toRect();
   if (!rect.isValid())
      return rect;
   return QRectF(
         rect.x() - horizontalScrollBar()->value(),
         rect.y() - verticalScrollBar()->value(),
         rect.width(),
         rect.height());
}

void GridView::scrollTo(const QModelIndex &index, QAbstractItemView::ScrollHint)
{
   QRect viewRect = viewport()->rect();
   QRect itemRect = visualRect(index);

   if (itemRect.left() < viewRect.left())
      horizontalScrollBar()->setValue(horizontalScrollBar()->value()
            + itemRect.left() - viewRect.left());
   else if (itemRect.right() > viewRect.right())
      horizontalScrollBar()->setValue(horizontalScrollBar()->value()
            + ((itemRect.right() - viewRect.right()) < (itemRect.left() - viewRect.left()) ? (itemRect.right() - viewRect.right()) : (itemRect.left() - viewRect.left())));
   if (itemRect.top() < viewRect.top())
      verticalScrollBar()->setValue(verticalScrollBar()->value()
            + itemRect.top() - viewRect.top());
   else if (itemRect.bottom() > viewRect.bottom())
      verticalScrollBar()->setValue(verticalScrollBar()->value()
            + ((itemRect.bottom() - viewRect.bottom()) < (itemRect.top() - viewRect.top()) ? (itemRect.bottom() - viewRect.bottom()) : (itemRect.top() - viewRect.top())));
   viewport()->update();
}

/* TODO: Make this more efficient by changing m_rectForRow for
 * another data structure. Look at how Qt's own views do it. */
QModelIndex GridView::indexAt(const QPoint &point_) const
{
   QPoint point(point_);
   QHash<int, QRectF>::const_iterator i;
   point.rx() += horizontalScrollBar()->value();
   point.ry() += verticalScrollBar()->value();

   calculateRectsIfNecessary();

   i = m_rectForRow.constBegin();

   while (i != m_rectForRow.constEnd())
   {
      if (i.value().contains(point))
         return model()->index(i.key(), 0, rootIndex());
      i++;
   }
   return QModelIndex();
}

void GridView::dataChanged(const QModelIndex &topLeft,
      const QModelIndex &bottomRight, const QVector<int> &roles)
{
   m_hashIsDirty = true;
   QAbstractItemView::dataChanged(topLeft, bottomRight);
}

void GridView::refresh()
{
   m_hashIsDirty = true;
   calculateRectsIfNecessary();
   updateGeometries();
}

void GridView::rowsInserted(const QModelIndex &parent, int start, int end)
{
   QAbstractItemView::rowsInserted(parent, start, end);
   refresh();
}

void GridView::rowsRemoved(const QModelIndex &parent, int start, int end)
{
   refresh();
}

void GridView::setGridSize(const int newSize)
{
   if (newSize != m_size)
   {
      m_size = newSize;
      refresh();
   }
}

void GridView::resizeEvent(QResizeEvent*)
{
   refresh();
}

void GridView::reset()
{
   m_visibleIndexes.clear();
   QAbstractItemView::reset();
   refresh();
}

QModelIndex GridView::moveCursor(QAbstractItemView::CursorAction cursorAction,
      Qt::KeyboardModifiers)
{
   QModelIndex index = currentIndex();
   if (index.isValid())
   {
      if (     (cursorAction == MoveLeft  && index.row() > 0)
            || (cursorAction == MoveRight && index.row() + 1 < model()->rowCount()))
      {
         const int offset = (cursorAction == MoveLeft ? -1 : 1);
         index = model()->index(index.row() + offset, index.column(), index.parent());
      }
      else if ((cursorAction == MoveUp   && index.row() > 0)
            || (cursorAction == MoveDown && index.row() + 1 < model()->rowCount()))
      {
         const int offset = ((m_size + m_spacing) * (cursorAction == MoveUp ? -1 : 1));
         QRect rect = viewportRectForRow(index.row()).toRect();
         QPoint point(rect.center().x(), rect.center().y() + offset);
         index = indexAt(point);
      }
   }
   return index;
}

int GridView::horizontalOffset() const
{
   return horizontalScrollBar()->value();
}

int GridView::verticalOffset() const
{
   return verticalScrollBar()->value();
}

void GridView::scrollContentsBy(int dx, int dy)
{
   scrollDirtyRegion(dx, dy);
   viewport()->scroll(dx, dy);
   emit(visibleItemsChangedMaybe());
}

/* TODO: Maybe add a way to get the previous/next visible indexes. */
QVector<QModelIndex> GridView::visibleIndexes() const {
   return m_visibleIndexes;
}

void GridView::setSelection(const QRect &rect,
      QFlags<QItemSelectionModel::SelectionFlag> flags)
{
   QRect rectangle;
   QHash<int, QRectF>::const_iterator i;
   int firstRow = model()->rowCount();
   int lastRow  = -1;

   calculateRectsIfNecessary();

   rectangle    = rect.translated(horizontalScrollBar()->value(),
         verticalScrollBar()->value()).normalized();

   i = m_rectForRow.constBegin();

   while (i != m_rectForRow.constEnd())
   {
      if (i.value().intersects(rectangle))
      {
         firstRow = firstRow < i.key() ? firstRow : i.key();
         lastRow = lastRow > i.key() ? lastRow : i.key();
      }
      i++;
   }
   if (firstRow != model()->rowCount() && lastRow != -1)
   {
      QItemSelection selection(model()->index(
               firstRow, 0, rootIndex()),
            model()->index(lastRow, 0, rootIndex()));
      selectionModel()->select(selection, flags);
   }
   else
   {
      QModelIndex invalid;
      QItemSelection selection(invalid, invalid);
      selectionModel()->select(selection, flags);
   }
}

QRegion GridView::visualRegionForSelection(const QItemSelection &selection) const
{
   int i;
   QRegion region;
   QItemSelectionRange range;

   for (i = 0; i < selection.size(); i++)
   {
      range = selection.at(i);
      int row;
      for (row = range.top(); row <= range.bottom(); row++)
      {
         int column;
         for (column = range.left(); column < range.right(); column++)
         {
            QModelIndex index = model()->index(row, column, rootIndex());
            region += visualRect(index);
         }
      }
   }

   return region;
}

void GridView::paintEvent(QPaintEvent*)
{
   QPainter painter(viewport());
   int row;

   painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
   m_visibleIndexes.clear();

   for (row = 0; row < model()->rowCount(rootIndex()); ++row)
   {
      QModelIndex index = model()->index(row, 0, rootIndex());
      QRectF rect = viewportRectForRow(row);
#if (QT_VERSION > QT_VERSION_CHECK(6, 0, 0))
      QStyleOptionViewItem option;
      initViewItemOption(&option);
#else
      QStyleOptionViewItem option = viewOptions();
#endif

      if (    !rect.isValid()
            || rect.bottom() < 0
            || rect.y() > viewport()->height())
         continue;

      m_visibleIndexes.append(index);
      option.rect = rect.toRect();

      if (selectionModel()->isSelected(index))
         option.state |= QStyle::State_Selected;

      if (currentIndex() == index)
         option.state |= QStyle::State_HasFocus;

      itemDelegate()->paint(&painter, option, index);
   }
}

void GridView::updateGeometries()
{
   const int RowHeight = m_size + m_spacing;

   QAbstractItemView::updateGeometries();

   verticalScrollBar()->setSingleStep(RowHeight);
   verticalScrollBar()->setPageStep(viewport()->height());
   verticalScrollBar()->setRange(0, ((0) > (m_idealHeight - viewport()->height()) ? (0) : (m_idealHeight - viewport()->height())));

   horizontalScrollBar()->setPageStep(viewport()->width());
   horizontalScrollBar()->setRange(0, ((0) > (RowHeight - viewport()->width()) ? (0) : (RowHeight - viewport()->width())));

   emit(visibleItemsChangedMaybe());
}

QString GridView::getLayout() const
{
   switch (m_viewMode)
   {
      case Simple:
         return QString("simple");
      case Anchored:
         return QString("anchored");
      case Centered:
      default:
         break;
   }

   return QString("centered");
}

void GridView::setLayout(QString layout)
{
   if (layout == QLatin1String("anchored"))
      m_viewMode = Anchored;
   else if (layout == QLatin1String("centered"))
      m_viewMode = Centered;
   else if (layout == QLatin1String("fixed"))
      m_viewMode = Simple;
}

int GridView::getSpacing() const
{
   return m_spacing;
}

void GridView::setSpacing(const int spacing) { m_spacing = spacing; }

GridItem::GridItem(QWidget* parent) : QWidget(parent)
, thumbnailVerticalAlignmentFlag(Qt::AlignBottom)
, padding(11) { }

int GridItem::getPadding() const { return padding; }
void GridItem::setPadding(const int value) { padding = value; }

QString GridItem::getThumbnailVerticalAlign() const
{
   switch (thumbnailVerticalAlignmentFlag)
   {
      case Qt::AlignTop:
         return QString("top");
      case Qt::AlignVCenter:
         return QString("center");
      case Qt::AlignBottom:
      default:
         break;
   }

   return QString("bottom");
}

void GridItem::setThumbnailVerticalAlign(const QString valign)
{
   if (valign == QLatin1String("top"))
      thumbnailVerticalAlignmentFlag = Qt::AlignTop;
   else if (valign == QLatin1String("center"))
      thumbnailVerticalAlignmentFlag = Qt::AlignVCenter;
   else if (valign == QLatin1String("bottom"))
      thumbnailVerticalAlignmentFlag = Qt::AlignBottom;
}

#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif

static inline bool comp_string_lower(const QString &lhs, const QString &rhs)
{
   return lhs.toLower() < rhs.toLower();
}

static inline bool comp_hash_ui_display_name_key_lower(const QHash<QString,
		QString> &lhs, const QHash<QString, QString> &rhs)
{
   return    lhs.value("ui_display_name").toLower()
	   < rhs.value("ui_display_name").toLower();
}

PlaylistEntryDialog::PlaylistEntryDialog(MainWindow *mainwindow, QWidget *parent) :
   QDialog(parent)
   ,m_mainwindow(mainwindow)
   ,m_settings(mainwindow->settings())
   ,m_nameLineEdit(new QLineEdit(this))
   ,m_pathLineEdit(new QLineEdit(this))
   ,m_extensionsLineEdit(new QLineEdit(this))
   ,m_coreComboBox(new QComboBox(this))
   ,m_databaseComboBox(new QComboBox(this))
   ,m_extensionArchiveCheckBox(new QCheckBox(
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_FILTER_INSIDE_ARCHIVES),
            this))
{
   QFormLayout *form                = new QFormLayout();
   QDialogButtonBox *buttonBox      = new QDialogButtonBox(
         QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
   QVBoxLayout *databaseVBoxLayout  = new QVBoxLayout();
   QHBoxLayout *pathHBoxLayout      = new QHBoxLayout();
   QHBoxLayout *extensionHBoxLayout = new QHBoxLayout();
   QLabel *databaseLabel            = new QLabel(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_FOR_THUMBNAILS), this);
   QToolButton *pathPushButton      = new QToolButton(this);

   pathPushButton->setText(QString("..."));

   pathHBoxLayout->addWidget(m_pathLineEdit);
   pathHBoxLayout->addWidget(pathPushButton);

   databaseVBoxLayout->addWidget(m_databaseComboBox);
   databaseVBoxLayout->addWidget(databaseLabel);

   extensionHBoxLayout->addWidget(m_extensionsLineEdit);
   extensionHBoxLayout->addWidget(m_extensionArchiveCheckBox);

   m_extensionsLineEdit->setPlaceholderText(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS_PLACEHOLDER));

   /* Ensure placeholder text is completely visible. */
   m_extensionsLineEdit->setMinimumWidth(QFontMetrics(
            m_extensionsLineEdit->font()).boundingRect(
            m_extensionsLineEdit->placeholderText()).width()
          + m_extensionsLineEdit->frameSize().width());

   setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY));

   form->setFormAlignment(Qt::AlignCenter);
   form->setLabelAlignment(Qt::AlignCenter);

   setLayout(new QVBoxLayout(this));

   connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
   connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

   connect(this, SIGNAL(accepted()), this, SLOT(onAccepted()));
   connect(this, SIGNAL(rejected()), this, SLOT(onRejected()));

   form->addRow(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_NAME), m_nameLineEdit);
   form->addRow(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_PATH), pathHBoxLayout);
   form->addRow(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_CORE), m_coreComboBox);
   form->addRow(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE), databaseVBoxLayout);
   form->addRow(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS), extensionHBoxLayout);

   qobject_cast<QVBoxLayout*>(layout())->addLayout(form);
   layout()->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));
   layout()->addWidget(buttonBox);

   connect(pathPushButton, SIGNAL(clicked()), this, SLOT(onPathClicked()));
}

bool PlaylistEntryDialog::filterInArchive()
{
   return m_extensionArchiveCheckBox->isChecked();
}

void PlaylistEntryDialog::onPathClicked()
{
   QString filePath = QFileDialog::getOpenFileName(this);

   if (filePath.isEmpty())
      return;

   m_pathLineEdit->setText(filePath);
}

void PlaylistEntryDialog::loadPlaylistOptions()
{
   unsigned i, j;
   core_info_list_t *core_info_list = NULL;

   m_nameLineEdit->clear();
   m_pathLineEdit->clear();
   m_coreComboBox->clear();
   m_databaseComboBox->clear();

   m_coreComboBox->addItem(
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK));
   m_databaseComboBox->addItem(
           QString("<")
         + msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE)
         + ">",
         QFileInfo(m_mainwindow->getCurrentPlaylistPath()).fileName().remove(".lpl"));

   core_info_get_list(&core_info_list);

   if (core_info_list && core_info_list->count > 0)
   {
      QVector<QHash<QString, QString> > allCores;
      QStringList allDatabases;

      for (i = 0; i < core_info_list->count; i++)
      {
         QString ui_display_name;
         QHash<QString, QString> hash;
         const core_info_t *core = &core_info_list->list[i];
         QStringList databases   = string_split_to_qt(QString(core->databases), '|');

         hash["core_name"]         = core->core_name;
         hash["core_display_name"] = core->display_name;
         hash["core_path"]         = core->path;
         hash["core_databases"]    = core->databases;

         ui_display_name           = hash.value("core_name");

         if (ui_display_name.isEmpty())
            ui_display_name        = hash.value("core_display_name");
         if (ui_display_name.isEmpty())
            ui_display_name        = QFileInfo(
                  hash.value("core_path")).fileName();

         if (ui_display_name.isEmpty())
            continue;

         hash["ui_display_name"] = ui_display_name;

         for (j = 0; static_cast<int>(j) < databases.count(); j++)
         {
            QString database = databases.at(static_cast<int>(j));

            if (database.isEmpty())
               continue;

            if (!allDatabases.contains(database))
               allDatabases.append(database);
         }

         if (!allCores.contains(hash))
            allCores.append(hash);
      }

      std::sort(allCores.begin(), allCores.end(), comp_hash_ui_display_name_key_lower);
      std::sort(allDatabases.begin(), allDatabases.end(), comp_string_lower);

      for (j = 0; static_cast<int>(j) < allCores.count(); j++)
      {
         const QHash<QString, QString> &hash = allCores.at(static_cast<int>(j));

         m_coreComboBox->addItem(hash.value("ui_display_name"), QVariant::fromValue(hash));
      }

      for (j = 0; static_cast<int>(j) < allDatabases.count(); j++)
      {
         QString database = allDatabases.at(static_cast<int>(j));
         m_databaseComboBox->addItem(database, database);
      }
   }
}

bool PlaylistEntryDialog::nameFieldEnabled()
{
   return m_nameLineEdit->isEnabled();
}

void PlaylistEntryDialog::setEntryValues(
      const QHash<QString, QString> &contentHash)
{
   QString db;
   QString coreName = contentHash.value("core_name");
   int foundDB = 0;
   int i       = 0;

   loadPlaylistOptions();

   if (contentHash.isEmpty())
   {
      m_nameLineEdit->setText(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE));
      m_pathLineEdit->setText(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE));
      m_nameLineEdit->setEnabled(false);
      m_pathLineEdit->setEnabled(false);
   }
   else
   {
      m_nameLineEdit->setText(contentHash.value("label"));
      m_pathLineEdit->setText(contentHash.value("path"));
      m_nameLineEdit->setEnabled(true);
      m_pathLineEdit->setEnabled(true);
   }

   for (i = 0; i < m_coreComboBox->count(); i++)
   {
      const QHash<QString, QString> hash =
         m_coreComboBox->itemData(i,
               Qt::UserRole).value<QHash<QString, QString> >();

      if (hash.isEmpty() || coreName.isEmpty())
         continue;

      if (hash.value("core_name") == coreName)
      {
         m_coreComboBox->setCurrentIndex(i);
         break;
      }
   }

   db = contentHash.value("db_name");

   if (!db.isEmpty())
   {
      foundDB = m_databaseComboBox->findText(db);

      if (foundDB >= 0)
         m_databaseComboBox->setCurrentIndex(foundDB);
   }
}

const QHash<QString, QString> PlaylistEntryDialog::getSelectedCore()
{
   return m_coreComboBox->currentData(
         Qt::UserRole).value<QHash<QString, QString> >();
}

const QString PlaylistEntryDialog::getSelectedName()
{
   return m_nameLineEdit->text();
}

const QString PlaylistEntryDialog::getSelectedPath()
{
   return m_pathLineEdit->text();
}

const QString PlaylistEntryDialog::getSelectedDatabase()
{
   return m_databaseComboBox->currentData(Qt::UserRole).toString();
}

const QStringList PlaylistEntryDialog::getSelectedExtensions()
{
   QStringList list;
   QString text = m_extensionsLineEdit->text();

   /* Otherwise it would create a QStringList with a single blank entry... */
   if (!text.isEmpty())
      list   = string_split_to_qt(text, ' ');
   return list;
}

void PlaylistEntryDialog::onAccepted() { }
void PlaylistEntryDialog::onRejected() { }
void PlaylistEntryDialog::hideDialog() { reject(); }

bool PlaylistEntryDialog::showDialog(const QHash<QString, QString> &hash)
{
   loadPlaylistOptions();
   setEntryValues(hash);
   return (exec() == QDialog::Accepted);
}

CoreInfoDialog::CoreInfoDialog(MainWindow *mainwindow, QWidget *parent) :
   QDialog(parent)
   ,m_formLayout(new QFormLayout())
   ,m_mainwindow(mainwindow)
{
   QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);

   connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
   connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

   setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFORMATION));

   m_formLayout->setFormAlignment(Qt::AlignCenter);
   m_formLayout->setLabelAlignment(Qt::AlignCenter);

   setLayout(new QVBoxLayout());

   qobject_cast<QVBoxLayout*>(layout())->addLayout(m_formLayout);
   layout()->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));
   layout()->addWidget(buttonBox);
}

void CoreInfoDialog::showCoreInfo()
{
   int i, row;
   int row_count = m_formLayout->rowCount();
   QVector<QHash<QString, QString> > info_list
                 = m_mainwindow->getCoreInfo();

   if (row_count > 0)
   {
      for (row = 0; row < row_count; row++)
      {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
         /* removeRow() and takeRow() was only added in 5.8! */
         m_formLayout->removeRow(0);
#else
         /* something is buggy here...
          * sometimes items appear duplicated, and other times not */
         QLayoutItem *item = m_formLayout->itemAt(0);
         QWidget        *w = NULL;

         if (item)
         {
            w = item->widget();

            if (w)
            {
               QWidget *label = m_formLayout->labelForField(w);

               if (label)
                  delete label;

               m_formLayout->removeWidget(w);

               delete w;
            }
         }
#endif
      }
   }

   if (info_list.count() == 0)
      return;

   for (i = 0; i < info_list.count(); i++)
   {
      const QHash<QString, QString> &line = info_list.at(i);
      QLabel                       *label = new QLabel(line.value("key"));
      CoreInfoLabel                *value = new CoreInfoLabel(line.value("value"));
      QString                  labelStyle = line.value("label_style");
      QString                  valueStyle = line.value("value_style");

      if (!labelStyle.isEmpty())
         label->setStyleSheet(labelStyle);

      if (!valueStyle.isEmpty())
         value->setStyleSheet(valueStyle);

      m_formLayout->addRow(label, value);
   }

   show();
}

#ifdef HAVE_MENU
QPixmap getColorizedPixmap(const QPixmap& oldPixmap, const QColor& color)
{
   QPixmap pixmap = oldPixmap;
   QBitmap   mask = pixmap.createMaskFromColor(Qt::transparent,
         Qt::MaskInColor);
   pixmap.fill(color);
   pixmap.setMask(mask);
   return pixmap;
}

QColor getLabelColor(const QString& objectName)
{
   QLabel dummyColor;
   dummyColor.setObjectName(objectName);
   dummyColor.ensurePolished();
   return dummyColor.palette().color(QPalette::WindowText);
}

/* stolen from Qt Creator */
class SmartScrollArea : public QScrollArea
{
public:
   explicit SmartScrollArea(QWidget *parent) :
      QScrollArea(parent)
   {
      setFrameStyle(QFrame::NoFrame | QFrame::Plain);
      viewport()->setAutoFillBackground(false);
      setWidgetResizable(true);
   }
private:
   void resizeEvent(QResizeEvent *event) final
   {
      QWidget *inner = widget();

      if (inner)
      {
         int              fw = frameWidth() * 2;
         QSize     innerSize = event->size() - QSize(fw, fw);
         QSize innerSizeHint = inner->minimumSizeHint();

         /* Widget wants to be bigger than available space */
         if (innerSizeHint.height() > innerSize.height())
         {
            innerSize.setWidth(innerSize.width() - scrollBarWidth());
            innerSize.setHeight(innerSizeHint.height());
         }
         inner->resize(innerSize);
      }
      QScrollArea::resizeEvent(event);
   }

   QSize minimumSizeHint() const final
   {
      static const int max_min_width  = 250;
      static const int max_min_height = 250;
      QWidget *inner                  = widget();

      if (inner)
      {
         int        fw                = frameWidth() * 2;
         QSize minSize                = inner->minimumSizeHint();

         minSize                     += QSize(fw, fw);
         minSize                     += QSize(scrollBarWidth(), 0);
         minSize.setWidth(((minSize.width()) < (max_min_width) ? (minSize.width()) : (max_min_width)));
         minSize.setHeight(((minSize.height()) < (max_min_height) ? (minSize.height()) : (max_min_height)));
         return minSize;
      }
      return QSize(0, 0);
   }

   bool event(QEvent *event) final
   {
      if (event->type() == QEvent::LayoutRequest)
         updateGeometry();
      return QScrollArea::event(event);
   }

   int scrollBarWidth() const
   {
      SmartScrollArea *that = const_cast<SmartScrollArea *>(this);
      QWidgetList list = that->scrollBarWidgets(Qt::AlignRight);
      if (list.isEmpty())
         return 0;
      return list.first()->sizeHint().width();
   }
};

ViewOptionsDialog::ViewOptionsDialog(MainWindow *mainwindow,
      QWidget *parent) :
   QDialog(mainwindow)
   ,m_optionsList(new QListWidget(this))
   ,m_optionsStack(new QStackedLayout)
{
   int width;
   QGridLayout        *layout = new QGridLayout(this);
   QLabel      *m_headerLabel = new QLabel(this);
   /* Header label with large font and a bit of spacing
    * (align with group boxes) */
   QFont      headerLabelFont = m_headerLabel->font();
   const int        pointSize = headerLabelFont.pointSize();
   QHBoxLayout *headerHLayout = new QHBoxLayout;
   const int       leftMargin = QApplication::style()->pixelMetric(
         QStyle::PM_LayoutLeftMargin);

#if (QT_VERSION > QT_VERSION_CHECK(6, 0, 0))
   m_optionsStack->setContentsMargins(0, 0, 0, 0);
#else
   m_optionsStack->setMargin(0);
#endif

   headerLabelFont.setBold(true);

   /* Paranoia: Should a font be set in pixels... */
   if (pointSize > 0)
      headerLabelFont.setPointSize(pointSize + 2);

   m_headerLabel->setFont(headerLabelFont);

   headerHLayout->addSpacerItem(new QSpacerItem(leftMargin, 0,
            QSizePolicy::Fixed, QSizePolicy::Ignored));
   headerHLayout->addWidget(m_headerLabel);

   addCategory(new DriversCategory(this));
   addCategory(new VideoCategory(this));
   addCategory(new AudioCategory(this));
   addCategory(new InputCategory(this));
   addCategory(new LatencyCategory(this));
   addCategory(new CoreCategory(this));
   addCategory(new ConfigurationCategory(this));
   addCategory(new SavingCategory(this));
   addCategory(new LoggingCategory(this));
   addCategory(new FrameThrottleCategory(this));
   addCategory(new RecordingCategory(this));
   addCategory(new OnscreenDisplayCategory(this));
   addCategory(new UserInterfaceCategory(mainwindow, this));
   addCategory(new AIServiceCategory(this));
#ifdef HAVE_CHEEVOS
   addCategory(new AchievementsCategory(this));
#endif
   addCategory(new NetworkCategory(this));
   addCategory(new PlaylistsCategory(this));
   addCategory(new UserCategory(this));
   addCategory(new DirectoryCategory(this));

   width  =
        m_optionsList->sizeHintForColumn(0)
      + m_optionsList->frameWidth() * 2
      + 5;
   width += m_optionsList->verticalScrollBar()->sizeHint().width();

   m_optionsList->setMaximumWidth(width);
   m_optionsList->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);

   setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE));

   layout->addWidget(m_optionsList, 0, 0, 2, 1);
   layout->addLayout(headerHLayout, 0, 1);
   layout->addLayout(m_optionsStack, 1, 1);

   connect(m_optionsList, SIGNAL(currentRowChanged(int)),
         m_optionsStack, SLOT(setCurrentIndex(int)));
   connect(m_optionsList, SIGNAL(currentTextChanged(const QString&)),
         m_headerLabel, SLOT(setText(const QString&)));

   connect(this, SIGNAL(rejected()), this, SLOT(onRejected()));
}

QIcon getIcon(OptionsCategory *category)
{
   settings_t *settings        = config_get_ptr();
   const char *path_dir_assets = settings->paths.directory_assets;
   QPixmap pixmap              = QPixmap(QString(path_dir_assets)
         + "/xmb/monochrome/png/"
         + category->categoryIconName()
         + ".png");
   return QIcon(getColorizedPixmap(pixmap, getLabelColor("iconColor")));
}

void ViewOptionsDialog::addCategory(OptionsCategory *category)
{
   QTabWidget *tabWidget = new QTabWidget();

   m_categoryList.append(category);

   for (OptionsPage* page : category->pages())
   {
      SmartScrollArea *scrollArea = new SmartScrollArea(this);
      QWidget             *widget = page->widget();

      scrollArea->setWidget(widget);
      widget->setAutoFillBackground(false);
      tabWidget->addTab(scrollArea, page->displayName());
   }

#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
   tabWidget->setTabBarAutoHide(true);
#else
   /* TODO remove the tabBar's space */
   if (tabWidget->count() < 2)
      tabWidget->tabBar()->hide();
#endif
   m_optionsList->addItem(
         new QListWidgetItem(getIcon(category),
            category->displayName()));
   m_optionsStack->addWidget(tabWidget);
}

void ViewOptionsDialog::repaintIcons()
{
   unsigned i;
   size_t list_size = (size_t)m_categoryList.size();

   for (i = 0; i < list_size; i++)
      m_optionsList->item(i)->setIcon(getIcon(m_categoryList.at(i)));
}
#else
ViewOptionsDialog::ViewOptionsDialog(MainWindow *mainwindow, QWidget *parent) :
   QDialog(mainwindow)
   , m_viewOptionsWidget(new ViewOptionsWidget(mainwindow))
{
   QVBoxLayout         *layout = new QVBoxLayout;
   QDialogButtonBox *buttonBox = new QDialogButtonBox(
         QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

   connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
   connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

   connect(this, SIGNAL(accepted()), m_viewOptionsWidget, SLOT(onAccepted()));
   connect(this, SIGNAL(rejected()), m_viewOptionsWidget, SLOT(onRejected()));

   setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE));

   layout->setContentsMargins(0, 0, 0, 0);

   layout->addWidget(m_viewOptionsWidget);
   layout->addWidget(buttonBox);

   setLayout(layout);
}
#endif

void ViewOptionsDialog::showDialog()
{
#ifdef HAVE_MENU
   unsigned i;
   size_t list_size = (size_t)m_categoryList.size();
   for (i = 0; i < list_size; i++)
      m_categoryList.at(i)->load();
#else
   m_viewOptionsWidget->loadViewOptions();
#endif
   show();
   activateWindow();
}

void ViewOptionsDialog::hideDialog() { reject(); }

void ViewOptionsDialog::onRejected()
{
#ifdef HAVE_MENU
   unsigned i;
   size_t list_size = (size_t)m_categoryList.size();
   for (i = 0; i < list_size; i++)
      m_categoryList.at(i)->apply();
#endif
}

ViewOptionsWidget::ViewOptionsWidget(MainWindow *mainwindow, QWidget *parent) :
   QWidget(parent)
   ,m_mainwindow(mainwindow)
   ,m_settings(mainwindow->settings())
   ,m_saveGeometryCheckBox(new QCheckBox(this))
   ,m_saveDockPositionsCheckBox(new QCheckBox(this))
   ,m_saveLastTabCheckBox(new QCheckBox(this))
   ,m_showHiddenFilesCheckBox(new QCheckBox(this))
   ,m_themeComboBox(new QComboBox(this))
   ,m_thumbnailCacheSpinBox(new QSpinBox(this))
   ,m_thumbnailDropSizeSpinBox(new QSpinBox(this))
   ,m_startupPlaylistComboBox(new QComboBox(this))
   ,m_highlightColorPushButton(new QPushButton(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CHOOSE), this))
   ,m_highlightColor()
   ,m_highlightColorLabel(new QLabel(
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR), this))
   ,m_customThemePath()
   ,m_suggestLoadedCoreFirstCheckBox(new QCheckBox(this))
   /* ,m_allPlaylistsListMaxCountSpinBox(new QSpinBox(this)) */
   /* ,m_allPlaylistsGridMaxCountSpinBox(new QSpinBox(this)) */
{
   QVBoxLayout *layout = new QVBoxLayout;
   QFormLayout *form   = new QFormLayout;

   m_themeComboBox->addItem(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT),
         MainWindow::THEME_SYSTEM_DEFAULT);
   m_themeComboBox->addItem(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_DARK),
         MainWindow::THEME_DARK);
   m_themeComboBox->addItem(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM),
         MainWindow::THEME_CUSTOM);

   m_thumbnailCacheSpinBox->setSuffix(" MB");
   m_thumbnailCacheSpinBox->setRange(0, 99999);

   m_thumbnailDropSizeSpinBox->setSuffix(" px");
   m_thumbnailDropSizeSpinBox->setRange(0, 99999);

   /* m_allPlaylistsListMaxCountSpinBox->setRange(0, 99999); */
   /* m_allPlaylistsGridMaxCountSpinBox->setRange(0, 99999); */

   form->setFormAlignment(Qt::AlignCenter);
   form->setLabelAlignment(Qt::AlignCenter);

   form->addRow(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY),
         m_saveGeometryCheckBox);
   form->addRow(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS),
         m_saveDockPositionsCheckBox);
   form->addRow(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB), m_saveLastTabCheckBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES), m_showHiddenFilesCheckBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST), m_suggestLoadedCoreFirstCheckBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_STARTUP_PLAYLIST), m_startupPlaylistComboBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT), m_thumbnailCacheSpinBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_DROP_SIZE_LIMIT), m_thumbnailDropSizeSpinBox);
   form->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME), m_themeComboBox);
   form->addRow(m_highlightColorLabel, m_highlightColorPushButton);

   layout->addLayout(form);

   layout->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));

   setLayout(layout);

   loadViewOptions();

   connect(m_themeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onThemeComboBoxIndexChanged(int)));
   connect(m_highlightColorPushButton, SIGNAL(clicked()), this, SLOT(onHighlightColorChoose()));
}

void ViewOptionsWidget::onThemeComboBoxIndexChanged(int)
{
   MainWindow::Theme theme = static_cast<MainWindow::Theme>(m_themeComboBox->currentData(Qt::UserRole).toInt());

   if (theme == MainWindow::THEME_CUSTOM)
   {
      QString filePath = QFileDialog::getOpenFileName(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SELECT_THEME));

      if (filePath.isEmpty())
      {
         int oldThemeIndex = m_themeComboBox->findData(m_mainwindow->getThemeFromString(m_settings->value("theme", "default").toString()));

         if (m_themeComboBox->count() > oldThemeIndex)
         {
            disconnect(m_themeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onThemeComboBoxIndexChanged(int)));
            m_themeComboBox->setCurrentIndex(oldThemeIndex);
            connect(m_themeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onThemeComboBoxIndexChanged(int)));
         }
      }
      else
      {
         m_customThemePath = filePath;

         if (m_mainwindow->setCustomThemeFile(filePath))
            m_mainwindow->setTheme(theme);
      }
   }
   else
      m_mainwindow->setTheme(theme);

   showOrHideHighlightColor();
}

void ViewOptionsWidget::onHighlightColorChoose()
{
   QPixmap highlightPixmap(m_highlightColorPushButton->iconSize());
   QColor currentHighlightColor = m_settings->value("highlight_color",
         QApplication::palette().highlight().color()).value<QColor>();
   QColor newHighlightColor     = QColorDialog::getColor(
         currentHighlightColor, this,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR));

   if (newHighlightColor.isValid())
   {
      MainWindow::Theme theme = static_cast<MainWindow::Theme>(
            m_themeComboBox->currentData(Qt::UserRole).toInt());

      m_highlightColor = newHighlightColor;
      m_settings->setValue("highlight_color", m_highlightColor);
      highlightPixmap.fill(m_highlightColor);
      m_highlightColorPushButton->setIcon(highlightPixmap);
      m_mainwindow->setTheme(theme);
   }
}

void ViewOptionsWidget::loadViewOptions()
{
   int i;
   int themeIndex    = 0;
   int playlistIndex = 0;
   QColor highlightColor                       =
      m_settings->value("highlight_color",
            QApplication::palette().highlight().color()).value<QColor>();
   QPixmap highlightPixmap(m_highlightColorPushButton->iconSize());
   QVector<QPair<QString, QString> > playlists = m_mainwindow->getPlaylists();
   QString initialPlaylist = m_settings->value("initial_playlist",
         m_mainwindow->getSpecialPlaylistPath(
            SPECIAL_PLAYLIST_HISTORY)).toString();

   m_saveGeometryCheckBox->setChecked(m_settings->value("save_geometry", false).toBool());
   m_saveDockPositionsCheckBox->setChecked(m_settings->value("save_dock_positions", false).toBool());
   m_saveLastTabCheckBox->setChecked(m_settings->value("save_last_tab", false).toBool());
   m_showHiddenFilesCheckBox->setChecked(m_settings->value("show_hidden_files", true).toBool());
   m_suggestLoadedCoreFirstCheckBox->setChecked(m_settings->value("suggest_loaded_core_first", false).toBool());
   m_thumbnailCacheSpinBox->setValue(m_settings->value("thumbnail_cache_limit", 512).toInt());
   m_thumbnailDropSizeSpinBox->setValue(m_settings->value("thumbnail_max_size", 0).toInt());

   themeIndex = m_themeComboBox->findData(m_mainwindow->getThemeFromString(m_settings->value("theme", "default").toString()));

   if (m_themeComboBox->count() > themeIndex)
      m_themeComboBox->setCurrentIndex(themeIndex);

   if (highlightColor.isValid())
   {
      m_highlightColor = highlightColor;
      highlightPixmap.fill(m_highlightColor);
      m_highlightColorPushButton->setIcon(highlightPixmap);
   }

   showOrHideHighlightColor();

   m_startupPlaylistComboBox->clear();

   for (i = 0; i < playlists.count(); i++)
   {
      const QPair<QString, QString> &pair = playlists.at(i);

      m_startupPlaylistComboBox->addItem(pair.first, pair.second);
   }

   playlistIndex = m_startupPlaylistComboBox->findData(
         initialPlaylist, Qt::UserRole, Qt::MatchFixedString);

   if (playlistIndex >= 0)
      m_startupPlaylistComboBox->setCurrentIndex(playlistIndex);
}

void ViewOptionsWidget::showOrHideHighlightColor()
{
   if (m_mainwindow->theme() == MainWindow::THEME_DARK)
   {
      m_highlightColorLabel->show();
      m_highlightColorPushButton->show();
   }
   else
   {
      m_highlightColorLabel->hide();
      m_highlightColorPushButton->hide();
   }
}

void ViewOptionsWidget::saveViewOptions()
{
   m_settings->setValue("save_geometry", m_saveGeometryCheckBox->isChecked());
   m_settings->setValue("save_dock_positions", m_saveDockPositionsCheckBox->isChecked());
   m_settings->setValue("save_last_tab", m_saveLastTabCheckBox->isChecked());
   m_settings->setValue("theme", m_mainwindow->getThemeString(static_cast<MainWindow::Theme>(m_themeComboBox->currentData(Qt::UserRole).toInt())));
   m_settings->setValue("show_hidden_files", m_showHiddenFilesCheckBox->isChecked());
   m_settings->setValue("highlight_color", m_highlightColor);
   m_settings->setValue("suggest_loaded_core_first", m_suggestLoadedCoreFirstCheckBox->isChecked());
   m_settings->setValue("initial_playlist", m_startupPlaylistComboBox->currentData(Qt::UserRole).toString());
   m_settings->setValue("thumbnail_cache_limit", m_thumbnailCacheSpinBox->value());
   m_settings->setValue("thumbnail_max_size", m_thumbnailDropSizeSpinBox->value());

   if (!m_mainwindow->customThemeString().isEmpty())
      m_settings->setValue("custom_theme", m_customThemePath);

   m_mainwindow->setThumbnailCacheLimit(m_thumbnailCacheSpinBox->value());
}

void ViewOptionsWidget::onAccepted() { saveViewOptions(); }
void ViewOptionsWidget::onRejected() { loadViewOptions(); }

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

CoreOptionsDialog::~CoreOptionsDialog() { }

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
      {
         QList<QObject*> _children = children();
         for (int _i = 0; _i < _children.size(); _i++)
            _children[_i]->deleteLater();
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
#ifdef HAVE_MENU
   struct menu_state *menu_st = menu_state_get_ptr();
#endif
   if (!core_options_create_override(true))
      QMessageBox::critical(this, msg_hash_to_str(MSG_ERROR), msg_hash_to_str(MSG_ERROR_SAVING_CORE_OPTIONS_FILE));
#ifdef HAVE_MENU
   menu_st->flags                 |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                   |  MENU_ST_FLAG_PREVENT_POPULATE;
#endif
}

void CoreOptionsDialog::onSaveFolderSpecificOptions()
{
#ifdef HAVE_MENU
   struct menu_state *menu_st = menu_state_get_ptr();
#endif
   if (!core_options_create_override(false))
      QMessageBox::critical(this, msg_hash_to_str(MSG_ERROR), msg_hash_to_str(MSG_ERROR_SAVING_CORE_OPTIONS_FILE));
#ifdef HAVE_MENU
   menu_st->flags                 |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                   |  MENU_ST_FLAG_PREVENT_POPULATE;
#endif
}

void CoreOptionsDialog::onCoreOptionComboBoxCurrentIndexChanged(int index)
{
   unsigned i, k;
   QString key, val;
   runloop_state_t *runloop_st  = runloop_state_get_ptr();
   QComboBox *combo_box         = qobject_cast<QComboBox*>(sender());

   if (!combo_box)
      return;

   key = combo_box->itemData(index, Qt::UserRole).toString();
   val = combo_box->itemText(index);

   if (runloop_st->core_options)
   {
      core_option_manager_t *coreopts = NULL;

      retroarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

      if (coreopts)
      {
         size_t opts = runloop_st->core_options->size;

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
                     core_option_manager_set_val(coreopts, i, k, true);
               }
            }
         }
      }
   }
}

void CoreOptionsDialog::buildLayout()
{
   unsigned j, k;
   QFormLayout *form            = NULL;
   settings_t *settings         = config_get_ptr();
   runloop_state_t *runloop_st  = runloop_state_get_ptr();
   bool has_core_options        = (runloop_st->core_options != NULL);
   size_t opts                  = (runloop_st->core_options) ? runloop_st->core_options->size : 0;

   clearLayout();

   if (has_core_options)
   {
      core_option_manager_t *coreopts = NULL;

      form = new QFormLayout();

      if (settings->bools.game_specific_options)
      {
         QString contentLabel;
         QString label;
         rarch_system_info_t *sys_info = &runloop_st->system;

         /* TODO/FIXME - why have this check here? sys_info is not used */
         if (sys_info)
            contentLabel = QFileInfo(path_get(RARCH_PATH_BASENAME)).completeBaseName();

         if (!contentLabel.isEmpty())
         {
            uint32_t flags = runloop_st->flags;

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

      retroarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

      if (coreopts)
      {
         QToolButton *resetAllButton = new QToolButton(this);

         resetAllButton->setDefaultAction(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_RESET_ALL), this));
         connect(resetAllButton, SIGNAL(clicked()), this, SLOT(onCoreOptionResetAllClicked()));

         for (j = 0; j < opts; j++)
         {
            QString desc               =
               core_option_manager_get_desc(coreopts, j, false);
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

            if (option->info && *option->info)
            {
               char *new_info;
               size_t option_info_len = strlen(option->info);
               size_t new_info_len    = option_info_len + 1;

               if (!(new_info = (char *)malloc(new_info_len)))
                  return;
               new_info[0] = '\0';

               word_wrap(new_info, new_info_len, option->info,
                     option_info_len, 50, 100, 0);
               descLabel->setToolTip(new_info);
               combo_box->setToolTip(new_info);
               free(new_info);
            }

            for (k = 0; k < option->vals->size; k++)
               combo_box->addItem(option->vals->elems[k].data, QVariant::fromValue(QString(option->key)));

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

#if defined(HAVE_MENU)
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
enum
{
   QT_SHADER_PRESET_GLOBAL = 0,
   QT_SHADER_PRESET_CORE,
   QT_SHADER_PRESET_PARENT,
   QT_SHADER_PRESET_GAME,
   QT_SHADER_PRESET_NORMAL
};

ShaderPass::ShaderPass(struct video_shader_pass *passToCopy) :
   pass(NULL)
{
   if (passToCopy)
   {
      pass = (struct video_shader_pass*)calloc(1, sizeof(*pass));
      memcpy(pass, passToCopy, sizeof(*pass));
   }
}

ShaderPass::~ShaderPass()
{
   if (pass)
      free(pass);
}

ShaderPass& ShaderPass::operator=(const ShaderPass &other)
{
   if (this != &other && other.pass)
   {
      pass = (struct video_shader_pass*)calloc(1, sizeof(*pass));
      memcpy(pass, other.pass, sizeof(*pass));
   }

   return *this;
}

ShaderParamsDialog::ShaderParamsDialog(QWidget *parent) :
   QDialog(parent)
   ,m_layout()
   ,m_scrollArea()
{
   setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS));
   setObjectName("shaderParamsDialog");

   resize(720, 480);

   QTimer::singleShot(0, this, SLOT(clearLayout()));
}

ShaderParamsDialog::~ShaderParamsDialog()
{
}

void ShaderParamsDialog::resizeEvent(QResizeEvent *event)
{
   QDialog::resizeEvent(event);

   if (!m_scrollArea)
      return;

   m_scrollArea->resize(event->size());

   emit resized(event->size());
}

void ShaderParamsDialog::closeEvent(QCloseEvent *event)
{
   QDialog::closeEvent(event);

   emit closed();
}

void ShaderParamsDialog::paintEvent(QPaintEvent *event)
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

void ShaderParamsDialog::clearLayout()
{
   QWidget *widget = NULL;

   if (m_scrollArea)
   {
      {
         QList<QObject*> _children = children();
         for (int _i = 0; _i < _children.size(); _i++)
            delete _children[_i];
      }
   }

   m_layout = new QVBoxLayout();

   widget   = new QWidget();
   widget->setLayout(m_layout);
   widget->setObjectName("shaderParamsWidget");

   m_scrollArea = new QScrollArea();

   m_scrollArea->setParent(this);
   m_scrollArea->setWidgetResizable(true);
   m_scrollArea->setWidget(widget);
   m_scrollArea->setObjectName("shaderParamsScrollArea");
   m_scrollArea->show();
}

void ShaderParamsDialog::getShaders(struct video_shader **menu_shader, struct video_shader **video_shader)
{
   video_shader_ctx_t shader_info = {0};
   struct video_shader    *shader = menu_shader_get();

   if (menu_shader)
   {
      if (shader)
         *menu_shader = shader;
      else
         *menu_shader = NULL;
   }

   if (video_shader)
   {
      if (shader)
         *video_shader = shader_info.data;
      else
         *video_shader = NULL;
   }

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
         *video_shader = shader_info.data;
      else
         *video_shader = NULL;
   }
}

void ShaderParamsDialog::onFilterComboBoxIndexChanged(int)
{
   QVariant passVariant;
   QComboBox               *comboBox = qobject_cast<QComboBox*>(sender());
   int pass                          = 0;
   bool ok                           = false;
   struct video_shader *menu_shader  = NULL;
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

   if (     menu_shader
         && (pass >= 0)
         && (pass < static_cast<int>(menu_shader->passes)))
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

            video_shader->flags |= SHDR_FLAG_MODIFIED;

            command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
         }
      }
   }
}

void ShaderParamsDialog::onScaleComboBoxIndexChanged(int)
{
   QVariant passVariant;
   QComboBox *comboBox               = qobject_cast<QComboBox*>(sender());
   int pass                          = 0;
   bool ok                           = false;
   struct video_shader *menu_shader  = NULL;
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
               menu_shader->pass[pass].fbo.scale_x   = scale;
               menu_shader->pass[pass].fbo.scale_y   = scale;
               if (scale)
                  menu_shader->pass[pass].fbo.flags |=  FBO_SCALE_FLAG_VALID;
               else
                  menu_shader->pass[pass].fbo.flags &= ~FBO_SCALE_FLAG_VALID;
            }

            if (video_shader)
            {
               video_shader->pass[pass].fbo.scale_x   = scale;
               video_shader->pass[pass].fbo.scale_y   = scale;
               if (scale)
                  video_shader->pass[pass].fbo.flags |=  FBO_SCALE_FLAG_VALID;
               else
                  video_shader->pass[pass].fbo.flags &= ~FBO_SCALE_FLAG_VALID;
            }

            video_shader->flags |= SHDR_FLAG_MODIFIED;

            command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
         }
      }
   }
}

void ShaderParamsDialog::onShaderPassMoveDownClicked()
{
   QVariant passVariant;
   bool ok                           = false;
   struct video_shader *menu_shader  = NULL;
   struct video_shader *video_shader = NULL;
   QToolButton *button               = qobject_cast<QToolButton*>(sender());
   int pass                          = 0;

   getShaders(&menu_shader, &video_shader);

   if (!button)
      return;

   passVariant                       = button->property("pass");

   if (!passVariant.isValid())
      return;

   pass = passVariant.toInt(&ok);

   if (!ok || pass < 0)
      return;

   if (video_shader)
   {
      ShaderPass tempPass;
      int i;

      if (pass >= static_cast<int>(video_shader->passes) - 1)
         return;

      for (i = 0; i < static_cast<int>(video_shader->num_parameters); i++)
      {
         struct video_shader_parameter *param = &video_shader->parameters[i];

         if (param->pass == pass)
            param->pass += 1;
         else if (param->pass == pass + 1)
            param->pass -= 1;
      }

      tempPass = ShaderPass(&video_shader->pass[pass]);
      memcpy(&video_shader->pass[pass], &video_shader->pass[pass + 1], sizeof(struct video_shader_pass));
      memcpy(&video_shader->pass[pass + 1], tempPass.pass, sizeof(struct video_shader_pass));
   }

   if (menu_shader)
   {
      ShaderPass tempPass;
      int i;

      if (pass >= static_cast<int>(menu_shader->passes) - 1)
         return;

      for (i = 0; i < static_cast<int>(menu_shader->num_parameters); i++)
      {
         struct video_shader_parameter *param = &menu_shader->parameters[i];

         if (param->pass == pass)
            param->pass += 1;
         else if (param->pass == pass + 1)
            param->pass -= 1;
      }

      tempPass = ShaderPass(&menu_shader->pass[pass]);
      memcpy(&menu_shader->pass[pass], &menu_shader->pass[pass + 1], sizeof(struct video_shader_pass));
      memcpy(&menu_shader->pass[pass + 1], tempPass.pass, sizeof(struct video_shader_pass));
   }

   menu_shader->flags |= SHDR_FLAG_MODIFIED;

   reload();
}

void ShaderParamsDialog::onShaderPassMoveUpClicked()
{
   QVariant passVariant;
   int pass                          = 0;
   bool ok                           = false;
   struct video_shader *menu_shader  = NULL;
   struct video_shader *video_shader = NULL;
   QToolButton *button               = qobject_cast<QToolButton*>(sender());

   getShaders(&menu_shader, &video_shader);

   if (!button)
      return;

   passVariant = button->property("pass");

   if (!passVariant.isValid())
      return;

   pass = passVariant.toInt(&ok);

   if (!ok || pass <= 0)
      return;

   if (video_shader)
   {
      ShaderPass tempPass;
      int i;

      if (pass > static_cast<int>(video_shader->passes) - 1)
         return;

      for (i = 0; i < static_cast<int>(video_shader->num_parameters); i++)
      {
         struct video_shader_parameter *param = &video_shader->parameters[i];

         if (param->pass == pass)
            param->pass -= 1;
         else if (param->pass == pass - 1)
            param->pass += 1;
      }

      tempPass = ShaderPass(&video_shader->pass[pass - 1]);
      memcpy(&video_shader->pass[pass - 1], &video_shader->pass[pass], sizeof(struct video_shader_pass));
      memcpy(&video_shader->pass[pass], tempPass.pass, sizeof(struct video_shader_pass));
   }

   if (menu_shader)
   {
      ShaderPass tempPass;
      int i;

      if (pass > static_cast<int>(menu_shader->passes) - 1)
         return;

      for (i = 0; i < static_cast<int>(menu_shader->num_parameters); i++)
      {
         struct video_shader_parameter *param = &menu_shader->parameters[i];

         if (param->pass == pass)
            param->pass -= 1;
         else if (param->pass == pass - 1)
            param->pass += 1;
      }

      tempPass = ShaderPass(&menu_shader->pass[pass - 1]);
      memcpy(&menu_shader->pass[pass - 1], &menu_shader->pass[pass], sizeof(struct video_shader_pass));
      memcpy(&menu_shader->pass[pass], tempPass.pass, sizeof(struct video_shader_pass));
   }

   menu_shader->flags |= SHDR_FLAG_MODIFIED;

   reload();
}

void ShaderParamsDialog::onShaderLoadPresetClicked()
{
   QString path, filter;
   QByteArray pathArray;
   gfx_ctx_flags_t flags;
   struct video_shader *menu_shader  = NULL;
   struct video_shader *video_shader = NULL;
   const char *pathData              = NULL;
   enum rarch_shader_type type       = RARCH_SHADER_NONE;
#if !defined(HAVE_MENU)
   settings_t *settings              = config_get_ptr();
   const char *shader_preset_dir     = settings->paths.directory_video_shader;
#else
   const char *shader_preset_dir     = NULL;
   menu_driver_get_last_shader_preset_path(&shader_preset_dir, NULL);
#endif

   getShaders(&menu_shader, &video_shader);

   if (!menu_shader)
      return;

   filter.append("Shader Preset (");

   flags.flags     = 0;
   video_context_driver_get_flags(&flags);

   /* NOTE: Maybe we should have a way to get a list
    * of all shader types instead of hard-coding this? */
   if (BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_CG))
   {
      filter.append(QLatin1String(" *"));
      filter.append(".cgp");
   }

   if (BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_GLSL))
   {
      filter.append(QLatin1String(" *"));
      filter.append(".glslp");
   }

   if (BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_SLANG))
   {
      filter.append(QLatin1String(" *"));
      filter.append(".slangp");
   }

   filter.append(")");
   path       = QFileDialog::getOpenFileName(
         this,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET),
         shader_preset_dir, filter);

   if (path.isEmpty())
      return;

   pathArray  = path.toUtf8();
   pathData   = pathArray.constData();
   type       = video_shader_parse_type(pathData);

#if defined(HAVE_MENU)
   /* Cache selected shader parent directory */
   menu_driver_set_last_shader_preset_path(pathData);
#endif

   menu_shader_manager_set_preset(menu_shader, type, pathData, true);
}

void ShaderParamsDialog::onShaderResetPass(int pass)
{
   unsigned i;
   struct video_shader *menu_shader  = NULL;
   struct video_shader *video_shader = NULL;

   getShaders(&menu_shader, &video_shader);

   if (menu_shader)
   {
      for (i = 0; i < menu_shader->num_parameters; i++)
      {
         struct video_shader_parameter *param = &menu_shader->parameters[i];

         /* if pass < 0, reset all params,
          * otherwise only reset the selected pass */
         if (pass >= 0 && param->pass != pass)
            continue;

         param->current = param->initial;
      }
   }

   if (video_shader)
   {
      for (i = 0; i < video_shader->num_parameters; i++)
      {
         struct video_shader_parameter *param = &video_shader->parameters[i];

         /* if pass < 0, reset all params,
          * otherwise only reset the selected pass */
         if (pass >= 0 && param->pass != pass)
            continue;

         param->current = param->initial;
      }

      video_shader->flags |= SHDR_FLAG_MODIFIED;
   }

   reload();
}

void ShaderParamsDialog::onShaderResetParameter(QString parameter)
{
   struct video_shader *menu_shader  = NULL;
   struct video_shader *video_shader = NULL;

   getShaders(&menu_shader, &video_shader);

   if (menu_shader)
   {
      int i;
      struct video_shader_parameter *param = NULL;

      for (i = 0; i < static_cast<int>(menu_shader->num_parameters); i++)
      {
         QString id = menu_shader->parameters[i].id;

         if (id == parameter)
            param = &menu_shader->parameters[i];
      }

      if (param)
         param->current = param->initial;
   }

   if (video_shader)
   {
      int i;
      struct video_shader_parameter *param = NULL;

      for (i = 0; i < static_cast<int>(video_shader->num_parameters); i++)
      {
         QString id = video_shader->parameters[i].id;

         if (id == parameter)
            param = &video_shader->parameters[i];
      }

      if (param)
         param->current = param->initial;

      video_shader->flags |= SHDR_FLAG_MODIFIED;
   }

   reload();
}

void ShaderParamsDialog::onShaderResetAllPasses()
{
   onShaderResetPass(-1);
}

void ShaderParamsDialog::onShaderAddPassClicked()
{
   gfx_ctx_flags_t flags;
   QString path, filter;
   QByteArray pathArray;
   struct video_shader *menu_shader      = NULL;
   struct video_shader *video_shader     = NULL;
   struct video_shader_pass *shader_pass = NULL;
   const char *pathData                  = NULL;
#if !defined(HAVE_MENU)
   settings_t *settings                  = config_get_ptr();
   const char *shader_pass_dir           = settings->paths.directory_video_shader;
#else
   const char *shader_pass_dir           = NULL;

   menu_driver_get_last_shader_pass_path(&shader_pass_dir, NULL);
#endif

   getShaders(&menu_shader, &video_shader);

   if (!menu_shader)
      return;

   filter.append("Shader (");

   flags.flags     = 0;
   video_context_driver_get_flags(&flags);

   /* NOTE: Maybe we should have a way to get a list
    * of all shader types instead of hard-coding this? */
   if (BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_CG))
      filter.append(QLatin1String(" *.cg"));
   if (BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_GLSL))
      filter.append(QLatin1String(" *.glsl"));
   if (BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_SLANG))
      filter.append(QLatin1String(" *.slang"));

   filter.append(")");

   path = QFileDialog::getOpenFileName(
         this,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET),
         shader_pass_dir, filter);

   if (path.isEmpty())
      return;

   /* Qt uses '/' as a directory separator regardless
    * of host platform. Have to convert to native separators,
    * or video_shader_resolve_parameters() will fail on
    * non-Linux platforms */
   path      = QDir::toNativeSeparators(path);

   pathArray = path.toUtf8();
   pathData  = pathArray.constData();

   if (menu_shader->passes < GFX_MAX_SHADERS)
      menu_shader->passes++;
   else
      return;

   menu_shader->flags   |= SHDR_FLAG_MODIFIED;
   shader_pass           = &menu_shader->pass[menu_shader->passes - 1];

   if (!shader_pass)
      return;

   strlcpy(shader_pass->source.path,
         pathData,
         sizeof(shader_pass->source.path));

#if defined(HAVE_MENU)
   /* Cache selected shader parent directory */
   menu_driver_set_last_shader_pass_path(pathData);
#endif

   video_shader_resolve_parameters(menu_shader);

   command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
}

void ShaderParamsDialog::onShaderSavePresetAsClicked()
{
   QByteArray pathArray;
   const char *pathData              = NULL;
   settings_t *settings              = config_get_ptr();
   const char *path_dir_video_shader = settings->paths.directory_video_shader;
   QString path                      = QFileDialog::getSaveFileName(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS), path_dir_video_shader);

   if (path.isEmpty())
      return;

   pathArray                         = path.toUtf8();
   pathData                          = pathArray.constData();

   operateShaderPreset(true, pathData, QT_SHADER_PRESET_NORMAL);
}

/** save or remove shader preset */
void ShaderParamsDialog::operateShaderPreset(bool save, const char *path, unsigned action_type)
{
   bool ret;
   enum auto_shader_type preset_type;
   settings_t              *settings = config_get_ptr();
   const char *path_dir_video_shader = settings->paths.directory_video_shader;
   const char *path_dir_menu_config  = settings->paths.directory_menu_config;

   switch (action_type)
   {
      case QT_SHADER_PRESET_GLOBAL:
         preset_type = SHADER_PRESET_GLOBAL;
         break;
      case QT_SHADER_PRESET_CORE:
         preset_type = SHADER_PRESET_CORE;
         break;
      case QT_SHADER_PRESET_PARENT:
         preset_type = SHADER_PRESET_PARENT;
         break;
      case QT_SHADER_PRESET_GAME:
         preset_type = SHADER_PRESET_GAME;
         break;
      case QT_SHADER_PRESET_NORMAL:
         break;
      default:
         return;
   }

   if (save)
   {
      if (action_type == QT_SHADER_PRESET_NORMAL)
         ret = menu_shader_manager_save_preset(
               menu_shader_get(),
               path,
               path_dir_video_shader,
               path_dir_menu_config,
               true);
      else
         ret = menu_shader_manager_save_auto_preset(
               menu_shader_get(),
               preset_type,
               path_dir_video_shader,
               path_dir_menu_config,
               true);

      if (ret)
      {
         const char *_msg = msg_hash_to_str(MSG_SHADER_PRESET_SAVED_SUCCESSFULLY);
         runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
      else
      {
         const char *_msg = msg_hash_to_str(MSG_ERROR_SAVING_SHADER_PRESET);
         runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
      }
   }
   else
   {
      if (action_type != QT_SHADER_PRESET_NORMAL &&
            menu_shader_manager_remove_auto_preset(preset_type,
               path_dir_video_shader,
               path_dir_menu_config))
      {
         const char *_msg = msg_hash_to_str(MSG_SHADER_PRESET_REMOVED_SUCCESSFULLY);
         runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

#ifdef HAVE_MENU
         menu_state_get_ptr()->flags |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
#endif
      }
      else
      {
         const char *_msg = msg_hash_to_str(MSG_ERROR_REMOVING_SHADER_PRESET);
         runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
      }
   }
}

void ShaderParamsDialog::onShaderSaveGlobalPresetClicked()
{
   operateShaderPreset(true, NULL, QT_SHADER_PRESET_GLOBAL);
}

void ShaderParamsDialog::onShaderSaveCorePresetClicked()
{
   operateShaderPreset(true, NULL, QT_SHADER_PRESET_CORE);
}

void ShaderParamsDialog::onShaderSaveParentPresetClicked()
{
   operateShaderPreset(true, NULL, QT_SHADER_PRESET_PARENT);
}

void ShaderParamsDialog::onShaderSaveGamePresetClicked()
{
   operateShaderPreset(true, NULL, QT_SHADER_PRESET_GAME);
}

void ShaderParamsDialog::onShaderRemoveGlobalPresetClicked()
{
   operateShaderPreset(false, NULL, QT_SHADER_PRESET_GLOBAL);
}

void ShaderParamsDialog::onShaderRemoveCorePresetClicked()
{
   operateShaderPreset(false, NULL, QT_SHADER_PRESET_CORE);
}

void ShaderParamsDialog::onShaderRemoveParentPresetClicked()
{
   operateShaderPreset(false, NULL, QT_SHADER_PRESET_PARENT);
}

void ShaderParamsDialog::onShaderRemoveGamePresetClicked()
{
   operateShaderPreset(false, NULL, QT_SHADER_PRESET_GAME);
}

void ShaderParamsDialog::onShaderRemoveAllPassesClicked()
{
   struct video_shader *menu_shader  = NULL;
   struct video_shader *video_shader = NULL;

   getShaders(&menu_shader, &video_shader);

   if (!menu_shader)
      return;

   menu_shader->passes   = 0;
   menu_shader->flags   |= SHDR_FLAG_MODIFIED;

   onShaderApplyClicked();
}

void ShaderParamsDialog::onShaderRemovePassClicked()
{
   QVariant passVariant;
   QAction                   *action = qobject_cast<QAction*>(sender());
   int pass                          = 0;
   bool ok                           = false;

   if (!action)
      return;
   passVariant = action->data();

   if (!passVariant.isValid())
      return;

   pass = passVariant.toInt(&ok);

   if (!ok)
      return;

   onShaderRemovePass(pass);
}

void ShaderParamsDialog::onShaderRemovePass(int pass)
{
   int i;
   struct video_shader *menu_shader  = NULL;
   struct video_shader *video_shader = NULL;

   getShaders(&menu_shader, &video_shader);

   if (!menu_shader || menu_shader->passes == 0)
      return;

   if (pass < 0 || pass > static_cast<int>(menu_shader->passes))
      return;

   /* move selected pass to the bottom */
   for (i = pass; i < static_cast<int>(menu_shader->passes) - 1; i++)
      std::swap(menu_shader->pass[i], menu_shader->pass[i + 1]);

   menu_shader->passes--;

   menu_shader->flags   |= SHDR_FLAG_MODIFIED;

   onShaderApplyClicked();
}

void ShaderParamsDialog::onShaderApplyClicked()
{
   command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
}

void ShaderParamsDialog::updateRemovePresetButtonsState()
{
   settings_t      *settings         = config_get_ptr();
   const char *path_dir_video_shader = settings->paths.directory_video_shader;
   const char *path_dir_menu_config  = settings->paths.directory_menu_config;

   if (removeGlobalPresetAction)
      removeGlobalPresetAction->setEnabled(
            menu_shader_manager_auto_preset_exists(
               SHADER_PRESET_GLOBAL,
               path_dir_video_shader,
               path_dir_menu_config
               ));
   if (removeCorePresetAction)
      removeCorePresetAction->setEnabled(
            menu_shader_manager_auto_preset_exists(
               SHADER_PRESET_CORE,
               path_dir_video_shader,
               path_dir_menu_config
               ));
   if (removeParentPresetAction)
      removeParentPresetAction->setEnabled(
            menu_shader_manager_auto_preset_exists(
               SHADER_PRESET_PARENT,
               path_dir_video_shader,
               path_dir_menu_config
               ));
   if (removeGamePresetAction)
      removeGamePresetAction->setEnabled(
            menu_shader_manager_auto_preset_exists(
               SHADER_PRESET_GAME,
               path_dir_video_shader,
               path_dir_menu_config
               ));
}

void ShaderParamsDialog::reload()
{
   buildLayout();
}

void ShaderParamsDialog::buildLayout()
{
   unsigned i;
   bool hasPasses                           = false;
#if defined(HAVE_MENU)
   CheckableSettingsGroup *topSettingsGroup = NULL;
#endif
   QPushButton *loadButton                  = NULL;
   QPushButton *saveButton                  = NULL;
   QPushButton *removeButton                = NULL;
   QPushButton *removePassButton            = NULL;
   QPushButton *applyButton                 = NULL;
   QHBoxLayout *topButtonLayout             = NULL;
   QMenu *loadMenu                          = NULL;
   QMenu *saveMenu                          = NULL;
   QMenu *removeMenu                        = NULL;
   QMenu *removePassMenu                    = NULL;
   struct video_shader *menu_shader         = NULL;
   struct video_shader *video_shader        = NULL;
   struct video_shader *avail_shader        = NULL;
   const char *shader_path                  = NULL;

   getShaders(&menu_shader, &video_shader);

   /* NOTE: For some reason, menu_shader_get() returns a COPY
    * of what get_current_shader() gives us.
    * And if you want to be able to change shader settings/parameters
    * from both the raster menu and
    * Qt at the same time... you must change BOTH or one will
    * overwrite the other.
    *
    * AND, during a context reset, video_shader will be NULL
    * but not menu_shader, so don't totally bail
    * just because video_shader is NULL.
    *
    * Someone please fix this mess.
    */

   if (video_shader)
   {
      avail_shader = video_shader;

      if (video_shader->passes == 0)
         setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS));
   }
   /* Normally we'd only use video_shader,
    * but the Vulkan driver returns a NULL shader when there
    * are zero passes, so just fall back to menu_shader.
    */
   else if (menu_shader)
   {
      avail_shader = menu_shader;

      if (menu_shader->passes == 0)
         setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS));
   }
   else
   {
      setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS));

      /* no shader is available yet, just keep retrying until it is */
      QTimer::singleShot(0, this, SLOT(buildLayout()));
      return;
   }

   clearLayout();

   /* Only check video_shader for the path, menu_shader seems stale...
    * e.g. if you remove all the shader passes,
    * it still has the old path in it, but video_shader does not
    */
   if (video_shader)
   {
      if (*video_shader->path)
      {
         shader_path = video_shader->path;
         setWindowTitle(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER)) + ": " + QFileInfo(shader_path).fileName());
      }
   }
   else if (menu_shader)
   {
      if (*menu_shader->path)
      {
         shader_path = menu_shader->path;
         setWindowTitle(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER)) + ": " + QFileInfo(shader_path).fileName());
      }
   }
   else
      setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS));

   loadButton       = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_LOAD), this);
   saveButton       = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SAVE), this);
   removeButton     = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_REMOVE), this);
   removePassButton = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_REMOVE_PASSES), this);
   applyButton      = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_APPLY), this);

   loadMenu         = new QMenu(loadButton);

   loadMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET), this, SLOT(onShaderLoadPresetClicked()));
   loadMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SHADER_ADD_PASS), this, SLOT(onShaderAddPassClicked()));

   loadButton->setMenu(loadMenu);

   saveMenu         = new QMenu(saveButton);
   saveMenu->addAction(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS)) + "...", this, SLOT(onShaderSavePresetAsClicked()));
   saveMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GLOBAL), this, SLOT(onShaderSaveGlobalPresetClicked()));
   saveMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE), this, SLOT(onShaderSaveCorePresetClicked()));
   saveMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT), this, SLOT(onShaderSaveParentPresetClicked()));
   saveMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME), this, SLOT(onShaderSaveGamePresetClicked()));

   saveButton->setMenu(saveMenu);

   removeMenu = new QMenu(removeButton);
   removeGlobalPresetAction = removeMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL), this, SLOT(onShaderRemoveGlobalPresetClicked()));
   removeCorePresetAction   = removeMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE),   this, SLOT(onShaderRemoveCorePresetClicked()));
   removeParentPresetAction = removeMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT), this, SLOT(onShaderRemoveParentPresetClicked()));
   removeGamePresetAction   = removeMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME),   this, SLOT(onShaderRemoveGamePresetClicked()));

   removeButton->setMenu(removeMenu);

   connect(removeMenu, SIGNAL(aboutToShow()), this, SLOT(updateRemovePresetButtonsState()));

   removePassMenu = new QMenu(removeButton);

   /* When there are no passes, at least on first startup, it seems video_shader erroneously shows 1 pass, with an empty source file.
    * So we use menu_shader instead for that.
    */
   if (menu_shader)
   {
      for (i = 0; i < menu_shader->passes; i++)
      {
         QFileInfo fileInfo(menu_shader->pass[i].source.path);
         QString shaderBasename = fileInfo.completeBaseName();
         QAction        *action = removePassMenu->addAction(shaderBasename, this, SLOT(onShaderRemovePassClicked()));

         action->setData(i);
      }
   }

   removePassMenu->addAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SHADER_CLEAR_ALL_PASSES), this, SLOT(onShaderRemoveAllPassesClicked()));

   removePassButton->setMenu(removePassMenu);

   connect(applyButton, SIGNAL(clicked()), this, SLOT(onShaderApplyClicked()));

#if defined(HAVE_MENU)
   topSettingsGroup = new CheckableSettingsGroup(MENU_ENUM_LABEL_VIDEO_SHADERS_ENABLE);
   topSettingsGroup->add(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_REFERENCE);
   topSettingsGroup->add(MENU_ENUM_LABEL_SHADER_WATCH_FOR_CHANGES);
   topSettingsGroup->add(MENU_ENUM_LABEL_VIDEO_SHADER_REMEMBER_LAST_DIR);
#endif

   topButtonLayout = new QHBoxLayout();
   topButtonLayout->addWidget(loadButton);
   topButtonLayout->addWidget(saveButton);
   topButtonLayout->addWidget(removeButton);
   topButtonLayout->addWidget(removePassButton);
   topButtonLayout->addWidget(applyButton);

#if defined(HAVE_MENU)
   m_layout->addWidget(topSettingsGroup);
#endif
   m_layout->addLayout(topButtonLayout);

   /* NOTE: We assume that parameters are always grouped in order by the pass number, e.g., all parameters for pass 0 come first, then params for pass 1, etc. */
   for (i = 0; avail_shader && i < avail_shader->passes; i++)
   {
      QFormLayout                  *form = NULL;
      QGroupBox                *groupBox = NULL;
      QFileInfo fileInfo(avail_shader->pass[i].source.path);
      QString             shaderBasename = fileInfo.completeBaseName();
      QHBoxLayout *filterScaleHBoxLayout = NULL;
      QComboBox          *filterComboBox = new QComboBox(this);
      QComboBox           *scaleComboBox = new QComboBox(this);
      QToolButton        *moveDownButton = NULL;
      QToolButton          *moveUpButton = NULL;
      unsigned                         j = 0;

      /* Sometimes video_shader shows 1 pass with no source file, when there are really 0 passes. */
      if (shaderBasename.isEmpty())
         continue;

      hasPasses                          = true;

      filterComboBox->setProperty("pass", i);
      scaleComboBox->setProperty("pass", i);

      moveDownButton                     = new QToolButton(this);
      moveDownButton->setText("↓");
      moveDownButton->setProperty("pass", i);

      moveUpButton                       = new QToolButton(this);
      moveUpButton->setText("↑");
      moveUpButton->setProperty("pass", i);

      /* Can't move down if we're already at the bottom. */
      if (i < avail_shader->passes - 1)
         connect(moveDownButton, SIGNAL(clicked()),
               this, SLOT(onShaderPassMoveDownClicked()));
      else
         moveDownButton->setDisabled(true);

      /* Can't move up if we're already at the top. */
      if (i > 0)
         connect(moveUpButton, SIGNAL(clicked()),
               this, SLOT(onShaderPassMoveUpClicked()));
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

      filterComboBox->setCurrentIndex(static_cast<int>(avail_shader->pass[i].filter));
      scaleComboBox->setCurrentIndex(static_cast<int>(avail_shader->pass[i].fbo.scale_x));

      /* connect the signals only after the initial index is set */
      connect(filterComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onFilterComboBoxIndexChanged(int)));
      connect(scaleComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onScaleComboBoxIndexChanged(int)));

      form     = new QFormLayout();
      groupBox = new QGroupBox(shaderBasename);
      groupBox->setLayout(form);
      groupBox->setProperty("pass", i);
      groupBox->setContextMenuPolicy(Qt::CustomContextMenu);

      connect(groupBox, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onGroupBoxContextMenuRequested(const QPoint&)));

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

      for (j = 0; j < avail_shader->num_parameters; j++)
      {
         struct video_shader_parameter *param = &avail_shader->parameters[j];

         if (param->pass != static_cast<int>(i))
            continue;

         addShaderParam(param, form);
      }
   }

   if (!hasPasses)
   {
      QLabel *noParamsLabel = new QLabel(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_SHADER_NO_PASSES), this);
      noParamsLabel->setAlignment(Qt::AlignCenter);

      m_layout->addWidget(noParamsLabel);
   }

   m_layout->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));

   /* Why is this required?? The layout is corrupt without both resizes. */
   resize(width() + 1, height());
   show();
   resize(width() - 1, height());
}

void ShaderParamsDialog::onParameterLabelContextMenuRequested(const QPoint&)
{
   QVariant paramVariant;
   QString parameter;
   QPointer<QAction> action;
   QList<QAction*> actions;
   QScopedPointer<QAction> resetParamAction;
   QLabel *label = qobject_cast<QLabel*>(sender());

   if (!label)
      return;

   paramVariant  = label->property("parameter");

   if (!paramVariant.isValid())
      return;

   parameter     = paramVariant.toString();

   resetParamAction.reset(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_RESET_PARAMETER), 0));

   actions.append(resetParamAction.data());

   action        = QMenu::exec(actions, QCursor::pos(), NULL, label);

   if (!action)
      return;

   if (action == resetParamAction.data())
      onShaderResetParameter(parameter);
}

void ShaderParamsDialog::onGroupBoxContextMenuRequested(const QPoint&)
{
   QPointer<QAction> action;
   QList<QAction*> actions;
   QScopedPointer<QAction> resetPassAction;
   QScopedPointer<QAction> resetAllPassesAction;
   QVariant passVariant;
   int pass            = 0;
   bool ok             = false;
   QGroupBox *groupBox = qobject_cast<QGroupBox*>(sender());

   if (!groupBox)
      return;

   passVariant         = groupBox->property("pass");

   if (!passVariant.isValid())
      return;

   pass = passVariant.toInt(&ok);

   if (!ok)
      return;

   resetPassAction.reset(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_RESET_PASS), 0));
   resetAllPassesAction.reset(new QAction(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_RESET_ALL_PASSES), 0));

   actions.append(resetPassAction.data());
   actions.append(resetAllPassesAction.data());

   action              = QMenu::exec(actions, QCursor::pos(), NULL, groupBox);

   if (!action)
      return;

   if (action == resetPassAction.data())
      onShaderResetPass(pass);
   else if (action == resetAllPassesAction.data())
      onShaderResetAllPasses();
}

void ShaderParamsDialog::addShaderParam(struct video_shader_parameter *param, QFormLayout *form)
{
   QString      desc = param->desc;
   QString parameter = param->id;
   QLabel     *label = new QLabel(desc);

   label->setProperty("parameter", parameter);
   label->setContextMenuPolicy(Qt::CustomContextMenu);

   connect(label, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(onParameterLabelContextMenuRequested(const QPoint&)));

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

      form->addRow(label, checkBox);
   }
   else
   {
      QDoubleSpinBox *doubleSpinBox = NULL;
      QSpinBox *spinBox             = NULL;
      QHBoxLayout *box              = new QHBoxLayout();
      QSlider *slider               = new QSlider(Qt::Horizontal, this);
      double value                  = lerp(
            param->minimum, param->maximum, 0, 100, param->current);
      double intpart                = 0;
      bool stepIsFractional         = modf(param->step, &intpart);

      slider->setRange(0, 100);
      slider->setSingleStep(1);
      slider->setValue(value);
      slider->setProperty("param", parameter);

      connect(slider, SIGNAL(valueChanged(int)),
            this, SLOT(onShaderParamSliderValueChanged(int)));

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

      form->addRow(label, box);
   }
}

void ShaderParamsDialog::onShaderParamCheckBoxClicked()
{
   QVariant paramVariant;
   QCheckBox *checkBox               = qobject_cast<QCheckBox*>(sender());
   struct video_shader *menu_shader  = NULL;
   struct video_shader *video_shader = NULL;

   getShaders(&menu_shader, &video_shader);

   if (!checkBox)
      return;

   if (menu_shader && menu_shader->passes == 0)
      return;

   paramVariant = checkBox->property("param");

   if (paramVariant.isValid())
   {
      QString parameter = paramVariant.toString();

      if (menu_shader)
      {
         int i;
         struct video_shader_parameter *param = NULL;

         for (i = 0; i < static_cast<int>(menu_shader->num_parameters); i++)
         {
            QString id = menu_shader->parameters[i].id;

            if (id == parameter)
               param = &menu_shader->parameters[i];
         }

         if (param)
            param->current = (checkBox->isChecked() ? param->maximum : param->minimum);
      }

      if (video_shader)
      {
         int i;
         struct video_shader_parameter *param = NULL;

         for (i = 0; i < static_cast<int>(video_shader->num_parameters); i++)
         {
            QString id = video_shader->parameters[i].id;

            if (id == parameter)
               param = &video_shader->parameters[i];
         }

         if (param)
            param->current = (checkBox->isChecked() ? param->maximum : param->minimum);
      }

      video_shader->flags   |= SHDR_FLAG_MODIFIED;
   }
}

void ShaderParamsDialog::onShaderParamSliderValueChanged(int)
{
   QVariant spinBoxVariant;
   QVariant paramVariant;
   QSlider *slider = qobject_cast<QSlider*>(sender());
   struct video_shader *menu_shader  = NULL;
   struct video_shader *video_shader = NULL;
   double                   newValue = 0.0;

   getShaders(&menu_shader, &video_shader);

   if (!slider)
      return;

   spinBoxVariant = slider->property("spinBox");
   paramVariant   = slider->property("param");

   if (paramVariant.isValid())
   {
      QString parameter = paramVariant.toString();

      if (menu_shader)
      {
         int i;
         struct video_shader_parameter *param = NULL;

         for (i = 0; i < static_cast<int>(menu_shader->num_parameters); i++)
         {
            QString id = menu_shader->parameters[i].id;

            if (id == parameter)
               param = &menu_shader->parameters[i];
         }

         if (param)
         {
            newValue = lerp(0, 100, param->minimum, param->maximum, slider->value());
            newValue = round(newValue / param->step) * param->step;
            param->current = newValue;
         }
      }

      if (video_shader)
      {
         int i;
         struct video_shader_parameter *param = NULL;

         for (i = 0; i < static_cast<int>(video_shader->num_parameters); i++)
         {
            QString id = video_shader->parameters[i].id;

            if (id == parameter)
               param = &video_shader->parameters[i];
         }

         if (param)
         {
            newValue = lerp(0, 100, param->minimum, param->maximum, slider->value());
            newValue = round(newValue / param->step) * param->step;
            param->current = newValue;
         }

         video_shader->flags   |= SHDR_FLAG_MODIFIED;
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
   QVariant sliderVariant;
   QVariant paramVariant;
   QSpinBox                 *spinBox = qobject_cast<QSpinBox*>(sender());
   QSlider                   *slider = NULL;
   struct video_shader  *menu_shader = NULL;
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
      QString parameter = paramVariant.toString();
      double   newValue = 0.0;

      if (menu_shader)
      {
         int i;
         struct video_shader_parameter *param = NULL;

         for (i = 0; i < static_cast<int>(menu_shader->num_parameters); i++)
         {
            QString id = menu_shader->parameters[i].id;

            if (id == parameter)
               param = &menu_shader->parameters[i];
         }

         if (param)
         {
            param->current = value;
            newValue       = lerp(
                  param->minimum, param->maximum, 0, 100, param->current);
            slider->blockSignals(true);
            slider->setValue(newValue);
            slider->blockSignals(false);
         }
      }

      if (video_shader)
      {
         int i;
         struct video_shader_parameter *param = NULL;

         for (i = 0; i < static_cast<int>(video_shader->num_parameters); i++)
         {
            QString id = video_shader->parameters[i].id;

            if (id == parameter)
               param = &video_shader->parameters[i];
         }

         if (param)
         {
            param->current = value;
            newValue       = lerp(
                  param->minimum, param->maximum, 0, 100, param->current);
            slider->blockSignals(true);
            slider->setValue(newValue);
            slider->blockSignals(false);
         }

         video_shader->flags   |= SHDR_FLAG_MODIFIED;
      }
   }
}

void ShaderParamsDialog::onShaderParamDoubleSpinBoxValueChanged(double value)
{
   QVariant sliderVariant;
   QVariant paramVariant;
   QSlider                   *slider = NULL;
   QDoubleSpinBox     *doubleSpinBox = qobject_cast<QDoubleSpinBox*>(sender());
   struct video_shader  *menu_shader = NULL;
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
      QString parameter = paramVariant.toString();
      double newValue   = 0.0;

      if (menu_shader)
      {
         int i;
         struct video_shader_parameter *param = NULL;

         for (i = 0; i < static_cast<int>(menu_shader->num_parameters); i++)
         {
            QString id = menu_shader->parameters[i].id;

            if (id == parameter)
               param = &menu_shader->parameters[i];
         }

         if (param)
         {
            param->current = value;
            newValue       = lerp(
                  param->minimum, param->maximum, 0, 100, param->current);
            slider->blockSignals(true);
            slider->setValue(newValue);
            slider->blockSignals(false);
         }
      }

      if (video_shader)
      {
         int i;
         struct video_shader_parameter *param = NULL;

         for (i = 0; i < static_cast<int>(video_shader->num_parameters); i++)
         {
            QString id = video_shader->parameters[i].id;

            if (id == parameter)
               param = &video_shader->parameters[i];
         }

         if (param)
         {
            param->current = value;
            newValue       = lerp(
                  param->minimum, param->maximum, 0, 100, param->current);
            slider->blockSignals(true);
            slider->setValue(newValue);
            slider->blockSignals(false);
         }

         video_shader->flags   |= SHDR_FLAG_MODIFIED;
      }
   }
}
#endif
#endif


#undef TEMP_EXTENSION
#undef USER_AGENT
#define USER_AGENT "RetroArch-WIMP/" PACKAGE_VERSION
#define PARTIAL_EXTENSION ".partial"
#define TEMP_EXTENSION ".tmp"
#define THUMBNAILPACK_URL_HEADER "http://thumbnailpacks.libretro.com/"
#define THUMBNAILPACK_EXTENSION ".zip"
#define THUMBNAIL_URL_HEADER "https://thumbnails.libretro.com/"
#define THUMBNAIL_IMAGE_EXTENSION ".png"

/* Userdata structs for task callbacks */
typedef struct qt_download_userdata
{
   MainWindow *mainwindow;
   char system[PATH_MAX_LENGTH];
   char title[PATH_MAX_LENGTH];
   char download_type[64];
   char output_path[PATH_MAX_LENGTH];
   bool is_playlist_download;
} qt_download_userdata_t;

static void cb_extract_thumbnail_pack(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   decompress_task_data_t *dec = (decompress_task_data_t*)task_data;
   MainWindow *mainwindow      = (MainWindow*)user_data;

   if (err)
      RARCH_ERR("[Qt] %s", err);

   if (dec)
   {
      if (filestream_exists(dec->source_file))
         filestream_delete(dec->source_file);

      free(dec->source_file);
      free(dec);
   }

   mainwindow->onThumbnailPackExtractFinished(!err || !*err);
}

static void cb_http_thumbnail_pack(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   http_transfer_data_t *data  = (http_transfer_data_t*)task_data;
   qt_download_userdata_t *ud  = (qt_download_userdata_t*)user_data;

   if (!ud)
      return;

   if (!data || !data->data || data->status != 200 || err)
   {
      RARCH_ERR("[Qt] Thumbnail pack download failed (HTTP %d).\n",
            data ? data->status : 0);
      if (ud->mainwindow)
         ud->mainwindow->showErrorMessageDeferred(
               QString(msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR)));
      free(ud);
      return;
   }

   /* Write downloaded data to .partial file */
   if (!filestream_write_file(ud->output_path, data->data, data->len))
   {
      RARCH_ERR("[Qt] Could not write thumbnail pack to \"%s\".\n",
            ud->output_path);
      free(ud);
      return;
   }

   /* Rename .partial to final name */
   {
      char final_path[PATH_MAX_LENGTH];
      strlcpy(final_path, ud->output_path, sizeof(final_path));

      /* Strip PARTIAL_EXTENSION from end */
      {
         char *ext = strstr(final_path, PARTIAL_EXTENSION);
         if (ext) *ext = '\0';
      }

      if (path_is_valid(final_path))
         filestream_delete(final_path);

      if (filestream_rename(ud->output_path, final_path) == 0)
      {
         settings_t *settings = config_get_ptr();
         if (settings)
         {
            RARCH_LOG("[Qt] Thumbnail pack download finished successfully.\n");
            if (ud->mainwindow)
               QMetaObject::invokeMethod(ud->mainwindow,
                     "onExtractArchive",
                     Q_ARG(QString, QString(final_path)),
                     Q_ARG(QString, QString(settings->paths.directory_thumbnails)),
                     Q_ARG(QString, QString(TEMP_EXTENSION)),
                     Q_ARG(retro_task_callback_t, cb_extract_thumbnail_pack));
         }
      }
      else
      {
         RARCH_ERR("[Qt] Thumbnail pack download finished, but file could not be renamed.\n");
         if (ud->mainwindow)
            ud->mainwindow->showErrorMessageDeferred(
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE));
      }
   }

   free(ud);
}

static void cb_http_thumbnail(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   http_transfer_data_t *data  = (http_transfer_data_t*)task_data;
   qt_download_userdata_t *ud  = (qt_download_userdata_t*)user_data;

   if (!ud)
      return;

   if (!data || !data->data || data->status != 200 || err)
   {
      if (data && data->status != 200)
         RARCH_ERR("[Qt] Thumbnail download failed with HTTP status: %d.\n",
               data->status);

      /* Continue to next type if available */
      if (ud->mainwindow)
      {
         if (ud->is_playlist_download)
            ud->mainwindow->onPlaylistThumbnailDownloadFinishedInternal(
                  ud->system, ud->title, NULL, false);
         else
            ud->mainwindow->onSingleThumbnailDownloadFinishedInternal(
                  ud->system, ud->title, NULL, false);
      }
      free(ud);
      return;
   }

   /* Create output directory */
   {
      char output_dir[PATH_MAX_LENGTH];
      strlcpy(output_dir, ud->output_path, sizeof(output_dir));
      path_basedir_wrapper(output_dir);
      path_mkdir(output_dir);
   }

   /* Write downloaded data to .partial file */
   if (!filestream_write_file(ud->output_path, data->data, data->len))
   {
      RARCH_ERR("[Qt] Could not write thumbnail to \"%s\".\n",
            ud->output_path);
      if (ud->mainwindow)
      {
         if (ud->is_playlist_download)
            ud->mainwindow->onPlaylistThumbnailDownloadFinishedInternal(
                  ud->system, ud->title, NULL, false);
         else
            ud->mainwindow->onSingleThumbnailDownloadFinishedInternal(
                  ud->system, ud->title, NULL, false);
      }
      free(ud);
      return;
   }

   /* Rename .partial to final name */
   {
      char final_path[PATH_MAX_LENGTH];
      strlcpy(final_path, ud->output_path, sizeof(final_path));

      {
         char *ext = strstr(final_path, PARTIAL_EXTENSION);
         if (ext) *ext = '\0';
      }

      if (path_is_valid(final_path))
         filestream_delete(final_path);

      if (filestream_rename(ud->output_path, final_path) != 0)
      {
         RARCH_ERR("[Qt] Thumbnail download finished, but file could not be renamed.\n");
      }
      else
         RARCH_LOG("[Qt] Thumbnail download finished: \"%s\".\n", final_path);

      if (ud->mainwindow)
      {
         if (ud->is_playlist_download)
            ud->mainwindow->onPlaylistThumbnailDownloadFinishedInternal(
                  ud->system, ud->title, final_path, true);
         else
            ud->mainwindow->onSingleThumbnailDownloadFinishedInternal(
                  ud->system, ud->title, final_path, true);
      }
   }

   free(ud);
}

/* ---- Thumbnail Pack Download ---- */

void MainWindow::onThumbnailPackDownloadCanceled()
{
   if (m_currentHttpTask)
   {
      task_set_flags(m_currentHttpTask,
            RETRO_TASK_FLG_CANCELLED, true);
      m_currentHttpTask = NULL;
   }
}

void MainWindow::downloadAllThumbnails(QString system, QUrl url)
{
   QString urlString;
   QByteArray urlArray;
   QByteArray fileNameArray;
   settings_t *settings = config_get_ptr();
   qt_download_userdata_t *ud = NULL;
   const char *urlData  = NULL;

   if (!settings)
      return;

   urlString = QString(THUMBNAILPACK_URL_HEADER)
      + system
      + THUMBNAILPACK_EXTENSION;

   if (url.isEmpty())
      url = urlString;

   urlArray = url.toEncoded();
   urlData  = urlArray.constData();

   ud = (qt_download_userdata_t*)calloc(1, sizeof(*ud));
   if (!ud)
      return;

   ud->mainwindow = this;
   strlcpy(ud->system, system.toUtf8().constData(), sizeof(ud->system));

   {
      const char *path_dir_thumbnails = settings->paths.directory_thumbnails;
      QString fileName = QString(path_dir_thumbnails)
         + "/" + system + THUMBNAILPACK_EXTENSION + PARTIAL_EXTENSION;

      fileNameArray = fileName.toUtf8();

      /* Ensure directory exists */
      path_mkdir(path_dir_thumbnails);

      strlcpy(ud->output_path,
            fileNameArray.constData(),
            sizeof(ud->output_path));
   }

   RARCH_LOG("[Qt] Starting thumbnail pack download...\n");
   RARCH_LOG("[Qt] Downloading URL \"%s\"\n", urlData);

   m_thumbnailPackDownloadProgressDialog->setWindowModality(Qt::NonModal);
   m_thumbnailPackDownloadProgressDialog->setMinimumDuration(0);
   m_thumbnailPackDownloadProgressDialog->setRange(0, 100);
   m_thumbnailPackDownloadProgressDialog->setAutoClose(true);
   m_thumbnailPackDownloadProgressDialog->setAutoReset(true);
   m_thumbnailPackDownloadProgressDialog->setValue(0);
   m_thumbnailPackDownloadProgressDialog->setLabelText(
         QString(msg_hash_to_str(MSG_DOWNLOADING)) + "...");
   m_thumbnailPackDownloadProgressDialog->setCancelButtonText(tr("Cancel"));
   m_thumbnailPackDownloadProgressDialog->show();

   m_currentHttpTask = (retro_task_t*)task_push_http_transfer_with_user_agent(
         urlData, true, NULL, USER_AGENT,
         cb_http_thumbnail_pack, ud);

   if (!m_currentHttpTask)
   {
      free(ud);
      m_thumbnailPackDownloadProgressDialog->cancel();
      RARCH_ERR("[Qt] Failed to start HTTP task for thumbnail pack.\n");
   }
}

void MainWindow::onThumbnailPackExtractFinished(bool success)
{
   m_updateProgressDialog->cancel();

   if (!success)
   {
      RARCH_ERR("[Qt] Thumbnail pack extraction failed.\n");
      emit showErrorMessageDeferred(msg_hash_to_str(MSG_DECOMPRESSION_FAILED));
      return;
   }

   RARCH_LOG("[Qt] Thumbnail pack extracted successfully.\n");

   emit showInfoMessageDeferred(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_PACK_DOWNLOADED_SUCCESSFULLY));

   updateVisibleItems();

   /* Reload thumbnail image */
   emit itemChanged();
}

/* ---- Single Thumbnail Download ---- */

void MainWindow::onThumbnailDownloadCanceled()
{
   if (m_currentHttpTask)
   {
      task_set_flags(m_currentHttpTask,
            RETRO_TASK_FLG_CANCELLED, true);
      m_currentHttpTask = NULL;
   }
   m_pendingThumbnailDownloadTypes.clear();
}

void MainWindow::onDownloadThumbnail(QString system, QString title)
{
   downloadThumbnail(system, title);
}

void MainWindow::onSingleThumbnailDownloadFinishedInternal(
      const char *system, const char *title, const char *final_path, bool success)
{
   QString systemStr(system ? system : "");
   QString titleStr(title ? title : "");

   m_currentHttpTask = NULL;

   if (success && final_path)
   {
      /* Force reload of current item's thumbnail */
      QModelIndex index = getCurrentContentIndex();
      if (index.isValid())
      {
         m_playlistModel->reloadThumbnail(index);
         onCurrentItemChanged(index);
      }

      updateVisibleItems();
   }

   if (!m_pendingThumbnailDownloadTypes.isEmpty())
   {
      /* Defer next download to let Qt event loop process */
      QMetaObject::invokeMethod(this, "onDownloadThumbnail",
            Qt::QueuedConnection,
            Q_ARG(QString, systemStr),
            Q_ARG(QString, titleStr));
   }
   else
   {
      m_thumbnailDownloadProgressDialog->cancel();
   }
}

void MainWindow::downloadThumbnail(QString system, QString title, QUrl url)
{
   QString urlString;
   QString downloadType;
   QByteArray urlArray;
   QByteArray fileNameArray;
   const char *urlData       = NULL;
   settings_t *settings      = config_get_ptr();
   qt_download_userdata_t *ud = NULL;

   if (!settings || m_pendingThumbnailDownloadTypes.isEmpty())
      return;

   title        = getScrubbedString(title);
   downloadType = m_pendingThumbnailDownloadTypes.takeFirst();
   urlString    = QString(THUMBNAIL_URL_HEADER)
      + system + "/" + downloadType + "/" + title + THUMBNAIL_IMAGE_EXTENSION;

   if (url.isEmpty())
      url = urlString;

   urlArray = url.toEncoded();
   urlData  = urlArray.constData();

   ud = (qt_download_userdata_t*)calloc(1, sizeof(*ud));
   if (!ud)
      return;

   ud->mainwindow = this;
   ud->is_playlist_download = false;
   strlcpy(ud->system, system.toUtf8().constData(), sizeof(ud->system));
   strlcpy(ud->title, title.toUtf8().constData(), sizeof(ud->title));
   strlcpy(ud->download_type, downloadType.toUtf8().constData(),
         sizeof(ud->download_type));

   {
      const char *path_dir_thumbnails = settings->paths.directory_thumbnails;
      QString dirString = QString(path_dir_thumbnails) + "/" + system + "/" + downloadType;
      QString fileName  = dirString + "/" + title + THUMBNAIL_IMAGE_EXTENSION + PARTIAL_EXTENSION;

      fileNameArray = fileName.toUtf8();

      path_mkdir(dirString.toUtf8().constData());

      strlcpy(ud->output_path,
            fileNameArray.constData(),
            sizeof(ud->output_path));
   }

   RARCH_LOG("[Qt] Starting thumbnail download...\n");
   RARCH_LOG("[Qt] Downloading URL %s\n", urlData);

   m_thumbnailDownloadProgressDialog->setWindowModality(Qt::NonModal);
   m_thumbnailDownloadProgressDialog->setMinimumDuration(0);
   m_thumbnailDownloadProgressDialog->setRange(0, 100);
   m_thumbnailDownloadProgressDialog->setAutoClose(true);
   m_thumbnailDownloadProgressDialog->setAutoReset(true);
   m_thumbnailDownloadProgressDialog->setValue(0);
   m_thumbnailDownloadProgressDialog->setLabelText(
         QString(msg_hash_to_str(MSG_DOWNLOADING)) + "...");
   m_thumbnailDownloadProgressDialog->setCancelButtonText(tr("Cancel"));
   m_thumbnailDownloadProgressDialog->show();

   m_currentHttpTask = (retro_task_t*)task_push_http_transfer_with_user_agent(
         urlData, true, NULL, USER_AGENT,
         cb_http_thumbnail, ud);

   if (!m_currentHttpTask)
   {
      free(ud);
      m_thumbnailDownloadProgressDialog->cancel();
      RARCH_ERR("[Qt] Failed to start HTTP task for thumbnail.\n");
   }
}

/* ---- Playlist Thumbnail Download ---- */

void MainWindow::onPlaylistThumbnailDownloadCanceled()
{
   m_playlistThumbnailDownloadWasCanceled = true;
   if (m_currentHttpTask)
   {
      task_set_flags(m_currentHttpTask,
            RETRO_TASK_FLG_CANCELLED, true);
      m_currentHttpTask = NULL;
   }
   m_pendingPlaylistThumbnails.clear();
}

void MainWindow::onPlaylistThumbnailDownloadFinishedInternal(
      const char *system, const char *title, const char *final_path, bool success)
{
   m_currentHttpTask = NULL;

   if (success)
   {
      m_downloadedThumbnails++;
      if (final_path)
         m_playlistModel->reloadThumbnailPath(QString(final_path));
   }
   else
      m_failedThumbnails++;

   if (m_playlistThumbnailDownloadWasCanceled)
      return;

   if (m_pendingPlaylistThumbnails.count() > 0)
   {
      QHash<QString, QString> nextThumbnail = m_pendingPlaylistThumbnails.takeAt(0);

      m_playlistThumbnailDownloadProgressDialog->setValue(
              m_playlistThumbnailDownloadProgressDialog->maximum()
            - m_pendingPlaylistThumbnails.count());
      {
         QString labelText = QString(msg_hash_to_str(MSG_DOWNLOADING))
            + "...\n"
            + QString(msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_PLAYLIST_THUMBNAIL_PROGRESS)).arg(
                  m_downloadedThumbnails).arg(m_failedThumbnails);
         m_playlistThumbnailDownloadProgressDialog->setLabelText(labelText);
      }

      downloadNextPlaylistThumbnail(
            nextThumbnail.value("db_name"),
            nextThumbnail.value("label_noext"),
            nextThumbnail.value("type"));
   }
   else
   {
      m_playlistThumbnailDownloadProgressDialog->cancel();
      /* Force reload of current item's thumbnail */
      QModelIndex index = getCurrentContentIndex();
      if (index.isValid())
      {
         m_playlistModel->reloadThumbnail(index);
         onCurrentItemChanged(index);
      }
      updateVisibleItems();
   }
}

void MainWindow::downloadNextPlaylistThumbnail(
      QString system, QString title, QString type, QUrl url)
{
   QString urlString;
   QByteArray urlArray;
   QByteArray fileNameArray;
   const char *urlData       = NULL;
   settings_t *settings      = config_get_ptr();
   qt_download_userdata_t *ud = NULL;

   if (!settings)
      return;

   title = getScrubbedString(title);

   urlString = QString(THUMBNAIL_URL_HEADER)
      + system + "/" + type + "/" + title + THUMBNAIL_IMAGE_EXTENSION;

   if (url.isEmpty())
      url = urlString;

   urlArray = url.toEncoded();
   urlData  = urlArray.constData();

   ud = (qt_download_userdata_t*)calloc(1, sizeof(*ud));
   if (!ud)
   {
      m_failedThumbnails++;
      return;
   }

   ud->mainwindow = this;
   ud->is_playlist_download = true;
   strlcpy(ud->system, system.toUtf8().constData(), sizeof(ud->system));
   strlcpy(ud->title, title.toUtf8().constData(), sizeof(ud->title));
   strlcpy(ud->download_type, type.toUtf8().constData(),
         sizeof(ud->download_type));

   {
      const char *path_dir_thumbnails = settings->paths.directory_thumbnails;
      QString dirString = QString(path_dir_thumbnails)
         + "/" + system + "/" + type;

      QString fileName  = dirString + "/" + title
         + THUMBNAIL_IMAGE_EXTENSION + PARTIAL_EXTENSION;

      fileNameArray = fileName.toUtf8();

      /* Create all thumbnail type dirs */
      path_mkdir((QString(path_dir_thumbnails)
               + "/" + system + "/" + THUMBNAIL_BOXART).toUtf8().constData());
      path_mkdir((QString(path_dir_thumbnails)
               + "/" + system + "/" + THUMBNAIL_SCREENSHOT).toUtf8().constData());
      path_mkdir((QString(path_dir_thumbnails)
               + "/" + system + "/" + THUMBNAIL_TITLE).toUtf8().constData());
      path_mkdir((QString(path_dir_thumbnails)
               + "/" + system + "/" + THUMBNAIL_LOGO).toUtf8().constData());

      strlcpy(ud->output_path,
            fileNameArray.constData(),
            sizeof(ud->output_path));
   }

   m_currentHttpTask = (retro_task_t*)task_push_http_transfer_with_user_agent(
         urlData, true, NULL, USER_AGENT,
         cb_http_thumbnail, ud);

   if (!m_currentHttpTask)
   {
      free(ud);
      m_failedThumbnails++;

      if (m_pendingPlaylistThumbnails.count() > 0)
      {
         QHash<QString, QString> nextThumbnail = m_pendingPlaylistThumbnails.takeAt(0);
         downloadNextPlaylistThumbnail(
               nextThumbnail.value("db_name"),
               nextThumbnail.value("label_noext"),
               nextThumbnail.value("type"));
      }
      else
         m_playlistThumbnailDownloadProgressDialog->cancel();
   }
}

void MainWindow::downloadPlaylistThumbnails(QString playlistPath)
{
   int i, count;
   QFile playlistFile(playlistPath);
   settings_t *settings = config_get_ptr();

   if (!settings || !playlistFile.exists())
      return;

   m_pendingPlaylistThumbnails.clear();
   m_downloadedThumbnails                 = 0;
   m_failedThumbnails                     = 0;
   m_playlistThumbnailDownloadWasCanceled = false;

   count = m_playlistModel->rowCount();

   if (count == 0)
      return;

   for (i = 0; i < count; i++)
   {
      QHash<QString, QString> hash;
      QHash<QString, QString> hash2;
      QHash<QString, QString> hash3;
      QHash<QString, QString> hash4;
      const QHash<QString, QString> &itemHash =
         m_playlistModel->index(i, 0).data(
               PlaylistModel::HASH).value< QHash<QString, QString> >();

      hash["db_name"]     = itemHash.value("db_name");
      hash["label_noext"] = itemHash.value("label_noext");
      hash["type"]        = THUMBNAIL_BOXART;

      hash2               = hash;
      hash3               = hash;
      hash4               = hash;

      hash2["type"]       = THUMBNAIL_SCREENSHOT;
      hash3["type"]       = THUMBNAIL_TITLE;
      hash4["type"]       = THUMBNAIL_LOGO;

      m_pendingPlaylistThumbnails.append(hash);
      m_pendingPlaylistThumbnails.append(hash2);
      m_pendingPlaylistThumbnails.append(hash3);
      m_pendingPlaylistThumbnails.append(hash4);
   }

   m_playlistThumbnailDownloadProgressDialog->setWindowModality(Qt::NonModal);
   m_playlistThumbnailDownloadProgressDialog->setMinimumDuration(0);
   m_playlistThumbnailDownloadProgressDialog->setRange(0,
         m_pendingPlaylistThumbnails.count());
   m_playlistThumbnailDownloadProgressDialog->setAutoClose(true);
   m_playlistThumbnailDownloadProgressDialog->setAutoReset(true);
   m_playlistThumbnailDownloadProgressDialog->setValue(0);
   m_playlistThumbnailDownloadProgressDialog->setLabelText(
         QString(msg_hash_to_str(MSG_DOWNLOADING)) + "...");
   m_playlistThumbnailDownloadProgressDialog->setCancelButtonText(tr("Cancel"));
   m_playlistThumbnailDownloadProgressDialog->show();

   {
      QHash<QString, QString> firstThumbnail =
         m_pendingPlaylistThumbnails.takeAt(0);
      downloadNextPlaylistThumbnail(
            firstThumbnail.value("db_name"),
            firstThumbnail.value("label_noext"),
            firstThumbnail.value("type"));
   }
}

AchievementsCategory::AchievementsCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS);
   setCategoryIcon("menu_achievements");
}

QVector<OptionsPage*> AchievementsCategory::pages()
{
   QVector<OptionsPage*> pages;
   pages << new AchievementsPage(this);
   return pages;
}

AchievementsPage::AchievementsPage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *AchievementsPage::widget()
{
   unsigned i;
   QWidget             *widget = new QWidget;
   QVBoxLayout         *layout = new QVBoxLayout;
   settings_t*        settings = config_get_ptr();
   CheckBox   *enabledCheckBox = new CheckBox(MENU_ENUM_LABEL_CHEEVOS_ENABLE);
   file_list_t    *generalList = (file_list_t*)calloc(1, sizeof(*generalList));
   file_list_t *appearanceList = (file_list_t*)calloc(1, sizeof(*appearanceList));
   file_list_t *visibilityList = (file_list_t*)calloc(1, sizeof(*visibilityList));

   m_generalGroup              = new SettingsGroup("General");
   m_appearanceGroup           = new SettingsGroup(msg_hash_to_str(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_SETTINGS));
   m_visibilityGroup           = new SettingsGroup(msg_hash_to_str(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SETTINGS));

   menu_displaylist_build_list(generalList, settings,
      DISPLAYLIST_RETRO_ACHIEVEMENTS_SETTINGS_LIST, true);

   for (i = 0; i < generalList->size; i++)
   {
      menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
         file_list_get_actiondata_at_offset(generalList, i);

      if (cbs->enum_idx == MENU_ENUM_LABEL_CHEEVOS_ENABLE)
         continue;

      m_generalGroup->add(cbs->enum_idx);
   }
   file_list_free(generalList);

   menu_displaylist_build_list(appearanceList, settings,
      DISPLAYLIST_CHEEVOS_APPEARANCE_SETTINGS_LIST, true);

   for (i = 0; i < appearanceList->size; i++)
   {
      menu_file_list_cbs_t* cbs = (menu_file_list_cbs_t*)
         file_list_get_actiondata_at_offset(appearanceList, i);

      m_appearanceGroup->add(cbs->enum_idx);
   }
   file_list_free(appearanceList);

   menu_displaylist_build_list(visibilityList, settings,
      DISPLAYLIST_CHEEVOS_VISIBILITY_SETTINGS_LIST, true);

   for (i = 0; i < visibilityList->size; i++)
   {
      menu_file_list_cbs_t* cbs = (menu_file_list_cbs_t*)
         file_list_get_actiondata_at_offset(visibilityList, i);

      m_visibilityGroup->add(cbs->enum_idx);
   }
   file_list_free(visibilityList);

   connect(enabledCheckBox, SIGNAL(stateChanged(int)),
           this,            SLOT(onAchievementEnabledChanged(int)));

   onAchievementEnabledChanged(enabledCheckBox->checkState());

   layout->addWidget(enabledCheckBox);
   layout->addWidget(m_generalGroup);
   layout->addWidget(m_appearanceGroup);
   layout->addWidget(m_visibilityGroup);

   layout->addStretch();

   widget->setLayout(layout);

   return widget;
}

void AchievementsPage::onAchievementEnabledChanged(int state)
{
   m_generalGroup->setDisabled(state == Qt::Unchecked);
   m_appearanceGroup->setDisabled(state == Qt::Unchecked);
   m_visibilityGroup->setDisabled(state == Qt::Unchecked);
}

AudioCategory::AudioCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS);
   setCategoryIcon("menu_audio");
}

QVector<OptionsPage*> AudioCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new AudioPage(this);
   pages << new MenuSoundsPage(this);

   return pages;
}

AudioPage::AudioPage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *AudioPage::widget()
{
   QWidget *widget                = new QWidget;
   QVBoxLayout *layout            = new QVBoxLayout;
   SettingsGroup *outputGroup     = new SettingsGroup("Output");
   SettingsGroup *resamplerGroup  = new SettingsGroup("Resampler");
   SettingsGroup *syncGroup       = new SettingsGroup(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_AUDIO_SYNC));
   SettingsGroup *dspGroup        = new SettingsGroup("DSP plugin");
   SettingsGroup *volumeGroup     = new SettingsGroup("Volume");
   QGridLayout *volumeLayout      = new QGridLayout();

   outputGroup->add(MENU_ENUM_LABEL_AUDIO_ENABLE);
   outputGroup->add(MENU_ENUM_LABEL_AUDIO_DRIVER);
   outputGroup->add(MENU_ENUM_LABEL_AUDIO_DEVICE);
   outputGroup->add(MENU_ENUM_LABEL_AUDIO_LATENCY);

   resamplerGroup->add(MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER);
   resamplerGroup->add(MENU_ENUM_LABEL_AUDIO_RESAMPLER_QUALITY);
   resamplerGroup->add(MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE);

   syncGroup->add(MENU_ENUM_LABEL_AUDIO_SYNC);
   syncGroup->add(MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW);
   syncGroup->add(MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA);

   dspGroup->add(MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN);

   volumeLayout->addWidget(new QLabel(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME), widget),
         1, 1);
   volumeLayout->addWidget(new CheckableIcon(
            MENU_ENUM_LABEL_AUDIO_MUTE,
            qApp->style()->standardIcon(QStyle::SP_MediaVolumeMuted)),
         1, 2);
   volumeLayout->addLayout(new FloatSliderAndSpinBox(MENU_ENUM_LABEL_AUDIO_VOLUME),
         1, 3, 1, 1);

   volumeLayout->addWidget(new QLabel(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME), widget),
         2, 1);
   volumeLayout->addWidget(new CheckableIcon(
            MENU_ENUM_LABEL_AUDIO_MIXER_MUTE,
            qApp->style()->standardIcon(QStyle::SP_MediaVolumeMuted)),
         2, 2);
   volumeLayout->addLayout(new FloatSliderAndSpinBox(
            MENU_ENUM_LABEL_AUDIO_MIXER_VOLUME),
         2, 3, 1, 1);

   volumeGroup->addRow(volumeLayout);

   volumeGroup->add(MENU_ENUM_LABEL_AUDIO_FASTFORWARD_MUTE);
   volumeGroup->add(MENU_ENUM_LABEL_AUDIO_FASTFORWARD_SPEEDUP);
   volumeGroup->add(MENU_ENUM_LABEL_AUDIO_REWIND_MUTE);

   layout->addWidget(outputGroup);
   layout->addWidget(resamplerGroup);
   layout->addWidget(syncGroup);
   layout->addWidget(dspGroup);
   layout->addWidget(volumeGroup);

   layout->addStretch();

   widget->setLayout(layout);

   return widget;
}

MenuSoundsPage::MenuSoundsPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_MENU_SOUNDS);
}

QWidget *MenuSoundsPage::widget()
{
   return create_widget(DISPLAYLIST_MENU_SOUNDS_LIST);
}

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
   OptionsPage(parent) { }

QWidget *InputPage::widget()
{
   unsigned i;
   QWidget *widget       = new QWidget;
   FormLayout *layout    = new FormLayout;
   settings_t *settings  = config_get_ptr();
   file_list_t *list     = (file_list_t*)calloc(1, sizeof(*list));

   menu_displaylist_build_list(list, settings,
         DISPLAYLIST_INPUT_SETTINGS_LIST, true);

   for (i = 0; i < list->size; i++)
   {
      menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
         file_list_get_actiondata_at_offset(list, i);

      layout->add(menu_setting_find_enum(cbs->enum_idx));
   }

   file_list_free(list);

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
   settings_t *settings    = config_get_ptr();
   file_list_t *list       = (file_list_t*)calloc(1, sizeof(*list));

   menu_displaylist_build_list(list, settings,
         DISPLAYLIST_INPUT_HOTKEY_BINDS_LIST, true);

   for (i = 0; i < list->size; i++)
   {
      menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
         file_list_get_actiondata_at_offset(list, i);

      mainLayout->add(menu_setting_find_enum(cbs->enum_idx));
   }

   file_list_free(list);

   layout->addLayout(mainLayout);

   widget->setLayout(layout);

   return widget;
}

UserBindsPage::UserBindsPage(QObject *parent) :
   OptionsPage(parent) { setDisplayName("User Binds"); }

QWidget *UserBindsPage::widget()
{
   unsigned p, retro_id;
   settings_t *settings      = config_get_ptr();
   unsigned max_users    = settings->uints.input_max_users;
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

         input_config_get_bind_string(settings, descriptor,
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

LatencyCategory::LatencyCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS);
   setCategoryIcon("menu_latency");
}

QVector<OptionsPage*> LatencyCategory::pages()
{
   QVector<OptionsPage*> pages;
   pages << new LatencyPage(this);
   return pages;
}

LatencyPage::LatencyPage(QObject *parent) :
   OptionsPage(parent) { }

QWidget *LatencyPage::widget()
{
   QWidget                       *widget = new QWidget;
   FormLayout                    *layout = new FormLayout;
   SettingsGroup *runAheadGroup          = new SettingsGroup(
           msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE));

   rarch_setting_t *hardSyncSetting      = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_HARD_SYNC);

   if (hardSyncSetting)
   {
      CheckableSettingsGroup *hardSyncGroup = new CheckableSettingsGroup(hardSyncSetting);

      hardSyncGroup->add(MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES);

      layout->addRow(hardSyncGroup);
   }

   layout->add(menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES));
   layout->add(menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_WAITABLE_SWAPCHAINS));
   layout->add(menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_MAX_FRAME_LATENCY));

   layout->add(menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_FRAME_DELAY));
   layout->add(menu_setting_find_enum(MENU_ENUM_LABEL_AUDIO_LATENCY));
   layout->add(menu_setting_find_enum(MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR));

   runAheadGroup->add(MENU_ENUM_LABEL_RUNAHEAD_MODE);
   runAheadGroup->add(MENU_ENUM_LABEL_RUN_AHEAD_FRAMES);
   runAheadGroup->add(MENU_ENUM_LABEL_RUN_AHEAD_HIDE_WARNINGS);
   layout->addRow(runAheadGroup);

   widget->setLayout(layout);

   return widget;
}

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

   checksLayout->add(menu_setting_find_enum(MENU_ENUM_LABEL_NETPLAY_PUBLIC_ANNOUNCE));
   checksLayout->add(menu_setting_find_enum(MENU_ENUM_LABEL_NETPLAY_START_AS_SPECTATOR));
   checksLayout->add(menu_setting_find_enum(MENU_ENUM_LABEL_NETPLAY_FADE_CHAT));
   checksLayout->add(menu_setting_find_enum(MENU_ENUM_LABEL_NETPLAY_ALLOW_PAUSING));
   checksLayout->add(menu_setting_find_enum(MENU_ENUM_LABEL_NETWORK_ON_DEMAND_THUMBNAILS));

   serverForm->add(menu_setting_find_enum(MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS));
   serverForm->add(menu_setting_find_enum(MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT));
   serverForm->add(menu_setting_find_enum(MENU_ENUM_LABEL_NETPLAY_MAX_CONNECTIONS));
   serverForm->add(menu_setting_find_enum(MENU_ENUM_LABEL_NETPLAY_MAX_PING));
   serverForm->add(menu_setting_find_enum(MENU_ENUM_LABEL_NETPLAY_PASSWORD));
   serverForm->add(menu_setting_find_enum(MENU_ENUM_LABEL_NETPLAY_SPECTATE_PASSWORD));
   serverForm->add(menu_setting_find_enum(MENU_ENUM_LABEL_NETPLAY_NAT_TRAVERSAL));

   serverLayout->addWidget(createMitmServerGroup());
   serverLayout->addSpacing(30);
   serverLayout->addLayout(serverForm);

   serverGroup->setLayout(serverLayout);

   slaveGroup->add(MENU_ENUM_LABEL_NETPLAY_ALLOW_SLAVES);
   slaveGroup->add(MENU_ENUM_LABEL_NETPLAY_REQUIRE_SLAVES);

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
      requestGrid->addWidget(new CheckBox((enum msg_hash_enums)(
                  MENU_ENUM_LABEL_NETPLAY_REQUEST_DEVICE_1 + i)), row, column);
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
   size_t i;
   const char *netplay_mitm_server;
   CheckableSettingsGroup *groupBox = new CheckableSettingsGroup(
      MENU_ENUM_LABEL_NETPLAY_USE_MITM_SERVER);
   QButtonGroup *buttonGroup        = new QButtonGroup(this);
   rarch_setting_t *setting         = menu_setting_find_enum(
      MENU_ENUM_LABEL_NETPLAY_MITM_SERVER);

   if (!setting)
      return nullptr;

   netplay_mitm_server = setting->value.target.string;

   for (i = 0; i < ARRAY_SIZE(netplay_mitm_server_list); i++)
   {
      const mitm_server_t *server      = &netplay_mitm_server_list[i];
      QRadioButton        *radioButton = new QRadioButton(
         msg_hash_to_str(server->description));

      if (string_is_equal(server->name, netplay_mitm_server))
         radioButton->setChecked(true);

      buttonGroup->addButton(radioButton, i);
      groupBox->addRow(radioButton);
   }

   groupBox->add(MENU_ENUM_LABEL_NETPLAY_CUSTOM_MITM_SERVER);

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
   connect(buttonGroup, &QButtonGroup::idClicked, this,
         &NetplayPage::onRadioButtonClicked);
#else
   connect(buttonGroup, SIGNAL(buttonClicked(int)), this,
      SLOT(onRadioButtonClicked(int)));
#endif

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

OnscreenDisplayCategory::OnscreenDisplayCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS);
   setCategoryIcon("menu_osd");
}

QVector<OptionsPage*> OnscreenDisplayCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new OverlayPage(this);
   pages << new NotificationsPage(this);

   return pages;
}

NotificationsPage::NotificationsPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS);
}

QWidget *NotificationsPage::widget()
{
   QWidget                            *widget = new QWidget;
   QVBoxLayout                        *layout = new QVBoxLayout;
   CheckableSettingsGroup *notificationsGroup = new CheckableSettingsGroup(
         MENU_ENUM_LABEL_VIDEO_FONT_ENABLE);
   CheckableSettingsGroup            *bgGroup = new CheckableSettingsGroup(
         MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE);

   notificationsGroup->add(MENU_ENUM_LABEL_FPS_SHOW);
   notificationsGroup->add(MENU_ENUM_LABEL_FPS_UPDATE_INTERVAL);
   notificationsGroup->add(MENU_ENUM_LABEL_FRAMECOUNT_SHOW);
   notificationsGroup->add(MENU_ENUM_LABEL_MEMORY_SHOW);
   notificationsGroup->add(MENU_ENUM_LABEL_MEMORY_UPDATE_INTERVAL);
   notificationsGroup->add(MENU_ENUM_LABEL_STATISTICS_SHOW);
   notificationsGroup->add(MENU_ENUM_LABEL_NETPLAY_PING_SHOW);
   notificationsGroup->add(MENU_ENUM_LABEL_VIDEO_FONT_PATH);
   notificationsGroup->add(MENU_ENUM_LABEL_VIDEO_FONT_SIZE);
   notificationsGroup->add(MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_X);
   notificationsGroup->add(MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_Y);
   /* TODO/FIXME - localize */
   notificationsGroup->addRow("Notification Color: ", new FloatColorButton(
      MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_RED,
      MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_GREEN,
      MENU_ENUM_LABEL_VIDEO_MESSAGE_COLOR_BLUE));
   /* TODO/FIXME - localize */
   bgGroup->addRow("Notification Background Color: ", new UIntColorButton(
      MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_RED,
      MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_GREEN,
      MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_BLUE));
   bgGroup->add(MENU_ENUM_LABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY);

   notificationsGroup->addRow(bgGroup);

   notificationsGroup->add(MENU_ENUM_LABEL_MENU_WIDGETS_ENABLE);
   notificationsGroup->add(MENU_ENUM_LABEL_MENU_WIDGET_SCALE_AUTO);
   notificationsGroup->add(MENU_ENUM_LABEL_MENU_WIDGET_SCALE_FACTOR);
#if !(defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))
   notificationsGroup->add(MENU_ENUM_LABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED);
#endif
   notificationsGroup->add(MENU_ENUM_LABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_AUTOCONFIG);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_AUTOCONFIG_FAILS);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_REMAP_LOAD);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_SAVE_STATE);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_FAST_FORWARD);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_CHEATS_APPLIED);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_SCREENSHOT);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_REFRESH_RATE);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_NETPLAY_EXTRA);
   notificationsGroup->add(MENU_ENUM_LABEL_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE);

   layout->addWidget(notificationsGroup);

   layout->addStretch();

   widget->setLayout(layout);

   return widget;
}

OverlayPage::OverlayPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS);
}

QWidget *OverlayPage::widget()
{
   QWidget                      *widget = new QWidget;
#if defined(HAVE_OVERLAY)
   QVBoxLayout                  *layout = new QVBoxLayout;

   CheckableSettingsGroup *overlayGroup = new CheckableSettingsGroup(
         MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE);

   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_AUTOLOAD_PREFERRED);
   overlayGroup->add(MENU_ENUM_LABEL_INPUT_OVERLAY_AUTO_ROTATE);
   overlayGroup->add(MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU);
   overlayGroup->add(MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED);
   overlayGroup->add(MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR);

   overlayGroup->add(MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_INPUTS);
   overlayGroup->add(MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_INPUTS_PORT);

   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_PRESET);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_OPACITY);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_SCALE_LANDSCAPE);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_ASPECT_ADJUST_LANDSCAPE);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_X_SEPARATION_LANDSCAPE);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_Y_SEPARATION_LANDSCAPE);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_X_OFFSET_LANDSCAPE);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_Y_OFFSET_LANDSCAPE);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_SCALE_PORTRAIT);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_ASPECT_ADJUST_PORTRAIT);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_X_SEPARATION_PORTRAIT);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_X_OFFSET_PORTRAIT);
   overlayGroup->add(MENU_ENUM_LABEL_OVERLAY_Y_OFFSET_PORTRAIT);

   layout->addWidget(overlayGroup);

   layout->addStretch();

   widget->setLayout(layout);
#endif

   return widget;
}

PlaylistsCategory::PlaylistsCategory(QWidget *parent) : OptionsCategory(parent)
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
   CheckableSettingsGroup *history = new CheckableSettingsGroup(
         MENU_ENUM_LABEL_HISTORY_LIST_ENABLE);

   history->add(MENU_ENUM_LABEL_CONTENT_HISTORY_SIZE);

   layout->addRow(history);

   layout->add(menu_setting_find_enum(MENU_ENUM_LABEL_PLAYLIST_ENTRY_RENAME));
   layout->add(menu_setting_find_enum(MENU_ENUM_LABEL_PLAYLIST_SORT_ALPHABETICAL));
   layout->add(menu_setting_find_enum(MENU_ENUM_LABEL_PLAYLIST_USE_OLD_FORMAT));
   layout->add(menu_setting_find_enum(MENU_ENUM_LABEL_PLAYLIST_COMPRESSION));
   layout->add(menu_setting_find_enum(MENU_ENUM_LABEL_PLAYLIST_SHOW_SUBLABELS));
   layout->add(menu_setting_find_enum(MENU_ENUM_LABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH));
   layout->add(menu_setting_find_enum(MENU_ENUM_LABEL_SCAN_WITHOUT_CORE_MATCH));

   widget->setLayout(layout);

   return widget;
}

RecordingCategory::RecordingCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS);
   setCategoryIcon("menu_record");
}

QVector<OptionsPage*> RecordingCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new RecordingPage(this);

   return pages;
}

RecordingPage::RecordingPage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *RecordingPage::widget()
{
   QWidget              * widget = new QWidget;
   QVBoxLayout           *layout = new QVBoxLayout;
   SettingsGroup *recordingGroup = new SettingsGroup("Recording");
   SettingsGroup *streamingGroup = new SettingsGroup("Streaming");
   QHBoxLayout               *hl = new QHBoxLayout;

   recordingGroup->add(MENU_ENUM_LABEL_VIDEO_RECORD_QUALITY);
   recordingGroup->add(MENU_ENUM_LABEL_RECORD_CONFIG);
   recordingGroup->add(MENU_ENUM_LABEL_VIDEO_RECORD_THREADS);
   recordingGroup->add(MENU_ENUM_LABEL_RECORDING_OUTPUT_DIRECTORY);
   recordingGroup->add(MENU_ENUM_LABEL_VIDEO_POST_FILTER_RECORD);
   recordingGroup->add(MENU_ENUM_LABEL_VIDEO_GPU_RECORD);

   hl->addWidget(new UIntRadioButtons(MENU_ENUM_LABEL_STREAMING_MODE));
   hl->addWidget(new UIntRadioButtons(MENU_ENUM_LABEL_VIDEO_STREAM_QUALITY));

   streamingGroup->addRow(hl);

   streamingGroup->add(MENU_ENUM_LABEL_STREAM_CONFIG);
   streamingGroup->add(MENU_ENUM_LABEL_STREAMING_TITLE);
   streamingGroup->add(MENU_ENUM_LABEL_STREAMING_URL);
   streamingGroup->add(MENU_ENUM_LABEL_UDP_STREAM_PORT);

   layout->addWidget(recordingGroup);
   layout->addWidget(streamingGroup);

   layout->addStretch();

   widget->setLayout(layout);

   return widget;
}

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
   QWidget                             *widget = new QWidget;
   FormLayout                          *layout = new FormLayout;
   SettingsGroup                   *savesGroup = new SettingsGroup("Saves");
   SettingsGroup              *savestatesGroup = new SettingsGroup("Savestates");
   CheckableSettingsGroup *autoSavestatesGroup = new CheckableSettingsGroup(
         MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE);
   SettingsGroup                 *saveRamGroup = new SettingsGroup("SaveRAM");
   SettingsGroup          *systemFilesDirGroup = new SettingsGroup("System Files");
   SettingsGroup          *screenshotsDirGroup = new SettingsGroup("Screenshots");
   SettingsGroup          *runtimeLogGroup     = new SettingsGroup("Runtime Log");

   savesGroup->add(MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE);
   savesGroup->add(MENU_ENUM_LABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE);
   savesGroup->add(MENU_ENUM_LABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE);

   savestatesGroup->add(MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX);

   autoSavestatesGroup->add(MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD);

   savestatesGroup->addRow(autoSavestatesGroup);
   savestatesGroup->add(MENU_ENUM_LABEL_SAVESTATE_THUMBNAIL_ENABLE);
   savestatesGroup->add(MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE);
   savestatesGroup->add(MENU_ENUM_LABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE);
   savestatesGroup->add(MENU_ENUM_LABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE);
   savestatesGroup->add(MENU_ENUM_LABEL_SAVESTATE_FILE_COMPRESSION);

   saveRamGroup->add(MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE);
   saveRamGroup->add(MENU_ENUM_LABEL_AUTOSAVE_INTERVAL);
   saveRamGroup->add(MENU_ENUM_LABEL_SAVE_FILE_COMPRESSION);

   systemFilesDirGroup->add(MENU_ENUM_LABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE);

   screenshotsDirGroup->add(MENU_ENUM_LABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE);
   screenshotsDirGroup->add(MENU_ENUM_LABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE);

   runtimeLogGroup->add(MENU_ENUM_LABEL_CONTENT_RUNTIME_LOG);
   runtimeLogGroup->add(MENU_ENUM_LABEL_CONTENT_RUNTIME_LOG_AGGREGATE);

   layout->addRow(savesGroup);
   layout->addRow(savestatesGroup);
   layout->addRow(saveRamGroup);
   layout->addRow(systemFilesDirGroup);
   layout->addRow(screenshotsDirGroup);
   layout->addRow(runtimeLogGroup);

   widget->setLayout(layout);

   return widget;
}

UserInterfaceCategory::UserInterfaceCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS);
   setCategoryIcon("menu_ui");
}

UserInterfaceCategory::UserInterfaceCategory(MainWindow *mainwindow, QWidget *parent) :
   OptionsCategory(parent)
   ,m_mainwindow(mainwindow)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS);
   setCategoryIcon("menu_ui");

   m_pages << new UserInterfacePage(this);
   m_pages << new ViewsPage(this);
   m_pages << new AppearancePage(this);
   m_pages << new DesktopMenuPage(m_mainwindow, this);
}

QVector<OptionsPage*> UserInterfaceCategory::pages()
{
   return m_pages;
}

UserInterfacePage::UserInterfacePage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *UserInterfacePage::widget()
{
   QWidget                     * widget = new QWidget;
   QVBoxLayout                  *layout = new QVBoxLayout;
   SettingsGroup             *menuGroup = new SettingsGroup("Menu");
   SettingsGroup            *inputGroup = new SettingsGroup("Input");
   SettingsGroup             *miscGroup = new SettingsGroup("Miscellaneous");
   CheckableSettingsGroup *desktopGroup = new CheckableSettingsGroup(
         MENU_ENUM_LABEL_DESKTOP_MENU_ENABLE);
   rarch_setting_t           *kioskMode = menu_setting_find_enum(
         MENU_ENUM_LABEL_MENU_ENABLE_KIOSK_MODE);

   menuGroup->add(MENU_ENUM_LABEL_SHOW_ADVANCED_SETTINGS);

   /* only on XMB and Ozone*/
   if (kioskMode)
   {
      CheckableSettingsGroup *kioskGroup = new CheckableSettingsGroup(kioskMode, widget);

      kioskGroup->add(MENU_ENUM_LABEL_MENU_KIOSK_MODE_PASSWORD);

      menuGroup->addRow(kioskGroup);
   }

   menuGroup->add(MENU_ENUM_LABEL_NAVIGATION_WRAPAROUND);
   menuGroup->add(MENU_ENUM_LABEL_PAUSE_LIBRETRO);

   inputGroup->add(MENU_ENUM_LABEL_MOUSE_ENABLE);
   inputGroup->add(MENU_ENUM_LABEL_POINTER_ENABLE);

   menuGroup->addRow(inputGroup);
   menuGroup->add(MENU_ENUM_LABEL_THREADED_DATA_RUNLOOP_ENABLE);

   miscGroup->add(MENU_ENUM_LABEL_PAUSE_NONACTIVE);
   miscGroup->add(MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION);

   menuGroup->add(MENU_ENUM_LABEL_UI_COMPANION_ENABLE);
   menuGroup->add(MENU_ENUM_LABEL_UI_COMPANION_START_ON_BOOT);
   menuGroup->add(MENU_ENUM_LABEL_UI_MENUBAR_ENABLE);
   menuGroup->add(MENU_ENUM_LABEL_MENU_SCROLL_FAST);

   /* layout->add(MENU_ENUM_LABEL_DESKTOP_MENU_ENABLE); */
   desktopGroup->add(MENU_ENUM_LABEL_UI_COMPANION_TOGGLE);

   layout->addWidget(menuGroup);
   layout->addWidget(miscGroup);
   layout->addWidget(desktopGroup);
   layout->addStretch();

   widget->setLayout(layout);

   return widget;
}

ViewsPage::ViewsPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS);
}

QWidget *ViewsPage::widget()
{
   unsigned i;
   QWidget           * widget = new QWidget();
   QHBoxLayout *mainLayout    = new QHBoxLayout;
   FormLayout *leftLayout     = new FormLayout;
   QVBoxLayout *rightLayout   = new QVBoxLayout;
   SettingsGroup *quickMenu   = new SettingsGroup("Quick Menu");
   QuickMenuPage *quickPage   = new QuickMenuPage(this);
   SettingsGroup *mainMenu    = new SettingsGroup("Main Menu");
   SettingsGroup *settings    = new SettingsGroup("Settings");
   SettingsGroup *tabs        = new SettingsGroup("Tabs");
   SettingsGroup *status      = new SettingsGroup("Status");
   SettingsGroup *startScreen = new SettingsGroup("StartScreen");
   settings_t *_settings      = config_get_ptr();
   unsigned tabs_begin        = 0;
   unsigned status_begin      = 0;
   file_list_t *list          = (file_list_t*)calloc(1, sizeof(*list));

   {
      rarch_setting_t *kiosk_mode = NULL;
      menu_displaylist_build_list(list, _settings,
            DISPLAYLIST_MENU_VIEWS_SETTINGS_LIST, true);
      kiosk_mode                  = menu_setting_find_enum(
            MENU_ENUM_LABEL_MENU_ENABLE_KIOSK_MODE);

      for (i = 0; i < list->size; i++)
      {
         menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
            file_list_get_actiondata_at_offset(list, i);

         if (cbs->enum_idx == (kiosk_mode
                  ? MENU_ENUM_LABEL_CONTENT_SHOW_SETTINGS
                  : MENU_ENUM_LABEL_CONTENT_SHOW_EXPLORE))
         {
            tabs_begin = i;
            break;
         }

         mainMenu->add(cbs->enum_idx);
      }

      for (i = tabs_begin; i < list->size; i++)
      {
         menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
            file_list_get_actiondata_at_offset(list, i);

         if (cbs->enum_idx == MENU_ENUM_LABEL_TIMEDATE_ENABLE)
         {
            status_begin = i;
            break;
         }

         tabs->add(cbs->enum_idx);
      }

      for (i = status_begin; i < list->size; i++)
      {
         menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
            file_list_get_actiondata_at_offset(list, i);

         if (cbs->enum_idx == MENU_ENUM_LABEL_MENU_SHOW_SUBLABELS)
            break;

         status->add(cbs->enum_idx);
      }

      file_list_free(list);
   }

   {
      unsigned i;
      file_list_t *list = (file_list_t*)calloc(1, sizeof(*list));
      menu_displaylist_build_list(list, _settings,
            DISPLAYLIST_SETTINGS_VIEWS_SETTINGS_LIST, true);

      for (i = 0; i < list->size; i++)
      {
         menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
            file_list_get_actiondata_at_offset(list, i);

         settings->add(cbs->enum_idx);
      }

      file_list_free(list);
   }

   startScreen->add(MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN);

   quickMenu->layout()->setContentsMargins(0, 0, 0, 0);
   quickMenu->addRow(quickPage->widget());

   leftLayout->addRow(mainMenu);
   leftLayout->addRow(settings);
   leftLayout->addRow(startScreen);
   leftLayout->add(menu_setting_find_enum(MENU_ENUM_LABEL_MENU_SHOW_SUBLABELS));

   rightLayout->addWidget(tabs);
   rightLayout->addWidget(quickMenu);
   rightLayout->addWidget(status);
   rightLayout->addStretch();

   mainLayout->addLayout(leftLayout);
   mainLayout->addLayout(rightLayout);

   widget->setLayout(mainLayout);

   return widget;
}

QuickMenuPage::QuickMenuPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS);
}

QWidget *QuickMenuPage::widget()
{
   return create_widget(DISPLAYLIST_QUICK_MENU_VIEWS_SETTINGS_LIST);
}

AppearancePage::AppearancePage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_MENU_SETTINGS);
}

QWidget *AppearancePage::widget()
{
   unsigned i;
   QWidget            * widget = new QWidget;
   FormLayout          *layout = new FormLayout;
   file_list_t           *list = (file_list_t*)calloc(1, sizeof(*list));
   settings_t *settings        = config_get_ptr();

   menu_displaylist_build_list(
         list, settings, DISPLAYLIST_MENU_SETTINGS_LIST, true);

   /* TODO/FIXME - we haven't yet figured out how to
    * put a radio button setting next to another radio
    * button on the same row */

   for (i = 0; i < list->size; i++)
   {
      menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
         file_list_get_actiondata_at_offset(list, i);

      switch (cbs->enum_idx)
      {
         /* TODO/FIXME - this is a dirty hack - if we
          * detect this setting, we instead replace it with a
          * color button and ignore the other two font color
          * settings since they are already covered by this one
          * color button */
         case MENU_ENUM_LABEL_MENU_FONT_COLOR_RED:
            /* TODO/FIXME - localize */
            layout->addUIntColorButton("Menu Font Color: ",
                  MENU_ENUM_LABEL_MENU_FONT_COLOR_RED,
                  MENU_ENUM_LABEL_MENU_FONT_COLOR_GREEN,
                  MENU_ENUM_LABEL_MENU_FONT_COLOR_BLUE);
            break;
         case MENU_ENUM_LABEL_MENU_FONT_COLOR_GREEN:
         case MENU_ENUM_LABEL_MENU_FONT_COLOR_BLUE:
            break;
         default:
            layout->add(menu_setting_find_enum(cbs->enum_idx));
            break;
      }
   }

   file_list_free(list);

   widget->setLayout(layout);

   return widget;
}

DesktopMenuPage::DesktopMenuPage(MainWindow *mainwindow, QObject *parent) :
   OptionsPage(parent)
   ,m_widget(new ViewOptionsWidget(mainwindow))
{
   setDisplayName("Desktop Menu");
}

void DesktopMenuPage::apply()
{
   m_widget->saveViewOptions();
}

void DesktopMenuPage::load()
{
   m_widget->loadViewOptions();
}

QWidget *DesktopMenuPage::widget()
{
   return m_widget;
}

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

   layout->add(menu_setting_find_enum(MENU_ENUM_LABEL_NETPLAY_NICKNAME));
   layout->add(menu_setting_find_enum(MENU_ENUM_LABEL_USER_LANGUAGE));

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
   QWidget *widget              = new QWidget;
   QVBoxLayout *layout          = new QVBoxLayout;
   SettingsGroup *youtubeGroup  = new SettingsGroup(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACCOUNTS_YOUTUBE));
   SettingsGroup *twitchGroup   = new SettingsGroup(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACCOUNTS_TWITCH));
   SettingsGroup *facebookGroup = new SettingsGroup(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACCOUNTS_FACEBOOK));
#ifdef HAVE_CHEEVOS
   SettingsGroup *cheevosGroup  = new SettingsGroup(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS));

   cheevosGroup->add(MENU_ENUM_LABEL_CHEEVOS_USERNAME);
   cheevosGroup->add(MENU_ENUM_LABEL_CHEEVOS_PASSWORD);

   layout->addWidget(cheevosGroup);
#endif

   youtubeGroup->add(MENU_ENUM_LABEL_YOUTUBE_STREAM_KEY);

   layout->addWidget(youtubeGroup);

   twitchGroup->add(MENU_ENUM_LABEL_TWITCH_STREAM_KEY);

   layout->addWidget(twitchGroup);

   facebookGroup->add(MENU_ENUM_LABEL_FACEBOOK_STREAM_KEY);

   layout->addWidget(facebookGroup);

   layout->addStretch();

   widget->setLayout(layout);

   return widget;
}

VideoCategory::VideoCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS);
   setCategoryIcon("menu_video");
}

QVector<OptionsPage*> VideoCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new VideoPage(this);
   pages << new CrtSwitchresPage(this);

   return pages;
}

VideoPage::VideoPage(QObject *parent) :
   OptionsPage(parent)
   ,m_resolutionCombo(new QComboBox())
{
}

QWidget *VideoPage::widget()
{
   QWidget               *widget       = new QWidget;

   QVBoxLayout        *layout          = new QVBoxLayout;

   SettingsGroup *outputGroup          = new SettingsGroup("Output");
   SettingsGroup *aspectGroup          = new SettingsGroup("Scaling");

   SettingsGroup *fullscreenGroup      = new SettingsGroup("Fullscreen Mode");
   SettingsGroup *windowedGroup        = new SettingsGroup("Windowed Mode");

   QHBoxLayout *fullcreenSizeLayout    = new QHBoxLayout;
   FormLayout *leftFullscreenSizeForm  = new FormLayout;
   FormLayout *rightFullscreenSizeForm = new FormLayout;

   QHBoxLayout *windowedSizeLayout     = new QHBoxLayout;
   FormLayout *leftWindowedSizeForm    = new FormLayout;
   FormLayout *rightWindowedSizeForm   = new FormLayout;
   QHBoxLayout *windowedCustomSizeLayout   = new QHBoxLayout;
   FormLayout *leftWindowedCustomSizeForm  = new FormLayout;
   FormLayout *rightWindowedCustomSizeForm = new FormLayout;
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   CheckableSettingsGroup *savePosGroup    = new CheckableSettingsGroup(MENU_ENUM_LABEL_VIDEO_WINDOW_SAVE_POSITION);
#else
   CheckableSettingsGroup *savePosGroup    = new CheckableSettingsGroup(MENU_ENUM_LABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE);
#endif

   SettingsGroup *hdrGroup             = new SettingsGroup("HDR");
   QHBoxLayout *hdrLayout              = new QHBoxLayout;

   SettingsGroup *syncGroup            = new SettingsGroup("Synchronization");
   CheckableSettingsGroup *vSyncGroup  = new CheckableSettingsGroup(MENU_ENUM_LABEL_VIDEO_VSYNC);

   QHBoxLayout *outputScalingLayout    = new QHBoxLayout;
   QHBoxLayout *modeLayout             = new QHBoxLayout;
   QHBoxLayout *syncMiscLayout         = new QHBoxLayout;

   SettingsGroup *miscGroup            = new SettingsGroup("Miscellaneous");
   SettingsGroup *filterGroup          = new SettingsGroup("Video Filter");

   unsigned i, size                    = 0;
   struct video_display_config *list   = (struct video_display_config*) video_display_server_get_resolution_list(&size);

   if (list)
   {
      for (i = 0; i < size; i++)
      {
         char val_d[NAME_MAX_LENGTH], str[NAME_MAX_LENGTH];
         snprintf(str, sizeof(str), "%dx%d (%d Hz)", list[i].width, list[i].height, list[i].refreshrate);
         snprintf(val_d, sizeof(val_d), "%d", i);

         m_resolutionCombo->addItem(str);

         if (list[i].current)
            m_resolutionCombo->setCurrentIndex(i);
      }

      free(list);
   }

   outputGroup->add(MENU_ENUM_LABEL_VIDEO_DRIVER);
   outputGroup->add(MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX);
   outputGroup->add(MENU_ENUM_LABEL_VIDEO_ROTATION);
   outputGroup->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION), m_resolutionCombo);
   outputGroup->add(MENU_ENUM_LABEL_VIDEO_FORCE_SRGB_DISABLE);
   outputGroup->add(MENU_ENUM_LABEL_VIDEO_AUTOSWITCH_REFRESH_RATE);

   fullscreenGroup->add(MENU_ENUM_LABEL_VIDEO_FULLSCREEN);
   fullscreenGroup->add(MENU_ENUM_LABEL_VIDEO_WINDOWED_FULLSCREEN);

   leftFullscreenSizeForm->addRow("Width:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_FULLSCREEN_X));
   rightFullscreenSizeForm->addRow("Height:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_FULLSCREEN_Y));

   fullcreenSizeLayout->addLayout(leftFullscreenSizeForm);
   fullcreenSizeLayout->addLayout(rightFullscreenSizeForm);

   fullscreenGroup->addRow(fullcreenSizeLayout);

   aspectGroup->add(MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER);
   aspectGroup->addRow(new AspectRatioGroup("Aspect Ratio"));

   leftWindowedSizeForm->addRow("Scale:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_SCALE));
   leftWindowedSizeForm->addRow("Max Width:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX));

   rightWindowedSizeForm->addRow("Opacity:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_WINDOW_OPACITY));
   rightWindowedSizeForm->addRow("Max Height:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX));

   windowedSizeLayout->addLayout(leftWindowedSizeForm);
   windowedSizeLayout->addLayout(rightWindowedSizeForm);

   windowedGroup->addRow(windowedSizeLayout);

   leftWindowedCustomSizeForm->addRow("Width:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_WINDOW_WIDTH));
   rightWindowedCustomSizeForm->addRow("Height:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_WINDOW_HEIGHT));

   windowedCustomSizeLayout->addLayout(leftWindowedCustomSizeForm);
   windowedCustomSizeLayout->addLayout(rightWindowedCustomSizeForm);

   savePosGroup->addRow(windowedCustomSizeLayout);
   windowedGroup->addRow(savePosGroup);

   windowedGroup->add(MENU_ENUM_LABEL_VIDEO_WINDOW_SHOW_DECORATIONS);
   windowedGroup->add(MENU_ENUM_LABEL_UI_MENUBAR_ENABLE);

   vSyncGroup->add(MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL);
   vSyncGroup->add(MENU_ENUM_LABEL_VIDEO_SHADER_SUBFRAMES);
   vSyncGroup->add(MENU_ENUM_LABEL_VIDEO_SCAN_SUBFRAMES);
   vSyncGroup->add(MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION);
   vSyncGroup->add(MENU_ENUM_LABEL_VIDEO_ADAPTIVE_VSYNC);
   vSyncGroup->add(MENU_ENUM_LABEL_VIDEO_FRAME_DELAY);
   vSyncGroup->add(MENU_ENUM_LABEL_VIDEO_FRAME_DELAY_AUTO);
   syncGroup->addRow(vSyncGroup);

   rarch_setting_t *hardSyncSetting = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_HARD_SYNC);

   if (hardSyncSetting)
   {
      CheckableSettingsGroup *hardSyncGroup = new CheckableSettingsGroup(hardSyncSetting);

      hardSyncGroup->add(MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES);

      syncGroup->addRow(hardSyncGroup);
   }

   syncGroup->add(MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES);
   syncGroup->add(MENU_ENUM_LABEL_VIDEO_WAITABLE_SWAPCHAINS);
   syncGroup->add(MENU_ENUM_LABEL_VIDEO_MAX_FRAME_LATENCY);
   syncGroup->add(MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE);

   miscGroup->add(MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE);
   miscGroup->add(MENU_ENUM_LABEL_VIDEO_THREADED);
   miscGroup->add(MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT);
   miscGroup->add(MENU_ENUM_LABEL_VIDEO_SMOOTH);
   miscGroup->add(MENU_ENUM_LABEL_VIDEO_CTX_SCALING);
   miscGroup->add(MENU_ENUM_LABEL_VIDEO_SHADER_DELAY);

   if (video_driver_supports_hdr())
   {
      hdrGroup->add(MENU_ENUM_LABEL_VIDEO_HDR_ENABLE);
      hdrGroup->add(MENU_ENUM_LABEL_MENU_HDR_BRIGHTNESS_NITS);
      hdrGroup->add(MENU_ENUM_LABEL_VIDEO_HDR_PAPER_WHITE_NITS);
      hdrGroup->add(MENU_ENUM_LABEL_VIDEO_HDR_SCANLINES);

      hdrLayout->addWidget(hdrGroup);
   }

   syncMiscLayout->addWidget(syncGroup);
   syncMiscLayout->addWidget(miscGroup);

   filterGroup->add(MENU_ENUM_LABEL_VIDEO_FILTER);

   modeLayout->addWidget(fullscreenGroup);
   modeLayout->addWidget(windowedGroup);

   aspectGroup->add(MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN);

   outputScalingLayout->addWidget(outputGroup);
   outputScalingLayout->addWidget(aspectGroup);

   layout->addLayout(outputScalingLayout);
   layout->addLayout(modeLayout);
   layout->addLayout(hdrLayout);
   layout->addLayout(syncMiscLayout);
   layout->addWidget(filterGroup);

   layout->addStretch();

#if (QT_VERSION > QT_VERSION_CHECK(6, 0, 0))
   void (VideoPage::*cb)(int) = &VideoPage::onResolutionComboIndexChanged;
   connect(m_resolutionCombo, &QComboBox::currentIndexChanged, this, cb);
#else
   connect(m_resolutionCombo, SIGNAL(currentIndexChanged(const QString&)), this,
         SLOT(onResolutionComboIndexChanged(const QString&)));
#endif

   widget->setLayout(layout);

   return widget;
}

AspectRatioGroup::AspectRatioGroup(const QString &title, QWidget *parent) :
   SettingsGroup(title, parent)
   ,m_radioButton(new AspectRatioRadioButton(ASPECT_RATIO_4_3, ASPECT_RATIO_32_9))
   ,m_comboBox(new UIntComboBox(MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX, ASPECT_RATIO_4_3, ASPECT_RATIO_32_9))
{
   QHBoxLayout *aspectLayout   = new QHBoxLayout;
   QHBoxLayout *preset         = new QHBoxLayout;
   QHBoxLayout *custom         = new QHBoxLayout;
   QVBoxLayout *customRadio    = new QVBoxLayout;
   QHBoxLayout *config         = new QHBoxLayout;
   QHBoxLayout *full           = new QHBoxLayout;
   QHBoxLayout *aspectL        = new QHBoxLayout;
   FormLayout *leftAspectForm  = new FormLayout;
   FormLayout *rightAspectForm = new FormLayout;
   FormLayout *leftAspect      = new FormLayout;
   FormLayout *rightAspect     = new FormLayout;

   leftAspectForm->addRow("X Pos.:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_X));
   leftAspectForm->addRow("Width:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH));
   rightAspectForm->addRow("Y Pos.:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_Y));
   rightAspectForm->addRow("Height:", new UIntSpinBox(MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT));

   aspectLayout->addLayout(leftAspectForm);
   aspectLayout->addLayout(rightAspectForm);

   preset->addWidget(m_radioButton);
   preset->addWidget(m_comboBox);
   preset->setStretch(1, 1);

   customRadio->addWidget(new UIntRadioButton(MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX, ASPECT_RATIO_CUSTOM), Qt::AlignTop);
   customRadio->addStretch();

   custom->addLayout(customRadio);
   custom->addLayout(aspectLayout);
   custom->addStretch();

   config->addWidget(new UIntRadioButton(MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX, ASPECT_RATIO_CONFIG));
   config->addWidget(new FloatSpinBox(MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO));
   config->setStretch(1, 1);
   config->setSizeConstraint(QLayout::SetMinimumSize);

   full->addWidget(new UIntRadioButton(MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX, ASPECT_RATIO_FULL));

   leftAspect->addRow(new UIntRadioButton(MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX, ASPECT_RATIO_CORE));
   leftAspect->addRow(preset);

   rightAspect->addRow(config);
   rightAspect->addRow(new UIntRadioButton(MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX, ASPECT_RATIO_SQUARE));

   aspectL->addLayout(leftAspect);
   aspectL->addStretch();
   aspectL->addSpacing(30);
   aspectL->addLayout(rightAspect);

   addRow(aspectL);
   addRow(full);
   addRow(custom);

   connect(m_radioButton, SIGNAL(clicked(bool)), this, SLOT(onAspectRadioClicked(bool)));
}

void AspectRatioGroup::paintEvent(QPaintEvent *event)
{
   settings_t *settings = config_get_ptr();
   unsigned       value = settings->uints.video_aspect_ratio_idx;

   if (ASPECT_RATIO_4_3 >= value || value <= ASPECT_RATIO_32_9)
   {
      m_comboBox->blockSignals(false);
      m_radioButton->setChecked(true);
   }
   else
      m_comboBox->blockSignals(true);

   SettingsGroup::paintEvent(event);
}

void AspectRatioGroup::onAspectRadioToggled(bool checked)
{
   if (checked)
      m_comboBox->currentIndexChanged(m_comboBox->currentIndex());
   else
      m_comboBox->blockSignals(true);
}

void AspectRatioGroup::onAspectRadioClicked(bool checked)
{
   m_comboBox->blockSignals(false);
   m_comboBox->currentIndexChanged(m_comboBox->currentIndex());
   setChecked(true);
}

CrtSwitchresPage::CrtSwitchresPage(QObject *parent) :
   OptionsPage(parent)
   ,m_crtSuperResolutionCombo(new QComboBox())
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS);
}

QWidget *CrtSwitchresPage::widget()
{
   QWidget *widget    = new QWidget;
   FormLayout *layout = new FormLayout;

   m_crtSuperResolutionCombo->addItem(msg_hash_to_str(MSG_NATIVE), 0);
   m_crtSuperResolutionCombo->addItem("1920", 1920);
   m_crtSuperResolutionCombo->addItem("2560", 2560);
   m_crtSuperResolutionCombo->addItem("3840", 3840);

   layout->add(menu_setting_find_enum(MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION));
   layout->addRow(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER), m_crtSuperResolutionCombo);
   layout->add(menu_setting_find_enum(MENU_ENUM_LABEL_CRT_SWITCH_X_AXIS_CENTERING));
   layout->add(menu_setting_find_enum(MENU_ENUM_LABEL_CRT_SWITCH_VERTICAL_ADJUST));
   layout->add(menu_setting_find_enum(MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE));

   connect(m_crtSuperResolutionCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onCrtSuperResolutionComboIndexChanged(int)));

   widget->setLayout(layout);

   return widget;
}

void VideoPage::onResolutionComboIndexChanged(const QString &text)
{
   const char *path     = text.toUtf8().constData();
   action_cb_push_dropdown_item_resolution(path,
         NULL, 0, 0, 0);
}

void VideoPage::onResolutionComboIndexChanged(int index)
{
   const QString& text  = m_resolutionCombo->itemText(index);
   const char *path     = text.toUtf8().constData();
   action_cb_push_dropdown_item_resolution(path,
         NULL, 0, 0, 0);
}

void CrtSwitchresPage::onCrtSuperResolutionComboIndexChanged(int index)
{
   settings_t *settings = config_get_ptr();
   (void)(index);

   settings->uints.crt_switch_resolution_super =
   m_crtSuperResolutionCombo->currentData().value<unsigned>();
}

AspectRatioRadioButton::AspectRatioRadioButton(unsigned min, unsigned max, QWidget *parent) :
   QRadioButton(parent)
   ,m_min(min)
   ,m_max(max)
{
}

/* DRIVERS */

DriversCategory::DriversCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS);
   setCategoryIcon("menu_drivers");
}

QVector<OptionsPage*> DriversCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new DriversPage(this);

   return pages;
}

DriversPage::DriversPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS);
}

QWidget *DriversPage::widget()
{
   return create_widget(DISPLAYLIST_DRIVER_SETTINGS_LIST);
}

/* DIRECTORY */

DirectoryCategory::DirectoryCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS);
   setCategoryIcon("folder");
}

QVector<OptionsPage*> DirectoryCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new DirectoryPage(this);

   return pages;
}

DirectoryPage::DirectoryPage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *DirectoryPage::widget()
{
   return create_widget(DISPLAYLIST_DIRECTORY_SETTINGS_LIST);
}

/* CONFIGURATION */

ConfigurationCategory::ConfigurationCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS);
   setCategoryIcon("setting");
}

QVector<OptionsPage*> ConfigurationCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new ConfigurationPage(this);

   return pages;
}

ConfigurationPage::ConfigurationPage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *ConfigurationPage::widget()
{
   return create_widget(DISPLAYLIST_CONFIGURATION_SETTINGS_LIST);
}

/* CORE */

CoreCategory::CoreCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_CORE_SETTINGS);
   setCategoryIcon("core-options");
}

QVector<OptionsPage*> CoreCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new CorePage(this);

   return pages;
}

CorePage::CorePage(QObject *parent) :
   OptionsPage(parent)
{
}

QWidget *CorePage::widget()
{
   return create_widget(DISPLAYLIST_CORE_SETTINGS_LIST);
}

/* LOGGING */

LoggingCategory::LoggingCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS);
   setCategoryIcon("menu_log");
}

QVector<OptionsPage*> LoggingCategory::pages()
{
   QVector<OptionsPage*> pages;
   pages << new LoggingPage(this);
   return pages;
}

LoggingPage::LoggingPage(QObject *parent) :
   OptionsPage(parent) { }

QWidget *LoggingPage::widget()
{
   return create_widget(DISPLAYLIST_LOGGING_SETTINGS_LIST);
}

/* AI SERVICE */

AIServiceCategory::AIServiceCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS);
   setCategoryIcon("menu_drivers");
}

QVector<OptionsPage*> AIServiceCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new AIServicePage(this);

   return pages;
}

AIServicePage::AIServicePage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS);
}

QWidget *AIServicePage::widget()
{
   return create_widget(DISPLAYLIST_AI_SERVICE_SETTINGS_LIST);
}

/* FRAME THROTTLE */

FrameThrottleCategory::FrameThrottleCategory(QWidget *parent) :
   OptionsCategory(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS);
   setCategoryIcon("menu_frameskip");
}

QVector<OptionsPage*> FrameThrottleCategory::pages()
{
   QVector<OptionsPage*> pages;

   pages << new FrameThrottlePage(this);
   pages << new RewindPage(this);

   return pages;
}

FrameThrottlePage::FrameThrottlePage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS);
}

QWidget *FrameThrottlePage::widget()
{
   return create_widget(DISPLAYLIST_FRAME_THROTTLE_SETTINGS_LIST);
}

RewindPage::RewindPage(QObject *parent) :
   OptionsPage(parent)
{
   setDisplayName(MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS);
}

QWidget *RewindPage::widget()
{
   return create_widget(DISPLAYLIST_REWIND_SETTINGS_LIST);
}

PlaylistModel::PlaylistModel(QObject *parent)
   : QAbstractListModel(parent)
{
   m_fileSanitizerRegex = QRegularExpression("[&*/:`<>?\\|]");
   m_thumbnailLoader    = new ThumbnailLoader(this);
   setThumbnailCacheLimit(500);
   connect(m_thumbnailLoader, SIGNAL(imageLoaded(QImage,QModelIndex,QString)),
         this, SLOT(onImageLoaded(QImage,QModelIndex,QString)));
   m_thumbnailLoader->start();
}

int PlaylistModel::rowCount(const QModelIndex & /* parent */) const
{
   return m_contents.count();
}

int PlaylistModel::columnCount(const QModelIndex & /* parent */) const
{
   return 1;
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const
{
   if (index.column() == 0)
   {
      if (!index.isValid() || index.row() >= m_contents.size() || index.row() < 0)
         return QVariant();

      switch (role)
      {
         case Qt::DisplayRole:
         case Qt::EditRole:
         case Qt::ToolTipRole:
            return m_contents.at(index.row())["label_noext"];
         case HASH:
            return QVariant::fromValue(m_contents.at(index.row()));
         case THUMBNAIL:
            {
               QPixmap *cachedPreview = m_cache.object(getCurrentTypeThumbnailPath(index));
               if (cachedPreview)
                  return *cachedPreview;
            }
            break;
      }
   }
   return QVariant();
}

Qt::ItemFlags PlaylistModel::flags(const QModelIndex &index) const
{
   if (!index.isValid())
      return Qt::ItemIsEnabled;

   return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

bool PlaylistModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
   if (index.isValid() && role == Qt::EditRole)
   {
      QHash<QString, QString> hash = m_contents.at(index.row());

      hash["label"]       = value.toString();
      hash["label_noext"] = QFileInfo(value.toString()).completeBaseName();

      m_contents.replace(index.row(), hash);
      emit dataChanged(index, index, { role });
      return true;
   }
   return false;
}

QVariant PlaylistModel::headerData(int section,
      Qt::Orientation orientation, int role) const
{
   if (role != Qt::DisplayRole)
      return QVariant();

   if (orientation == Qt::Horizontal)
      return msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NAME);
   return section + 1;
}

void PlaylistModel::setThumbnailType(const ThumbnailType type)
{
   m_thumbnailType = type;
}

void PlaylistModel::setThumbnailCacheLimit(int limit)
{
   m_cache.setMaxCost(limit * 1024);
}

QString PlaylistModel::getThumbnailPath(const QModelIndex &index,
      QString type) const
{
   return getThumbnailPath(m_contents.at(index.row()), type);
}

QString PlaylistModel::getPlaylistThumbnailsDir(
      const QString playlistName) const
{
   settings_t *settings            = config_get_ptr();
   const char *path_dir_thumbnails = settings->paths.directory_thumbnails;
   return QDir::cleanPath(QString(path_dir_thumbnails)) + "/" + playlistName;
}

bool PlaylistModel::isSupportedImage(const QString path) const
{
   QByteArray pathArray = path.toUtf8();
   return image_texture_get_type(pathArray.constData()) != IMAGE_TYPE_NONE;
}

QString PlaylistModel::getSanitizedThumbnailName(QString dir, QString label) const
{
   QDir tnDir(dir);

   QString tnName = label.replace(m_fileSanitizerRegex, "_");
   if (tnDir.exists(tnName + ".png"))
      return dir + tnName + ".png";
   if (tnDir.exists(tnName + ".jpg"))
      return dir + tnName + ".jpg";
   if (tnDir.exists(tnName + ".jpeg"))
      return dir + tnName + ".jpeg";
   if (tnDir.exists(tnName + ".bmp"))
      return dir + tnName + ".bmp";
   if (tnDir.exists(tnName + ".tga"))
      return dir + tnName + ".tga";
   return dir + tnName + ".png";

}

QString PlaylistModel::getThumbnailPath(const QHash<QString, QString> &hash, QString type) const
{
   /* use thumbnail widgets to show regular image files */
   if (isSupportedImage(hash["path"]))
      return hash["path"];

   return getSanitizedThumbnailName(
      getPlaylistThumbnailsDir(hash.value("db_name"))
      + QString("/")
      + type
      + QString("/"),
      hash["label_noext"]);
}

QString PlaylistModel::getCurrentTypeThumbnailPath(const QModelIndex &index) const
{
   switch (m_thumbnailType)
   {
      case THUMBNAIL_TYPE_BOXART:
         return getThumbnailPath(index, THUMBNAIL_BOXART);
      case THUMBNAIL_TYPE_SCREENSHOT:
         return getThumbnailPath(index, THUMBNAIL_SCREENSHOT);
      case THUMBNAIL_TYPE_TITLE_SCREEN:
         return getThumbnailPath(index, THUMBNAIL_TITLE);
      case THUMBNAIL_TYPE_LOGO:
         return getThumbnailPath(index, THUMBNAIL_LOGO);
      default:
         break;
   }

   return QString();
}

void PlaylistModel::reloadThumbnail(const QModelIndex &index)
{
   if (index.isValid())
   {
      reloadThumbnailPath(getCurrentTypeThumbnailPath(index));
      loadThumbnail(index);
   }
}

void PlaylistModel::reloadSystemThumbnails(const QString system)
{
   int i = 0;
   settings_t *settings            = config_get_ptr();
   const char *path_dir_thumbnails = settings->paths.directory_thumbnails;
   QString           path          = QDir::cleanPath(QString(path_dir_thumbnails))
	   + QString("/") + system;
   QList<QString>             keys = m_cache.keys();
   QList<QString>          pending = m_pendingImages.values();

   for (i = 0; i < keys.size(); i++)
   {
      QString key = keys.at(i);
      if (key.startsWith(path))
         m_cache.remove(key);
   }

   for (i = 0; i < pending.size(); i++)
   {
      QString key = pending.at(i);
      if (key.startsWith(path))
         m_pendingImages.remove(key);
   }
}

void PlaylistModel::reloadThumbnailPath(const QString path)
{
   m_cache.remove(path);
   m_pendingImages.remove(path);
}

void PlaylistModel::loadThumbnail(const QModelIndex &index)
{
   QString path = getCurrentTypeThumbnailPath(index);

   if (!m_pendingImages.contains(path) && !m_cache.contains(path))
   {
      m_pendingImages.insert(path);
      m_thumbnailLoader->request(index, path);
   }
}

void PlaylistModel::onImageLoaded(const QImage image,
		const QModelIndex &index, const QString &path)
{
   QPixmap *pixmap = new QPixmap(QPixmap::fromImage(image));
   const int  cost = pixmap->width() * pixmap->height() * pixmap->depth() / (8 * 1024);
   m_cache.insert(path, pixmap, cost);
   if (index.isValid())
      emit dataChanged(index, index, { THUMBNAIL });
   m_pendingImages.remove(path);
}

static inline bool comp_hash_name_key_lower(const QHash<QString,
		QString> &lhs, const QHash<QString, QString> &rhs)
{
   return lhs.value("name").toLower() < rhs.value("name").toLower();
}

bool MainWindow::addDirectoryFilesToList(QProgressDialog *dialog,
      QStringList &list, QDir &dir, QStringList &extensions)
{
   int i;
   PlaylistEntryDialog *playlistDialog = playlistEntryDialog();
   QStringList                 dirList = dir.entryList(QStringList(),
		     QDir::Dirs   | QDir::Files | QDir::NoDotAndDotDot
		   | QDir::Hidden | QDir::System,
		   QDir::Name);

   for (i = 0; i < dirList.count(); i++)
   {
      QString path(dir.path() + QString("/") + dirList.at(i));
      QByteArray pathArray = path.toUtf8();
      QFileInfo fileInfo(path);
      const char *pathData = pathArray.constData();

      if (dialog->wasCanceled())
         return false;

      /* Needed to update progress dialog while doing
       * a lot of stuff on the main thread. */
      if (i % 25 == 0)
         qApp->processEvents();

      if (fileInfo.isDir())
      {
         QDir fileInfoDir(path);
         if (!addDirectoryFilesToList(
               dialog, list, fileInfoDir, extensions))
            return false;
         continue;
      }

      if (fileInfo.isFile())
      {
         bool add = false;

         if (extensions.isEmpty())
            add = true;
         else
         {
            if (extensions.contains(fileInfo.suffix()))
               add = true;
            else
            {
               if (path_is_compressed_file(pathData))
               {
                  struct string_list *archive_list =
                     file_archive_get_file_list(pathData, NULL);

                  if (archive_list)
                  {
                     if (archive_list->size == 1)
                     {
                        /* Assume archives with one file should have
                         * that file loaded directly.
                         * Don't just extend this to add all files
                         * in a ZIP, because we might hit something like
                         * MAME/FBA where only the archives themselves
                         * are valid content. */
                        pathArray = (QString(pathData)
			      + QString("#")
                              + archive_list->elems[0].data).toUtf8();
                        pathData = pathArray.constData();

                        if (!extensions.isEmpty() && playlistDialog->filterInArchive())
                        {
                           /* If the user chose to filter extensions
                            * inside archives, and this particular file
                            * inside the archive
                            * doesn't have one of the chosen extensions,
                            * then we skip it. */
                           if (extensions.contains(QFileInfo(pathData).suffix()))
                              add = true;
                        }
                     }

                     string_list_free(archive_list);
                  }
               }
            }
         }

         if (add)
            list.append(fileInfo.absoluteFilePath());
      }
   }

   return true;
}

void MainWindow::onPlaylistFilesDropped(QStringList files)
{
   addFilesToPlaylist(files);
}

/* Takes a list of files and folders and adds them to the
 * currently selected playlist. Folders will have their
 * contents added recursively. */
void MainWindow::addFilesToPlaylist(QStringList files)
{
   int i;
   QStringList list;
   QString currentPlaylistPath;
   QByteArray currentPlaylistArray;
   QScopedPointer<QProgressDialog> dialog(NULL);
   QHash<QString, QString> selectedCore;
   QHash<QString, QString> itemToAdd;
   QString selectedDatabase;
   QString selectedName;
   QString selectedPath;
   QStringList selectedExtensions;
   playlist_config_t playlist_config;
   QListWidgetItem        *currentItem = m_listWidget->currentItem();
   PlaylistEntryDialog *playlistDialog = playlistEntryDialog();
   const char *currentPlaylistData     = NULL;
   playlist_t *playlist                = NULL;
   settings_t *settings                = config_get_ptr();

   playlist_config.capacity            = COLLECTION_SIZE;
   playlist_config.old_format          = settings->bools.playlist_use_old_format;
   playlist_config.compress            = settings->bools.playlist_compression;
   playlist_config.fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;
   playlist_config_set_base_content_directory(&playlist_config, settings->bools.playlist_portable_paths ? settings->paths.directory_menu_content : NULL);

   /* Assume a blank list means we will manually enter in all fields. */
   if (files.isEmpty())
   {
      /* Make sure hash isn't blank, that would mean there's
       * multiple entries to add at once. */
      itemToAdd["label"] = QLatin1String("");
      itemToAdd["path"]  = QLatin1String("");
   }
   else if (files.count() == 1)
   {
      QString path = files.at(0);
      QFileInfo info(path);

      if (info.isFile())
      {
         itemToAdd["label"] = info.completeBaseName();
         itemToAdd["path"]  = path;
      }
   }

   if (currentItem)
   {
      currentPlaylistPath = currentItem->data(Qt::UserRole).toString();

      if (!currentPlaylistPath.isEmpty())
      {
         currentPlaylistArray = currentPlaylistPath.toUtf8();
         currentPlaylistData  = currentPlaylistArray.constData();
      }
   }

   if (currentPlaylistPath == ALL_PLAYLISTS_TOKEN)
   {
      showMessageBox(msg_hash_to_str(
		      MENU_ENUM_LABEL_VALUE_QT_CANNOT_ADD_TO_ALL_PLAYLISTS),
		      MainWindow::MSGBOX_TYPE_ERROR,
		      Qt::ApplicationModal, false);
      return;
   }

   /* a blank itemToAdd means there will be multiple */
   if (!playlistDialog->showDialog(itemToAdd))
      return;

   selectedName       = m_playlistEntryDialog->getSelectedName();
   selectedPath       = m_playlistEntryDialog->getSelectedPath();
   selectedCore       = m_playlistEntryDialog->getSelectedCore();
   selectedDatabase   = m_playlistEntryDialog->getSelectedDatabase();
   selectedExtensions = m_playlistEntryDialog->getSelectedExtensions();

   if (!selectedExtensions.isEmpty())
      selectedExtensions.replaceInStrings(QRegularExpression("^\\."), "");

   if (selectedDatabase.isEmpty())
      selectedDatabase = QFileInfo(currentPlaylistPath).fileName();
   else
      selectedDatabase.append(".lpl");

   dialog.reset(new QProgressDialog(msg_hash_to_str(
			   MENU_ENUM_LABEL_VALUE_QT_GATHERING_LIST_OF_FILES),
			   "Cancel", 0, 0, this));
   dialog->setWindowModality(Qt::ApplicationModal);
   dialog->show();

   qApp->processEvents();

   if (     selectedName.isEmpty()
         || selectedPath.isEmpty()
         || selectedDatabase.isEmpty())
   {
      showMessageBox(msg_hash_to_str(
		MENU_ENUM_LABEL_VALUE_QT_PLEASE_FILL_OUT_REQUIRED_FIELDS),
		MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
      return;
   }

   if (files.isEmpty())
      files.append(selectedPath);

   for (i = 0; i < files.count(); i++)
   {
      QString path(files.at(i));
      QFileInfo fileInfo(path);

      if (dialog->wasCanceled())
         return;

      /* Needed to update progress dialog while
       * doing a lot of stuff on the main thread. */
      if (i % 25 == 0)
         qApp->processEvents();

      if (fileInfo.isDir())
      {
         QDir dir(path);
         if (!addDirectoryFilesToList(
               dialog.data(), list, dir, selectedExtensions))
            return;
         continue;
      }

      if (fileInfo.isFile())
      {
         bool add = false;

         if (selectedExtensions.isEmpty())
            add = true;
         else
         {
            QByteArray pathArray = path.toUtf8();
            const char *pathData = pathArray.constData();

            if (selectedExtensions.contains(fileInfo.suffix()))
               add = true;
            else if (playlistDialog->filterInArchive()
                  && path_is_compressed_file(pathData))
            {
               /* We'll add it here, but really just delay
                * the check until later when the archive
                * contents are iterated. */
               add = true;
            }
         }

         if (add)
            list.append(fileInfo.absoluteFilePath());
      }
      else if (files.count() == 1)
      {
         /* If adding a single file, tell user that it doesn't exist. */
         showMessageBox(msg_hash_to_str(
			MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST),
			MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal,
			false);
         return;
      }
   }

   dialog->setLabelText(msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_ADDING_FILES_TO_PLAYLIST));
   dialog->setMaximum(list.count());

   playlist_config_set_path(&playlist_config, currentPlaylistData);
   playlist = playlist_init(&playlist_config);

   for (i = 0; i < list.count(); i++)
   {
      QFileInfo fileInfo;
      QByteArray fileBaseNameArray;
      QByteArray pathArray;
      QByteArray corePathArray;
      QByteArray coreNameArray;
      QByteArray databaseArray;
      QString fileName            = list.at(i);
      const char *pathData        = NULL;
      const char *fileNameNoExten = NULL;
      const char *corePathData    = NULL;
      const char *coreNameData    = NULL;
      const char *databaseData    = NULL;

      /* Cancel out of everything, the
       * current progress will not be written
       * to the playlist at all. */
      if (dialog->wasCanceled())
      {
         playlist_free(playlist);
         return;
      }

      if (fileName.isEmpty())
         continue;

      /* a modal QProgressDialog calls processEvents()
       * automatically in setValue() */
      dialog->setValue(i + 1);

      fileInfo = QFileInfo(fileName);

      /* Make sure we're looking at a user-specified field
       * and not just "<multiple>"
       * in case it was a folder with one file in it */
      if (     files.count() == 1
            && list.count()  == 1
            && i == 0
            && playlistDialog->nameFieldEnabled())
      {
         fileBaseNameArray = selectedName.toUtf8();
         pathArray = QDir::toNativeSeparators(selectedPath).toUtf8();
      }
      /* Otherwise just use the file name itself (minus extension)
       * for the playlist entry title */
      else
      {
         fileBaseNameArray = fileInfo.completeBaseName().toUtf8();
         pathArray         = QDir::toNativeSeparators(fileName).toUtf8();
      }

      fileNameNoExten      = fileBaseNameArray.constData();

      pathData             = pathArray.constData();

      if (selectedCore.isEmpty())
      {
         corePathData = "DETECT";
         coreNameData = "DETECT";
      }
      else
      {
         corePathArray = QDir::toNativeSeparators(
               selectedCore.value("core_path")).toUtf8();
         coreNameArray = selectedCore.value("core_name").toUtf8();
         corePathData  = corePathArray.constData();
         coreNameData  = coreNameArray.constData();
      }

      databaseArray = selectedDatabase.toUtf8();
      databaseData = databaseArray.constData();

      if (path_is_compressed_file(pathData))
      {
         struct string_list *list = file_archive_get_file_list(pathData, NULL);

         if (list)
         {
            if (list->size == 1)
            {
               /* Assume archives with one file should have that
                * file loaded directly.
                * Don't just extend this to add all files in a zip,
                * because we might hit
                * something like MAME/FBA where only the archives
                * themselves are valid content. */
               pathArray = QDir::toNativeSeparators(QString(pathData)
                     + QString("#")
		     + list->elems[0].data).toUtf8();
               pathData  = pathArray.constData();

               if (     !selectedExtensions.isEmpty()
                     &&  playlistDialog->filterInArchive())
               {
                  /* If the user chose to filter extensions inside archives,
                   * and this particular file inside the archive
                   * doesn't have one of the chosen extensions,
                   * then we skip it. */
                  if (!selectedExtensions.contains(
                           QFileInfo(pathData).suffix()))
                  {
                     string_list_free(list);
                     continue;
                  }
               }
            }

            string_list_free(list);
         }
      }

      {
         struct playlist_entry entry = {0};

         /* the push function reads our entry as const,
          * so these casts are safe */
         entry.path      = const_cast<char*>(pathData);
         entry.label     = const_cast<char*>(fileNameNoExten);
         entry.core_path = const_cast<char*>(corePathData);
         entry.core_name = const_cast<char*>(coreNameData);
         entry.crc32     = const_cast<char*>("00000000|crc");
         entry.db_name   = const_cast<char*>(databaseData);

         playlist_push(playlist, &entry);
      }
   }

   playlist_write_file(playlist);
   playlist_free(playlist);

   reloadPlaylists();
}

bool MainWindow::updateCurrentPlaylistEntry(
      const QHash<QString, QString> &contentHash)
{
   QString path;
   QString label;
   QString corePath;
   QString coreName;
   QString dbName;
   QString crc32;
   QByteArray playlistPathArray;
   QByteArray pathArray;
   QByteArray labelArray;
   QByteArray corePathArray;
   QByteArray coreNameArray;
   QByteArray dbNameArray;
   QByteArray crc32Array;
   playlist_config_t playlist_config;
   QString playlistPath         = getCurrentPlaylistPath();
   const char *playlistPathData = NULL;
   const char *pathData         = NULL;
   const char *labelData        = NULL;
   const char *corePathData     = NULL;
   const char *coreNameData     = NULL;
   const char *dbNameData       = NULL;
   const char *crc32Data        = NULL;
   playlist_t *playlist         = NULL;
   unsigned index               = 0;
   bool ok                      = false;
   settings_t *settings         = config_get_ptr();

   playlist_config.capacity            = COLLECTION_SIZE;
   playlist_config.old_format          = settings->bools.playlist_use_old_format;
   playlist_config.compress            = settings->bools.playlist_compression;
   playlist_config.fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;
   playlist_config_set_base_content_directory(&playlist_config,
		    settings->bools.playlist_portable_paths
		  ? settings->paths.directory_menu_content : NULL);

   if (      playlistPath.isEmpty()
         ||  contentHash.isEmpty()
         || !contentHash.contains("index"))
      return false;

   index = contentHash.value("index").toUInt(&ok);

   if (!ok)
      return false;

   path     = contentHash.value("path");
   label    = contentHash.value("label");
   coreName = contentHash.value("core_name");
   corePath = contentHash.value("core_path");
   dbName   = contentHash.value("db_name");
   crc32    = contentHash.value("crc32");

   if (   path.isEmpty()
       || label.isEmpty()
       || coreName.isEmpty()
       || corePath.isEmpty()
      )
      return false;

   playlistPathArray = playlistPath.toUtf8();
   pathArray         = QDir::toNativeSeparators(path).toUtf8();
   labelArray        = label.toUtf8();
   coreNameArray     = coreName.toUtf8();
   corePathArray     = QDir::toNativeSeparators(corePath).toUtf8();

   if (!dbName.isEmpty())
   {
      dbNameArray    = (dbName + ".lpl").toUtf8();
      dbNameData     = dbNameArray.constData();
   }

   playlistPathData  = playlistPathArray.constData();
   pathData          = pathArray.constData();
   labelData         = labelArray.constData();
   coreNameData      = coreNameArray.constData();
   corePathData      = corePathArray.constData();

   if (!crc32.isEmpty())
   {
      crc32Array     = crc32.toUtf8();
      crc32Data      = crc32Array.constData();
   }

   if (path_is_compressed_file(pathData))
   {
      struct string_list *list = file_archive_get_file_list(pathData, NULL);

      if (list)
      {
         if (list->size == 1)
         {
            /* assume archives with one file should have that file loaded directly */
            pathArray = QDir::toNativeSeparators(QString(pathData)
		      + QString("#")
		      + list->elems[0].data).toUtf8();
            pathData  = pathArray.constData();
         }

         string_list_free(list);
      }
   }

   playlist_config_set_path(&playlist_config, playlistPathData);
   playlist = playlist_init(&playlist_config);

   {
      struct playlist_entry entry = {0};

      /* The update function reads our entry as const,
       * so these casts are safe */
      entry.path      = const_cast<char*>(pathData);
      entry.label     = const_cast<char*>(labelData);
      entry.core_path = const_cast<char*>(corePathData);
      entry.core_name = const_cast<char*>(coreNameData);
      entry.crc32     = const_cast<char*>(crc32Data);
      entry.db_name   = const_cast<char*>(dbNameData);

      playlist_update(playlist, index, &entry);
   }

   playlist_write_file(playlist);
   playlist_free(playlist);

   reloadPlaylists();

   return true;
}

void MainWindow::onPlaylistWidgetContextMenuRequested(const QPoint&)
{
   QString currentPlaylistDirPath;
   QString currentPlaylistPath;
   QString currentPlaylistFileName;
   QFile currentPlaylistFile;
   QFileInfo currentPlaylistFileInfo;
   QMap<QString, const core_info_t*> coreList;
   QScopedPointer<QMenu> menu;
   QScopedPointer<QMenu> associateMenu;
   QScopedPointer<QMenu> hiddenPlaylistsMenu;
   QScopedPointer<QMenu> downloadAllThumbnailsMenu;
   QScopedPointer<QAction> hideAction;
   QScopedPointer<QAction> newPlaylistAction;
   QScopedPointer<QAction> deletePlaylistAction;
   QScopedPointer<QAction> renamePlaylistAction;
   QScopedPointer<QAction> downloadAllThumbnailsEntireSystemAction;
   QScopedPointer<QAction> downloadAllThumbnailsThisPlaylistAction;
   QPointer<QAction> selectedAction;
   playlist_config_t playlist_config;
   QPoint cursorPos                    = QCursor::pos();
   settings_t *settings                = config_get_ptr();
   const char *path_dir_playlist       = settings->paths.directory_playlist;
   QDir playlistDir(path_dir_playlist);
   QListWidgetItem *selectedItem       = m_listWidget->itemAt(
         m_listWidget->viewport()->mapFromGlobal(cursorPos));
   QString playlistDirAbsPath          = playlistDir.absolutePath();
   core_info_list_t *core_info_list    = NULL;
   unsigned i                          = 0;
   int j                               = 0;
   bool specialPlaylist                = false;
   bool foundHiddenPlaylist            = false;

   playlist_config.capacity            = COLLECTION_SIZE;
   playlist_config.old_format          = settings->bools.playlist_use_old_format;
   playlist_config.compress            = settings->bools.playlist_compression;
   playlist_config.fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;
   playlist_config_set_base_content_directory(&playlist_config,
		   settings->bools.playlist_portable_paths
		 ? settings->paths.directory_menu_content : NULL);

   if (selectedItem)
   {
      currentPlaylistPath     = selectedItem->data(Qt::UserRole).toString();
      currentPlaylistFile.setFileName(currentPlaylistPath);

      currentPlaylistFileInfo = QFileInfo(currentPlaylistPath);
      currentPlaylistFileName = currentPlaylistFileInfo.fileName();
      currentPlaylistDirPath  = currentPlaylistFileInfo.absoluteDir().absolutePath();
   }

   menu.reset(new QMenu(this));
   menu->setObjectName("menu");

   hiddenPlaylistsMenu.reset(new QMenu(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS), this));
   newPlaylistAction.reset(new QAction(QString(
		msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST))
	      + QString("..."), this));

   hiddenPlaylistsMenu->setObjectName("hiddenPlaylistsMenu");

   menu->addAction(newPlaylistAction.data());

   if (currentPlaylistFile.exists())
   {
      deletePlaylistAction.reset(new QAction(
               QString(msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST))
	           + QString("..."), this));
      menu->addAction(deletePlaylistAction.data());

      renamePlaylistAction.reset(new QAction(QString(
	  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST))
	+ QString("..."), this));
      menu->addAction(renamePlaylistAction.data());
   }

   if (selectedItem)
   {
      hideAction.reset(new QAction(msg_hash_to_str(
	MENU_ENUM_LABEL_VALUE_QT_HIDE), this));
      menu->addAction(hideAction.data());
   }

   for (j = 0; j < m_listWidget->count(); j++)
   {
      QListWidgetItem *item = m_listWidget->item(j);
#if (QT_VERSION > QT_VERSION_CHECK(6, 0, 0))
      bool           hidden = item->isHidden();
#else
      bool           hidden = m_listWidget->isItemHidden(item);
#endif

      if (hidden)
      {
         QAction *action = hiddenPlaylistsMenu->addAction(item->text());
         action->setProperty("row", j);
         action->setProperty("core_path", item->data(Qt::UserRole).toString());
         foundHiddenPlaylist = true;
      }
   }

   if (!foundHiddenPlaylist)
   {
      QAction *action = hiddenPlaylistsMenu->addAction(
		msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE));
      action->setProperty("row", -1);
   }

   menu->addMenu(hiddenPlaylistsMenu.data());

   /* Don't just compare strings in case there are
    * case differences on Windows that should be ignored.
    * Special playlists like history etc. can't have an association */
   if (QDir(currentPlaylistDirPath) != QDir(playlistDirAbsPath))
      specialPlaylist = true;

   if (!specialPlaylist)
   {
      associateMenu.reset(new QMenu(msg_hash_to_str(
		MENU_ENUM_LABEL_VALUE_QT_ASSOCIATE_CORE), this));
      associateMenu->setObjectName("associateMenu");

      core_info_get_list(&core_info_list);

      for (i = 0; i < core_info_list->count && core_info_list->count > 0; i++)
      {
         const core_info_t   *core = &core_info_list->list[i];
         coreList[core->core_name] = core;
      }

      {
         QMapIterator<QString, const core_info_t*> coreListIterator(coreList);
         QVector<QHash<QString, QString> > cores;

         while (coreListIterator.hasNext())
         {
            QString key, name;
            const core_info_t *core = NULL;
            QHash<QString, QString> hash;

            coreListIterator.next();

            key = coreListIterator.key();
            core = coreList.value(key);

            if (!core->core_name || !*core->core_name)
               name = core->display_name;
            else
               name = core->core_name;

            if (name.isEmpty())
               continue;

            hash["name"] = name;
            hash["core_path"] = core->path;

            cores.append(hash);
         }

         std::sort(cores.begin(), cores.end(), comp_hash_name_key_lower);

         for (j = 0; j < cores.count(); j++)
         {
            const QHash<QString, QString> &hash = cores.at(j);
            QAction *action = associateMenu->addAction(hash.value("name"));

            action->setProperty("core_path", hash.value("core_path"));
         }
      }

      menu->addMenu(associateMenu.data());
   }

   if (!specialPlaylist)
   {
      downloadAllThumbnailsMenu.reset(new QMenu(
		msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS), this));
      downloadAllThumbnailsMenu->setObjectName("downloadAllThumbnailsMenu");

      downloadAllThumbnailsThisPlaylistAction.reset(
		      new QAction(msg_hash_to_str(
		MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_THIS_PLAYLIST),
			      downloadAllThumbnailsMenu.data()));
      downloadAllThumbnailsEntireSystemAction.reset(
		      new QAction(msg_hash_to_str(
		MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_ENTIRE_SYSTEM),
			      downloadAllThumbnailsMenu.data()));

      downloadAllThumbnailsMenu->addAction(
		      downloadAllThumbnailsThisPlaylistAction.data());
      downloadAllThumbnailsMenu->addAction(
		      downloadAllThumbnailsEntireSystemAction.data());

      menu->addMenu(downloadAllThumbnailsMenu.data());
   }

   selectedAction = menu->exec(cursorPos);

   if (!selectedAction)
      return;

   if (!specialPlaylist && selectedAction->parent() == associateMenu.data())
   {
      core_info_t *coreInfo                   = NULL;
      playlist_t *cachedPlaylist              = playlist_get_cached();
      playlist_t *playlist                    = NULL;
      bool loadPlaylist                       = true;
      QByteArray currentPlaylistPathByteArray = currentPlaylistPath.toUtf8();
      const char *currentPlaylistPathCString  = currentPlaylistPathByteArray.data();
      QByteArray corePathByteArray            = selectedAction->property("core_path").toString().toUtf8();
      const char *corePath                    = corePathByteArray.data();

      /* Load playlist, if required */
      if (cachedPlaylist)
      {
         if (string_is_equal(currentPlaylistPathCString,
                  playlist_get_conf_path(cachedPlaylist)))
         {
            playlist     = cachedPlaylist;
            loadPlaylist = false;
         }
      }

      if (loadPlaylist)
      {
         playlist_config_set_path(&playlist_config, currentPlaylistPathCString);
         playlist = playlist_init(&playlist_config);
      }

      if (playlist)
      {
         /* Get core info */
         if (core_info_find(corePath, &coreInfo))
         {
            /* Set new core association */
            playlist_set_default_core_path(playlist, coreInfo->path);
            playlist_set_default_core_name(playlist, coreInfo->display_name);
         }
         else
         {
            playlist_set_default_core_path(playlist, "DETECT");
            playlist_set_default_core_name(playlist, "DETECT");
         }

         /* Write changes to disk */
         playlist_write_file(playlist);

         /* Free playlist, if required */
         if (loadPlaylist)
            playlist_free(playlist);
      }
   }
   else if (selectedItem && selectedAction == deletePlaylistAction.data())
   {
      if (currentPlaylistFile.exists())
      {
         if (showMessageBox(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST)).arg(selectedItem->text()), MainWindow::MSGBOX_TYPE_QUESTION_YESNO, Qt::ApplicationModal, false))
         {
            if (currentPlaylistFile.remove())
               reloadPlaylists();
            else
               showMessageBox(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE), MainWindow::MSGBOX_TYPE_ERROR, Qt::ApplicationModal, false);
         }
      }
   }
   else if (selectedItem && selectedAction == renamePlaylistAction.data())
   {
      if (currentPlaylistFile.exists())
      {
         QString oldName = selectedItem->text();
         QString name = QInputDialog::getText(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST), msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME), QLineEdit::Normal, oldName);

         if (!name.isEmpty())
         {
            renamePlaylistItem(selectedItem, name);
            reloadPlaylists();
         }
      }
   }
   else if (selectedAction == newPlaylistAction.data())
   {
      QString name            = QInputDialog::getText(this,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME));
      QString newPlaylistPath = playlistDirAbsPath + "/" + name + ".lpl";
      QFile file(newPlaylistPath);

      if (!name.isEmpty())
      {
         if (file.open(QIODevice::WriteOnly))
            file.close();

         reloadPlaylists();
      }
   }
   else if (selectedItem && selectedAction == hideAction.data())
   {
      int row = m_listWidget->row(selectedItem);

      if (row >= 0)
      {
         QStringList hiddenPlaylists = m_settings->value("hidden_playlists").toStringList();

         if (!hiddenPlaylists.contains(currentPlaylistFileName))
         {
            hiddenPlaylists.append(currentPlaylistFileName);
            m_settings->setValue("hidden_playlists", hiddenPlaylists);
         }

         m_listWidget->setRowHidden(row, true);
      }
   }
   else if (selectedAction->parent() == hiddenPlaylistsMenu.data())
   {
      QVariant rowVariant = selectedAction->property("row");

      if (rowVariant.isValid())
      {
         QStringList hiddenPlaylists = m_settings->value("hidden_playlists").toStringList();
         int row = rowVariant.toInt();

         if (row >= 0)
         {
            QString playlistPath     = selectedAction->property("core_path").toString();
            QFileInfo playlistFileInfo(playlistPath);
            QString playlistFileName = playlistFileInfo.fileName();

            if (hiddenPlaylists.contains(playlistFileName))
            {
               hiddenPlaylists.removeOne(playlistFileName);
               m_settings->setValue("hidden_playlists", hiddenPlaylists);
            }

            m_listWidget->setRowHidden(row, false);
         }
      }
   }
   else if (selectedItem && !specialPlaylist && selectedAction->parent() == downloadAllThumbnailsMenu.data())
   {
      if (selectedAction == downloadAllThumbnailsEntireSystemAction.data())
      {
         int row = m_listWidget->row(selectedItem);

         if (row >= 0)
            downloadAllThumbnails(currentPlaylistFileInfo.completeBaseName());
      }
      else if (selectedAction == downloadAllThumbnailsThisPlaylistAction.data())
         downloadPlaylistThumbnails(currentPlaylistPath);
   }

   setCoreActions();
}

void MainWindow::deferReloadPlaylists() { emit gotReloadPlaylists(); }
void MainWindow::onGotReloadPlaylists() { reloadPlaylists(); }

void MainWindow::reloadPlaylists()
{
   int i;
   QString currentPlaylistPath;
   QListWidgetItem *allPlaylistsItem       = NULL;
   QListWidgetItem *favoritesPlaylistsItem = NULL;
   QListWidgetItem *imagePlaylistsItem     = NULL;
   QListWidgetItem *musicPlaylistsItem     = NULL;
   QListWidgetItem *videoPlaylistsItem     = NULL;
   QListWidgetItem *firstItem              = NULL;
   settings_t *settings                    = config_get_ptr();
   const char *path_dir_playlist           = settings->paths.directory_playlist;
   QDir playlistDir(path_dir_playlist);
   QStringList hiddenPlaylists             = m_settings->value(
         "hidden_playlists").toStringList();

   QListWidgetItem *currentItem            = m_listWidget->currentItem();

   if (currentItem)
      currentPlaylistPath = currentItem->data(Qt::UserRole).toString();

   getPlaylistFiles();

   m_listWidget->clear();
   m_listWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
   m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
   m_listWidget->setEditTriggers(QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);

   allPlaylistsItem = new QListWidgetItem(m_folderIcon, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS));
   allPlaylistsItem->setData(Qt::UserRole, ALL_PLAYLISTS_TOKEN);

   favoritesPlaylistsItem = new QListWidgetItem(m_folderIcon, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES_TAB));
   favoritesPlaylistsItem->setData(Qt::UserRole, QString(settings->paths.path_content_favorites));

   m_historyPlaylistsItem = new QListWidgetItem(m_folderIcon, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HISTORY_TAB));
   m_historyPlaylistsItem->setData(Qt::UserRole, QString(settings->paths.path_content_history));

   imagePlaylistsItem = new QListWidgetItem(m_folderIcon, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_IMAGES_TAB));
   imagePlaylistsItem->setData(Qt::UserRole, QString(settings->paths.path_content_image_history));

   musicPlaylistsItem = new QListWidgetItem(m_folderIcon, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MUSIC_TAB));
   musicPlaylistsItem->setData(Qt::UserRole, QString(settings->paths.path_content_music_history));

   videoPlaylistsItem = new QListWidgetItem(m_folderIcon, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_TAB));
   videoPlaylistsItem->setData(Qt::UserRole, QString(settings->paths.path_content_video_history));

   m_listWidget->addItem(allPlaylistsItem);
   m_listWidget->addItem(favoritesPlaylistsItem);
   m_listWidget->addItem(m_historyPlaylistsItem);
   m_listWidget->addItem(imagePlaylistsItem);
   m_listWidget->addItem(musicPlaylistsItem);
   m_listWidget->addItem(videoPlaylistsItem);

   if (hiddenPlaylists.contains(ALL_PLAYLISTS_TOKEN))
      m_listWidget->setRowHidden(m_listWidget->row(allPlaylistsItem), true);
   if (hiddenPlaylists.contains(QFileInfo(settings->paths.path_content_favorites).fileName()))
      m_listWidget->setRowHidden(m_listWidget->row(favoritesPlaylistsItem), true);
   if (hiddenPlaylists.contains(QFileInfo(settings->paths.path_content_history).fileName()))
      m_listWidget->setRowHidden(m_listWidget->row(m_historyPlaylistsItem), true);
   if (hiddenPlaylists.contains(QFileInfo(settings->paths.path_content_image_history).fileName()))
      m_listWidget->setRowHidden(m_listWidget->row(imagePlaylistsItem), true);
   if (hiddenPlaylists.contains(QFileInfo(settings->paths.path_content_music_history).fileName()))
      m_listWidget->setRowHidden(m_listWidget->row(musicPlaylistsItem), true);
   if (hiddenPlaylists.contains(QFileInfo(settings->paths.path_content_video_history).fileName()))
      m_listWidget->setRowHidden(m_listWidget->row(videoPlaylistsItem), true);

   for (i = 0; i < m_playlistFiles.count(); i++)
   {
      QIcon icon;
      QString iconPath;
      QListWidgetItem   *item = NULL;
      const QString     &file = m_playlistFiles.at(i);

      /* don't show view files */
      if (file.endsWith(".lvw", Qt::CaseInsensitive))
         continue;

      QString fileDisplayName = file;
      QString        fileName = file;
      bool            hasIcon = false;

      fileDisplayName.remove(".lpl");

      iconPath                = QString(
            settings->paths.directory_assets)
         + ICON_PATH
         + fileDisplayName
         + ".png";

      hasIcon                 = QFile::exists(iconPath);

      if (hasIcon)
         icon                 = QIcon(iconPath);
      else
         icon                 = m_folderIcon;

      item                    = new QListWidgetItem(icon, fileDisplayName);
      item->setFlags(item->flags() | Qt::ItemIsEditable);
      item->setData(Qt::UserRole, playlistDir.absoluteFilePath(file));

      m_listWidget->addItem(item);

      if (hiddenPlaylists.contains(fileName))
      {
         int row = m_listWidget->row(item);

         if (row >= 0)
            m_listWidget->setRowHidden(row, true);
      }
   }

   if (m_listWidget->count() > 0)
   {
      firstItem = m_listWidget->item(0);

      if (firstItem)
      {
         bool            foundCurrent = false;
         bool            foundInitial = false;
         QString      initialPlaylist = m_settings->value("initial_playlist", m_historyPlaylistsItem->data(Qt::UserRole).toString()).toString();
         QListWidgetItem *initialItem = NULL;

         for (i = 0; i < m_listWidget->count(); i++)
         {
            QString path;
            QListWidgetItem *item = m_listWidget->item(i);

            if (item)
            {
               path = item->data(Qt::UserRole).toString();

               if (!path.isEmpty())
               {
                  /* don't break early here since we want
                   * to make sure we've found both initial
                   * and current items if they exist */
                  if (!foundInitial && path == initialPlaylist)
                  {
                     foundInitial = true;
                     initialItem  = item;
                  }
                  if (     !foundCurrent
                        && !currentPlaylistPath.isEmpty()
                        && path == currentPlaylistPath)
                  {
                     foundCurrent = true;
                     m_listWidget->setCurrentItem(item);
                  }
               }
            }
         }

         if (!foundCurrent)
         {
            if (foundInitial && initialItem)
               m_listWidget->setCurrentItem(initialItem);
            else
            {
               /* the previous playlist must be gone now,
                * just select the first one */
               m_listWidget->setCurrentItem(firstItem);
            }
         }
      }
   }
}

QString MainWindow::getCurrentPlaylistPath()
{
   QString playlistPath;
   QListWidgetItem *playlistItem = m_listWidget->currentItem();

   if (!playlistItem)
      return playlistPath;

   playlistPath = playlistItem->data(Qt::UserRole).toString();

   return playlistPath;
}

bool MainWindow::currentPlaylistIsSpecial()
{
   QFileInfo currentPlaylistFileInfo;
   QString currentPlaylistPath;
   QString currentPlaylistDirPath;
   settings_t *settings                 = config_get_ptr();
   QDir playlistDir(settings->paths.directory_playlist);
   QString playlistDirAbsPath           = playlistDir.absolutePath();
   QListWidgetItem *currentPlaylistItem = m_listWidget->currentItem();

   if (!currentPlaylistItem)
      return false;

   currentPlaylistPath     = currentPlaylistItem->data(Qt::UserRole).toString();
   currentPlaylistFileInfo = QFileInfo(currentPlaylistPath);
   currentPlaylistDirPath  = currentPlaylistFileInfo.absoluteDir().absolutePath();

   /* Don't just compare strings in case there are
    * case differences on Windows that should be ignored. */
   if (QDir(currentPlaylistDirPath) != QDir(playlistDirAbsPath))
      return true;
   return false;
}

bool MainWindow::currentPlaylistIsAll()
{
   QListWidgetItem *currentPlaylistItem = m_listWidget->currentItem();
   if (
            currentPlaylistItem
         && currentPlaylistItem->data(Qt::UserRole).toString()
         == ALL_PLAYLISTS_TOKEN)
      return true;
   return false;
}

void MainWindow::deleteCurrentPlaylistItem()
{
   QByteArray playlistArray;
   playlist_config_t playlist_config;
   QString playlistPath                = getCurrentPlaylistPath();
   QHash<QString, QString> contentHash = getCurrentContentHash();
   playlist_t *playlist                = NULL;
   const char *playlistData            = NULL;
   unsigned index                      = 0;
   bool ok                             = false;
   bool isAllPlaylist                  = currentPlaylistIsAll();
   settings_t *settings                = config_get_ptr();

   playlist_config.capacity            = COLLECTION_SIZE;
   playlist_config.old_format          = settings->bools.playlist_use_old_format;
   playlist_config.compress            = settings->bools.playlist_compression;
   playlist_config.fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;
   playlist_config_set_base_content_directory(&playlist_config,
		   settings->bools.playlist_portable_paths
		 ? settings->paths.directory_menu_content : NULL);

   if (isAllPlaylist)
      return;

   if (playlistPath.isEmpty())
      return;

   if (contentHash.isEmpty())
      return;

   playlistArray = playlistPath.toUtf8();
   playlistData = playlistArray.constData();

   index = contentHash.value("index").toUInt(&ok);

   if (!ok)
      return;

   if (!showMessageBox(QString(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST_ITEM)).arg(contentHash["label"]), MainWindow::MSGBOX_TYPE_QUESTION_YESNO, Qt::ApplicationModal, false))
      return;

   playlist_config_set_path(&playlist_config, playlistData);
   playlist = playlist_init(&playlist_config);

   playlist_delete_index(playlist, index);
   playlist_write_file(playlist);
   playlist_free(playlist);

   reloadPlaylists();
}

QString MainWindow::getPlaylistDefaultCore(QString plName)
{
   size_t _len;
   playlist_config_t playlist_config;
   char playlist_path[PATH_MAX_LENGTH];
   QByteArray plNameByteArray          = plName.toUtf8();
   const char *plNameCString           = plNameByteArray.data();
   playlist_t *cachedPlaylist          = playlist_get_cached();
   playlist_t *playlist                = NULL;
   bool loadPlaylist                   = true;
   QString corePath                    = QString();
   settings_t *settings                = config_get_ptr();

   playlist_config.capacity            = COLLECTION_SIZE;
   playlist_config.old_format          = settings->bools.playlist_use_old_format;
   playlist_config.compress            = settings->bools.playlist_compression;
   playlist_config.fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;
   playlist_config_set_base_content_directory(&playlist_config,
		   settings->bools.playlist_portable_paths
		 ? settings->paths.directory_menu_content : NULL);

   if (!settings || !plNameCString || !*plNameCString)
      return corePath;

   /* Get playlist path */
   _len = fill_pathname_join_special(
         playlist_path,  settings->paths.directory_playlist,
         plNameCString, sizeof(playlist_path));
   strlcpy(playlist_path       + _len, ".lpl",
         sizeof(playlist_path) - _len);

   /* Load playlist, if required */
   if (cachedPlaylist)
   {
      if (string_is_equal(playlist_path,
               playlist_get_conf_path(cachedPlaylist)))
      {
         playlist     = cachedPlaylist;
         loadPlaylist = false;
      }
   }

   if (loadPlaylist)
   {
      playlist_config_set_path(&playlist_config, playlist_path);
      playlist = playlist_init(&playlist_config);
   }

   if (playlist)
   {
      const char *defaultCorePath = playlist_get_default_core_path(playlist);

      /* Get default core path */
      if (   (defaultCorePath && *defaultCorePath)
          && !string_is_equal(defaultCorePath, "DETECT"))
         corePath = QString::fromUtf8(defaultCorePath);

      /* Free playlist, if required */
      if (loadPlaylist)
         playlist_free(playlist);
   }

   return corePath;
}

void MainWindow::getPlaylistFiles()
{
   settings_t *settings = config_get_ptr();
   QDir playlistDir(settings->paths.directory_playlist);

   m_playlistFiles = playlistDir.entryList(
         QDir::NoDotAndDotDot | QDir::Readable | QDir::Files, QDir::Name);
}

void PlaylistModel::getPlaylistItems(QString path)
{
   QByteArray pathArray;
   playlist_config_t playlist_config;
   const char *pathData                = NULL;
   const char *playlistName            = NULL;
   playlist_t *playlist                = NULL;
   unsigned playlistSize               = 0;
   unsigned            i               = 0;
   settings_t *settings                = config_get_ptr();

   playlist_config.capacity            = COLLECTION_SIZE;
   playlist_config.old_format          = settings->bools.playlist_use_old_format;
   playlist_config.compress            = settings->bools.playlist_compression;
   playlist_config.fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;
   playlist_config_set_base_content_directory(&playlist_config,
		   settings->bools.playlist_portable_paths
		 ? settings->paths.directory_menu_content : NULL);

   pathArray.append(path.toUtf8());
   pathData              = pathArray.constData();
   if (pathData && *pathData)
      playlistName       = path_basename(pathData);

   playlist_config_set_path(&playlist_config, pathData);
   playlist              = playlist_init(&playlist_config);
   playlistSize          = playlist_get_size(playlist);

   for (i = 0; i < playlistSize; i++)
   {
      QHash<QString, QString> hash;
      const struct playlist_entry *entry  = NULL;

      playlist_get_index(playlist, i, &entry);

      if (!entry->path || !*entry->path)
         continue;

      hash["path"]           = entry->path;
      hash["index"]          = QString::number(i);

      if (!entry->label || !*entry->label)
      {
         hash["label"]       = entry->path;
         hash["label_noext"] = entry->path;
      }
      else
      {
         hash["label"]       = entry->label;
         hash["label_noext"] = entry->label;
      }

      if (entry->core_path && *entry->core_path)
         hash["core_path"]   = entry->core_path;

      if (entry->core_name && *entry->core_name)
         hash["core_name"]   = entry->core_name;

      if (entry->crc32 && *entry->crc32)
         hash["crc32"]       = entry->crc32;

      if (entry->db_name && *entry->db_name)
      {
         hash["db_name"]     = entry->db_name;
         hash["db_name"].remove(".lpl");
      }

      if (playlistName && *playlistName)
      {
         hash["pl_name"]     = playlistName;
         hash["pl_name"].remove(".lpl");
      }

      m_contents.append(hash);
   }

   playlist_free(playlist);
   playlist = NULL;
}

void PlaylistModel::addPlaylistItems(const QStringList &paths, bool add)
{
   int i;

   if (paths.isEmpty())
      return;

   beginResetModel();

   m_contents.clear();

   for (i = 0; i < paths.size(); i++)
      getPlaylistItems(paths.at(i));

   endResetModel();
}

void PlaylistModel::addDir(QString path, QFlags<QDir::Filter> showHidden)
{
   int i;
   QDir            dir = path;
   QStringList dirList = dir.entryList(
        QDir::NoDotAndDotDot
      | QDir::Readable
      | QDir::Files
      | showHidden,
      QDir::Name);

   if (dirList.count() == 0)
      return;

   beginResetModel();

   m_contents.clear();

   for (i = 0; i < dirList.count(); i++)
   {
      QHash<QString, QString> hash;
      QString fileName    = dirList.at(i);
      QString filePath(
            QDir::toNativeSeparators(dir.absoluteFilePath(fileName)));
      QFileInfo fileInfo(filePath);

      hash["path"]        = filePath;
      hash["label"]       = hash["path"];
      hash["label_noext"] = fileInfo.completeBaseName();
      hash["db_name"]     = fileInfo.dir().dirName();

      m_contents.append(hash);
   }

   endResetModel();
}

void MainWindow::setAllPlaylistsListMaxCount(int count)
{
   if (count < 1)
      count = 0;
   m_allPlaylistsListMaxCount = count;
}

void MainWindow::setAllPlaylistsGridMaxCount(int count)
{
   if (count < 1)
      count = 0;
   m_allPlaylistsGridMaxCount = count;
}
