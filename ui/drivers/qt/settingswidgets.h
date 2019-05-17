#ifndef SETTINGSWIDGETS_H
#define SETTINGSWIDGETS_H

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QSlider>
#include <QGroupBox>
#include <QToolButton>
#include <QPushButton>
#include <QRadioButton>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPainter>
#include <QColorDialog>

#ifndef CXX_BUILD
extern "C" {
#endif

#include "../../../configuration.h"
#include "../../../setting_list.h"
#include "../../../menu/menu_setting.h"
#include "../../../menu/menu_cbs.h"

#ifndef CXX_BUILD
}
#endif

class FormLayout : public QFormLayout
{
public:
   FormLayout(QWidget *parent = 0);
   void addSizeSpinBox(rarch_setting_t *setting, unsigned scale = 1024 * 1024);
   void addUIntColorButton(const QString &title, msg_hash_enums r, msg_hash_enums g, msg_hash_enums b);
   void addUIntSpinBox(rarch_setting_t *setting);
   void addFloatSpinBox(rarch_setting_t *setting);
   void addCheckBox(rarch_setting_t *setting);
   void addUIntComboBox(rarch_setting_t *setting);
   bool addBindButton(rarch_setting_t *setting);
   void addFileSelector(rarch_setting_t *setting);
   void addDirectorySelector(rarch_setting_t *setting);
   void addFloatSliderAndSpinBox(rarch_setting_t *setting);
   void addUIntRadioButtons(rarch_setting_t *setting);
   void addStringComboBox(rarch_setting_t *setting);
   void addStringLineEdit(rarch_setting_t *setting);
   void addPasswordLineEdit(rarch_setting_t *setting);

   bool add(rarch_setting_t *setting)
   {
      enum ui_setting_type ui_type = ST_UI_TYPE_NONE;

      if (!setting)
         return false;

      ui_type = setting->ui_type;

      switch (ui_type)
      {
         case ST_UI_TYPE_CHECKBOX:
            this->addCheckBox(setting);
            break;
         case ST_UI_TYPE_UINT_COLOR_BUTTON:
            /* TODO/FIXME */
            break;
         case ST_UI_TYPE_UINT_SPINBOX:
            this->addUIntSpinBox(setting);
            break;
         case ST_UI_TYPE_UINT_COMBOBOX:
            this->addUIntComboBox(setting);
            break;
         case ST_UI_TYPE_UINT_RADIO_BUTTONS:
            this->addUIntRadioButtons(setting);
            break;
         case ST_UI_TYPE_FLOAT_COLOR_BUTTON:
            /* TODO/FIXME */
            break;
         case ST_UI_TYPE_FLOAT_SPINBOX:
            this->addFloatSpinBox(setting);
            break;
         case ST_UI_TYPE_FLOAT_SLIDER_AND_SPINBOX:
            this->addFloatSliderAndSpinBox(setting);
            break;
         case ST_UI_TYPE_SIZE_SPINBOX:
            this->addSizeSpinBox(setting);
            break;
         case ST_UI_TYPE_BIND_BUTTON:
            return this->addBindButton(setting);
         case ST_UI_TYPE_DIRECTORY_SELECTOR:
            this->addDirectorySelector(setting);
            break;
         case ST_UI_TYPE_FILE_SELECTOR:
            this->addFileSelector(setting);
            break;
         case ST_UI_TYPE_STRING_COMBOBOX:
            this->addStringComboBox(setting);
            break;
         case ST_UI_TYPE_STRING_LINE_EDIT:
            this->addStringLineEdit(setting);
            break;
         case ST_UI_TYPE_PASSWORD_LINE_EDIT:
            this->addPasswordLineEdit(setting);
            break;
         case ST_UI_TYPE_NONE:
         default:
            break;
      }

      return true;
   }

   bool add(msg_hash_enums enum_idx)
   {
      rarch_setting_t *setting     = menu_setting_find_enum(enum_idx);
      return add(setting);
   }
};

class SettingsGroup : public QGroupBox
{
   Q_OBJECT
public:
   SettingsGroup(const QString &title, QWidget *parent = 0);
   SettingsGroup(QWidget *parent = 0);
   void addWidget(QWidget *widget);
   void addRow(QString label, QWidget *widget);
   void addRow(QWidget *widget);
   void addRow(QLayout *layout);
   void addRow(QString label, QLayout *layout);
   void addUIntColorButton(const QString &title, msg_hash_enums r, msg_hash_enums g, msg_hash_enums b);

   bool add(rarch_setting_t *setting)
   {
      enum ui_setting_type ui_type = ST_UI_TYPE_NONE;

      if (!setting)
         return false;

      ui_type = setting->ui_type;

      switch (ui_type)
      {
         case ST_UI_TYPE_CHECKBOX:
            this->addCheckBox(setting);
            break;
         case ST_UI_TYPE_UINT_COLOR_BUTTON:
            /* TODO/FIXME */
            break;
         case ST_UI_TYPE_UINT_SPINBOX:
            this->addUIntSpinBox(setting);
            break;
         case ST_UI_TYPE_UINT_COMBOBOX:
            this->addUIntComboBox(setting);
            break;
         case ST_UI_TYPE_UINT_RADIO_BUTTONS:
            this->addUIntRadioButtons(setting);
            break;
         case ST_UI_TYPE_FLOAT_COLOR_BUTTON:
            /* TODO/FIXME */
            break;
         case ST_UI_TYPE_FLOAT_SPINBOX:
            this->addFloatSpinBox(setting);
            break;
         case ST_UI_TYPE_FLOAT_SLIDER_AND_SPINBOX:
            this->addFloatSliderAndSpinBox(setting);
            break;
         case ST_UI_TYPE_SIZE_SPINBOX:
            /* TODO/FIXME */
            break;
         case ST_UI_TYPE_BIND_BUTTON:
            /* TODO/FIXME - Why is the returntype void here and bool
             * for Layout? */
            this->addBindButton(setting);
            break;
         case ST_UI_TYPE_DIRECTORY_SELECTOR:
            this->addDirectorySelector(setting);
            break;
         case ST_UI_TYPE_FILE_SELECTOR:
            this->addFileSelector(setting);
            break;
         case ST_UI_TYPE_STRING_COMBOBOX:
            this->addStringComboBox(setting);
            break;
         case ST_UI_TYPE_STRING_LINE_EDIT:
            this->addStringLineEdit(setting);
            break;
         case ST_UI_TYPE_PASSWORD_LINE_EDIT:
            this->addPasswordLineEdit(setting);
            break;
         case ST_UI_TYPE_NONE:
         default:
            break;
      }

      return true;
   }

   bool add(msg_hash_enums enum_idx)
   {
      rarch_setting_t *setting     = menu_setting_find_enum(enum_idx);
      return add(setting);
   }
private:
   void addBindButton(rarch_setting_t *setting);
   void addCheckBox(rarch_setting_t *setting);
   void addUIntSpinBox(rarch_setting_t *setting);
   void addUIntComboBox(rarch_setting_t *setting);
   void addFloatSpinBox(rarch_setting_t *setting);
   void addFileSelector(rarch_setting_t *setting);
   void addDirectorySelector(rarch_setting_t *setting);
   void addFloatSliderAndSpinBox(rarch_setting_t *setting);
   void addUIntRadioButtons(rarch_setting_t *setting);
   void addStringComboBox(rarch_setting_t *setting);
   void addStringLineEdit(rarch_setting_t *setting);
   void addPasswordLineEdit(rarch_setting_t *setting);
   FormLayout *m_layout;
};

class CheckableSettingsGroup : public SettingsGroup
{
   Q_OBJECT
public:
   CheckableSettingsGroup(rarch_setting_t *setting, QWidget *parent = 0);
   CheckableSettingsGroup(const char *setting, QWidget *parent = 0);
   CheckableSettingsGroup(msg_hash_enums enum_idx, QWidget *parent = 0);
private slots:
   void onClicked(bool clicked);
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   bool *m_value;
};

class CheckBox : public QCheckBox
{
   Q_OBJECT
public:
   CheckBox(rarch_setting_t *setting, QWidget *parent = 0);
   CheckBox(const char *setting, QWidget *parent = 0);
   CheckBox(msg_hash_enums enum_idx, QWidget *parent = 0);
private slots:
   void onClicked(bool checked);
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   bool *m_value;
};

class CheckableIcon : public QToolButton
{
   Q_OBJECT
public:
   CheckableIcon(rarch_setting_t *setting, const QIcon &icon, QWidget *parent = 0);
   CheckableIcon(const char *setting, const QIcon &icon, QWidget *parent = 0);
   CheckableIcon(msg_hash_enums enum_idx, const QIcon &icon, QWidget *parent = 0);
private slots:
   void onToggled(bool checked);
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   bool *m_value;
};

class StringLineEdit : public QLineEdit
{
   Q_OBJECT
public:
   StringLineEdit(rarch_setting_t *setting, QWidget *parent = 0);
   StringLineEdit(const char *setting, QWidget *parent = 0);
private slots:
   void onEditingFinished();
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   char *m_value;
};

class PasswordLineEdit : public StringLineEdit
{
   Q_OBJECT
public:
   PasswordLineEdit(rarch_setting_t *setting, QWidget *parent = 0);
};

class StringComboBox : public QComboBox
{
   Q_OBJECT
public:
   StringComboBox(rarch_setting_t *setting, QWidget *parent = 0);
   StringComboBox(const char *setting, QWidget *parent = 0);
private slots:
   void onCurrentTextChanged(const QString &text);
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   char *m_value;
};

class UIntComboBox : public QComboBox
{
   Q_OBJECT
public:
   UIntComboBox(rarch_setting_t *setting, QWidget *parent = 0);
   UIntComboBox(rarch_setting_t *setting, double min, double max, QWidget *parent = 0);
   UIntComboBox(msg_hash_enums enum_idx, QWidget *parent = 0);
   UIntComboBox(msg_hash_enums enum_idx, double min, double max, QWidget *parent = 0);
   UIntComboBox(const char *setting, QWidget *parent = 0);
private slots:
   void onCurrentIndexChanged(int index);
   void paintEvent(QPaintEvent *event);
private:
   void populate(double min, double max);
   rarch_setting_t *m_setting;
   unsigned *m_value;
   QHash<unsigned, QString> m_hash;
};

class UIntRadioButton : public QRadioButton
{
   Q_OBJECT
public:
   UIntRadioButton(const QString &text, rarch_setting_t *setting, unsigned value, QWidget *parent = 0);
   UIntRadioButton(msg_hash_enums enum_idx, unsigned value, QWidget *parent = 0);
private slots:
   void onClicked(bool);
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   unsigned *m_target;
   unsigned m_value;
};

class UIntRadioButtons : public QGroupBox
{
   Q_OBJECT
public:
   UIntRadioButtons(rarch_setting_t *setting, QWidget *parent = 0);
   UIntRadioButtons(const char *setting, QWidget *parent = 0);
   UIntRadioButtons(msg_hash_enums enum_idx, QWidget *parent = 0);
signals:
   void currentUIntChanged(unsigned value);
private slots:
   void onButtonClicked(int id);
private:
   rarch_setting_t *m_setting;
   unsigned *m_value;
   QButtonGroup *m_buttonGroup;
};

class UIntSpinBox : public QSpinBox
{
   Q_OBJECT
public:
   UIntSpinBox(rarch_setting_t *setting, QWidget *parent = 0);
   UIntSpinBox(msg_hash_enums enum_idx, QWidget *parent = 0);
private slots:
   void onValueChanged(int value);
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   unsigned *m_value;
};

class SizeSpinBox : public QSpinBox
{
   Q_OBJECT
public:
   SizeSpinBox(rarch_setting_t *setting, unsigned scale = 1024*1024, QWidget *parent = 0);
   SizeSpinBox(msg_hash_enums enum_idx, unsigned scale = 1024 * 1024, QWidget *parent = 0);
private slots:
   void onValueChanged(int value);
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   size_t *m_value;
   unsigned m_scale;
};

class IntSpinBox : public QSpinBox
{
   Q_OBJECT
public:
   IntSpinBox(rarch_setting_t *setting, QWidget *parent = 0);
private slots:
   void onValueChanged(int value);
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   int *m_value;
};

class FloatSpinBox : public QDoubleSpinBox
{
   Q_OBJECT
public:
   FloatSpinBox(rarch_setting_t *setting, QWidget *parent = 0);
   FloatSpinBox(const char *setting, QWidget *parent = 0);
   FloatSpinBox(msg_hash_enums enum_idx, QWidget *parent = 0);
private slots:
   void onValueChanged(double value);
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   float *m_value;
   static const QRegularExpression DECIMALS_REGEX;
};

class PathButton : public QPushButton
{
   Q_OBJECT
public:
   PathButton(rarch_setting_t *setting, QWidget *parent = 0);
   PathButton(const char *setting, QWidget *parent = 0);
signals:
   void changed();
protected slots:
   virtual void onClicked(bool checked = false) { Q_UNUSED( checked); }
protected:
   QString m_filter;
   rarch_setting_t *m_setting;
   char *m_value;
   const char *m_dir;
   QString currentPath();
};

class DirectoryButton : public PathButton
{
   Q_OBJECT
public:
   DirectoryButton(rarch_setting_t *setting, QWidget *parent = 0) :
      PathButton(setting, parent) {}
private:
   void onClicked(bool checked = false);
};

class FileButton : public PathButton
{
   Q_OBJECT
public:
   FileButton(rarch_setting_t *setting, QWidget *parent = 0) :
      PathButton(setting, parent) {}
private:
   void onClicked(bool checked = false);
};

class DirectorySelector : public QHBoxLayout
{
   Q_OBJECT
public:
   DirectorySelector(rarch_setting_t *setting, QWidget *parent = 0);
private:
   StringLineEdit *m_lineEdit;
   DirectoryButton *m_button;
};

class FileSelector : public QHBoxLayout
{
   Q_OBJECT
public:
   FileSelector(rarch_setting_t *setting, QWidget *parent = 0);
   FileSelector(const char *setting, QWidget *parent = 0);
private:
   StringLineEdit *m_lineEdit;
   FileButton *m_button;
};

class FloatSlider : public QSlider
{
   Q_OBJECT
public:
   FloatSlider(rarch_setting_t *setting, QWidget *parent = 0);
   FloatSlider(const char *setting, QWidget *parent = 0);
private slots:
   void onValueChanged(int value);
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   float *m_value;
   QRegularExpression m_decimalsRegEx;
   unsigned int m_precision;
};

class FloatSliderAndSpinBox : public QHBoxLayout
{
   Q_OBJECT
public:
   FloatSliderAndSpinBox(rarch_setting_t *setting, QWidget *parent = 0);
   FloatSliderAndSpinBox(const char *setting, QWidget *parent = 0);
   FloatSliderAndSpinBox(msg_hash_enums enum_idx, QWidget *parent = 0);
private slots:
   void onSliderValueChanged(int value);
   void onSpinBoxValueChanged(double value);
private:
   FloatSlider *m_slider;
   FloatSpinBox *m_spinBox;
};

class BindButton : public QPushButton
{
   Q_OBJECT
public:
   BindButton(rarch_setting_t *setting, QWidget *parent = 0);
   BindButton(msg_hash_enums enum_idx, QWidget *parent = 0);
private slots:
   void onClicked(bool checked);
private:
   rarch_setting_t *m_setting;
};

class ColorButton : public QToolButton
{
   Q_OBJECT
public:
   ColorButton(rarch_setting_t *red, rarch_setting_t *green, rarch_setting_t *blue, QWidget *parent = 0);
   ColorButton(msg_hash_enums red, msg_hash_enums green, msg_hash_enums blue, QWidget *parent = 0);
protected slots:
   virtual void onColorChanged(const QColor& color) { Q_UNUSED(color); }
protected:
   virtual QColor color() { return QColor(); }
   void paintEvent(QPaintEvent *event);

   rarch_setting_t *m_red, *m_green, *m_blue;
   QColorDialog *m_dialog;
};

class UIntColorButton : public ColorButton
{
   Q_OBJECT
public:
   UIntColorButton(msg_hash_enums red, msg_hash_enums green, msg_hash_enums blue, QWidget *parent = 0);
   UIntColorButton(rarch_setting_t *red, rarch_setting_t *green, rarch_setting_t *blue, QWidget *parent = 0);
protected slots:
   void onColorChanged(const QColor& color);
protected:
   QColor color();
};

class FloatColorButton : public ColorButton
{
   Q_OBJECT
public:
   explicit FloatColorButton(msg_hash_enums red, msg_hash_enums green, msg_hash_enums blue, QWidget *parent = 0);
protected slots:
   void onColorChanged(const QColor& color);
protected:
   QColor color();
};

#endif
