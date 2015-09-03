/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Andres Suarez
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "wimp.h"
#include "stdio.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QDir>
#include <QDebug>
#include <QQmlContext>

QObject *topLevel;

int Wimp::CreateMainWindow()
{

   engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
   topLevel = engine.rootObjects().value(0);
   window = qobject_cast<QQuickWindow *>(topLevel);

   if(settings->playlist_directory[0] != '\0')
       GetCollections(settings->playlist_directory);
   else
       collections.append("Collection dir not defined");

   engine.rootContext()->setContextProperty("collections", QVariant::fromValue(collections));

   if(settings->libretro_directory[0] != '\0')
       GetCores(settings->libretro_directory);
   else
       cores.append("Core dir not defined");

   engine.rootContext()->setContextProperty("cores", QVariant::fromValue(cores));

   return this->exec();

}

void Wimp::GetSettings(settings_t *s)
{
    settings = s;
}

void Wimp::GetCollections(char* path)
{
   QDir dir(path);
   dir.setNameFilters(QStringList("*.lpl"));
   QStringList fileList = dir.entryList();

   if(fileList.count() == 0)
      collections.append("Empty");

   for (int i=0; i < fileList.count(); i++)
   {
      QString collection = fileList[i].section('.',0,0);
      collections.append(collection);
      qDebug() << "Found file: " << collection;
   }

}

void Wimp::GetCores(char* path)
{
   QDir dir(path);
   dir.setNameFilters(QStringList("*.dll"));
   QStringList fileList = dir.entryList();

   if(fileList.count() == 0)
      cores.append("Empty");

   for (int i=0; i < fileList.count(); i++)
   {
      QString core = fileList[i].section('.',0,0);
      cores.append(core);
      qDebug() << "Found file: " << core;
   }

}
