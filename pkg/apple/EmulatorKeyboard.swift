//
//  EmulatorKeyboard.swift
//
//  Created by Yoshi Sugawara on 7/30/20.
//

// TODO: shift key should change the label of the keys to uppercase (need callback mechanism?)
// pan gesture to outer edges of keyboard view for better dragging

import Foundation
import UIKit

class KeyboardButton: UIButton {
    let key: KeyCoded
    var toggleState = false
    
    // MARK: - Functions
    override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
        let newArea = CGRect(
            x: self.bounds.origin.x - 5.0,
            y: self.bounds.origin.y - 5.0,
            width: self.bounds.size.width + 20.0,
            height: self.bounds.size.height + 20.0
        )
        return newArea.contains(point)
    }
    
    override open var isHighlighted: Bool {
        didSet {
            // this is the problem - the isHighlighted sets to false for the touchesEnded callback
            // need to look ask something else
//            let shouldHighlight = isModifierToggle ? toggleState : isHighlighted
            if !isHighlighted && toggleState {
                // don't update the highlught
            } else {
                backgroundColor = isHighlighted ? .white : .clear
            }
        }
    }
    
    override open var isSelected: Bool {
        didSet {
            let shouldHighlight = key.isModifier ? toggleState : isSelected
            backgroundColor = shouldHighlight ? .red : .clear
        }
    }
    
    required init(key: KeyCoded) {
        self.key = key
        super.init(frame: .zero)
    }
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
}

@objc protocol EmulatorKeyboardKeyPressedDelegate: class {
    func keyDown(_ key: KeyCoded)
    func keyUp(_ key: KeyCoded)
    func updateTransparency(toAlpha alpha: CGFloat)
}

@objc protocol EmulatorKeyboardModifierPressedDelegate: class {
    func modifierPressedWithKey(_ key: KeyCoded, enable: Bool)
    func isModifierEnabled(key: KeyCoded) -> Bool
}

protocol EmulatorKeyboardViewDelegate: class {
    func toggleAlternateKeys()
    func refreshModifierStates()
}

class EmulatorKeyboardView: UIView {
    
    var viewModel = EmulatorKeyboardViewModel(keys: [[KeyCoded]]()) {
        didSet {
            setupWithModel(viewModel)
        }
    }
    var modifierButtons = Set<KeyboardButton>()
    
    weak var delegate: EmulatorKeyboardViewDelegate?
    
    private lazy var keyRowsStackView: UIStackView = {
       let stackView = UIStackView()
       stackView.translatesAutoresizingMaskIntoConstraints = false
       stackView.axis = .vertical
       stackView.distribution = .equalCentering
       stackView.spacing = 12
       return stackView
    }()
    
    private lazy var alternateKeyRowsStackView: UIStackView = {
        let stackView = UIStackView()
        stackView.translatesAutoresizingMaskIntoConstraints = false
        stackView.axis = .vertical
        stackView.distribution = .equalCentering
        stackView.spacing = 12
        stackView.isHidden = true
        return stackView
    }()
    
    let dragMeView: UILabel = {
        let label = UILabel(frame: .zero)
        label.translatesAutoresizingMaskIntoConstraints = false
        label.text = "DRAG ME!"
        label.textColor = UIColor.systemRed
        return label
    }()
    
    private var pressedKeyLabels = [String: UILabel]()
    
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
        backgroundColor = .clear
//        layer.borderColor = UIColor.white.cgColor
//        layer.borderWidth = 1.0
//        layer.cornerRadius = 15.0
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
        dragMeView.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -8.0).isActive = true
    }
    
    
    @objc private func keyPressed(_ sender: KeyboardButton) {
        if sender.key.keyCode == 9000 { // hack for now
            return
        }
        if !sender.key.isModifier {
            let label = UILabel(frame: .zero)
            label.text = sender.titleLabel?.text
            // hmm need to convert frame
            let converted = sender.convert(sender.bounds, to: self)
            print("sender frame: \(sender.frame), bounds: \(sender.bounds), convertedBounds = \(converted)")
            var labelFrame = converted.offsetBy(dx: 0, dy: -60)
            labelFrame = CGRect(x: labelFrame.origin.x, y: labelFrame.origin.y, width: labelFrame.width * 2, height: labelFrame.height * 2)
            label.backgroundColor = .white
            label.textColor = .black
            label.frame = labelFrame
            label.font = UIFont(name: "Print Char 21", size: 12)
            label.textAlignment = .center
            addSubview(label)
            pressedKeyLabels[label.text ?? "😭"] = label
        }
        viewModel.keyPressed(sender.key)
    }
    
    @objc private func keyCancelled(_ sender: KeyboardButton) {
        let title = sender.titleLabel?.text ?? "😭"
        if let label = pressedKeyLabels[title] {
            label.removeFromSuperview()
            pressedKeyLabels.removeValue(forKey: title)
        }
    }
    
    @objc private func keyReleased(_ sender: KeyboardButton) {
        if sender.key.keyCode == 9000 {
            delegate?.toggleAlternateKeys()
            return
        }
        let title = sender.titleLabel?.text ?? "😭"
        if let label = pressedKeyLabels[title] {
            label.removeFromSuperview()
            pressedKeyLabels.removeValue(forKey: title)
        }
        let modifierState = viewModel.modifierKeyToggleStateForKey(sender.key)
        sender.toggleState = modifierState
        sender.isSelected = modifierState
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
            button.toggleState = viewModel.modifierKeyToggleStateForKey(button.key)
            button.isSelected = button.toggleState
        }
    }
    
    private func createKey(_ keyCoded: KeyCoded) -> UIButton {
        let key = KeyboardButton(key: keyCoded)
        if let imageName = keyCoded.keyImageName {
            key.setImage(UIImage(named: imageName), for: .normal)
            if let highlightedImageName = keyCoded.keyImageNameHighlighted {
                key.setImage(UIImage(named: highlightedImageName), for: .highlighted)
                key.setImage(UIImage(named: highlightedImageName), for: .selected)
            }
        } else {
            key.setTitle(keyCoded.keyLabel, for: .normal)
            key.titleLabel?.font = UIFont(name: "Print Char 21", size: 12)
            key.setTitleColor(.white, for: .normal)
            key.setTitleColor(.black, for: .highlighted)
        }
        key.translatesAutoresizingMaskIntoConstraints = false
        key.widthAnchor.constraint(equalToConstant: (25 * CGFloat(keyCoded.keySize.rawValue))).isActive = true
        key.heightAnchor.constraint(equalToConstant: 35).isActive = true
        key.layer.borderWidth = 1.0
        key.layer.borderColor = UIColor.white.cgColor
        key.layer.cornerRadius = 6.0
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
                sliderKey.keyboardViewModel = self.viewModel
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

@objc enum KeySize: Int {
    case standard = 1, wide, wider
}

// represents a key that has an underlying code that gets sent to the emulator
@objc protocol KeyCoded: AnyObject {
    var keyLabel: String { get }
    var keyImageName: String? { get }
    var keyImageNameHighlighted: String? { get }
    var keyCode: Int { get }
    var keySize: KeySize { get }
    var isModifier: Bool { get }
}

protocol KeyRowsDataSource {
    func keyForPositionAt(_ position: KeyPosition) -> KeyCoded?
}

@objc class EmulatorKeyboardKey: NSObject, KeyCoded {
    let keyLabel: String
    var keyImageName: String?
    var keyImageNameHighlighted: String?
    let keyCode: Int
    let keySize: KeySize
    let isModifier: Bool
    
    override var description: String {
        return String(format: "\(keyLabel) (%02X)", keyCode)
    }
    init(label: String, code: Int, keySize: KeySize = .standard, isModifier: Bool = false, imageName: String? = nil, imageNameHighlighted: String? = nil)  {
        self.keyLabel = label
        self.keyCode = code
        self.keySize = keySize
        self.isModifier = isModifier
        self.keyImageName = imageName
        self.keyImageNameHighlighted = imageNameHighlighted
    }
}

class SpacerKey: KeyCoded {
    let keyLabel = ""
    let keyCode = 0
    let keySize: KeySize
    let isModifier = false
    let keyImageName: String? = nil
    let keyImageNameHighlighted: String? = nil
    init(keySize: KeySize = .standard) {
        self.keySize = keySize
    }
}

class SliderKey: KeyCoded {
    let keyLabel = ""
    let keyCode = 0
    let keySize: KeySize
    let isModifier = false
    let keyImageName: String? = nil
    let keyImageNameHighlighted: String? = nil
    weak var keyboardViewModel: EmulatorKeyboardViewModel?
    init(keySize: KeySize = .standard) {
        self.keySize = keySize
    }
    func createView() -> UIView {
        let slider = UISlider(frame: .zero)
        slider.minimumValue = 0.1
        slider.maximumValue = 1.0
        slider.addTarget(self, action: #selector(adjustKeyboardAlpha(_:)), for: .valueChanged)
        slider.value = 1.0
        return slider
    }
    @objc func adjustKeyboardAlpha(_ sender: UISlider) {
        keyboardViewModel?.delegate?.updateTransparency(toAlpha: CGFloat(sender.value))
    }
}

struct KeyPosition {
    let row: Int
    let column: Int
}

@objc class EmulatorKeyboardViewModel: NSObject, KeyRowsDataSource {
    var keys = [[KeyCoded]]()
    var alternateKeys: [[KeyCoded]]?
    var modifiers: [Int16: KeyCoded]?
    
    var isDraggable = true
    
    @objc weak var delegate: EmulatorKeyboardKeyPressedDelegate?
    @objc weak var modifierDelegate: EmulatorKeyboardModifierPressedDelegate?
    
    init(keys: [[KeyCoded]], alternateKeys: [[KeyCoded]]? = nil) {
        self.keys = keys
        self.alternateKeys = alternateKeys
    }
    
    func createView() -> EmulatorKeyboardView {
        let view = EmulatorKeyboardView()
        view.viewModel = self
        return view
    }
    
    func keyForPositionAt(_ position: KeyPosition) -> KeyCoded? {
        guard position.row < keys.count else {
            return nil
        }
        let row = keys[position.row]
        guard position.column < row.count else {
            return nil
        }
        return row[position.column]
    }
    
    func modifierKeyToggleStateForKey(_ key: KeyCoded) -> Bool {
        return key.isModifier && (modifierDelegate?.isModifierEnabled(key: key) ?? false)
    }
    
    func keyPressed(_ key: KeyCoded) {
        if key.isModifier {
            let isPressed = modifierDelegate?.isModifierEnabled(key: key) ?? false
            modifierDelegate?.modifierPressedWithKey(key, enable: !isPressed)
            return
        }
        delegate?.keyDown(key)
    }
    
    func keyReleased(_ key: KeyCoded) {
        if key.isModifier {
            return
        }
        delegate?.keyUp(key)
    }
    
    // KeyCoded can support a shifted key label
    // view can update with shifted key labels?
    // cluster can support alternate keys and view can swap them out?
}

@objc class EmulatorKeyboardController: UIViewController {
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
    
    // Global states for all the keyboards
    // uses bitwise masks for the state of shift keys, control, open-apple keys, etc
    @objc var modifierState: Int16 = 0
   
   @objc let leftKeyboardModel = EmulatorKeyboardViewModel(keys:
      [
         [
            EmulatorKeyboardKey(label: "esc", code: Int(RETROK_ESCAPE.rawValue)),
            EmulatorKeyboardKey(label: "tab", code: Int(RETROK_TAB.rawValue))
         ],
         [
            EmulatorKeyboardKey(label: "q", code: Int(RETROK_q.rawValue)),
            EmulatorKeyboardKey(label: "w", code: Int(RETROK_w.rawValue)),
            EmulatorKeyboardKey(label: "e", code: Int(RETROK_e.rawValue)),
            EmulatorKeyboardKey(label: "r", code: Int(RETROK_r.rawValue)),
            EmulatorKeyboardKey(label: "t", code: Int(RETROK_t.rawValue)),
         ],
         [
            EmulatorKeyboardKey(label: "a", code: Int(RETROK_a.rawValue)),
            EmulatorKeyboardKey(label: "s", code: Int(RETROK_s.rawValue)),
            EmulatorKeyboardKey(label: "d", code: Int(RETROK_d.rawValue)),
            EmulatorKeyboardKey(label: "f", code: Int(RETROK_f.rawValue)),
            EmulatorKeyboardKey(label: "g", code: Int(RETROK_g.rawValue)),
         ],
         [
            EmulatorKeyboardKey(label: "z", code: Int(RETROK_z.rawValue)),
            EmulatorKeyboardKey(label: "x", code: Int(RETROK_x.rawValue)),
            EmulatorKeyboardKey(label: "c", code: Int(RETROK_c.rawValue)),
            EmulatorKeyboardKey(label: "v", code: Int(RETROK_v.rawValue)),
            EmulatorKeyboardKey(label: "b", code: Int(RETROK_b.rawValue)),
         ],
         [
            EmulatorKeyboardKey(label: "SHIFT", code: Int(RETROK_LSHIFT.rawValue), keySize: .standard, isModifier: false, imageName: "shift", imageNameHighlighted: "shift.fill"),
            EmulatorKeyboardKey(label: "123", code: 9000, keySize: .standard, imageName: "textformat.123"),
            EmulatorKeyboardKey(label: "Alt", code: Int(RETROK_LALT.rawValue)),
            EmulatorKeyboardKey(label: "Space", code: Int(RETROK_SPACE.rawValue), keySize: .wide)
         ]
      ],
      alternateKeys: [
         [
            SliderKey(keySize: .standard)
         ],
         [
            EmulatorKeyboardKey(label: "1", code: Int(RETROK_1.rawValue)),
            EmulatorKeyboardKey(label: "2", code: Int(RETROK_2.rawValue)),
            EmulatorKeyboardKey(label: "3", code: Int(RETROK_3.rawValue)),
            EmulatorKeyboardKey(label: "4", code: Int(RETROK_4.rawValue)),
            EmulatorKeyboardKey(label: "5", code: Int(RETROK_5.rawValue)),
         ],
         [
            EmulatorKeyboardKey(label: "-", code: Int(RETROK_MINUS.rawValue)),
            EmulatorKeyboardKey(label: "=", code: Int(RETROK_EQUALS.rawValue)),
            EmulatorKeyboardKey(label: "/", code: Int(RETROK_SLASH.rawValue)),
            EmulatorKeyboardKey(label: "[", code: Int(RETROK_LEFTBRACKET.rawValue)),
            EmulatorKeyboardKey(label: "]", code: Int(RETROK_RIGHTBRACKET.rawValue)),
         ],
         [
            EmulatorKeyboardKey(label: ";", code: Int(RETROK_SEMICOLON.rawValue)),
            EmulatorKeyboardKey(label: "~", code: Int(RETROK_TILDE.rawValue)),
            EmulatorKeyboardKey(label: ":", code: Int(RETROK_COLON.rawValue)),
            EmulatorKeyboardKey(label: "?", code: Int(RETROK_QUESTION.rawValue)),
            EmulatorKeyboardKey(label: "!", code: Int(RETROK_EXCLAIM.rawValue)),
         ],
         [
            EmulatorKeyboardKey(label: "ABC", code: 9000, keySize: .standard, imageName: "textformat.123")
         ]
      ]
   )
   @objc let rightKeyboardModel = EmulatorKeyboardViewModel(keys: [
      [
         
      ],
      [
         EmulatorKeyboardKey(label: "y", code: Int(RETROK_y.rawValue)),
         EmulatorKeyboardKey(label: "u", code: Int(RETROK_u.rawValue)),
         EmulatorKeyboardKey(label: "i", code: Int(RETROK_i.rawValue)),
         EmulatorKeyboardKey(label: "o", code: Int(RETROK_o.rawValue)),
         EmulatorKeyboardKey(label: "p", code: Int(RETROK_p.rawValue)),
      ],
      [
         EmulatorKeyboardKey(label: "h", code: Int(RETROK_h.rawValue)),
         EmulatorKeyboardKey(label: "j", code: Int(RETROK_j.rawValue)),
         EmulatorKeyboardKey(label: "k", code: Int(RETROK_k.rawValue)),
         EmulatorKeyboardKey(label: "l", code: Int(RETROK_l.rawValue)),
         EmulatorKeyboardKey(label: "'", code: Int(RETROK_QUOTE.rawValue))
      ],
      [
         EmulatorKeyboardKey(label: "n", code: Int(RETROK_n.rawValue)),
         EmulatorKeyboardKey(label: "m", code: Int(RETROK_m.rawValue)),
         EmulatorKeyboardKey(label: ",", code: Int(RETROK_COMMA.rawValue)),
         EmulatorKeyboardKey(label: ".", code: Int(RETROK_PERIOD.rawValue)),
         EmulatorKeyboardKey(label: "BKSPC", code: Int(RETROK_BACKSPACE.rawValue))
      ],
      [
         EmulatorKeyboardKey(label: "ALT", code: Int(RETROK_RALT.rawValue)),
         EmulatorKeyboardKey(label: "RETURN", code: Int(RETROK_RETURN.rawValue), keySize: .wide)
      ],
   ],
   alternateKeys: [
      [
         EmulatorKeyboardKey(label: "6", code: Int(RETROK_6.rawValue)),
         EmulatorKeyboardKey(label: "7", code: Int(RETROK_7.rawValue)),
         EmulatorKeyboardKey(label: "8", code: Int(RETROK_8.rawValue)),
         EmulatorKeyboardKey(label: "9", code: Int(RETROK_9.rawValue)),
         EmulatorKeyboardKey(label: "0", code: Int(RETROK_0.rawValue))
      ],
      [
         EmulatorKeyboardKey(label: "PAGEUP", code: Int(RETROK_PAGEUP.rawValue)),
         EmulatorKeyboardKey(label: "HOME", code: Int(RETROK_HOME.rawValue)),
         EmulatorKeyboardKey(label: "⬆️", code: Int(RETROK_UP.rawValue)),
         EmulatorKeyboardKey(label: "END", code: Int(RETROK_END.rawValue)),
         EmulatorKeyboardKey(label: "PAGEDWN", code: Int(RETROK_PAGEDOWN.rawValue)),
      ],
      [
         SpacerKey(),
         EmulatorKeyboardKey(label: "⬅️", code: Int(RETROK_LEFT.rawValue)),
         EmulatorKeyboardKey(label: "⬇️", code: Int(RETROK_DOWN.rawValue)),
         EmulatorKeyboardKey(label: "➡️", code: Int(RETROK_RIGHT.rawValue)),
         SpacerKey()
      ],
      [
         EmulatorKeyboardKey(label: "SPACE", code: Int(RETROK_SPACE.rawValue), keySize: .wide),
         EmulatorKeyboardKey(label: "DEL", code: Int(RETROK_DELETE.rawValue)),
      ],
      [
         EmulatorKeyboardKey(label: "RETURN", code: Int(RETROK_RETURN.rawValue), keySize: .wide)
      ]
   ])
    
    init() {
        super.init(nibName: nil, bundle: nil)
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        setupView()
//        setupViewFrames()
//        setupKeyModels()
        
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
        leftKeyboardView.heightAnchor.constraint(equalToConstant: 250).isActive = true
        leftKeyboardView.widthAnchor.constraint(equalToConstant: 175).isActive = true
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
        rightKeyboardView.heightAnchor.constraint(equalToConstant: 250).isActive = true
        rightKeyboardView.widthAnchor.constraint(equalToConstant: 175).isActive = true
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
    
    override func viewWillTransition(to size: CGSize, with coordinator: UIViewControllerTransitionCoordinator) {
        // get relative positions of frames to size
//        for v in [leftKeyboardView, rightKeyboardView] {
//            let xPercent = v.frame.origin.x / view.frame.size.width
//            let yPercent = v.frame.origin.y / view.frame.size.height
//            var newX = size.width * xPercent
//            var newY = size.height * yPercent
//            // mmm need to check if the views fit within the frame and adjust
//            if newX + v.bounds.size.width > size.width {
//                newX = size.width - v.bounds.size.width
//            } else if newX < 0 {
//                newX = 0
//            }
//            if newY + v.bounds.size.height > size.height {
//                newY = size.height - v.bounds.size.height
//            }
//            let newFrame = CGRect(x: newX, y: newY, width: v.bounds.size.width, height: v.bounds.size.height)
//            v.frame = newFrame
//        }
    }
    
    func setupKeyModels() {
        leftKeyboardView.setupWithModel(leftKeyboardModel)
        rightKeyboardView.setupWithModel(rightKeyboardModel)
    }
    
    @objc func draggedView(_ sender:UIPanGestureRecognizer){
        guard let keyboardView = sender.view?.superview else {
            return
        }
//        NSLayoutConstraint.deactivate(keyboardConstraints)
//        self.view.bringSubviewToFront(keyboardView)
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
