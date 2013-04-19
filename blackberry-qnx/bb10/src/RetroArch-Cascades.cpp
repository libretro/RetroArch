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
#include "general.h"
#include "conf/config_file.h"
#include "file.h"

#ifdef HAVE_RGUI
#include "frontend/menu/rgui.h"
#endif

#include <bb/cascades/AbsoluteLayoutProperties>
#include <bb/cascades/ForeignWindowControl>
#include <bb/cascades/AbstractPane>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/Window>
#include <bb/cascades/pickers/FilePicker>
#include <bb/data/JsonDataAccess>

#include <screen/screen.h>
#include <bps/screen.h>
#include <bps/navigator.h>
#include <bps/bps.h>

#include <math.h>
#include <dirent.h>
#include <bb/cascades/DropDown>

using namespace bb::cascades;
using namespace bb::data;

//Use after calling findCores
#define GET_CORE_INFO(x, y) coreInfo[coreList[x]].toMap()[y].toString()

RetroArch::RetroArch()
{
   qmlRegisterType<bb::cascades::pickers::FilePicker>("bb.cascades.pickers", 1, 0, "FilePicker");
   qmlRegisterUncreatableType<bb::cascades::pickers::FileType>("bb.cascades.pickers", 1, 0, "FileType", "");

   // Create channel to signal threads on
   chid = ChannelCreate(0);
   coid = ConnectAttach(0, 0, chid, _NTO_SIDE_CHANNEL, 0);

   bool res = connect(
         OrientationSupport::instance(), SIGNAL(rotationCompleted()),
         this, SLOT(onRotationCompleted()));

   rarch_main_clear_state();

   strlcpy(g_settings.libretro, "app/native/lib", sizeof(g_settings.libretro));
   coreSelectedIndex = -1;

   QmlDocument *qml = QmlDocument::create("asset:///main.qml");

   if (!qml->hasErrors())
   {
      qml->setContextProperty("RetroArch", this);

      AbstractPane *mAppPane = qml->createRootObject<AbstractPane>();

      if (mAppPane)
      {
         //Get core DropDown reference to populate it in C++
         coreSelection = mAppPane->findChild<DropDown*>("dropdown_core");
         connect(coreSelection, SIGNAL(selectedValueChanged(QVariant)), this, SLOT(onCoreSelected(QVariant)));
         findCores();

         Application::instance()->setScene(mAppPane);

         // Start the thread in which we render to the custom window.
         start();
      }
   }
}

RetroArch::~RetroArch()
{
   free(coreList);
}

void RetroArch::aboutToQuit()
{
   recv_msg msg;

   msg.code = RETROARCH_EXIT;

   MsgSend(coid, (void*)&msg, sizeof(msg), (void*)NULL, 0);

   wait();
}

extern screen_window_t screen_win;
extern screen_context_t screen_ctx;
void RetroArch::run()
{
   int rcvid = -1;
   recv_msg msg;

   while (true) {
      rcvid = MsgReceive(chid, &msg, sizeof(msg), 0);

      if (rcvid > 0)
      {
         switch (msg.code)
         {
         case RETROARCH_START_REQUESTED:
         {
            printf("RetroArch Started Received\n");fflush(stdout);

            MsgReply(rcvid,0,NULL,0);

            screen_create_context(&screen_ctx, 0);

            bps_initialize();

            if (screen_request_events(screen_ctx) != BPS_SUCCESS)
            {
               RARCH_ERR("screen_request_events failed.\n");
            }

            if (navigator_request_events(0) != BPS_SUCCESS)
            {
               RARCH_ERR("navigator_request_events failed.\n");
            }

            if (navigator_rotation_lock(false) != BPS_SUCCESS)
            {
               RARCH_ERR("navigator_location_lock failed.\n");
            }

            screen_create_window_type(&screen_win, screen_ctx, SCREEN_CHILD_WINDOW);

            screen_join_window_group(screen_win, Application::instance()->mainWindow()->groupId().toAscii().constData());

            char *win_id = "RetroArch_Emulator_Window";
            screen_set_window_property_cv(screen_win, SCREEN_PROPERTY_ID_STRING, strlen(win_id), win_id);

            int z = 10;
            if (screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_ZORDER, &z) != 0) {
               return;
            }

            initRASettings();

            rarch_main(0, NULL);
            break;
         }
         case RETROARCH_EXIT:
            MsgReply(rcvid,0,NULL,0);
            goto exit;
         default:
            break;
         }
      }
   }
   exit:
   return;
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

QString RetroArch::getRomExtensions()
{
   return romExtensions;
}

/*
 * Slots
 */
void RetroArch::onRotationCompleted()
{
   if (OrientationSupport::instance()->orientation() == UIOrientation::Landscape)
   {
      if (state == RETROARCH_START_REQUESTED)
      {
         startEmulator();
      }
   }
}

void RetroArch::onCoreSelected(QVariant value)
{
   coreSelectedIndex = value.toInt();

   core.clear();
   core.append("app/native/lib/");
   core.append(coreList[coreSelectedIndex]);
   emit coreChanged(core);

   romExtensions = GET_CORE_INFO(coreSelectedIndex, "supported_extensions");
   emit romExtensionsChanged(romExtensions);

   qDebug() << "Core Selected: " << core;
   qDebug() << "Supported Extensions: " << romExtensions;
}

/*
 * Functions
 */
void RetroArch::startEmulator()
{
   state = RETROARCH_START_REQUESTED;

   if (OrientationSupport::instance()->orientation() == UIOrientation::Portrait)
   {
      OrientationSupport::instance()->setSupportedDisplayOrientation(SupportedDisplayOrientation::DisplayLandscape);
   }
   else
   {
      recv_msg msg;
      msg.code = RETROARCH_START_REQUESTED;

      MsgSend(coid, (void*)&msg, sizeof(msg), (void*)NULL, 0);

      state = RETROARCH_RUNNING;
   }
}

void RetroArch::findCores()
{
   DIR *dirp;
   struct dirent* direntp;
   int count=0, i=0;

   dirp = opendir(g_settings.libretro);
   if( dirp != NULL ) {
      for(;;) {
         direntp = readdir( dirp );
         if( direntp == NULL ) break;
         count++;
      }
      fflush(stdout);
      rewinddir(dirp);

      if(count==2){
         printf("No Cores Found");fflush(stdout);
      }

      coreList = (char**)malloc(count*sizeof(char*));
      count = 0;

      for(;;){
         direntp = readdir( dirp );
         if( direntp == NULL ) break;
         coreList[count++] = strdup((char*)direntp->d_name);
      }

      //Load info for Cores
      JsonDataAccess jda;

      coreInfo = jda.load("app/native/assets/coreInfo.json").toMap();

      Option *tmp;

      //Populate DropDown
      for (i = 2; i < count; ++i)
      {
         qDebug() << GET_CORE_INFO(i, "display_name");

         tmp = Option::create().text(GET_CORE_INFO(i, "display_name"))
                               .value(i);

         coreSelection->add(tmp);
      }
   }

   closedir(dirp);
}

void RetroArch::initRASettings()
{
   strlcpy(g_settings.libretro,(char *)core.toAscii().constData(), sizeof(g_settings.libretro));
   strlcpy(g_extern.fullpath, (char *)rom.toAscii().constData(), sizeof(g_extern.fullpath));
   strlcpy(g_settings.input.overlay, GET_CORE_INFO(coreSelectedIndex, "default_overlay").toAscii().constData(), sizeof(g_settings.input.overlay));
}

