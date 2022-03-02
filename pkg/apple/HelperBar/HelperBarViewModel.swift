//
//  HelperBarViewModel.swift
//  RetroArchiOS
//
//  Created by Yoshi Sugawara on 3/1/22.
//  Copyright © 2022 RetroArch. All rights reserved.
//

import Combine

protocol HelperBarViewModelDelegate: AnyObject {
   func setNavigationBarHidden(_ isHidden: Bool)
   func updateNavigationBarItems()
}

class HelperBarViewModel {
   @Published var didInteractWithBar = false
   private var cancellable: AnyCancellable?
   private var timer: DispatchSourceTimer?
   
   weak var delegate: HelperBarViewModelDelegate?
   weak var actionDelegate: HelperBarActionDelegate?
   
   lazy var barItems: [HelperBarItem] = [
      KeyboardBarItem(actionDelegate: actionDelegate),
      MouseBarItem(actionDelegate: actionDelegate)
   ]
   
   var barItemMapping = [UIBarButtonItem: HelperBarItem]()
   
   init(delegate: HelperBarViewModelDelegate? = nil, actionDelegate: HelperBarActionDelegate? = nil) {
      self.delegate = delegate
      self.actionDelegate = actionDelegate
      setupSubscription()
   }
   
   // Create a timer that will hide the navigation bar after 3 seconds if it's not interacted with
   private func setupTimer() {
      timer = DispatchSource.makeTimerSource()
      timer?.setEventHandler(handler: { [weak self] in
         guard let self = self else { return }
         if !self.didInteractWithBar {
            DispatchQueue.main.async { [weak self] in
               self?.didInteractWithBar = false
               self?.delegate?.setNavigationBarHidden(true)
            }
         }
      })
      timer?.schedule(deadline: .now() + .seconds(3))
      timer?.resume()
   }

   // Listen for changes on didInteractWithBar
   private func setupSubscription() {
      cancellable = $didInteractWithBar
         .receive(on: RunLoop.main)
         .sink(receiveValue: { [weak self] didInteract in
            print("didInteract changed to \(didInteract)")
            if didInteract {
               self?.delegate?.setNavigationBarHidden(false)
               self?.timer?.cancel()
               self?.setupTimer()
               self?.didInteractWithBar = false
            }
      })
   }
   
   func createBarButtonItems() -> [UIBarButtonItem] {
      barItemMapping.removeAll()
      return barItems.map{ [weak self] item in
         let barButtonItem = UIBarButtonItem(image: item.image, style: .plain, target: self, action: #selector(didTapBarItem(_:)))
         self?.barItemMapping[barButtonItem] = item
         return barButtonItem
      }
   }
   
   @objc private func didTapBarItem(_ sender: UIBarButtonItem) {
      guard let item = barItemMapping[sender] else { return }
      item.action()
      delegate?.updateNavigationBarItems()
   }
}

