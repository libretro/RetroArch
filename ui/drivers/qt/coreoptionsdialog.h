#ifndef COREOPTIONSDIALOG_H
#define COREOPTIONSDIALOG_H

#include <QDialog>
#include <QPointer>

class QCloseEvent;
class QResizeEvent;
class QVBoxLayout;
class QFormLayout;
class QLayout;
class QScrollArea;

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

#endif
