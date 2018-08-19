#ifndef SHADERPARAMSDIALOG_H
#define SHADERPARAMSDIALOG_H

#include <QDialog>

class QCloseEvent;
class QResizeEvent;
class QVBoxLayout;
class QFormLayout;
class QLayout;

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
   void onScaleComboBoxIndexChanged(int index);
   void onShaderPassMoveDownClicked();
   void onShaderPassMoveUpClicked();
private:
   QString getFilterLabel(unsigned filter);
   void addShaderParam(struct video_shader_parameter *param, int parameter, QFormLayout *form);
   void clearLayout(QLayout *layout);
   void getShaders(struct video_shader **menu_shader, struct video_shader **video_shader);

   QVBoxLayout *m_layout;
protected:
   void closeEvent(QCloseEvent *event);
   void resizeEvent(QResizeEvent *event);
};

#endif
