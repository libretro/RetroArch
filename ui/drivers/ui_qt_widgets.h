#ifndef UI_QT_WIDGETS_H
#define UI_QT_WIDGETS_H

#include <QAbstractItemView>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QToolButton>

#include <retro_common_api.h>

#ifndef CXX_BUILD
extern "C" {
#endif

#include "../../setting_list.h"
#include "../../menu/menu_setting.h"
#include "../../menu/menu_cbs.h"
#include "../../gfx/video_shader_parse.h"
#include "../../configuration.h"

#ifndef CXX_BUILD
}
#endif

class MainWindow;
class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QKeyEvent;
class QLabel;
class QLayout;
class QPaintEvent;
class QResizeEvent;
class QSettings;
class QVBoxLayout;

#ifdef HAVE_MENU
class QWidget;
class OptionsCategory;
class QListWidget;
class QStackedLayout;
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
private:
   StringLineEdit *m_lineEdit;
   FileButton *m_button;
};

class FloatSlider : public QSlider
{
   Q_OBJECT
public:
   FloatSlider(rarch_setting_t *setting, QWidget *parent = 0);
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
   virtual void onColorChanged(const QColor& color) { (void)(color); }
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

class FileDropWidget : public QStackedWidget
{
   Q_OBJECT
public:
   FileDropWidget(QWidget *parent = 0);
signals:
   void filesDropped(QStringList files);
   void enterPressed();
   void deletePressed();
protected:
   void dragEnterEvent(QDragEnterEvent *event);
   void dragMoveEvent(QDragMoveEvent *event);
   void dropEvent(QDropEvent *event);
   void keyPressEvent(QKeyEvent *event);
   void paintEvent(QPaintEvent *event);
};

#define DEFAULT_GRID_ITEM_MARGIN 11
#define DEFAULT_GRID_ITEM_THUMBNAIL_ALIGNMENT "bottom"
#define DEFAULT_GRID_SPACING 7
#define DEFAULT_GRID_LAYOUT "centered"

class GridItem;

class ThumbnailDelegate : public QStyledItemDelegate
{
   Q_OBJECT

public:
   ThumbnailDelegate(const GridItem &gridItem, QObject* parent = 0);
   void paint(QPainter* painter, const QStyleOptionViewItem &option, const QModelIndex& index) const;

private:
   const GridItem &m_style;
};

class GridView : public QAbstractItemView
{
   Q_OBJECT

   Q_PROPERTY(QString layout READ getLayout WRITE setLayout DESIGNABLE true SCRIPTABLE true)
   Q_PROPERTY(int spacing READ getSpacing WRITE setSpacing DESIGNABLE true SCRIPTABLE true)

public:
   enum ViewMode
   {
      Simple,
      Centered,
      Anchored
   };

   GridView(QWidget *parent = 0);
   ~GridView() {}

   QModelIndex indexAt(const QPoint &point_) const;
   QVector<QModelIndex> visibleIndexes() const;
   QRect visualRect(const QModelIndex &index) const;
   void setModel(QAbstractItemModel *model);
   void scrollTo(const QModelIndex &index, QAbstractItemView::ScrollHint);
   void setGridSize(const int newSize);
   void setviewMode(ViewMode mode);
   QString getLayout() const;
   void setLayout(QString layout);
   int getSpacing() const;
   void setSpacing(const int spacing);

signals:
   void visibleItemsChangedMaybe() const;

protected slots:
   void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles = QVector<int>());
   void rowsInserted(const QModelIndex &parent, int start, int end);
   void rowsRemoved(const QModelIndex &parent, int start, int end);
   void updateGeometries();
   void reset();

protected:
   QModelIndex moveCursor(QAbstractItemView::CursorAction cursorAction, Qt::KeyboardModifiers modifiers);
   QRegion visualRegionForSelection(const QItemSelection &selection) const;
   bool isIndexHidden(const QModelIndex&) const { return false; }
   int horizontalOffset() const;
   int verticalOffset() const;
   void scrollContentsBy(int dx, int dy);
   void setSelection(const QRect &rect, QFlags<QItemSelectionModel::SelectionFlag> flags);
   void paintEvent(QPaintEvent*);
   void resizeEvent(QResizeEvent*);

private:
   QRectF viewportRectForRow(int row) const;
   void calculateRectsIfNecessary() const;
   void refresh();

   int m_size = 255;
   int m_spacing = DEFAULT_GRID_SPACING;
   QVector<QModelIndex> m_visibleIndexes;
   ViewMode m_viewMode = Centered;
   mutable int m_idealHeight;
   mutable QHash<int, QRectF> m_rectForRow;
   mutable bool m_hashIsDirty;
};

class CoreInfoDialog : public QDialog
{
   Q_OBJECT
public:
   CoreInfoDialog(MainWindow *mainwindow, QWidget *parent = 0);
public slots:
   void showCoreInfo();
private:
   QFormLayout *m_formLayout;
   MainWindow *m_mainwindow;
};

class CoreOptionsDialog : public QDialog
{
   Q_OBJECT
public:
   CoreOptionsDialog(QWidget *parent = 0);
   ~CoreOptionsDialog();
signals:
   void closed();
   void resized(QSize size);
public slots:
   void reload();
private slots:
   void clearLayout();
   void buildLayout();
   void onSaveGameSpecificOptions();
   void onSaveFolderSpecificOptions();
   void onCoreOptionComboBoxCurrentIndexChanged(int index);
   void onCoreOptionResetClicked();
   void onCoreOptionResetAllClicked();
private:
   QPointer<QVBoxLayout> m_layout;
   QPointer<QScrollArea> m_scrollArea;
protected:
   void closeEvent(QCloseEvent *event);
   void resizeEvent(QResizeEvent *event);
   void paintEvent(QPaintEvent *event);
};

class ShaderPass
{
public:
   ShaderPass(struct video_shader_pass *passToCopy = NULL);
   ~ShaderPass();
   ShaderPass& operator=(const ShaderPass &other);
   struct video_shader_pass *pass;
};

class ShaderParamsDialog : public QDialog
{
   Q_OBJECT
public:
   ShaderParamsDialog(QWidget *parent = 0);
   ~ShaderParamsDialog();
signals:
   void closed();
   void resized(QSize size);
public slots:
   void reload();
private slots:
   void onShaderParamCheckBoxClicked();
   void onShaderParamSliderValueChanged(int value);
   void onShaderParamSpinBoxValueChanged(int value);
   void onShaderParamDoubleSpinBoxValueChanged(double value);
   void onFilterComboBoxIndexChanged(int index);
   void onGroupBoxContextMenuRequested(const QPoint &pos);
   void onParameterLabelContextMenuRequested(const QPoint &pos);
   void onScaleComboBoxIndexChanged(int index);
   void onShaderPassMoveDownClicked();
   void onShaderPassMoveUpClicked();
   void onShaderResetPass(int pass);
   void onShaderResetAllPasses();
   void onShaderRemovePass(int pass);
   void onShaderRemoveAllPassesClicked();
   void onShaderRemovePassClicked();
   void onShaderResetParameter(QString parameter);
   void onShaderLoadPresetClicked();
   void onShaderAddPassClicked();
   void onShaderSavePresetAsClicked();
   void onShaderSaveCorePresetClicked();
   void onShaderSaveParentPresetClicked();
   void onShaderSaveGamePresetClicked();
   void onShaderSaveGlobalPresetClicked();
   void onShaderRemoveCorePresetClicked();
   void onShaderRemoveParentPresetClicked();
   void onShaderRemoveGamePresetClicked();
   void onShaderRemoveGlobalPresetClicked();
   void onShaderApplyClicked();
   void updateRemovePresetButtonsState();
   void clearLayout();
   void buildLayout();
private:
   QString getFilterLabel(unsigned filter);
   void addShaderParam(struct video_shader_parameter *param, QFormLayout *form);
   void getShaders(struct video_shader **menu_shader, struct video_shader **video_shader);
   void operateShaderPreset(bool save, const char *path, unsigned action_type);

   QPointer<QVBoxLayout> m_layout;
   QPointer<QScrollArea> m_scrollArea;
   QAction *removeGlobalPresetAction;
   QAction *removeCorePresetAction;
   QAction *removeParentPresetAction;
   QAction *removeGamePresetAction;
protected:
   void closeEvent(QCloseEvent *event);
   void resizeEvent(QResizeEvent *event);
   void paintEvent(QPaintEvent *event);
};

class ViewOptionsWidget : public QWidget
{
   Q_OBJECT
public:
   ViewOptionsWidget(MainWindow *mainwindow, QWidget *parent = 0);
public slots:
   void onAccepted();
   void onRejected();
   void loadViewOptions();
   void saveViewOptions();
private slots:
   void onThemeComboBoxIndexChanged(int index);
   void onHighlightColorChoose();
private:
   void showOrHideHighlightColor();

   MainWindow *m_mainwindow;
   QSettings *m_settings;
   QCheckBox *m_saveGeometryCheckBox;
   QCheckBox *m_saveDockPositionsCheckBox;
   QCheckBox *m_saveLastTabCheckBox;
   QCheckBox *m_showHiddenFilesCheckBox;
   QComboBox *m_themeComboBox;
   QSpinBox *m_thumbnailCacheSpinBox;
   QSpinBox *m_thumbnailDropSizeSpinBox;
   QComboBox *m_startupPlaylistComboBox;
   QPushButton *m_highlightColorPushButton;
   QColor m_highlightColor;
   QLabel *m_highlightColorLabel;
   QString m_customThemePath;
   QCheckBox *m_suggestLoadedCoreFirstCheckBox;
   QSpinBox *m_allPlaylistsListMaxCountSpinBox;
   QSpinBox *m_allPlaylistsGridMaxCountSpinBox;
};

class ViewOptionsDialog : public QDialog
{
   Q_OBJECT
public:
   ViewOptionsDialog(MainWindow *window, QWidget *parent = 0);
#ifdef HAVE_MENU
   void repaintIcons();
#endif
public slots:
   void showDialog();
   void hideDialog();
private slots:
   void onRejected();
private:
#ifdef HAVE_MENU
   void addCategory(QWidget *widget, QString name, QString icon);
   void addCategory(OptionsCategory *category);
   QVector<OptionsCategory*> m_categoryList;
   QListWidget *m_optionsList;
   QStackedLayout *m_optionsStack;
#else
   ViewOptionsWidget *m_viewOptionsWidget;
#endif
};

class PlaylistEntryDialog : public QDialog
{
   Q_OBJECT
public:
   PlaylistEntryDialog(MainWindow *mainwindow, QWidget *parent = 0);
   const QHash<QString, QString> getSelectedCore();
   const QString getSelectedDatabase();
   const QString getSelectedName();
   const QString getSelectedPath();
   const QStringList getSelectedExtensions();
   bool filterInArchive();
   bool nameFieldEnabled();
   void setEntryValues(const QHash<QString, QString> &contentHash);
public slots:
   bool showDialog(const QHash<QString, QString> &hash = QHash<QString, QString>());
   void hideDialog();
   void onAccepted();
   void onRejected();
private slots:
   void onPathClicked();
private:
   void loadPlaylistOptions();

   MainWindow *m_mainwindow;
   QSettings *m_settings;
   QLineEdit *m_nameLineEdit;
   QLineEdit *m_pathLineEdit;
   QLineEdit *m_extensionsLineEdit;
   QComboBox *m_coreComboBox;
   QComboBox *m_databaseComboBox;
   QCheckBox *m_extensionArchiveCheckBox;
};

class OptionsPage : public QObject
{
   Q_OBJECT

public:
   OptionsPage(QObject *parent = nullptr)
      : QObject(parent)
   {
   }

   QString displayName() const
   {
      return m_displayName;
   }

   virtual QWidget *widget() = 0;
   virtual void load() {}
   virtual void apply() {}

protected:
   void setDisplayName(msg_hash_enums name)
   {
      m_displayName = msg_hash_to_str(name);
   }

   void setDisplayName(const QString& name)
   {
      m_displayName = name;
   }

   QString m_displayName = "General";
};

class OptionsCategory : public QObject
{
   Q_OBJECT
public:
   OptionsCategory(QObject *parent = nullptr) : QObject(parent) {}
   OptionsCategory(MainWindow *mainwindow, QObject *parent = nullptr) : QObject(parent) {}
   virtual QVector<OptionsPage*> pages() = 0;
   QString displayName() const { return m_displayName; }
   QString categoryIconName() const { return m_categoryIconName; }
   virtual void load()
   {
      size_t i;
      size_t _len = m_pages.size();
      for (i = 0; i < _len; i++)
         m_pages.at(i)->load();
   }
   virtual void apply()
   {
      size_t i;
      size_t _len = m_pages.size();
      for (i = 0; i < _len; i++)
         m_pages.at(i)->apply();
   }
protected:
   void setDisplayName(msg_hash_enums name) { m_displayName = msg_hash_to_str(name); }
   void setCategoryIcon(const QString &categoryIconName) { m_categoryIconName = categoryIconName; }
   QString m_displayName;
   QString m_categoryIconName = "setting";
   QVector<OptionsPage*> m_pages;
};

/***********************************************************
   Drivers
************************************************************/
class DriversCategory : public OptionsCategory
{
public:
   DriversCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class DriversPage : public OptionsPage
{
   Q_OBJECT
public:
   DriversPage(QObject *parent = nullptr);
   QWidget *widget();
};

/***********************************************************
   AI Service
************************************************************/
class AIServiceCategory : public OptionsCategory
{
public:
   AIServiceCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class AIServicePage : public OptionsPage
{
   Q_OBJECT
public:
   AIServicePage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Video
************************************************************/
class VideoCategory : public OptionsCategory
{
public:
   VideoCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class AspectRatioRadioButton : public QRadioButton
{
   Q_OBJECT
public:
   AspectRatioRadioButton(unsigned min, unsigned max, QWidget *parent = 0);
private:
   unsigned m_min;
   unsigned m_max;
};

class AspectRatioGroup : public SettingsGroup
{
   Q_OBJECT
public:
   AspectRatioGroup(const QString &title, QWidget *parent = 0);
private slots:
   void paintEvent(QPaintEvent *event);
   void onAspectRadioToggled(bool checked);
   void onAspectRadioClicked(bool checked);
private:
   AspectRatioRadioButton *m_radioButton;
   UIntComboBox *m_comboBox;
};

class VideoPage : public OptionsPage
{
   Q_OBJECT
public:
   VideoPage(QObject *parent = nullptr);
   QWidget *widget();
private slots:
   void onResolutionComboIndexChanged(const QString& value);
   void onResolutionComboIndexChanged(int index);
private:
   QComboBox *m_resolutionCombo;
};

class CrtSwitchresPage : public OptionsPage
{
   Q_OBJECT
public:
   CrtSwitchresPage(QObject *parent = nullptr);
   QWidget *widget();
private slots:
   void onCrtSuperResolutionComboIndexChanged(int index);
private:
   QComboBox *m_crtSuperResolutionCombo;
};

/************************************************************
   Audio
************************************************************/
class AudioCategory : public OptionsCategory
{
public:
   AudioCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class AudioPage : public OptionsPage
{
   Q_OBJECT
public:
   AudioPage(QObject *parent = nullptr);
   QWidget *widget();
};

class MenuSoundsPage : public OptionsPage
{
   Q_OBJECT
public:
   MenuSoundsPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Input
************************************************************/
class InputCategory : public OptionsCategory
{
public:
   InputCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class InputPage : public OptionsPage
{
   Q_OBJECT
public:
   InputPage(QObject *parent = nullptr);
   QWidget *widget();
};

class HotkeyBindsPage : public OptionsPage
{
   Q_OBJECT
public:
   HotkeyBindsPage(QObject *parent = nullptr);
   QWidget *widget();
};

class UserBindsPage : public OptionsPage
{
   Q_OBJECT
public:
   UserBindsPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Latency
************************************************************/
class LatencyCategory : public OptionsCategory
{
public:
   LatencyCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class LatencyPage : public OptionsPage
{
   Q_OBJECT
public:
   LatencyPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Core
************************************************************/
class CoreCategory : public OptionsCategory
{
public:
   CoreCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class CorePage : public OptionsPage
{
   Q_OBJECT
public:
   CorePage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Configuration
************************************************************/
class ConfigurationCategory : public OptionsCategory
{
public:
   ConfigurationCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class ConfigurationPage : public OptionsPage
{
   Q_OBJECT
public:
   ConfigurationPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Saving
************************************************************/
class SavingCategory : public OptionsCategory
{
public:
   SavingCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class SavingPage : public OptionsPage
{
   Q_OBJECT
public:
   SavingPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Logging
************************************************************/
class LoggingCategory : public OptionsCategory
{
public:
   LoggingCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class LoggingPage : public OptionsPage
{
   Q_OBJECT
public:
   LoggingPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Frame Throttle
************************************************************/
class FrameThrottleCategory : public OptionsCategory
{
public:
   FrameThrottleCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class FrameThrottlePage : public OptionsPage
{
   Q_OBJECT
public:
   FrameThrottlePage(QObject *parent = nullptr);
   QWidget *widget();
};

class RewindPage : public OptionsPage
{
   Q_OBJECT
public:
   RewindPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Recording
************************************************************/
class RecordingCategory : public OptionsCategory
{
public:
   RecordingCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class RecordingPage : public OptionsPage
{
   Q_OBJECT
public:
   RecordingPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   User Interface
************************************************************/
class UserInterfaceCategory : public OptionsCategory
{
public:
   UserInterfaceCategory(QWidget *parent);
   UserInterfaceCategory(MainWindow *mainwindow, QWidget *parent);
   QVector<OptionsPage*> pages();
private:
   MainWindow *m_mainwindow;
};

class UserInterfacePage : public OptionsPage
{
   Q_OBJECT
public:
   UserInterfacePage(QObject *parent = nullptr);
   QWidget *widget();
};

class ViewsPage : public OptionsPage
{
   Q_OBJECT
public:
   ViewsPage(QObject *parent = nullptr);
   QWidget *widget();
};

class QuickMenuPage : public OptionsPage
{
   Q_OBJECT
public:
   QuickMenuPage(QObject *parent = nullptr);
   QWidget *widget();
};

class AppearancePage : public OptionsPage
{
   Q_OBJECT
public:
   AppearancePage(QObject *parent = nullptr);
   QWidget *widget();
};

class DesktopMenuPage : public OptionsPage
{
   Q_OBJECT
public:
   DesktopMenuPage(MainWindow *mainwindow, QObject *parent = nullptr);
   QWidget *widget();
   void load();
   void apply();
private:
   ViewOptionsWidget *m_widget;
};

/************************************************************
   Onscreen Display
************************************************************/
class OnscreenDisplayCategory : public OptionsCategory
{
public:
   OnscreenDisplayCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class OverlayPage : public OptionsPage
{
   Q_OBJECT
public:
   OverlayPage(QObject *parent = nullptr);
   QWidget *widget();
};

class NotificationsPage : public OptionsPage
{
   Q_OBJECT
public:
   NotificationsPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Achievements
************************************************************/
class AchievementsCategory : public OptionsCategory
{
public:
   AchievementsCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class AchievementsPage : public OptionsPage
{
   Q_OBJECT
public:
   AchievementsPage(QObject *parent = nullptr);
   QWidget *widget();
private slots:
   void onAchievementEnabledChanged(int);
private:
   SettingsGroup* m_generalGroup;
   SettingsGroup* m_appearanceGroup;
   SettingsGroup* m_visibilityGroup;
};

/************************************************************
   Network
************************************************************/
class NetworkCategory : public OptionsCategory
{
public:
   NetworkCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class NetplayPage : public OptionsPage
{
   Q_OBJECT
public:
   NetplayPage(QObject *parent = nullptr);
   QWidget *widget();
private slots:
   void onRadioButtonClicked(int);
private:
   QGroupBox* createMitmServerGroup();
};

class UpdaterPage : public OptionsPage
{
   Q_OBJECT
public:
   UpdaterPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Playlists
************************************************************/
class PlaylistsCategory : public OptionsCategory
{
public:
   PlaylistsCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class PlaylistsPage : public OptionsPage
{
   Q_OBJECT
public:
   PlaylistsPage(QObject *parent = nullptr);
   QWidget *widget();
};

class AccountsPage : public OptionsPage
{
   Q_OBJECT
public:
   AccountsPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   User
************************************************************/
class UserCategory : public OptionsCategory
{
public:
   UserCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class UserPage : public OptionsPage
{
   Q_OBJECT
public:
   UserPage(QObject *parent = nullptr);
   QWidget *widget();
};

/************************************************************
   Directory
************************************************************/
class DirectoryCategory : public OptionsCategory
{
public:
   DirectoryCategory(QWidget *parent);
   QVector<OptionsPage*> pages();
};

class DirectoryPage : public OptionsPage
{
   Q_OBJECT
public:
   DirectoryPage(QObject *parent = nullptr);
   QWidget *widget();
};

static inline QWidget *create_widget(enum menu_displaylist_ctl_state name)
{
   unsigned i;
   QWidget             *widget = new QWidget;
   FormLayout          *layout = new FormLayout;
   file_list_t           *list = (file_list_t*)calloc(1, sizeof(*list));
   settings_t *settings        = config_get_ptr();

   menu_displaylist_build_list(list, settings, name, true);

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

#endif
