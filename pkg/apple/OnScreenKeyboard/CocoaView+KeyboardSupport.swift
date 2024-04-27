//
//  CocoaView+KeyboardSupport.swift
//  RetroArchiOS11
//
//  Created by Yoshi Sugawara on 2/25/21.
//  Copyright © 2021 RetroArch. All rights reserved.
//

import Foundation

extension CocoaView {
   var leftKeyboardModel: EmulatorKeyboardViewModel {
      return EmulatorKeyboardViewModel(keys: [
         [
            EmulatorKeyboardKey(label: "1", code: Int(RETROK_1.rawValue)),
            EmulatorKeyboardKey(label: "2", code: Int(RETROK_2.rawValue)),
            EmulatorKeyboardKey(label: "3", code: Int(RETROK_3.rawValue)),
            EmulatorKeyboardKey(label: "4", code: Int(RETROK_4.rawValue)),
            EmulatorKeyboardKey(label: "5", code: Int(RETROK_5.rawValue)),
         ],
         [
            EmulatorKeyboardKey(label: "Q", code: Int(RETROK_q.rawValue)),
            EmulatorKeyboardKey(label: "W", code: Int(RETROK_w.rawValue)),
            EmulatorKeyboardKey(label: "E", code: Int(RETROK_e.rawValue)),
            EmulatorKeyboardKey(label: "R", code: Int(RETROK_r.rawValue)),
            EmulatorKeyboardKey(label: "T", code: Int(RETROK_t.rawValue)),
         ],
         [
            EmulatorKeyboardKey(label: "A", code: Int(RETROK_a.rawValue)),
            EmulatorKeyboardKey(label: "S", code: Int(RETROK_s.rawValue)),
            EmulatorKeyboardKey(label: "D", code: Int(RETROK_d.rawValue)),
            EmulatorKeyboardKey(label: "F", code: Int(RETROK_f.rawValue)),
            EmulatorKeyboardKey(label: "G", code: Int(RETROK_g.rawValue)),
         ],
         [
            EmulatorKeyboardKey(label: "Z", code: Int(RETROK_z.rawValue)),
            EmulatorKeyboardKey(label: "X", code: Int(RETROK_x.rawValue)),
            EmulatorKeyboardKey(label: "C", code: Int(RETROK_c.rawValue)),
            EmulatorKeyboardKey(label: "V", code: Int(RETROK_v.rawValue)),
            EmulatorKeyboardKey(label: "B", code: Int(RETROK_b.rawValue)),
         ],
         [
            EmulatorKeyboardKey(label: "SHIFT", code: Int(RETROK_LSHIFT.rawValue), keySize: .standard, isModifier: true, imageName: "shift", imageNameHighlighted: "shift.fill"),
            EmulatorKeyboardKey(label: "Fn", code: 9000, keySize: .standard, imageName: "fn"),
            EmulatorKeyboardKey(label: "CTRL", code: Int(RETROK_LCTRL.rawValue), isModifier: true, imageName: "control"),
            EmulatorKeyboardKey(label: "Space", code: Int(RETROK_SPACE.rawValue), keySize: .wide)
         ]
      ],
      alternateKeys: [
         [
            EmulatorKeyboardKey(label: "ESC", code: Int(RETROK_ESCAPE.rawValue), imageName: "escape"),
            SliderKey(keySize: .standard)
         ],
         [
            EmulatorKeyboardKey(label: "F1", code: Int(RETROK_F1.rawValue)),
            EmulatorKeyboardKey(label: "F2", code: Int(RETROK_F2.rawValue)),
            EmulatorKeyboardKey(label: "F3", code: Int(RETROK_F3.rawValue)),
            EmulatorKeyboardKey(label: "F4", code: Int(RETROK_F4.rawValue)),
            EmulatorKeyboardKey(label: "F5", code: Int(RETROK_F5.rawValue)),
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
            EmulatorKeyboardKey(label: "SHIFT", code: Int(RETROK_LSHIFT.rawValue), keySize: .standard, isModifier: true, imageName: "shift", imageNameHighlighted: "shift.fill"),
            EmulatorKeyboardKey(label: "Fn", code: 9000, keySize: .standard, imageName: "fn"),
            EmulatorKeyboardKey(label: "CTRL", code: Int(RETROK_LCTRL.rawValue), isModifier: true, imageName: "control"),
            EmulatorKeyboardKey(label: "Space", code: Int(RETROK_SPACE.rawValue), keySize: .wide)
         ]
      ])
   }
   
   var rightKeyboardModel: EmulatorKeyboardViewModel {
      EmulatorKeyboardViewModel(keys: [
         [
            EmulatorKeyboardKey(label: "6", code: Int(RETROK_6.rawValue)),
            EmulatorKeyboardKey(label: "7", code: Int(RETROK_7.rawValue)),
            EmulatorKeyboardKey(label: "8", code: Int(RETROK_8.rawValue)),
            EmulatorKeyboardKey(label: "9", code: Int(RETROK_9.rawValue)),
            EmulatorKeyboardKey(label: "0", code: Int(RETROK_0.rawValue))
         ],
         [
            EmulatorKeyboardKey(label: "Y", code: Int(RETROK_y.rawValue)),
            EmulatorKeyboardKey(label: "U", code: Int(RETROK_u.rawValue)),
            EmulatorKeyboardKey(label: "I", code: Int(RETROK_i.rawValue)),
            EmulatorKeyboardKey(label: "O", code: Int(RETROK_o.rawValue)),
            EmulatorKeyboardKey(label: "P", code: Int(RETROK_p.rawValue)),
         ],
         [
            EmulatorKeyboardKey(label: "H", code: Int(RETROK_h.rawValue)),
            EmulatorKeyboardKey(label: "J", code: Int(RETROK_j.rawValue)),
            EmulatorKeyboardKey(label: "K", code: Int(RETROK_k.rawValue)),
            EmulatorKeyboardKey(label: "L", code: Int(RETROK_l.rawValue)),
            EmulatorKeyboardKey(label: "'", code: Int(RETROK_QUOTE.rawValue))
         ],
         [
            EmulatorKeyboardKey(label: "N", code: Int(RETROK_n.rawValue)),
            EmulatorKeyboardKey(label: "M", code: Int(RETROK_m.rawValue)),
            EmulatorKeyboardKey(label: ",", code: Int(RETROK_COMMA.rawValue)),
            EmulatorKeyboardKey(label: ".", code: Int(RETROK_PERIOD.rawValue)),
            EmulatorKeyboardKey(label: "BKSPC", code: Int(RETROK_BACKSPACE.rawValue), imageName: "delete.left", imageNameHighlighted: "delete.left.fill")
         ],
         [
            EmulatorKeyboardKey(label: "Alt", code: Int(RETROK_LALT.rawValue), isModifier: true, imageName: "alt"),
            EmulatorKeyboardKey(label: "tab", code: Int(RETROK_TAB.rawValue), imageName: "arrow.right.to.line"),
            EmulatorKeyboardKey(label: "RETURN", code: Int(RETROK_RETURN.rawValue), keySize: .wide)
         ],
      ],
      alternateKeys: [
         [
            EmulatorKeyboardKey(label: "F6", code: Int(RETROK_F6.rawValue)),
            EmulatorKeyboardKey(label: "F7", code: Int(RETROK_F7.rawValue)),
            EmulatorKeyboardKey(label: "F8", code: Int(RETROK_F8.rawValue)),
            EmulatorKeyboardKey(label: "F9", code: Int(RETROK_F9.rawValue)),
            EmulatorKeyboardKey(label: "F10", code: Int(RETROK_F10.rawValue)),
         ],
         [
            EmulatorKeyboardKey(label: "PAGEUP", code: Int(RETROK_PAGEUP.rawValue), imageName: "arrow.up.doc"),
            EmulatorKeyboardKey(label: "HOME", code: Int(RETROK_HOME.rawValue), imageName: "house"),
            EmulatorKeyboardKey(label: "INS", code: Int(RETROK_INSERT.rawValue), imageName: "text.insert"),
            EmulatorKeyboardKey(label: "END", code: Int(RETROK_END.rawValue)),
            EmulatorKeyboardKey(label: "PAGEDWN", code: Int(RETROK_PAGEDOWN.rawValue), imageName: "arrow.down.doc"),
         ],
         [
            EmulatorKeyboardKey(label: "F11", code: Int(RETROK_F11.rawValue)),
            EmulatorKeyboardKey(label: "⬆️", code: Int(RETROK_UP.rawValue), imageName: "arrow.up"),
            SpacerKey(),
            SpacerKey(),
            EmulatorKeyboardKey(label: "F12", code: Int(RETROK_F12.rawValue)),
         ],
         [
            EmulatorKeyboardKey(label: "⬅️", code: Int(RETROK_LEFT.rawValue), imageName: "arrow.left"),
            EmulatorKeyboardKey(label: "⬇️", code: Int(RETROK_DOWN.rawValue), imageName: "arrow.down"),
            EmulatorKeyboardKey(label: "➡️", code: Int(RETROK_RIGHT.rawValue), imageName: "arrow.right"),
            SpacerKey(),
            EmulatorKeyboardKey(label: "DEL", code: Int(RETROK_DELETE.rawValue), imageName: "clear", imageNameHighlighted: "clear.fill"),
         ],
         [
            EmulatorKeyboardKey(label: "RETURN", code: Int(RETROK_RETURN.rawValue), keySize: .wide)
         ]
      ])
   }
   
   @objc func setupEmulatorKeyboard() {
      keyboardController = EmulatorKeyboardController(leftKeyboardModel: leftKeyboardModel, rightKeyboardModel: rightKeyboardModel)
      keyboardController.leftKeyboardModel.delegate = self;
      keyboardController.rightKeyboardModel.delegate = self;
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
      keyboardController.leftKeyboardModel.modifierDelegate = self
      keyboardController.rightKeyboardModel.modifierDelegate = self
      keyboardController.view.isHidden = true
      keyboardModifierState = 0
   }   
}

extension CocoaView: EmulatorKeyboardKeyPressedDelegate {
   func keyUp(_ key: KeyCoded) {
      print("keyUp: code=\(key.keyCode) keyboardModifierState = \(keyboardModifierState)")
      apple_direct_input_keyboard_event(false, UInt32(key.keyCode), 0, keyboardModifierState, UInt32(RETRO_DEVICE_KEYBOARD))
   }

   func keyDown(_ key: KeyCoded) {
      print("keyDown: code=\(key.keyCode) keyboardModifierState = \(keyboardModifierState)")
      apple_direct_input_keyboard_event(true, UInt32(key.keyCode), 0, keyboardModifierState, UInt32(RETRO_DEVICE_KEYBOARD))
   }
}

extension CocoaView: EmulatorKeyboardModifierPressedDelegate {
   func modifierPressedWithKey(_ key: KeyCoded, enable: Bool) {
      switch UInt32(key.keyCode) {
      case RETROK_LSHIFT.rawValue:
         if enable {
            keyboardModifierState |= RETROKMOD_SHIFT.rawValue
            apple_direct_input_keyboard_event(true, UInt32(RETROK_LSHIFT.rawValue), 0, keyboardModifierState, UInt32(RETRO_DEVICE_KEYBOARD))
         } else {
            keyboardModifierState &= ~RETROKMOD_SHIFT.rawValue
            apple_direct_input_keyboard_event(false, UInt32(RETROK_LSHIFT.rawValue), 0, keyboardModifierState, UInt32(RETRO_DEVICE_KEYBOARD))
         }
      case RETROK_LCTRL.rawValue:
         if enable {
            keyboardModifierState |= RETROKMOD_CTRL.rawValue
            apple_direct_input_keyboard_event(true, UInt32(RETROK_LCTRL.rawValue), 0, keyboardModifierState, UInt32(RETRO_DEVICE_KEYBOARD))
         } else {
            keyboardModifierState &= ~RETROKMOD_CTRL.rawValue
            apple_direct_input_keyboard_event(false, UInt32(RETROK_LCTRL.rawValue), 0, keyboardModifierState, UInt32(RETRO_DEVICE_KEYBOARD))
         }
      case RETROK_LALT.rawValue:
         if enable {
            keyboardModifierState |= RETROKMOD_ALT.rawValue
            apple_direct_input_keyboard_event(true, UInt32(RETROK_LALT.rawValue), 0, keyboardModifierState, UInt32(RETRO_DEVICE_KEYBOARD))
         } else {
            keyboardModifierState &= ~RETROKMOD_ALT.rawValue
            apple_direct_input_keyboard_event(false, UInt32(RETROK_LALT.rawValue), 0, keyboardModifierState, UInt32(RETRO_DEVICE_KEYBOARD))
         }
      default:
         break
      }
   }
   
   func isModifierEnabled(key: KeyCoded) -> Bool {
      switch UInt32(key.keyCode) {
      case RETROK_LSHIFT.rawValue:
         return (keyboardModifierState & RETROKMOD_SHIFT.rawValue) != 0
      case RETROK_LCTRL.rawValue:
         return (keyboardModifierState & RETROKMOD_CTRL.rawValue) != 0
      case RETROK_LALT.rawValue:
         return (keyboardModifierState & RETROKMOD_ALT.rawValue) != 0
      default:
         break
      }
      return false
   }
}
