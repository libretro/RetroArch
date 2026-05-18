import retroApi, { StartParams } from 'libretroarch.so';
import { emitter } from '@kit.BasicServicesKit';
import { common, UIAbility } from '@kit.AbilityKit';
import { KeyCode } from '@kit.InputKit';

enum NativeEventId{
  EVENT_NATIVE_OPEN_KEYBOARD,
  EVENT_NATIVE_APP_SHUTDOWN
}
class GameXComponentController extends XComponentController{
  onSurfaceCreated(surfaceId: string): void {
    retroApi.surfaceChanged(parseInt(surfaceId), 0, 0)
  }
  onSurfaceChanged(surfaceId: string, rect: SurfaceRect): void {
    retroApi.surfaceChanged(parseInt(surfaceId), rect.surfaceWidth, rect.surfaceHeight)
  }
  onSurfaceDestroyed(surfaceId: string): void {
  }
}

@Entry
@ComponentV2
struct Index {

  xController = new GameXComponentController();
  @Local loading: boolean = true;
  @Local openKeyboard: boolean = false
  aboutToAppear(): void {
    emitter.on({eventId: 100000}, ()=>{
        this.loading = false
    })
    retroApi.onNativeEvent((eventId: number, value: number)=>{
      if(eventId == NativeEventId.EVENT_NATIVE_APP_SHUTDOWN){
        (this.getUIContext().getHostContext() as common.UIAbilityContext)?.terminateSelf();
      }
      if(eventId == NativeEventId.EVENT_NATIVE_OPEN_KEYBOARD){
        this.openKeyboard = true;
      }
    })
  }
  build() {
    Stack() {
      XComponent({id:"game", type: XComponentType.TEXTURE, controller: this.xController})
        .backgroundColor(Color.Black)
        .width('100%')
        .height('100%')
        .constraintSize({ maxWidth: '100%', maxHeight: '100%' })
        .onLoad(()=>{ }).onTouch((e: TouchEvent)=>{
          retroApi.onTouchEvent({
            id: e.touches[e.touches.length - 1]?.id ?? 0,
            type: e.type,
            timestamp: e.timestamp,
            touches: e.touches.map((t)=>{
              return {
                id: t.id,
                x: this.vpToPx(t.x),
                y: this.vpToPx(t.y)
              } as TouchObject
            })
          })
      }).defaultFocus(true).focusable(true).onKeyEvent((e: KeyEvent)=>{
        retroApi.onKeyEvent(e)
      })
      if(this.loading){
        Row () {
          Text("加载资源中").fontColor(Color.White)
        }
      }
    }.alignContent(Alignment.Center)
    .height('100%')
  }
  public vpToPx(vp: number){
    return this.getUIContext().vp2px(vp)
  }
  onBackPress(): boolean | void {
    // TODO 弹出提示退出
    if(this.openKeyboard){
      this.openKeyboard = false
      this.SendKeyEvent(KeyCode.KEYCODE_ENTER)
    }else{
      this.SendKeyEvent(KeyCode.KEYCODE_DEL)
    }

    return true;
  }

  private SendKeyEvent(keycode: number){
    retroApi.onKeyEvent({
      type: 0,
      keyCode: keycode,
      source: KeySource.Keyboard,
    })
    setTimeout(()=>{
      retroApi.onKeyEvent({
        type: 1,
        keyCode: keycode,
        source: KeySource.Keyboard,
      })
    }, 100)
  }

}
