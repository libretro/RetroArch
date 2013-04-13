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
#ifndef _HELLOFOREIGNWINDOW_H_
#define _HELLOFOREIGNWINDOW_H_

#include <bb/cascades/Application>
#include <screen/screen.h>
#include <QThread>

using namespace bb::cascades;

namespace bb
{
    namespace cascades
    {
        class Page;
    }
}

class RetroArch: public QThread
{
    Q_OBJECT

public:
    RetroArch();
    ~ RetroArch();

private:
    /**
     * This QThread-run function runs the custom window rendering in a separate thread to avoid lag
     * in the rest of the Cascades UI.
     */
    void run();

};

#endif // ifndef _HELLOFOREIGNWINDOW_H_
