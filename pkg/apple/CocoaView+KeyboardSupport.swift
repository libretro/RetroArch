//
//  CocoaView+KeyboardSupport.swift
//  RetroArchiOS11
//
//  Created by Yoshi Sugawara on 2/25/21.
//  Copyright © 2021 RetroArch. All rights reserved.
//

import Foundation

extension CocoaView: EmulatorKeyboardKeyPressedDelegate {
   func updateTransparency(toAlpha alpha: CGFloat) {
      // todo
   }
   
   func keyUp(_ key: KeyCoded) {
      input_keyboard_event(false, UInt32(key.keyCode), 0, 0, UInt32(RETRO_DEVICE_KEYBOARD))
   }
   
   func keyDown(_ key: KeyCoded) {
      input_keyboard_event(true, UInt32(key.keyCode), 0, 0, UInt32(RETRO_DEVICE_KEYBOARD))
   }
}

extension CocoaView {
   @objc func setupEmulatorKeyboard() {
      keyboardController = EmulatorKeyboardController()
      addChild(keyboardController)
      keyboardController.didMove(toParent: self)
      keyboardController.view.translatesAutoresizingMaskIntoConstraints = false
      view.addSubview(keyboardController.view)
      keyboardController.view.trailingAnchor.constraint(equalTo: view.trailingAnchor).isActive = true
      keyboardController.view.leadingAnchor.constraint(equalTo: view.leadingAnchor).isActive = true
      keyboardController.view.topAnchor.constraint(equalTo: view.topAnchor).isActive = true
      keyboardController.view.bottomAnchor.constraint(equalTo: view.bottomAnchor).isActive = true
      keyboardController.leftKeyboardModel.delegate = self
      keyboardController.rightKeyboardModel.delegate = self
      keyboardController.view.isHidden = true
   }
}
