import retroApi, { StartParams } from 'libretroarch.so';

enum CtlType{
  App_SHUTDOWN = 0
}

class GameXComponentController extends XComponentController{
  onSurfaceCreated(surfaceId: string): void {
    retroApi.surfaceChanged(parseInt(surfaceId), 0, 0)
  }
  onSurfaceChanged(surfaceId: string, rect: SurfaceRect): void {
    retroApi.surfaceChanged(parseInt(surfaceId), 800, 800)
  }
  onSurfaceDestroyed(surfaceId: string): void {
  }
}

@Entry
@Component
struct Index {
  xController = new GameXComponentController();
  aboutToAppear(): void {

  }
  build() {
    Row() {
      XComponent({id:"game", type: XComponentType.TEXTURE, controller: this.xController})
        .backgroundColor(Color.Black)
        .width('100%')
        .height('100%')
        .constraintSize({ maxWidth: '100%', maxHeight: '100%' })
        .onLoad(()=>{
          setTimeout(()=>{

          }, 1000)
        }).onTouch((e: TouchEvent)=>{

          retroApi.onTouchEvent({
            id: e.target.id ?? 0,
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
      })
    }
    .height('100%')
  }
  public vpToPx(vp: number){
    return this.getUIContext().vp2px(vp)
  }
  onBackPress(): boolean | void {
    // TODO 弹出提示退出
    retroApi.sendCtl(CtlType.App_SHUTDOWN)
    return true;
  }


}
