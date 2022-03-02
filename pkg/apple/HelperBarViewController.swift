//
//  HelperBarViewController.swift
//  RetroArchiOS
//
//  Created by Yoshi Sugawara on 2/27/22.
//  Copyright © 2022 RetroArch. All rights reserved.
//

import Combine

@objc protocol HelperBarActionDelegate: AnyObject {
   func keyboardButtonTapped()
   func mouseButtonTapped()
   func helpButtonTapped()
   var isKeyboardEnabled: Bool { get }
   var isMouseEnabled: Bool { get }
}

protocol HelperBarViewModelDelegate: AnyObject {
   func setNavigationBarHidden(_ isHidden: Bool)
   func updateNavigationBarItems()
}

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
      navBar.tintColor = .white
      navBar.isHidden = true
      return navBar
   }()
   
   override func viewDidLoad() {
      viewModel.delegate = self
      setupViews()
      setupBarItems()
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
   }
   
   private func setupBarItems() {
      let barButtonItems = viewModel.createBarButtonItems()
      let navItem = UINavigationItem()
      navItem.leftBarButtonItems = barButtonItems
      navigationBar.items = [navItem]
   }
   
   private func updateBarItems() {
      guard let navItem = navigationBar.items?[0],
            let barButtonItems = navItem.leftBarButtonItems else {
         return
      }
      for barButtonItem in barButtonItems {
         guard let helperBarItem = viewModel.barItemMapping[barButtonItem] else {
            continue
         }
         if helperBarItem.isSelected {
            barButtonItem.image = helperBarItem.selectedImage
         } else {
            barButtonItem.image = helperBarItem.image
         }
      }
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
   func updateNavigationBarItems() {
      updateBarItems()
   }
}

extension HelperBarViewController: UINavigationBarDelegate {
   func position(for bar: UIBarPositioning) -> UIBarPosition {
      return .topAttached
   }
}
