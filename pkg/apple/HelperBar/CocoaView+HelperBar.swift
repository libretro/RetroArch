//
//  CocoaView+HelperBar.swift
//  RetroArchiOS
//
//  Created by Yoshi Sugawara on 2/21/22.
//  Copyright © 2022 RetroArch. All rights reserved.
//

protocol HelperBarActionDelegate: AnyObject {
   func keyboardButtonTapped()
   func mouseButtonTapped()
   func helpButtonTapped()
   func orientationLockButtonTapped()
   var isKeyboardEnabled: Bool { get }
   var isMouseEnabled: Bool { get }
   var isOrientationLocked: Bool { get }
}

@available(iOS 13, *)
extension CocoaView {
   @objc func setupHelperBar() {
      let helperVC = HelperBarViewController()
      let viewModel = HelperBarViewModel(delegate: helperVC, actionDelegate: self)
      helperVC.viewModel = viewModel
      addChild(helperVC)
      helperVC.didMove(toParent: self)
      helperBarView = helperVC.view
      helperBarView.translatesAutoresizingMaskIntoConstraints = false

      view.addSubview(helperBarView)
      helperBarView.leadingAnchor.constraint(equalTo: view.leadingAnchor).isActive = true
      helperBarView.trailingAnchor.constraint(equalTo: view.trailingAnchor).isActive = true
      helperBarView.topAnchor.constraint(equalTo: view.topAnchor).isActive = true
      helperBarView.heightAnchor.constraint(equalToConstant: 75).isActive = true
   }
}

@available(iOS 13, *)
extension CocoaView: HelperBarActionDelegate {
   func keyboardButtonTapped() {
      toggleCustomKeyboard()
   }

   func mouseButtonTapped() {
      mouseHandler.enabled.toggle()
      let messageKey = mouseHandler.enabled ? MSG_IOS_TOUCH_MOUSE_ENABLED : MSG_IOS_TOUCH_MOUSE_DISABLED
      if let message = msg_hash_to_str(messageKey) {
         runloop_msg_queue_push(message, strlen(message), 1, 100, true, nil,
                                MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO)
      }
   }

   func helpButtonTapped() {
   }

   func orientationLockButtonTapped() {
      #if os(iOS)
      shouldLockCurrentInterfaceOrientation.toggle()
      if shouldLockCurrentInterfaceOrientation {
         let currentOrientation = UIApplication.shared.windows.first?.windowScene?.interfaceOrientation ?? UIInterfaceOrientation.portrait
         self.lockInterfaceOrientation = currentOrientation
      }
      if #available(iOS 16, *) {
         setNeedsUpdateOfSupportedInterfaceOrientations()
      }
      #endif
   }

   var isKeyboardEnabled: Bool {
      !keyboardController.view.isHidden
   }

   var isMouseEnabled: Bool {
      mouseHandler.enabled
   }

   var isOrientationLocked: Bool {
      shouldLockCurrentInterfaceOrientation
   }
}
