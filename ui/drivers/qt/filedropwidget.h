#ifndef FILEDROPWIDGET_H
#define FILEDROPWIDGET_H

#include <QWidget>

class QDragEnterEvent;
class QDropEvent;
class QKeyEvent;
class QPaintEvent;

class FileDropWidget : public QWidget
{
   Q_OBJECT
public:
   FileDropWidget(QWidget *parent = 0);
signals:
   void filesDropped(QStringList files);
   void deletePressed();
protected:
   void dragEnterEvent(QDragEnterEvent *event);
   void dropEvent(QDropEvent *event);
   void keyPressEvent(QKeyEvent *event);
   void paintEvent(QPaintEvent *event);
};

#endif
