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
   var leftKeyboardModel: EmulatorKeyboardViewModel {
      return EmulatorKeyboardViewModel(keys: [
         [
            EmulatorKeyboardKey(label: "esc", code: Int(RETROK_ESCAPE.rawValue)),
            EmulatorKeyboardKey(label: "tab", code: Int(RETROK_TAB.rawValue))
         ],
         [
            EmulatorKeyboardKey(label: "q", code: Int(RETROK_q.rawValue)),
            EmulatorKeyboardKey(label: "w", code: Int(RETROK_w.rawValue)),
            EmulatorKeyboardKey(label: "e", code: Int(RETROK_e.rawValue)),
            EmulatorKeyboardKey(label: "r", code: Int(RETROK_r.rawValue)),
            EmulatorKeyboardKey(label: "t", code: Int(RETROK_t.rawValue)),
         ],
         [
            EmulatorKeyboardKey(label: "a", code: Int(RETROK_a.rawValue)),
            EmulatorKeyboardKey(label: "s", code: Int(RETROK_s.rawValue)),
            EmulatorKeyboardKey(label: "d", code: Int(RETROK_d.rawValue)),
            EmulatorKeyboardKey(label: "f", code: Int(RETROK_f.rawValue)),
            EmulatorKeyboardKey(label: "g", code: Int(RETROK_g.rawValue)),
         ],
         [
            EmulatorKeyboardKey(label: "z", code: Int(RETROK_z.rawValue)),
            EmulatorKeyboardKey(label: "x", code: Int(RETROK_x.rawValue)),
            EmulatorKeyboardKey(label: "c", code: Int(RETROK_c.rawValue)),
            EmulatorKeyboardKey(label: "v", code: Int(RETROK_v.rawValue)),
            EmulatorKeyboardKey(label: "b", code: Int(RETROK_b.rawValue)),
         ],
         [
            EmulatorKeyboardKey(label: "SHIFT", code: Int(RETROK_LSHIFT.rawValue), keySize: .standard, isModifier: false, imageName: "shift", imageNameHighlighted: "shift.fill"),
            EmulatorKeyboardKey(label: "123", code: 9000, keySize: .standard, imageName: "textformat.123"),
            EmulatorKeyboardKey(label: "Alt", code: Int(RETROK_LALT.rawValue)),
            EmulatorKeyboardKey(label: "Space", code: Int(RETROK_SPACE.rawValue), keySize: .wide)
         ]
      ],
      alternateKeys: [
         [
            SliderKey(keySize: .standard)
         ],
         [
            EmulatorKeyboardKey(label: "1", code: Int(RETROK_1.rawValue)),
            EmulatorKeyboardKey(label: "2", code: Int(RETROK_2.rawValue)),
            EmulatorKeyboardKey(label: "3", code: Int(RETROK_3.rawValue)),
            EmulatorKeyboardKey(label: "4", code: Int(RETROK_4.rawValue)),
            EmulatorKeyboardKey(label: "5", code: Int(RETROK_5.rawValue)),
         ],
         [
            EmulatorKeyboardKey(label: "-", code: Int(RETROK_MINUS.rawValue)),
            EmulatorKeyboardKey(label: "=", code: Int(RETROK_EQUALS.rawValue)),
            EmulatorKeyboardKey(label: "/", code: Int(RETROK_SLASH.rawValue)),
            EmulatorKeyboardKey(label: "[", code: Int(RETROK_LEFTBRACKET.rawValue)),
            EmulatorKeyboardKey(label: "]", code: Int(RETROK_RIGHTBRACKET.rawValue)),
         ],
         [
            EmulatorKeyboardKey(label: ";", code: Int(RETROK_SEMICOLON.rawValue)),
            EmulatorKeyboardKey(label: "~", code: Int(RETROK_TILDE.rawValue)),
            EmulatorKeyboardKey(label: ":", code: Int(RETROK_COLON.rawValue)),
            EmulatorKeyboardKey(label: "?", code: Int(RETROK_QUESTION.rawValue)),
            EmulatorKeyboardKey(label: "!", code: Int(RETROK_EXCLAIM.rawValue)),
         ],
         [
            EmulatorKeyboardKey(label: "ABC", code: 9000, keySize: .standard, imageName: "textformat.123")
         ]
      ])
   }
   
   var rightKeyboardModel: EmulatorKeyboardViewModel {
      EmulatorKeyboardViewModel(keys: [
         [
            
         ],
         [
            EmulatorKeyboardKey(label: "y", code: Int(RETROK_y.rawValue)),
            EmulatorKeyboardKey(label: "u", code: Int(RETROK_u.rawValue)),
            EmulatorKeyboardKey(label: "i", code: Int(RETROK_i.rawValue)),
            EmulatorKeyboardKey(label: "o", code: Int(RETROK_o.rawValue)),
            EmulatorKeyboardKey(label: "p", code: Int(RETROK_p.rawValue)),
         ],
         [
            EmulatorKeyboardKey(label: "h", code: Int(RETROK_h.rawValue)),
            EmulatorKeyboardKey(label: "j", code: Int(RETROK_j.rawValue)),
            EmulatorKeyboardKey(label: "k", code: Int(RETROK_k.rawValue)),
            EmulatorKeyboardKey(label: "l", code: Int(RETROK_l.rawValue)),
            EmulatorKeyboardKey(label: "'", code: Int(RETROK_QUOTE.rawValue))
         ],
         [
            EmulatorKeyboardKey(label: "n", code: Int(RETROK_n.rawValue)),
            EmulatorKeyboardKey(label: "m", code: Int(RETROK_m.rawValue)),
            EmulatorKeyboardKey(label: ",", code: Int(RETROK_COMMA.rawValue)),
            EmulatorKeyboardKey(label: ".", code: Int(RETROK_PERIOD.rawValue)),
            EmulatorKeyboardKey(label: "BKSPC", code: Int(RETROK_BACKSPACE.rawValue))
         ],
         [
            EmulatorKeyboardKey(label: "ALT", code: Int(RETROK_RALT.rawValue)),
            EmulatorKeyboardKey(label: "RETURN", code: Int(RETROK_RETURN.rawValue), keySize: .wide)
         ],
      ],
      alternateKeys: [
         [
            EmulatorKeyboardKey(label: "6", code: Int(RETROK_6.rawValue)),
            EmulatorKeyboardKey(label: "7", code: Int(RETROK_7.rawValue)),
            EmulatorKeyboardKey(label: "8", code: Int(RETROK_8.rawValue)),
            EmulatorKeyboardKey(label: "9", code: Int(RETROK_9.rawValue)),
            EmulatorKeyboardKey(label: "0", code: Int(RETROK_0.rawValue))
         ],
         [
            EmulatorKeyboardKey(label: "PAGEUP", code: Int(RETROK_PAGEUP.rawValue)),
            EmulatorKeyboardKey(label: "HOME", code: Int(RETROK_HOME.rawValue)),
            EmulatorKeyboardKey(label: "⬆️", code: Int(RETROK_UP.rawValue)),
            EmulatorKeyboardKey(label: "END", code: Int(RETROK_END.rawValue)),
            EmulatorKeyboardKey(label: "PAGEDWN", code: Int(RETROK_PAGEDOWN.rawValue)),
         ],
         [
            SpacerKey(),
            EmulatorKeyboardKey(label: "⬅️", code: Int(RETROK_LEFT.rawValue)),
            EmulatorKeyboardKey(label: "⬇️", code: Int(RETROK_DOWN.rawValue)),
            EmulatorKeyboardKey(label: "➡️", code: Int(RETROK_RIGHT.rawValue)),
            SpacerKey()
         ],
         [
            EmulatorKeyboardKey(label: "SPACE", code: Int(RETROK_SPACE.rawValue), keySize: .wide),
            EmulatorKeyboardKey(label: "DEL", code: Int(RETROK_DELETE.rawValue)),
         ],
         [
            EmulatorKeyboardKey(label: "RETURN", code: Int(RETROK_RETURN.rawValue), keySize: .wide)
         ]
      ])
   }
   
   @objc func setupEmulatorKeyboard() {
      keyboardController = EmulatorKeyboardController(leftKeyboardModel: leftKeyboardModel, rightKeyboardModel: rightKeyboardModel)
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
