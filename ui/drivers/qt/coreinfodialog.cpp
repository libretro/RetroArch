#include <QMainWindow>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include "coreinfodialog.h"
#include "../ui_qt.h"

#ifndef CXX_BUILD
extern "C" {
#endif

#include "../../../msg_hash.h"

#ifndef CXX_BUILD
}
#endif

CoreInfoDialog::CoreInfoDialog(MainWindow *mainwindow, QWidget *parent) :
   QDialog(parent)
   ,m_formLayout(new QFormLayout())
   ,m_mainwindow(mainwindow)
{
   QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);

   connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
   connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

   setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFORMATION));

   m_formLayout->setFormAlignment(Qt::AlignCenter);
   m_formLayout->setLabelAlignment(Qt::AlignCenter);

   setLayout(new QVBoxLayout());

   qobject_cast<QVBoxLayout*>(layout())->addLayout(m_formLayout);
   layout()->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));
   layout()->addWidget(buttonBox);
}

void CoreInfoDialog::showCoreInfo()
{
   int row = 0;
   int rowCount = m_formLayout->rowCount();
   int i = 0;
   QVector<QHash<QString, QString> > infoList = m_mainwindow->getCoreInfo();

   if (rowCount > 0)
   {
      for (row = 0; row < rowCount; row++)
      {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
         /* removeRow() and takeRow() was only added in 5.8! */
         m_formLayout->removeRow(0);
#else
         /* something is buggy here... sometimes items appear duplicated, and other times not */
         QLayoutItem *item = m_formLayout->itemAt(0);
         QWidget *w = NULL;

         if (item)
         {
            w = item->widget();

            if (w)
            {
               QWidget *label = m_formLayout->labelForField(w);

               if (label)
                  delete label;

               m_formLayout->removeWidget(w);

               delete w;
            }
         }
#endif
      }
   }

   if (infoList.count() == 0)
      return;

   for (i = 0; i < infoList.count(); i++)
   {
      const QHash<QString, QString> &line = infoList.at(i);
      QLabel                       *label = new QLabel(line.value("key"));
      CoreInfoLabel                *value = new CoreInfoLabel(line.value("value"));
      QString                  labelStyle = line.value("label_style");
      QString                  valueStyle = line.value("value_style");

      if (!labelStyle.isEmpty())
         label->setStyleSheet(labelStyle);

      if (!valueStyle.isEmpty())
         value->setStyleSheet(valueStyle);

      m_formLayout->addRow(label, value);
   }

   show();
}
