#ifndef SHADERPARAMSDIALOG_H
#define SHADERPARAMSDIALOG_H

#include <QDialog>

class QCloseEvent;
class QResizeEvent;

class ShaderParamsDialog : public QDialog
{
   Q_OBJECT
public:
   ShaderParamsDialog(QWidget *parent = 0);
   ~ShaderParamsDialog();
signals:
   void closed();
   void resized(QSize size);
protected:
   void closeEvent(QCloseEvent *event);
   void resizeEvent(QResizeEvent *event);
};

#endif
