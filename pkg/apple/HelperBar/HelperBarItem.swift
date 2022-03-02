//
//  HelperBarItem.swift
//  RetroArchiOS
//
//  Created by Yoshi Sugawara on 3/1/22.
//  Copyright © 2022 RetroArch. All rights reserved.
//

protocol HelperBarItem {
   var image: UIImage? { get }
   var selectedImage: UIImage? { get }
   var isSelected: Bool { get }
   var shortDescription: String { get }
   var longDescription: String? { get }
   func action()
}

struct KeyboardBarItem: HelperBarItem {
   let image = UIImage(systemName: "keyboard")
   let selectedImage = UIImage(systemName: "keyboard.fill")
   var isSelected: Bool { actionDelegate?.isKeyboardEnabled ?? false }
   let shortDescription = Strings.shortDescription
   let longDescription: String? = Strings.longDescription
   weak var actionDelegate: HelperBarActionDelegate?
   
   init(actionDelegate: HelperBarActionDelegate?) {
      self.actionDelegate = actionDelegate
   }
   
   func action() {
      actionDelegate?.keyboardButtonTapped()
   }
   
   struct Strings {
      static let shortDescription = NSLocalizedString("An on-screen keyboard", comment: "Description for on-screen keyboard item on helper bar")
      static let longDescription = NSLocalizedString("An on-screen keyboard for cores that require keyboard input.", comment: "Description for on-screen keyboard item on helper bar")
   }
}

struct MouseBarItem: HelperBarItem {
   let image = UIImage(systemName: "computermouse")
   let selectedImage = UIImage(systemName: "computermouse.fill")
   var isSelected: Bool { actionDelegate?.isMouseEnabled ?? false }
   let shortDescription = NSLocalizedString("Use the touch screen for mouse input.", comment: "Description for touch screen mouse item on helper bar")
   var longDescription: String? { nil }
   weak var actionDelegate: HelperBarActionDelegate?

   init(actionDelegate: HelperBarActionDelegate?) {
      self.actionDelegate = actionDelegate
   }

   func action() {
      actionDelegate?.mouseButtonTapped()
   }
}
