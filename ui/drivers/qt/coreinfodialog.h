#ifndef COREINFODIALOG_H
#define COREINFODIALOG_H

#include <QDialog>

class QFormLayout;
class MainWindow;

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

#endif
