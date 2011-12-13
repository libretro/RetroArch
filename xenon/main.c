/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *  Copyright (C) 2011 - Daniel De Matteis
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#if 0
int ssnes_main(int argc, char *argv[]);

#undef main
// Temporary, a more sane implementation should go here.
int main(int argc, char *argv[])
{
   sys_spu_initialize(4, 3);
   char arg1[] = "ssnes";
   char arg2[] = "/dev_hdd0/game/SNES90000/USRDIR/main.sfc";
   char arg3[] = "-v";
   char arg4[] = "-c";
   char arg5[] = "/dev_hdd0/game/SSNE10000/USRDIR/ssnes.cfg";
   char *argv_[] = { arg1, arg2, arg3, arg4, arg5, NULL };
   return ssnes_main(sizeof(argv_) / sizeof(argv_[0]) - 1, argv_);
}

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <debug.h>
#include <xenos/xenos.h>
#include <diskio/dvd.h>
#include <diskio/ata.h>
#include <input/input.h>
#include <console/console.h>
#include <diskio/diskio.h>
#include <usb/usbmain.h>
#include <time/time.h>
#include <ppc/timebase.h>
#include <xenon_soc/xenon_power.h>
#include <elf/elf.h>
#include <dirent.h>

#define FG_COL -1
#define BG_COL 0

#define MAX_FILES 1000
#define STICK_THRESHOLD 25000
#define MAX_DISPLAYED_ENTRIES 20

#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))

struct dirent entries[MAX_FILES];
int entrycount=0;

void load_dir(char * path){
	DIR *d = opendir(path);

	entrycount=0;
	
	if (d){
		struct dirent *de;
		de=readdir(d);
		while(de){
			if(strcmp(de->d_name,".")){
				memcpy(&entries[entrycount],de,sizeof(struct dirent));
				++entrycount;
			}
			
			de=readdir(d);
		}
		closedir(d);
	}
}

void append_dir_to_path(char * path,char * dir){
  if (!strcmp(dir,"..")){
    int i=strlen(path);
    int delimcount=0;

    while(i>=0 && delimcount<2){
      if (path[i]=='/'){
        ++delimcount;

        if(delimcount>1){
          path[i+1]='\0';
        }
      }
      --i;
    }
  }else if (!strcmp(dir,".")){
    return;
  }else{
    strcat(path,dir);
    strcat(path,"/");
  }
}

int main(){
	const char * s;
	char path[256];
	
	int handle;
	struct controller_data_s pad;
	int pos=0,ppos=-1,start,count,i;

	xenos_init(VIDEO_MODE_AUTO);
	console_init();

	xenon_make_it_faster(XENON_SPEED_FULL);
	
	usb_init();
	usb_do_poll();
	
	xenon_ata_init();

	dvd_init();

	handle=-1;
    handle=bdev_enum(handle,&s);
    if(handle<0) return 0;

	strcpy(path,s);
	strcat(path,":/");	

	load_dir(path);
	
	for(;;){
 		usb_do_poll();		
 		get_controller_data(&pad, 0);
		
		if (pad.s1_y>STICK_THRESHOLD) --pos;
		if (pad.s1_y<-STICK_THRESHOLD) ++pos;
		
		if (entrycount && (pos<0 || pos>=entrycount)){
			pos=ppos;
			continue;
		}
		
		if (pad.logo) return 0;
		
		if (pad.a){
			if(entries[pos].d_type&DT_DIR){
				append_dir_to_path(path,entries[pos].d_name);
				load_dir(path);
				ppos=-1;
				pos=0;
			}else{
				char fn[256];
				strcpy(fn,path);
				strcat(fn,entries[pos].d_name);
				
				printf("%s\n",fn);

				if (strstr(entries[pos].d_name,".elf") || strstr(entries[pos].d_name,".elf32")) { // ugly
					elf_runFromDisk(fn);
				}else{
					FILE * f=fopen(fn,"rb");
					if(f){
						int size=1024*1024;
						int totred=0,red;

						void * buf=malloc(size);

						u64 beg=mftb();
						do{
							red=fread(buf,1,size,f);
							totred+=red;
							console_putch('.');
						}while(red==size);

						printf("\n%d bytes, %f KB/s\n",totred, (float)(totred/1024.0)/((float)(mftb()-beg)/PPC_TIMEBASE_FREQ));

						free(buf);

						fclose(f);
					}
				}
			}
		}
		
		if(pad.select){
			append_dir_to_path(path,"..");
			load_dir(path);
			ppos=-1;
			pos=0;
		}
		
		if(pad.b){
		    do{
				handle=bdev_enum(handle,&s);
			}while(handle<0);
			strcpy(path,s);
			strcat(path,":/");
			load_dir(path);
			ppos=-1;
			pos=0;
		}

		if (ppos==pos) continue;
		
		memset(&pad,0,sizeof(struct controller_data_s));
		
		console_set_colors(BG_COL,FG_COL);
		console_clrscr();
		printf("A: select, B: change disk, Back: parent dir, Logo: reload Xell\n\n%s\n\n",path);
		
		start=MAX(0,pos-MAX_DISPLAYED_ENTRIES/2);
		count=MIN(MAX_DISPLAYED_ENTRIES,entrycount-start);
		
		for(i=start;i<start+count;++i){
			struct dirent *de = &entries[i];

			if (i==pos){
				console_set_colors(FG_COL,BG_COL);
			}else{
				console_set_colors(BG_COL,FG_COL);
			}
			
			if (de->d_type&DT_DIR) console_putch('[');

			s=de->d_name;
			while(*s) console_putch(*s++);
			
			if (de->d_type&DT_DIR) console_putch(']');

			console_putch('\r');
			console_putch('\n');
		}
			
		ppos=pos;
		
		do{
	 		usb_do_poll();		
			get_controller_data(&pad, 0);
		}while(pad.a || pad.b || pad.select || pad.s1_y>STICK_THRESHOLD || pad.s1_y<-STICK_THRESHOLD);
	}
	
	
	return 0;
}
