//
//  CocoaView+NavBarIndicator.swift
//  RetroArchiOS
//
//  Created by Yoshi Sugawara on 2/21/22.
//  Copyright © 2022 RetroArch. All rights reserved.
//

import Combine

protocol HelperBarControllerDelegate: AnyObject {
   func keyboardButtonTapped()
   func mouseButtonTapped()
   func helpButtonTapped()
}

@objc class HelperBarController: UIViewController {
   @Published private var didInteractWithBar = false
   private var cancellable: AnyCancellable?
   
   private var timer: DispatchSourceTimer?
   
   private let indicatorImageView: UIImageView = {
      let imageView = UIImageView(frame: .zero)
      imageView.image = UIImage(systemName: "arrow.down.circle")
      imageView.tintColor = .white
      imageView.translatesAutoresizingMaskIntoConstraints = false
      imageView.alpha = 0
      return imageView
   }()
   
   private let navigationBar: UINavigationBar = {
      let navBar = UINavigationBar()
      navBar.barTintColor = .black
      navBar.isHidden = true
      return navBar
   }()
   
   override func viewDidLoad() {
      setupViews()
      setupSubscription()
   }
   
   override func viewDidAppear(_ animated: Bool) {
      DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) { [weak self] in
         self?.showIndicatorAndFadeAway()
      }
   }
   
   override func viewDidLayoutSubviews() {
      super.viewDidLayoutSubviews()
      DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) { [weak self] in
         self?.showIndicatorAndFadeAway()
      }
   }
   
   private func setupViews() {
      indicatorImageView.translatesAutoresizingMaskIntoConstraints = false
      view.addSubview(indicatorImageView)
      indicatorImageView.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 4).isActive = true
      indicatorImageView.centerXAnchor.constraint(equalTo: view.centerXAnchor).isActive = true
      indicatorImageView.heightAnchor.constraint(equalToConstant: 40).isActive = true
      indicatorImageView.widthAnchor.constraint(equalTo: indicatorImageView.heightAnchor).isActive = true
      view.addSubview(navigationBar)
      navigationBar.translatesAutoresizingMaskIntoConstraints = false
      navigationBar.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor).isActive = true
      navigationBar.leadingAnchor.constraint(equalTo: view.leadingAnchor).isActive = true
      navigationBar.trailingAnchor.constraint(equalTo: view.trailingAnchor).isActive = true
      let tap = UITapGestureRecognizer(target: self, action: #selector(didTap(_:)))
      tap.delegate = self
      view.addGestureRecognizer(tap)
      view.isUserInteractionEnabled = true
      navigationBar.delegate = self
      let item = UINavigationItem()
      item.leftBarButtonItem = UIBarButtonItem(barButtonSystemItem: .pause, target: self, action: #selector(didPressBarButton(_:)))
      navigationBar.items = [item]
   }
   
   @objc func didPressBarButton(_ sender: UIBarButtonItem) {
   }
   
   // Create a timer that will hide the navigation bar after 3 seconds if it's not interacted with
   private func setupTimer() {
      timer = DispatchSource.makeTimerSource()
      timer?.setEventHandler(handler: { [weak self] in
         guard let self = self else { return }
         if !self.didInteractWithBar {
            DispatchQueue.main.async { [weak self] in
               self?.navigationBar.isHidden = true
               self?.didInteractWithBar = false
            }
         }
      })
      timer?.schedule(deadline: .now() + .seconds(3))
      timer?.resume()
   }
   
   private func setupSubscription() {
      cancellable = $didInteractWithBar
         .receive(on: RunLoop.main)
         .sink(receiveValue: { [weak self] didInteract in
            print("didInteract changed to \(didInteract)")
            if didInteract {
               self?.navigationBar.isHidden = false
               self?.timer?.cancel()
               self?.setupTimer()
               self?.didInteractWithBar = false
            }
      })
   }
   
   private func showIndicatorAndFadeAway() {
      UIView.animateKeyframes(withDuration: 5.0, delay: 0) {
         UIView.addKeyframe(withRelativeStartTime: 0, relativeDuration: 4/5) { [weak self] in
            self?.indicatorImageView.alpha = 1.0
         }
         
         UIView.addKeyframe(withRelativeStartTime: 1/3, relativeDuration: 1/5) { [weak self] in
            self?.indicatorImageView.alpha = 0.0
         }
      }
   }

   @objc func didTap(_ sender: UIGestureRecognizer) {
      didInteractWithBar = true
   }
}

extension HelperBarController: UIGestureRecognizerDelegate {
   func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer, shouldRecognizeSimultaneouslyWith otherGestureRecognizer: UIGestureRecognizer) -> Bool {
      true
   }
}

extension HelperBarController: UINavigationBarDelegate {
   func position(for bar: UIBarPositioning) -> UIBarPosition {
      return .topAttached
   }
}

extension CocoaView {
   @objc func setupHelperBar() {
      let helperVC = HelperBarController()
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
