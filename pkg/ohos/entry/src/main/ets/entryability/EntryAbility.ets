import { AbilityConstant, ConfigurationConstant, UIAbility, Want } from '@kit.AbilityKit';
import retroApi, { StartParams } from 'libretroarch.so';

import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { fileIo, fileUri, picker } from '@kit.CoreFileKit';
import zlib from '@ohos.zlib';
import { emitter } from '@kit.BasicServicesKit';

const DOMAIN = 0x0000;


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


export default class EntryAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    try {
      this.context.getApplicationContext().setColorMode(ConfigurationConstant.ColorMode.COLOR_MODE_NOT_SET);
    } catch (err) {
      hilog.error(DOMAIN, 'testTag', 'Failed to set colorMode. Cause: %{public}s', JSON.stringify(err));
    }
    hilog.info(DOMAIN, 'testTag', '%{public}s', 'Ability onCreate');
  }

  onDestroy(): void {
    hilog.info(DOMAIN, 'testTag', '%{public}s', 'Ability onDestroy');
  }

  async onWindowStageCreate(windowStage: window.WindowStage): Promise<void> {
    // Main window is created, set main page for this ability
    hilog.info(DOMAIN, 'testTag', '%{public}s', 'Ability onWindowStageCreate');
    windowStage.getMainWindowSync().setPreferredOrientation(window.Orientation.AUTO_ROTATION)
    windowStage.getMainWindowSync().setWindowSystemBarEnable([])
    windowStage.loadContent('pages/Index', async (err) => {
      await this.StartApp();
      if (err.code) {
        hilog.error(DOMAIN, 'testTag', 'Failed to load the content. Cause: %{public}s', JSON.stringify(err));
        return;
      }
      hilog.info(DOMAIN, 'testTag', 'Succeeded in loading the content.');
    });
  }
  async StartApp(){
    let dataDir = (await initDownloadDir(this.context) ?? this.context.filesDir) + "/RetroArch"
    let configPath = dataDir + "/retroarch.cfg";
    try {
      if (!await fileIo.access(dataDir))
        await fileIo.mkdir(dataDir)
      if (!await fileIo.access(configPath, fileIo.AccessModeType.EXIST)){
        let file = await fileIo.open(configPath, fileIo.OpenMode.CREATE);
        await fileIo.close(file.fd);
      }
      await this.initAssets(dataDir)
    }catch (e) {
      console.error("testTag", e)
    }
    let coresDir =  this.context.bundleCodeDir + "/libs/arm64"

    emitter.emit({eventId:100000});
    retroApi.startApp({
      CONFIGFILE: configPath,
      IME:"",
      DATADIR: dataDir,
      LIBRETRO: coresDir,
      ROM: "",
    } as StartParams);
  }
  async initAssets(dataDir: string){
    await this.installAssets(dataDir, "assets");
    await this.installAssets(dataDir, "overlays");
  }

  async installAssets(dataDir: string, name: string){
    let assetsDir = dataDir +"/" + name;
    if(!await fileIo.access(assetsDir)){
      await fileIo.mkdir(assetsDir)
    }
    let assetsCount = (await fileIo.listFile(assetsDir)).length;
    if (assetsCount == 0) {
      await zlib.decompressFile(this.context.resourceDir + `/${name}.zip`, assetsDir);
    }
  }

  onWindowStageDestroy(): void {
    // Main window is destroyed, release UI related resources
    hilog.info(DOMAIN, 'testTag', '%{public}s', 'Ability onWindowStageDestroy');
  }

  onForeground(): void {
    // Ability has brought to foreground
    hilog.info(DOMAIN, 'testTag', '%{public}s', 'Ability onForeground');
  }

  onBackground(): void {
    // Ability has back to background
    hilog.info(DOMAIN, 'testTag', '%{public}s', 'Ability onBackground');
  }
}
