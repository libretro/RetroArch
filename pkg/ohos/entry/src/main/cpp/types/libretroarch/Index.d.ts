
export interface StartParams{
  CONFIGFILE: string;
  DATADIR: string;
  IME: string;
  EXTERNAL: string;
  ROM: string;
  LIBRETRO: string;
  Lang: string;
  Dpi: number;
}

export const startApp: (args: StartParams) => number;
export const stopApp: () => number;
export const surfaceChanged: (surfaceId: number, width:number, height:number)=>number;
export const onTouchEvent: (touch: any)=>number;
export const onKeyEvent: (touch: any)=>number;
export const onNativeEvent: (callback: (eventId: number, value: number)=>void)=> void;

export const sendCtl: (ctl: number) => number;
