//
//  HelperBarViewController.swift
//  RetroArchiOS
//
//  Created by Yoshi Sugawara on 2/27/22.
//  Copyright © 2022 RetroArch. All rights reserved.
//

import Combine

@objc protocol HelperBarControllerDelegate: AnyObject {
   func keyboardButtonTapped()
   func mouseButtonTapped()
   func helpButtonTapped()
}

protocol HelperBarViewModelDelegate: AnyObject {
   func setNavigationBarHidden(_ isHidden: Bool)
}

class HelperBarViewModel {
   @Published var didInteractWithBar = false
   private var cancellable: AnyCancellable?
   private var timer: DispatchSourceTimer?
   
   weak var delegate: HelperBarViewModelDelegate?
   
   init(delegate: HelperBarViewModelDelegate? = nil) {
      self.delegate = delegate
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
}

class HelperBarViewController: UIViewController {
   var viewModel = HelperBarViewModel()
   
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
      viewModel.delegate = self
      setupViews()
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
      viewModel.didInteractWithBar = true
   }
}

extension HelperBarViewController: UIGestureRecognizerDelegate {
   func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer, shouldRecognizeSimultaneouslyWith otherGestureRecognizer: UIGestureRecognizer) -> Bool {
      true
   }
}

extension HelperBarViewController: HelperBarViewModelDelegate {
   func setNavigationBarHidden(_ isHidden: Bool) {
      navigationBar.isHidden = isHidden
   }
}

extension HelperBarViewController: UINavigationBarDelegate {
   func position(for bar: UIBarPositioning) -> UIBarPosition {
      return .topAttached
   }
}
