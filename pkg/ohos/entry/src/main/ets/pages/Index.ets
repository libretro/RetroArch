import retroApi, { StartParams } from 'libretroarch.so';
import { KeyCode } from '@kit.InputKit';
import { RetroArchViewModel, retroArchVM } from '../RetroArchViewModel';
import window from '@ohos.window';

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
  private xController = new GameXComponentController();
  @Local viewmodel: RetroArchViewModel = retroArchVM;
  aboutToAppear(): void {
  }
  GetAspectRatio(): number{
    if(this.viewmodel.currentRotation == window.Orientation.PORTRAIT || this.viewmodel.currentRotation == window.Orientation.PORTRAIT_INVERTED){
      return 16 / 9
    }
    else
      return 8 / 16
  }

  build() {
    Stack() {
      XComponent({id:"game", type: XComponentType.TEXTURE, controller: this.xController})
        .backgroundColor(Color.Black)
        .width('100%')
        .height('100%')
        .constraintSize({ maxWidth: '100%', maxHeight: '100%' })
        .aspectRatio(!this.viewmodel.fullScreen ? this.GetAspectRatio(): undefined)
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
      if(this.viewmodel.loading){
        Row () {
          Text("加载资源中").fontColor(Color.White)
        }
      }
    }.alignContent(Alignment.Center)
    .height('100%')
    .width('100%')
  }
  public vpToPx(vp: number){
    return this.getUIContext().vp2px(vp)
  }
  onBackPress(): boolean | void {
    // TODO 弹出提示退出
    if(this.viewmodel.openKeyboard){
      this.viewmodel.openKeyboard = false
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
