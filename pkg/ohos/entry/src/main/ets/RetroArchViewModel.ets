import { fileIo, fileUri, picker } from '@kit.CoreFileKit';
import retroApi, { StartParams } from 'libretroarch.so';
import zlib from '@ohos.zlib';
import { common } from '@kit.AbilityKit';
import { displaySync } from '@kit.ArkGraphics2D';
import { display } from '@kit.ArkUI';
import window from '@ohos.window';

async function initDownloadDir(context: Context) : Promise<string | undefined>{
  try {
    const documentSaveOptions = new picker.DocumentSaveOptions();
    documentSaveOptions.pickerMode = picker.DocumentPickerMode.DOWNLOAD;
    const documentViewPicker = new picker.DocumentViewPicker(context);
    const path = await documentViewPicker.save(documentSaveOptions)
    const dir = new fileUri.FileUri(path[0]).path;
    return dir;
  } catch (e) {

  }
  return undefined;
}

enum NativeEventId{
  EVENT_NATIVE_OPEN_KEYBOARD,
  EVENT_NATIVE_APP_SHUTDOWN,
  EVENT_NATIVE_VIDEO_NOTCH_WRITE_OVER_ENABLE,
  EVENT_NATIVE_SET_SCREEN_ORIENTATION
}

@ObservedV2
export class RetroArchViewModel{
  context?: common.UIAbilityContext

  @Trace loading: boolean = true;
  @Trace openKeyboard: boolean = false;
  @Trace fullScreen: boolean = false;
  constructor() {
  }
  stopApp(){
    retroApi.stopApp()
  }
  init(context: common.UIAbilityContext){
    this.context = context;
    retroApi.onNativeEvent((eventId: number, value: number)=>{
      if(eventId == NativeEventId.EVENT_NATIVE_APP_SHUTDOWN){
        this.context?.terminateSelf();
      }
      if(eventId == NativeEventId.EVENT_NATIVE_OPEN_KEYBOARD){
        this.openKeyboard = true;
      }
      if(eventId == NativeEventId.EVENT_NATIVE_VIDEO_NOTCH_WRITE_OVER_ENABLE){
        this.fullScreen = value == 1;
      }
      if(eventId == NativeEventId.EVENT_NATIVE_VIDEO_NOTCH_WRITE_OVER_ENABLE){
        this.fullScreen = value == 1;
      }
      if(eventId == NativeEventId.EVENT_NATIVE_SET_SCREEN_ORIENTATION){
        if(value == 0){
          this.context?.windowStage.getMainWindowSync().setPreferredOrientation(window.Orientation.AUTO_ROTATION)
        }
        if(value == 1){
          this.context?.windowStage.getMainWindowSync().setPreferredOrientation(window.Orientation.LANDSCAPE)
        }
        if(value == 2){
          this.context?.windowStage.getMainWindowSync().setPreferredOrientation(window.Orientation.LANDSCAPE_INVERTED)
        }
        if(value == 3){
          this.context?.windowStage.getMainWindowSync().setPreferredOrientation(window.Orientation.PORTRAIT_INVERTED)
        }
      }
    })
  }
  async StartApp(){
    let dataDir = (await initDownloadDir(this.context!) ?? this.context!.filesDir) + "/RetroArch"
    let configPath = dataDir + "/retroarch.cfg";
    try {
      if (!await fileIo.access(dataDir))
        await fileIo.mkdir(dataDir)
      if (!await fileIo.access(configPath, fileIo.AccessModeType.EXIST)){
        let file = await fileIo.open(configPath, fileIo.OpenMode.CREATE);
        await fileIo.close(file.fd);
      }
      this.loading = true
      await this.initAssets(dataDir)
      this.loading = false;
    }catch (e) {
      console.error("testTag", e)
    }
    let coresDir =  this.context!.bundleCodeDir + "/libs/arm64/cores"
    let dpi = display.getDefaultDisplaySync().densityDPI
    retroApi.startApp({
      CONFIGFILE: configPath,
      IME:"",
      DATADIR: dataDir,
      LIBRETRO: coresDir,
      ROM: "",
      Dpi: dpi,
    } as StartParams);

  }
  async initAssets(dataDir: string){
    await this.installAssets(dataDir, "assets");
    await this.installAssets(dataDir, "overlays");
    await this.installAssets(dataDir, "info");
  }

  async installAssets(dataDir: string, name: string){
    let assetsDir = dataDir +"/" + name;
    if(!await fileIo.access(assetsDir)){
      await fileIo.mkdir(assetsDir)
    }
    let assetsCount = (await fileIo.listFile(assetsDir)).length;
    if (assetsCount == 0) {
      await zlib.decompressFile(this.context!.resourceDir + `/${name}.zip`, assetsDir);
    }
  }
}
export const retroArchVM =  new RetroArchViewModel()
