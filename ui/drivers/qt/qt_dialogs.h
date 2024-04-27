#ifndef COREINFODIALOG_H
#define COREINFODIALOG_H

#include <QDialog>
#include <QPointer>

#include <retro_common_api.h>

#ifndef CXX_BUILD
extern "C" {
#endif

#include "../../../gfx/video_shader_parse.h"

#ifndef CXX_BUILD
}
#endif

class QFormLayout;
class QCloseEvent;
class QResizeEvent;
class QVBoxLayout;
class QLayout;
class QScrollArea;
class MainWindow;
class QSettings;
class QLineEdit;
class QCheckBox;
class QComboBox;
class QPushButton;
class QColor;
class QLabel;
class QSpinBox;

#ifdef HAVE_MENU
class QWidget;
class OptionsCategory;
class QListWidget;
class QStackedLayout;
#endif

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

#endif
