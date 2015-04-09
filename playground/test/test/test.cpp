#include "test.h"
#include "stdio.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QListView>

int Test::CreateMainWindow()
{


   QQmlApplicationEngine engine;
   engine.load(QUrl(QStringLiteral("qrc:/main.qml")));

   return this->exec();
}



