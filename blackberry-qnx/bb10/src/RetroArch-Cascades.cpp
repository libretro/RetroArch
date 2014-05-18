#include "RetroArch-Cascades.h"
#include "../../../general.h"
#include "../../../conf/config_file.h"
#include "../../../file.h"
#include "../../../frontend/info/core_info.h"

#include "../../frontend_qnx.h"

#include <bb/cascades/Application>
#include <bb/cascades/AbsoluteLayoutProperties>
#include <bb/cascades/ForeignWindowControl>
#include <bb/cascades/AbstractPane>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/Window>
#include <bb/cascades/pickers/FilePicker>
#include <bb/data/JsonDataAccess>
#include <bb/device/HardwareInfo>
#include <bb/cascades/ListView>


#include <screen/screen.h>
#include <bps/screen.h>
#include <bps/navigator.h>
#include <bps/bps.h>

#include <math.h>
#include <dirent.h>
#include <bb/cascades/DropDown>

using namespace bb::cascades;
using namespace bb::data;
using namespace bb::device;

extern screen_window_t screen_win;
extern screen_context_t screen_ctx;

RetroArch::RetroArch() :
                QObject()
{
   bool res;
   QmlDocument *qml;

   qmlRegisterType<bb::cascades::pickers::FilePicker>("bb.cascades.pickers", 1, 0, "FilePicker");
   qmlRegisterUncreatableType<bb::cascades::pickers::FileType>("bb.cascades.pickers", 1, 0, "FileType", "");

   // Create channel to signal threads on
   chid = ChannelCreate(0);
   coid = ConnectAttach(0, 0, chid, _NTO_SIDE_CHANNEL, 0);

   res = connect(
         OrientationSupport::instance(), SIGNAL(rotationCompleted()),
         this, SLOT(onRotationCompleted()));
   (void)res;

   rarch_main_clear_state();
   strlcpy(g_extern.config_path, "app/native/retroarch.cfg", sizeof(g_extern.config_path));
   config_load();

   strlcpy(g_settings.libretro, "app/native/lib", sizeof(g_settings.libretro));
   coreSelectedIndex = -1;

   //Stop config overwritting values
   g_extern.block_config_read = true;

   qml = QmlDocument::create("asset:///main.qml");

   if (!qml->hasErrors())
   {
      qml->setContextProperty("RetroArch", this);

      AbstractPane *mAppPane = qml->createRootObject<AbstractPane>();

      if (mAppPane)
      {
         //Get core DropDown reference to populate it in C++
         coreSelection = mAppPane->findChild<DropDown*>("dropdown_core");
         connect(coreSelection, SIGNAL(selectedValueChanged(QVariant)), this, SLOT(onCoreSelected(QVariant)));
         core_info_list = core_info_list_new(g_settings.libretro);
         populateCores(core_info_list);

         Application::instance()->setScene(mAppPane);

         screen_create_context(&screen_ctx, 0);
         input_qnx.init();
         buttonMap = new ButtonMap(screen_ctx, (const char*)Application::instance()->mainWindow()->groupId().toAscii().constData(), coid);
         qml->setContextProperty("ButtonMap", buttonMap);

         deviceSelection = mAppPane->findChild<DropDown*>("dropdown_devices");
         buttonMap->deviceSelection = deviceSelection;
         findDevices();

         //Setup the datamodel for button mapping.
         mAppPane->findChild<ListView*>("buttonMapList")->setDataModel(buttonMap->buttonDataModel);

         // Start the thread in which we render to the custom window.
         start();
      }
   }
}

RetroArch::~RetroArch()
{
   core_info_list_free(core_info_list);
}

void RetroArch::run()
{
   int rcvid = -1;
   recv_msg msg;

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

   while (true)
   {
      rcvid = MsgReceive(chid, &msg, sizeof(msg), 0);

      if (rcvid > 0)
      {
         switch (msg.code)
         {
         case RETROARCH_START_REQUESTED:
         {
            char win_id[64];
            MsgReply(rcvid,0,NULL,0);

            if (screen_create_window_type(&screen_win, screen_ctx, SCREEN_CHILD_WINDOW) != BPS_SUCCESS)
            {
               RARCH_ERR("Screen create window failed.\n");
            }
            if (screen_join_window_group(screen_win, (const char*)Application::instance()->mainWindow()->groupId().toAscii().constData()) != BPS_SUCCESS)
            {
               RARCH_ERR("Screen join window group failed.\n");
            }

            strlcpy(win_id, "RetroArch_Window", sizeof(win_id));
            screen_set_window_property_cv(screen_win, SCREEN_PROPERTY_ID_STRING, strlen(win_id), win_id);

            int z = 10;
            if (screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_ZORDER, &z) != 0) {
               return;
            }

            initRASettings();

            rarch_main(0, NULL);
            Application::instance()->exit();
            break;
         }
         //The class should probably be it's own QThread, simplify things
         case RETROARCH_BUTTON_MAP:
            MsgReply(rcvid, buttonMap->mapNextButtonPressed(), NULL, 0);
            break;
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
QString RetroArch::getContent()
{
   return content;
}

void RetroArch::setContent(QString content)
{
   this->content = content;
}

QString RetroArch::getCore()
{
   return core;
}

void RetroArch::setCore(QString core)
{
   this->core = core;
}

QString RetroArch::getContentExtensions()
{
   return contentExtensions;
}

/*
 * Slots
 */
void RetroArch::onRotationCompleted()
{
   if (OrientationSupport::instance()->orientation() == UIOrientation::Landscape)
   {
      if (state == RETROARCH_START_REQUESTED)
         start();
   }
}

void RetroArch::onCoreSelected(QVariant value)
{
   coreSelectedIndex = value.toInt();

   core.clear();
   core.append(core_info_list->list[coreSelectedIndex].path);
   emit coreChanged(core);

   contentExtensions = QString("*.%1").arg(core_info_list->list[coreSelectedIndex].supported_extensions);
   contentExtensions.replace("|", "|*.");
   emit contentExtensionsChanged(contentExtensions);

   qDebug() << "Core Selected: " << core;
   qDebug() << "Supported Extensions: " << contentExtensions;
}

/*
 * Functions
 */
void RetroArch::start()
{
   state = RETROARCH_START_REQUESTED;

   if (OrientationSupport::instance()->orientation() == UIOrientation::Portrait &&
       OrientationSupport::instance()->supportedDisplayOrientation() != SupportedDisplayOrientation::DeviceNorth)
      OrientationSupport::instance()->setSupportedDisplayOrientation(SupportedDisplayOrientation::DisplayLandscape);
   else
   {
      recv_msg msg;
      msg.code = RETROARCH_START_REQUESTED;

      MsgSend(coid, (void*)&msg, sizeof(msg), (void*)NULL, 0);

      state = RETROARCH_RUNNING;
   }
}

void RetroArch::populateCores(core_info_list_t * info)
{
   unsigned i;
   Option *tmp;

   //Populate DropDown
   for (i = 0; i < info->count; ++i)
   {
      qDebug() << info->list[i].display_name;

      tmp = Option::create().text(QString(info->list[i].display_name))
                            .value(i);

      coreSelection->add(tmp);
   }
}

void RetroArch::findDevices()
{
   unsigned i;
   Option *tmp;

   deviceSelection->removeAll();

   //Find all connected devices
   //Populate DropDown
   for (i = 0; i < pads_connected; ++i)
   {
      tmp = Option::create().text(devices[i].device_name)
                            .value(i);

      deviceSelection->add(tmp);

      //QML shows player 1 by default, so set dropdown to their controller.
      if(devices[i].port == 0 || devices[i].device == DEVICE_KEYPAD)
         deviceSelection->setSelectedIndex(i);
   }
}

extern "C" void discoverControllers();
void RetroArch::discoverController(int player)
{
   //TODO: Check device, gamepad/keyboard and return accordingly.
   discoverControllers();
   findDevices();
   buttonMap->refreshButtonMap(player);
   return;
}

void RetroArch::initRASettings()
{
   HardwareInfo *hwInfo;

   strlcpy(g_settings.libretro,(char *)core.toAscii().constData(), sizeof(g_settings.libretro));
   strlcpy(g_extern.fullpath, (char *)content.toAscii().constData(), sizeof(g_extern.fullpath));

   hwInfo = new HardwareInfo();

   //If Physical keyboard or a device mapped to player 1, hide overlay
   //TODO: Should there be a minimized/quick settings only overlay?
   if(hwInfo->isPhysicalKeyboardDevice() || port_device[0])
      *g_settings.input.overlay = '\0';
}
