#include <tamtypes.h>
#include <kernel.h>
#include <libpad.h>
#include <fileio.h>
#include <malloc.h>
#include <string.h>

#include <ctype.h>

#include <gsDriver.h>
#include <gsPipe.h>
#include <gsFont.h>

#include <sifrpc.h>
#include <loadfile.h>

#include "cdvd_rpc.h"

#define scr_w 640
#define scr_h 480


#define TEXT_COL_BRIGHT GS_SET_RGBA(0xFF, 0xFF, 0xFF, 0x80)
#define TEXT_COL_DIM GS_SET_RGBA(0x40, 0x40, 0xFF, 0x80)

#define TRUE 1
#define FALSE 0

gsDriver *pDisplay;

extern int screen;
int filelisty = 60;

static float selected = 1;  // The currently selected file/dir


int button_released = TRUE;

//A pointer to an array of TocEntries to be alloc'd later
static struct TocEntry *TocEntryList;



gsFontTex *fontTex;



// pathname on CD
char pathname[1024 + 1] __attribute__((aligned(64)));

gsFont myFont;

int WaitPadReady(int port, int slot);
void SetPadMode(int port, int slot);


void ClearScreen(void)
{
    pDisplay->drawPipe.setAlphaEnable(GS_DISABLE);
    pDisplay->drawPipe.RectFlat(0, 0, scr_w, scr_h, 0, GS_SET_RGBA(0, 0, 0, 0x80));
    pDisplay->drawPipe.setAlphaEnable(GS_ENABLE);
    pDisplay->drawPipe.Flush();
}


int main(void)
{
    SifInitRpc(0);

    SifLoadModule("rom0:SIO2MAN", 0, NULL); /* load sio2 manager irx */
    SifLoadModule("rom0:PADMAN", 0, NULL);  /* load pad manager irx */

    SifLoadModule("host:cdvd.irx", 0, NULL);

    CDVD_Init();

    padInit(0);

    char *padBuf = (char *)memalign(64, 256);

    padPortOpen(0, 0, padBuf);

    SetPadMode(0, 0);

    // allocate the memory for a large file list
    TocEntryList = (TocEntry *)memalign(64, 4000 * sizeof(struct TocEntry));


    // open the font file, find the size of the file, allocate memory for it, and load it
    int fontfile = fioOpen("host:font.fnt", O_RDONLY);
    int fontsize = fioLseek(fontfile, 0, SEEK_END);
    fioLseek(fontfile, 0, SEEK_SET);
    fontTex = (gsFontTex *)memalign(64, fontsize);
    fioRead(fontfile, fontTex, fontsize);
    fioClose(fontfile);

    // Upload the background to the texture buffer
    pDisplay = new gsDriver;

    pDisplay->setDisplayMode(scr_w, scr_h,
                             170, 80,
                             GS_PSMCT32, 2,
                             GS_TV_AUTO, GS_TV_INTERLACE,
                             GS_DISABLE, GS_DISABLE);

    // Enable Alpha Blending
    pDisplay->drawPipe.setAlphaEnable(GS_ENABLE);

    myFont.assignPipe(&(pDisplay->drawPipe));

    // Upload the font into the texture memory past the screen buffers
    myFont.uploadFont(fontTex, pDisplay->getTextureBufferBase(),
                      fontTex->TexWidth,  // Use the fontTex width as texbuffer width (can use diff width)
                      0, 0);


#define list_max 21
    int list_size = 21;  // Number of files to display in list
    int first_file = 1;  // The first file to display in the on-screen list
    int num_files = 0;   // The total number of files in the list
    int offset;          // Offset of the selected file into the displayed list

    struct padButtonStatus padButtons;
    int ps2_buttons;

    button_released = TRUE;

    while (1) {

        while (1)  // until we've selected a file
        {
            // Get entries from specified path, don't filter by file extension,
            // get files and directories, get a maximum of 4000 entries, and update path if dir changed
            num_files = CDVD_GetDir(pathname, NULL, CDVD_GET_FILES_AND_DIRS, TocEntryList, 4000, pathname);

            if (num_files < list_max)
                list_size = num_files;
            else
                list_size = list_max;

            // Don't leave the drive spinning, it's annoying !
            CDVD_Stop();

            while (1)  // Until  we've selected something (dir or file)
            {

                // Get button presses
                // If X then select the previously highlighted file
                // If up/down then increase/decrease the selected file
                int padState;

                // only listen to pad input if it's plugged in, and stable
                padState = padGetState(0, 0);
                if (padState == PAD_STATE_STABLE) {
                    padRead(0, 0, &padButtons);
                    ps2_buttons = (padButtons.btns[0] << 8) | padButtons.btns[1];
                    ps2_buttons = ~ps2_buttons;

                    if (num_files > 0) {
                        // file Selected
                        if (ps2_buttons & PAD_CROSS) {
                            if (button_released == TRUE) {
                                button_released = FALSE;
                                break;
                            }
                        } else
                            button_released = TRUE;

                        // DPAD + Shoulder file Selection
                        if (ps2_buttons & PAD_UP)
                            selected -= 0.15;
                        else if (ps2_buttons & PAD_DOWN)
                            selected += 0.15;
                        else if (ps2_buttons & PAD_R1)
                            selected -= 1;
                        else if (ps2_buttons & PAD_R2)
                            selected += 1;
                        else if (ps2_buttons & PAD_L1)
                            selected = 1;
                        else if (ps2_buttons & PAD_L2)
                            selected = num_files;
                    }

                    if (ps2_buttons & PAD_SELECT) {
                        strcpy(pathname, "/");

                        ClearScreen();

                        myFont.Print(0, 640, 220, 4,
                                     TEXT_COL_BRIGHT,
                                     GSFONT_ALIGN_CENTRE, "Please Change CD\nThen Press 'X'");

                        pDisplay->drawPipe.Flush();

                        // Wait for VSync and then swap buffers
                        pDisplay->WaitForVSync();

                        pDisplay->swapBuffers();


                        CDVD_FlushCache();
                        strcpy(pathname, "/");

                        while (1) {
                            int padState;

                            // only listen to pad input if it's plugged in, and stable
                            padState = padGetState(0, 0);
                            if (padState == PAD_STATE_STABLE) {
                                padRead(0, 0, &padButtons);
                                ps2_buttons = (padButtons.btns[0] << 8) | padButtons.btns[1];
                                ps2_buttons = ~ps2_buttons;

                                // ROM Selected
                                if (ps2_buttons & PAD_CROSS) {
                                    break;
                                }
                            }

                            pDisplay->WaitForVSync();
                        }

                        num_files = CDVD_GetDir(pathname, NULL, CDVD_GET_FILES_AND_DIRS, TocEntryList, 4000, pathname);

                        if (num_files < list_max)
                            list_size = num_files;
                        else
                            list_size = list_max;

                        selected = 1;

                        CDVD_Stop();
                    }

                    if ((padButtons.mode >> 4) == 0x07) {
                        // Analogue file selection
                        float pad_v;

                        pad_v = (float)(padButtons.ljoy_v - 128);  // Range = +127 to -128


                        if (pad_v > 32) {
                            // scrolling down, so incrementing selected tom
                            pad_v -= 32;
                            selected += (pad_v / 96);
                        }

                        if (pad_v < -32) {
                            // scrolling down, so incrementing selected tom
                            pad_v += 32;
                            selected += (pad_v / 96);
                        }
                    }



                    if (selected < 1)
                        selected = 1;

                    if ((int)selected > num_files)
                        selected = (float)num_files;
                }

                // calculate which file to display first in the list
                if ((int)selected <= list_size / 2)
                    first_file = 1;
                else {
                    if ((int)selected >= (num_files - (list_size / 2) + 1))
                        first_file = num_files - list_size + 1;
                    else
                        first_file = (int)selected - ((list_size / 2));
                }

                // calculate the offset of the selected file into the displayed list
                offset = (int)selected - first_file;


                ClearScreen();

                if (num_files > 0) {
                    //				pDisplay->drawPipe.setScissorRect(list_xpos,list_ypos,list_xpos+list_width,list_ypos+list_height);

                    for (int file = 0; file < list_size; file++) {
                        // if the entry is a dir, then display the directory symbol before the name
                        if (TocEntryList[first_file + file - 1].fileProperties & 0x02) {
                            // display a dir symbol (character 001 in the bitmap font)
                            myFont.Print(128, 640, filelisty + (file * 18), 4, GS_SET_RGBA(0x80, 0x80, 0x80, 0x80), GSFONT_ALIGN_LEFT, "\001");

                            if (file == ((int)selected - first_file)) {
                                myFont.Print(148, 640, filelisty + (file * 18), 4, TEXT_COL_BRIGHT, GSFONT_ALIGN_LEFT,
                                             TocEntryList[first_file + file - 1].filename);
                            } else {
                                myFont.Print(148, 640, filelisty + (file * 18), 4, TEXT_COL_DIM, GSFONT_ALIGN_LEFT,
                                             TocEntryList[first_file + file - 1].filename);
                            }
                        } else {
                            if (file == ((int)selected - first_file)) {
                                myFont.Print(128, 640, filelisty + (file * 18), 4, TEXT_COL_BRIGHT, GSFONT_ALIGN_LEFT,
                                             TocEntryList[first_file + file - 1].filename);
                            } else {
                                myFont.Print(128, 640, filelisty + (file * 18), 4, TEXT_COL_DIM, GSFONT_ALIGN_LEFT,
                                             TocEntryList[first_file + file - 1].filename);
                            }
                        }
                    }

                    myFont.Print(420, 640, 440, 0, TEXT_COL_BRIGHT, GSFONT_ALIGN_LEFT,
                                 "Press X to Select");

                    myFont.Print(420, 640, 458, 0, TEXT_COL_BRIGHT, GSFONT_ALIGN_LEFT,
                                 "Press SELECT to Change CD");
                }


                pDisplay->drawPipe.Flush();

                // Wait for VSync and then swap buffers
                pDisplay->WaitForVSync();

                pDisplay->swapBuffers();
            }

            // We've selected something, but is it a file or a dir ?
            if (TocEntryList[((int)selected) - 1].fileProperties & 0x02) {
                // Append name onto current path
                //gui_getfile_dispname((int)selected, tempname);
                strcat(pathname, "/");
                strcat(pathname, TocEntryList[((int)selected) - 1].filename);

                // file list will be got next time round the while loop

                // Start from top of list
                selected = 1;
            } else {
                // It's not a dir, so it must be a file
                break;
            }
        }

        char size_string[64];

        if (TocEntryList[((int)selected) - 1].fileSize < (2 * 1024))
            sprintf(size_string, "%d bytes", TocEntryList[((int)selected) - 1].fileSize);
        else {
            if (TocEntryList[((int)selected) - 1].fileSize < (2 * 1024 * 1024))
                sprintf(size_string, "%d Kb", TocEntryList[((int)selected) - 1].fileSize / 1024);
            else
                sprintf(size_string, "%d Mb", TocEntryList[((int)selected) - 1].fileSize / (1024 * 1024));
        }


        for (int frame = 0; frame < 200; frame++) {

            // Selected a file, so display file properties for a couple of seconds
            ClearScreen();

            myFont.Print(100, 200, 220, 1, TEXT_COL_BRIGHT, GSFONT_ALIGN_LEFT,
                         "File name:");

            myFont.Print(200, 640, 220, 1, TEXT_COL_BRIGHT, GSFONT_ALIGN_LEFT,
                         TocEntryList[((int)selected) - 1].filename);

            myFont.Print(100, 200, 240, 1, TEXT_COL_BRIGHT, GSFONT_ALIGN_LEFT,
                         "File path:");

            myFont.Print(200, 640, 240, 1, TEXT_COL_BRIGHT, GSFONT_ALIGN_LEFT,
                         pathname);


            myFont.Print(100, 200, 260, 1, TEXT_COL_BRIGHT, GSFONT_ALIGN_LEFT,
                         "File size:");

            myFont.Print(200, 640, 260, 1, TEXT_COL_BRIGHT, GSFONT_ALIGN_LEFT,
                         size_string);

            pDisplay->drawPipe.Flush();

            // Wait for VSync and then swap buffers
            pDisplay->WaitForVSync();

            pDisplay->swapBuffers();
        }
    }

    free(fontTex);

    free(TocEntryList);

    delete pDisplay;

    return 0;
}


int WaitPadReady(int port, int slot)
{
    int state = 0;

    while ((state != PAD_STATE_STABLE) &&
           (state != PAD_STATE_FINDCTP1)) {
        state = padGetState(port, slot);

        if (state == PAD_STATE_DISCONN)
            break;  // If no pad connected then dont wait for it to be plugged in

        //pEmuDisplay->WaitForVSync();
    }

    return state;
}


void SetPadMode(int port, int slot)
{
    // If the controller is already plugged in then
    // put the controller into Analogue mode (and lock it)
    // so that analogue stick can be used

    if (WaitPadReady(port, slot) == PAD_STATE_STABLE)  // if pad is connected then initialise it
    {
        padSetMainMode(port, slot, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);

        WaitPadReady(port, slot);
    }
}
