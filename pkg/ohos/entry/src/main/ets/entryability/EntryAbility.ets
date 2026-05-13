import { AbilityConstant, ConfigurationConstant, UIAbility, Want } from '@kit.AbilityKit';
import retroApi, { StartParams } from 'libretroarch.so';


import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { fileIo } from '@kit.CoreFileKit';

const DOMAIN = 0x0000;


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
    windowStage.getMainWindowSync().setPreferredOrientation(window.Orientation.AUTO_ROTATION_LANDSCAPE_RESTRICTED)
    windowStage.getMainWindowSync().setWindowSystemBarEnable([])
    await this.StartApp();
    windowStage.loadContent('pages/Index', (err) => {
      if (err.code) {
        hilog.error(DOMAIN, 'testTag', 'Failed to load the content. Cause: %{public}s', JSON.stringify(err));
        return;
      }
      hilog.info(DOMAIN, 'testTag', 'Succeeded in loading the content.');
    });
  }
  async StartApp(){
    let dataDir = this.context?.filesDir + "/RetroArch";
    let configPath = dataDir + "/retroarch.cfg";
    try {
      if(!await fileIo.access(this.context?.filesDir + "/RetroArch")){
        await fileIo.mkdir(this.context?.filesDir + "/RetroArch")
      }
      if(!await fileIo.access(configPath, fileIo.AccessModeType.EXIST)){

        let file = await fileIo.open(configPath, fileIo.OpenMode.CREATE);
        await fileIo.close(file.fd);
      }
    }catch (e) {
      console.error("testTag", e)
    }
    retroApi.startApp({
      CONFIGFILE: configPath,
      IME:"",
      DATADIR: dataDir,
      LIBRETRO: dataDir + "/cores",
      ROM: "",
    } as StartParams);
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
