/* Copyright (c) 2012 Research In Motion Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "RetroArch-Cascades.h"

#include <bb/cascades/AbsoluteLayoutProperties>
#include <bb/cascades/ForeignWindowControl>
#include <bb/cascades/AbstractPane>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/Window>
#include <bb/cascades/pickers/FilePicker>

#include <math.h>

using namespace bb::cascades;

RetroArch::RetroArch()
{
	qmlRegisterType<bb::cascades::pickers::FilePicker>("bb.cascades.pickers", 1, 0, "FilePicker");
	qmlRegisterUncreatableType<bb::cascades::pickers::FileType>("bb.cascades.pickers", 1, 0, "FileType", "");

    // Create a QML document and load the main UI QML file, using build patterns.
    QmlDocument *qml = QmlDocument::create("asset:///mainPage.qml");

    if (!qml->hasErrors()) {

        // Set the context property we want to use from inside the QML document. Functions exposed
        // via Q_INVOKABLE will be found with this property and the name of the function.
        qml->setContextProperty("RetroArch", this);

        // The application Page is created from QML.
        AbstractPane *mAppPane = qml->createRootObject<AbstractPane>();

        if (mAppPane) {

            Application::instance()->setScene(mAppPane);

            // Start the thread in which we render to the custom window.
            start();
        }
    }
}

RetroArch::~RetroArch()
{
    // Stop the thread.
    terminate();
    wait();
}

void RetroArch::run()
{
    while (true) {
    	sleep(1);
    }
}

/*
 * Properties
 */

QString RetroArch::getRom()
{
	return rom;
}

void RetroArch::setRom(QString rom)
{
	this->rom = rom;
}

QString RetroArch::getCore()
{
	return core;
}
void RetroArch::setCore(QString core)
{
	this->core = core;
}
