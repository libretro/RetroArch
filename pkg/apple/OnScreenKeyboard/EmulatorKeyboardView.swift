//
//  EmulatorKeyboard.swift
//
//  Created by Yoshi Sugawara on 7/30/20.
//

// TODO: shift key should change the label of the keys to uppercase (need callback mechanism?)
// pan gesture to outer edges of keyboard view for better dragging

@objc protocol EmulatorKeyboardKeyPressedDelegate: AnyObject {
    func keyDown(_ key: KeyCoded)
    func keyUp(_ key: KeyCoded)
}

@objc protocol EmulatorKeyboardModifierPressedDelegate: AnyObject {
    func modifierPressedWithKey(_ key: KeyCoded, enable: Bool)
    func isModifierEnabled(key: KeyCoded) -> Bool
}

protocol EmulatorKeyboardViewDelegate: AnyObject {
    func toggleAlternateKeys()
    func refreshModifierStates()
    func updateTransparency(toAlpha alpha: Float)
}

class EmulatorKeyboardView: UIView {
   
   static var keyboardBackgroundColor = UIColor.systemGray6.withAlphaComponent(0.5)
   static var keyboardCornerRadius = 6.0
   static var keyboardDragColor = UIColor.systemGray
   
   static var keyCornerRadius = 6.0
   static var keyBorderWidth = 1.0
   
   static var rowSpacing = 12.0
   static var keySpacing = 8.0
   
   static var keyNormalFont = UIFont.systemFont(ofSize: 12)
   static var keyPressedFont = UIFont.boldSystemFont(ofSize: 24)
   
   static var keyNormalBackgroundColor = UIColor.systemGray4.withAlphaComponent(0.5)
   static var keyNormalBorderColor = keyNormalBackgroundColor
   static var keyNormalTextColor = UIColor.label
   
   static var keyPressedBackgroundColor = UIColor.systemGray2
   static var keyPressedBorderColor = keyPressedBackgroundColor
   static var keyPressedTextColor = UIColor.label
   
   static var keySelectedBackgroundColor = UIColor.systemGray2.withAlphaComponent(0.8)
   static var keySelectedBorderColor = keySelectedBackgroundColor
   static var keySelectedTextColor = UIColor.label
   
    var viewModel = EmulatorKeyboardViewModel(keys: [[KeyCoded]]()) {
        didSet {
            setupWithModel(viewModel)
        }
    }
    var modifierButtons = Set<EmulatorKeyboardButton>()
    
    weak var delegate: EmulatorKeyboardViewDelegate?
    
    private lazy var keyRowsStackView: UIStackView = {
       let stackView = UIStackView()
       stackView.translatesAutoresizingMaskIntoConstraints = false
       stackView.axis = .vertical
       stackView.distribution = .equalCentering
       stackView.spacing = Self.rowSpacing
       return stackView
    }()
    
    private lazy var alternateKeyRowsStackView: UIStackView = {
        let stackView = UIStackView()
        stackView.translatesAutoresizingMaskIntoConstraints = false
        stackView.axis = .vertical
        stackView.distribution = .equalCentering
        stackView.spacing = Self.rowSpacing
        stackView.isHidden = true
        return stackView
    }()
    
    let dragMeView: UIView = {
      let view = UIView(frame: .zero)
      view.backgroundColor = EmulatorKeyboardView.keyboardDragColor
      view.translatesAutoresizingMaskIntoConstraints = false
      view.widthAnchor.constraint(equalToConstant: 80).isActive = true
      view.heightAnchor.constraint(equalToConstant: 2).isActive = true
      let outerView = UIView(frame: .zero)
      outerView.backgroundColor = .clear
      outerView.translatesAutoresizingMaskIntoConstraints = false
      outerView.addSubview(view)
      view.centerXAnchor.constraint(equalTo: outerView.centerXAnchor).isActive = true
      view.centerYAnchor.constraint(equalTo: outerView.centerYAnchor).isActive = true
      outerView.heightAnchor.constraint(equalToConstant: 20).isActive = true
      outerView.widthAnchor.constraint(equalToConstant: 100).isActive = true
      return outerView
    }()
    
    private var pressedKeyViews = [UIControl: UIView]()
    
    convenience init() {
        self.init(frame: CGRect.zero)
    }

    override init(frame: CGRect) {
        super.init(frame: frame)
        commonInit()
    }

    required init?(coder aDecoder: NSCoder) {
        super.init(coder: aDecoder)
        commonInit()
    }

    private func commonInit() {
        backgroundColor = Self.keyboardBackgroundColor
        layer.cornerRadius = Self.keyboardCornerRadius
        layoutMargins = UIEdgeInsets(top: 16, left: 4, bottom: 16, right: 4)
        insetsLayoutMarginsFromSafeArea = false
        addSubview(keyRowsStackView)
        keyRowsStackView.topAnchor.constraint(equalTo: layoutMarginsGuide.topAnchor).isActive = true
        keyRowsStackView.leadingAnchor.constraint(equalTo: layoutMarginsGuide.leadingAnchor, constant: 4.0).isActive = true
        keyRowsStackView.trailingAnchor.constraint(equalTo: layoutMarginsGuide.trailingAnchor, constant: -4.0).isActive = true
        addSubview(alternateKeyRowsStackView)
        alternateKeyRowsStackView.topAnchor.constraint(equalTo: layoutMarginsGuide.topAnchor).isActive = true
        alternateKeyRowsStackView.leadingAnchor.constraint(equalTo: layoutMarginsGuide.leadingAnchor, constant: 4.0).isActive = true
        alternateKeyRowsStackView.trailingAnchor.constraint(equalTo: layoutMarginsGuide.trailingAnchor, constant: -4.0).isActive = true
        addSubview(dragMeView)
        dragMeView.centerXAnchor.constraint(equalTo: centerXAnchor).isActive = true
        dragMeView.bottomAnchor.constraint(equalTo: bottomAnchor).isActive = true
    }
    
    
    @objc private func keyPressed(_ sender: EmulatorKeyboardButton) {
        if sender.key.keyCode == 9000 { // hack for now
            return
        }
        if !sender.key.isModifier {
           // make a "stand-in" for our key, and scale up key
           let view = UIView()
           view.backgroundColor = EmulatorKeyboardView.keyPressedBackgroundColor
           view.layer.cornerRadius = EmulatorKeyboardView.keyCornerRadius
           view.layer.maskedCorners = [.layerMinXMaxYCorner, .layerMaxXMaxYCorner]
           view.frame = sender.convert(sender.bounds, to: self)
           addSubview(view)
           
           var tx = 0.0
           let ty = sender.bounds.height * -1.20
           
           if let window = self.window {
               let rect = sender.convert(sender.bounds, to:window)
               
               if rect.maxX > window.bounds.width * 0.9 {
                   tx = sender.bounds.width * -0.5
               }
               if rect.minX < window.bounds.width * 0.1 {
                   tx = sender.bounds.width * 0.5
               }
           }

           sender.superview!.bringSubviewToFront(sender)
           sender.transform = CGAffineTransform(translationX:tx, y:ty).scaledBy(x:2, y:2)
           
           pressedKeyViews[sender] = view
        }
        viewModel.keyPressed(sender.key)
    }
    
    @objc private func keyCancelled(_ sender: EmulatorKeyboardButton) {
       sender.transform = .identity
       if let view = pressedKeyViews[sender] {
          view.removeFromSuperview()
          pressedKeyViews.removeValue(forKey: sender)
       }
    }
    
    @objc private func keyReleased(_ sender: EmulatorKeyboardButton) {
       sender.transform = .identity
       if sender.key.keyCode == 9000 {
          delegate?.toggleAlternateKeys()
          return
       }
       if let view = pressedKeyViews[sender] {
          view.removeFromSuperview()
          pressedKeyViews.removeValue(forKey: sender)
       }
       sender.isSelected = viewModel.modifierKeyToggleStateForKey(sender.key)
       viewModel.keyReleased(sender.key)
       self.delegate?.refreshModifierStates()
    }
    
    func setupWithModel(_ model: EmulatorKeyboardViewModel) {
        for row in model.keys {
            let keysInRow = createKeyRow(keys: row)
            keyRowsStackView.addArrangedSubview(keysInRow)
        }
        if let altKeys = model.alternateKeys {
            for row in altKeys {
                let keysInRow = createKeyRow(keys: row)
                alternateKeyRowsStackView.addArrangedSubview(keysInRow)
            }
        }
        if !model.isDraggable {
            dragMeView.isHidden = true
        }
    }
    
    func toggleKeysStackView() {
        if viewModel.alternateKeys != nil {
            keyRowsStackView.isHidden.toggle()
            alternateKeyRowsStackView.isHidden.toggle()
            refreshModifierStates()
        }
    }
    
    func refreshModifierStates() {
       modifierButtons.forEach{ button in
          button.isSelected = viewModel.modifierKeyToggleStateForKey(button.key)
       }
    }
    
    private func createKey(_ keyCoded: KeyCoded) -> UIButton {
       let key = EmulatorKeyboardButton(key: keyCoded)
       if let imageName = keyCoded.keyImageName {
          key.tintColor = EmulatorKeyboardView.keyNormalTextColor
          key.setImage(UIImage(systemName: imageName), for: .normal)
          if let highlightedImageName = keyCoded.keyImageNameHighlighted {
             key.setImage(UIImage(systemName: highlightedImageName), for: .highlighted)
             key.setImage(UIImage(systemName: highlightedImageName), for: .selected)
          }
       } else {
          key.setTitle(keyCoded.keyLabel, for: .normal)
          key.titleLabel?.font = EmulatorKeyboardView.keyNormalFont
          key.setTitleColor(EmulatorKeyboardView.keyNormalTextColor, for: .normal)
          key.setTitleColor(EmulatorKeyboardView.keySelectedTextColor, for: .selected)
          key.setTitleColor(EmulatorKeyboardView.keyPressedTextColor, for: .highlighted)
       }
       
       key.translatesAutoresizingMaskIntoConstraints = false
       key.widthAnchor.constraint(equalToConstant: (25 * CGFloat(keyCoded.keySize.rawValue))).isActive = true
       key.heightAnchor.constraint(equalToConstant: 35).isActive = true
       key.backgroundColor = EmulatorKeyboardView.keyNormalBackgroundColor
       key.layer.borderWidth = EmulatorKeyboardView.keyBorderWidth
       key.layer.borderColor = EmulatorKeyboardView.keyNormalBorderColor.cgColor
       key.layer.cornerRadius = EmulatorKeyboardView.keyCornerRadius
       key.addTarget(self, action: #selector(keyPressed(_:)), for: .touchDown)
       key.addTarget(self, action: #selector(keyReleased(_:)), for: .touchUpInside)
       key.addTarget(self, action: #selector(keyReleased(_:)), for: .touchUpOutside)
       key.addTarget(self, action: #selector(keyCancelled(_:)), for: .touchCancel)
       if keyCoded.isModifier {
          modifierButtons.update(with: key)
       }
       return key
    }

    private func createKeyRow(keys: [KeyCoded]) -> UIStackView {
        let subviews: [UIView] = keys.enumerated().map { index, keyCoded -> UIView in
            if keyCoded is SpacerKey {
                let spacer = UIView()
                spacer.widthAnchor.constraint(equalToConstant: 25.0 * CGFloat(keyCoded.keySize.rawValue)).isActive = true
                spacer.heightAnchor.constraint(equalToConstant: 25.0).isActive = true
                return spacer
            } else if let sliderKey = keyCoded as? SliderKey {
                sliderKey.keyboardView = self
                return sliderKey.createView()
            }
            return createKey(keyCoded)
        }
        let stack = UIStackView(arrangedSubviews: subviews)
        stack.axis = .horizontal
        stack.distribution = .fill
        stack.spacing = 8
        return stack
    }
}

extension UIImage {
    static func dot(size:CGSize, color:UIColor) -> UIImage {
        return UIGraphicsImageRenderer(size: size).image { context in
            context.cgContext.setFillColor(color.cgColor)
            context.cgContext.fillEllipse(in: CGRect(origin:.zero, size:size))
        }
    }
}
