#ifndef SHADERPARAMSDIALOG_H
#define SHADERPARAMSDIALOG_H

#include <QDialog>
#include <QPointer>

#ifndef CXX_BUILD
extern "C" {
#endif

#include "../.././gfx/video_shader_parse.h"

#ifndef CXX_BUILD
}
#endif

class QCloseEvent;
class QResizeEvent;
class QVBoxLayout;
class QFormLayout;
class QLayout;
class QScrollArea;

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

#endif
