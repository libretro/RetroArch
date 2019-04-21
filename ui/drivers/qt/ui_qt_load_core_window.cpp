/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ui_qt_load_core_window.h"

#include <QFileDialog>
#include <QDesktopWidget>

#ifndef CXX_BUILD
extern "C" {
#endif

#include <string/stdstring.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>

#include "../../../core_info.h"
#include "../../../verbosity.h"
#include "../../../configuration.h"
#include "../../../msg_hash.h"
#include "../../../paths.h"
#include "../../../retroarch.h"
#include "../../../command.h"
#include "../../../frontend/frontend_driver.h"

#ifndef CXX_BUILD
}
#endif

#define CORE_NAME_COLUMN 0
#define CORE_VERSION_COLUMN 1

LoadCoreTableWidget::LoadCoreTableWidget(QWidget *parent) :
   QTableWidget(parent)
{
}

void LoadCoreTableWidget::keyPressEvent(QKeyEvent *event)
{
   if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
   {
      event->accept();
      emit enterPressed();
   }
   else
      QTableWidget::keyPressEvent(event);
}

LoadCoreWindow::LoadCoreWindow(QWidget *parent) :
   QMainWindow(parent)
   ,m_layout()
   ,m_table(new LoadCoreTableWidget())
   ,m_statusLabel(new QLabel())
{
   QHBoxLayout *hbox = new QHBoxLayout();
   QPushButton *customCoreButton = new QPushButton(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_LOAD_CUSTOM_CORE));

   connect(customCoreButton, SIGNAL(clicked()), this, SLOT(onLoadCustomCoreClicked()));
   connect(m_table, SIGNAL(enterPressed()), this, SLOT(onCoreEnterPressed()));
   connect(m_table, SIGNAL(cellDoubleClicked(int,int)), this, SLOT(onCellDoubleClicked(int,int)));

   setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE));

   setCentralWidget(new QWidget());

   centralWidget()->setLayout(&m_layout);

   hbox->addWidget(customCoreButton);
   hbox->addItem(new QSpacerItem(width(), 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

   m_layout.addWidget(m_table);
   m_layout.addLayout(hbox);

   statusBar()->addPermanentWidget(m_statusLabel);
}

void LoadCoreWindow::closeEvent(QCloseEvent *event)
{
   emit windowClosed();

   QWidget::closeEvent(event);
}

void LoadCoreWindow::keyPressEvent(QKeyEvent *event)
{
   if (event->key() == Qt::Key_Escape)
   {
      event->accept();
      close();
   }
   else
      QMainWindow::keyPressEvent(event);
}

void LoadCoreWindow::setStatusLabel(QString label)
{
   m_statusLabel->setText(label);
}

void LoadCoreWindow::onCellDoubleClicked(int, int)
{
   onCoreEnterPressed();
}

void LoadCoreWindow::loadCore(const char *path)
{
   QProgressDialog progress(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_LOADING_CORE), QString(), 0, 0, this);
   progress.setWindowTitle(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE));
   progress.setMinimumDuration(0);
   progress.setValue(progress.minimum());
   progress.show();

   /* Because core loading will block, we need to go ahead and process pending events that would allow the progress dialog to fully show its contents before actually starting the core loading process. Must call processEvents() twice. */
   qApp->processEvents();
   qApp->processEvents();

#ifdef HAVE_DYNAMIC
   path_set(RARCH_PATH_CORE, path);

   command_event(CMD_EVENT_CORE_INFO_DEINIT, NULL);
   command_event(CMD_EVENT_CORE_INFO_INIT, NULL);

   core_info_init_current_core();

   if (!command_event(CMD_EVENT_LOAD_CORE, NULL))
   {
      QMessageBox::critical(this, msg_hash_to_str(MSG_ERROR), msg_hash_to_str(MSG_FAILED_TO_OPEN_LIBRETRO_CORE));
      return;
   }

   setProperty("last_launch_with_index", -1);

   emit coreLoaded();
#endif
}

void LoadCoreWindow::onCoreEnterPressed()
{
   QTableWidgetItem *selectedCoreItem = NULL;
   QString path;
   QByteArray pathArray;
   const char *pathData = NULL;
   QVariantHash hash;

   selectedCoreItem = m_table->item(m_table->currentRow(), CORE_NAME_COLUMN);
   hash = selectedCoreItem->data(Qt::UserRole).toHash();
   path = hash["path"].toString();

   pathArray.append(path);
   pathData = pathArray.constData();

   loadCore(pathData);
}

void LoadCoreWindow::onLoadCustomCoreClicked()
{
   QString path;
   QByteArray pathArray;
   settings_t *settings = config_get_ptr();
   char core_ext[255] = {0};
   char filters[PATH_MAX_LENGTH] = {0};
   const char *pathData = NULL;

   frontend_driver_get_core_extension(core_ext, sizeof(core_ext));

   strlcpy(filters, "Cores (*.", sizeof(filters));
   strlcat(filters, core_ext, sizeof(filters));
   strlcat(filters, ");;All Files (*.*)", sizeof(filters));

   path = QFileDialog::getOpenFileName(this, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE), settings->paths.directory_libretro, filters, NULL);

   if (path.isEmpty())
      return;

   pathArray.append(path);
   pathData = pathArray.constData();

   loadCore(pathData);
}

void LoadCoreWindow::initCoreList(const QStringList &extensionFilters)
{
   core_info_list_t *cores = NULL;
   QStringList horizontal_header_labels;
   QDesktopWidget *desktop = qApp->desktop();
   QRect desktopRect = desktop->availableGeometry();
   unsigned i = 0;
   int j = 0;

   horizontal_header_labels << msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NAME);
   horizontal_header_labels << msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE_VERSION);

   core_info_get_list(&cores);

   m_table->clear();
   m_table->setColumnCount(0);
   m_table->setRowCount(0);
   m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
   m_table->setSelectionMode(QAbstractItemView::SingleSelection);
   m_table->setSortingEnabled(false);
   m_table->setColumnCount(2);
   m_table->setHorizontalHeaderLabels(horizontal_header_labels);

   if (cores)
   {
      m_table->setRowCount(cores->count);

      for (i = 0; i < cores->count; i++)
      {
         core_info_t *core = core_info_get(cores, i);
         QTableWidgetItem *name_item = NULL;
         QTableWidgetItem *version_item = new QTableWidgetItem(core->display_version);
         QVariantHash hash;
         const char *name = core->display_name;

         if (string_is_empty(name))
            name = path_basename(core->path);

         name_item = new QTableWidgetItem(name);

         hash["path"] = core->path;
         hash["extensions"] = QString(core->supported_extensions).split("|");

         name_item->setData(Qt::UserRole, hash);
         name_item->setFlags(name_item->flags() & ~Qt::ItemIsEditable);
         version_item->setFlags(version_item->flags() & ~Qt::ItemIsEditable);

         m_table->setItem(i, CORE_NAME_COLUMN, name_item);
         m_table->setItem(i, CORE_VERSION_COLUMN, version_item);
      }
   }

   if (!extensionFilters.isEmpty())
   {
      QVector<int> rowsToHide;

      for (j = 0; j < m_table->rowCount(); j++)
      {
         bool found = false;
         QTableWidgetItem *item = m_table->item(j, CORE_NAME_COLUMN);
         QVariantHash hash;
         QStringList extensions;
         int k = 0;

         if (!item)
            continue;

         hash = item->data(Qt::UserRole).toHash();
         extensions = hash["extensions"].toStringList();

         if (!extensions.isEmpty())
         {
            for (k = 0; k < extensions.size(); k++)
            {
               QString ext = extensions.at(k).toLower();

               if (extensionFilters.contains(ext, Qt::CaseInsensitive))
               {
                  found = true;
                  break;
               }
            }

            if (!found)
               rowsToHide.append(j);
         }
      }

      if (rowsToHide.size() != m_table->rowCount())
      {
         int i = 0;

         for (i = 0; i < rowsToHide.count() && rowsToHide.count() > 0; i++)
         {
            const int &row = rowsToHide.at(i);
            m_table->setRowHidden(row, true);
         }
      }
   }

   m_table->setSortingEnabled(true);
   m_table->resizeColumnsToContents();
   m_table->sortByColumn(0, Qt::AscendingOrder);
   m_table->selectRow(0);
   m_table->setAlternatingRowColors(true);

   resize(qMin(desktopRect.width(), contentsMargins().left() + m_table->horizontalHeader()->length() + contentsMargins().right()), height());
}
