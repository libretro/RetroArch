#include <QCloseEvent>
#include <QResizeEvent>
#include "shaderparamsdialog.h"

ShaderParamsDialog::ShaderParamsDialog(QWidget *parent) :
   QDialog(parent)
{
}

ShaderParamsDialog::~ShaderParamsDialog()
{
}

void ShaderParamsDialog::resizeEvent(QResizeEvent *event)
{
   QDialog::resizeEvent(event);

   emit resized(event->size());
}

void ShaderParamsDialog::closeEvent(QCloseEvent *event)
{
   QDialog::closeEvent(event);

   emit closed();
}

