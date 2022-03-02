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
   var isKeyboardEnabled: Bool { get }
   var isMouseEnabled: Bool { get }
}

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

extension CocoaView: HelperBarActionDelegate {
   func keyboardButtonTapped() {
      toggleCustomKeyboard()
   }
   
   func mouseButtonTapped() {
      mouseHandler.enabled.toggle()
   }
   
   func helpButtonTapped() {
   }
   
   var isKeyboardEnabled: Bool {
      !keyboardController.view.isHidden
   }
   
   var isMouseEnabled: Bool {
      mouseHandler.enabled
   }
}
