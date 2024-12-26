//
//  CocoaView+MouseSupport.swift
//  RetroArchiOS
//
//  Created by Yoshi Sugawara on 3/2/22.
//  Copyright © 2022 RetroArch. All rights reserved.
//

extension CocoaView {
   @available(iOS 13, *)
   @objc func setupMouseSupport() {
      mouseHandler = EmulatorTouchMouseHandler(view: view, delegate: self as? EmulatorTouchMouseHandlerDelegate)
   }
   
   open override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
      mouseHandler?.touchesBegan(touches: touches, event: event)
   }
   
   open override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
      mouseHandler?.touchesMoved(touches: touches)
   }
   
   open override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
      mouseHandler?.touchesCancelled(touches: touches, event: event)
   }
   
   open override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
      mouseHandler?.touchesEnded(touches: touches, event: event)
   }
   
}
