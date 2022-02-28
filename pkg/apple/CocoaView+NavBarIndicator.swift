//
//  CocoaView+NavBarIndicator.swift
//  RetroArchiOS
//
//  Created by Yoshi Sugawara on 2/21/22.
//  Copyright © 2022 RetroArch. All rights reserved.
//

extension CocoaView {
   @objc func setupHelperBar() {
      let helperVC = HelperBarViewController()
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
