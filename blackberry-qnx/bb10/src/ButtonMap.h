#ifndef _BUTTONMAP_H_
#define _BUTTONMAP_H_

#include <bb/cascades/Application>
#include <bb/cascades/ArrayDataModel>
#include <bb/cascades/DropDown>

#include <screen/screen.h>
#include <sys/neutrino.h>
#include "general.h"
#include "conf/config_file.h"
#include "file.h"

/*
typedef struct {
   int32_t button;
   QString label;
} ButtonMap_t;
*/

using namespace bb::cascades;

class ButtonMap: public QObject
{
   Q_OBJECT

public:
    ButtonMap(screen_context_t screen_cxt, QString groupId, int coid);
    ~ ButtonMap();

    QString getLabel(int button);
    //pass in RARCH button enum for button, map to g_setting
    int mapNextButtonPressed();
    //Call in our emulator thread with seperate screen_cxt
    int getButtonMapping(int player, int button);
    //Call from frontend
    int requestButtonMapping(screen_device_t device, int player, int button);


   Q_INVOKABLE int mapButton(int player, int button);
   Q_INVOKABLE QString buttonToString(int player, int button);
   Q_INVOKABLE void refreshButtonMap(int player);
   Q_INVOKABLE void mapDevice(int index, int player);

    ArrayDataModel *buttonDataModel;
    DropDown *deviceSelection;

private:
    screen_context_t screen_cxt;
    screen_window_t screen_win;
    screen_buffer_t screen_buf;
    screen_device_t device;
    int player;
    int button;
    int screen_resolution[2];
    QString groupId;
    int coid;

    //use g_settings.input.binds[port][i].joykey = SCREEN_* for mapping
};

#endif
