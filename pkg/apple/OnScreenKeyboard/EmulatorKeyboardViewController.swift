//
//  EmulatorKeyboardViewController.swift
//  RetroArchiOS
//
//  Created by Yoshi Sugawara on 3/3/22.
//  Copyright © 2022 RetroArch. All rights reserved.
//

@objc class EmulatorKeyboardController: UIViewController {

   class EmulatorKeyboardPassthroughView: UIView {
      override func hitTest(_ point: CGPoint, with event: UIEvent?) -> UIView? {
         let hitView = super.hitTest(point, with: event)
         if hitView == self {
            return nil
         }
         return hitView
      }
   }
   
   @objc let leftKeyboardModel: EmulatorKeyboardViewModel
   @objc let rightKeyboardModel: EmulatorKeyboardViewModel

   @objc lazy var leftKeyboardView: EmulatorKeyboardView = {
        let view = leftKeyboardModel.createView()
        view.delegate = self
        return view
    }()
    @objc lazy var rightKeyboardView: EmulatorKeyboardView = {
        let view = rightKeyboardModel.createView()
        view.delegate = self
        return view
    }()
    var keyboardConstraints = [NSLayoutConstraint]()
        
   init(leftKeyboardModel: EmulatorKeyboardViewModel, rightKeyboardModel: EmulatorKeyboardViewModel) {
      self.leftKeyboardModel = leftKeyboardModel
      self.rightKeyboardModel = rightKeyboardModel
      super.init(nibName: nil, bundle: nil)
   }
    
   required init?(coder: NSCoder) {
      fatalError("init(coder:) has not been implemented")
   }
   
   override func loadView() {
      view = EmulatorKeyboardPassthroughView()
   }

    override func viewDidLoad() {
        super.viewDidLoad()
        setupView()
        
        let panGesture = UIPanGestureRecognizer(target: self, action: #selector(draggedView(_:)))
        leftKeyboardView.dragMeView.isUserInteractionEnabled = true
        leftKeyboardView.dragMeView.addGestureRecognizer(panGesture)
        let panGestureRightKeyboard = UIPanGestureRecognizer(target: self, action: #selector(draggedView(_:)))
        rightKeyboardView.dragMeView.isUserInteractionEnabled = true
        rightKeyboardView.dragMeView.addGestureRecognizer(panGestureRightKeyboard)
    }
    
    func setupView() {
        NSLayoutConstraint.deactivate(keyboardConstraints)
        keyboardConstraints.removeAll()
        leftKeyboardView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(leftKeyboardView)
        leftKeyboardView.heightAnchor.constraint(equalToConstant: 270).isActive = true
        leftKeyboardView.widthAnchor.constraint(equalToConstant: 180).isActive = true
        keyboardConstraints.append(contentsOf: [
            leftKeyboardView.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor),
            leftKeyboardView.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor)
        ])
        rightKeyboardView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(rightKeyboardView)
        keyboardConstraints.append(contentsOf: [
            rightKeyboardView.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor),
            rightKeyboardView.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor)
        ])
        rightKeyboardView.heightAnchor.constraint(equalToConstant: 270).isActive = true
        rightKeyboardView.widthAnchor.constraint(equalToConstant: 180).isActive = true
        NSLayoutConstraint.activate(keyboardConstraints)
    }
    
    func setupViewFrames() {
        // initial placement on the bottom corners
        // since we don't know the frame of this view yet until layout time,
        // assume it's taking the full screen
        let screenFrame = UIScreen.main.bounds
        let keyboardHeight: CGFloat = 250.0
        let keyboardWidth: CGFloat = 180.0
        let bottomLeftFrame = CGRect(
            x: 0,
            y: screenFrame.size.height - 40 - keyboardHeight - 20,
            width: keyboardWidth, height: keyboardHeight)
        let bottomRightFrame = CGRect(
            x: screenFrame.size.width - 20 - keyboardWidth,
            y:screenFrame.size.height - 40 - keyboardHeight - 20,
            width: keyboardWidth, height: keyboardHeight
        )
        view.addSubview(leftKeyboardView)
        view.addSubview(rightKeyboardView)
        leftKeyboardView.frame = bottomLeftFrame
        rightKeyboardView.frame = bottomRightFrame
    }
    
    func setupKeyModels() {
        leftKeyboardView.setupWithModel(leftKeyboardModel)
        rightKeyboardView.setupWithModel(rightKeyboardModel)
    }
    
    @objc func draggedView(_ sender:UIPanGestureRecognizer){
        guard let keyboardView = sender.view?.superview else {
            return
        }
        let translation = sender.translation(in: self.view)
        keyboardView.center = CGPoint(x: keyboardView.center.x + translation.x, y: keyboardView.center.y + translation.y)
        sender.setTranslation(CGPoint.zero, in: self.view)
    }
}

extension EmulatorKeyboardController: EmulatorKeyboardViewDelegate {
    func toggleAlternateKeys() {
        for keyboard in [leftKeyboardView, rightKeyboardView] {
            keyboard.toggleKeysStackView()
        }
    }
    func refreshModifierStates() {
        for keyboard in [leftKeyboardView, rightKeyboardView] {
            keyboard.refreshModifierStates()
        }
    }
    func updateTransparency(toAlpha alpha: Float) {
        for keyboard in [leftKeyboardView, rightKeyboardView] {
            keyboard.alpha = CGFloat(alpha)
        }
    }
}

