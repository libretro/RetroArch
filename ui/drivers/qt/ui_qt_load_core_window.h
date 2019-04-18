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

#ifndef _QT_LOAD_CORE_WINDOW_H
#define _QT_LOAD_CORE_WINDOW_H

#ifndef CXX_BUILD
extern "C" {
#endif

#include <retro_common_api.h>

#ifndef CXX_BUILD
}
#endif

#include <QtWidgets>

class LoadCoreTableWidget : public QTableWidget
{
   Q_OBJECT
public:
   LoadCoreTableWidget(QWidget *parent = NULL);
signals:
   void enterPressed();
protected:
   void keyPressEvent(QKeyEvent *event);
};

class LoadCoreWindow : public QMainWindow
{
   Q_OBJECT
public:
   LoadCoreWindow(QWidget *parent = 0);
   void initCoreList(const QStringList &extensionFilters = QStringList());
   void setStatusLabel(QString label);
signals:
   void coreLoaded();
   void windowClosed();
private slots:
   void onLoadCustomCoreClicked();
   void onCoreEnterPressed();
   void onCellDoubleClicked(int row, int column);
protected:
   void keyPressEvent(QKeyEvent *event);
   void closeEvent(QCloseEvent *event);
private:
   void loadCore(const char *path);

   QVBoxLayout m_layout;
   LoadCoreTableWidget *m_table;
   QLabel *m_statusLabel;
};

RETRO_BEGIN_DECLS

RETRO_END_DECLS

#endif
