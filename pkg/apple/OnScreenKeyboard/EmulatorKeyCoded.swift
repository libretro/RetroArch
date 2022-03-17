//
//  EmulatorKeyCoded.swift
//  RetroArchiOS
//
//  Created by Yoshi Sugawara on 3/3/22.
//  Copyright © 2022 RetroArch. All rights reserved.
//

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
   weak var keyboardView: EmulatorKeyboardView?
   
   init(keySize: KeySize = .standard) {
      self.keySize = keySize
   }
   
   func createView() -> UIView {
      let slider = UISlider(frame: .zero)
      slider.minimumValue = 0.1
      slider.maximumValue = 1.0
      slider.addTarget(self, action: #selector(adjustKeyboardAlpha(_:)), for: .valueChanged)
      slider.value = 1.0
      let size = CGSize(width:EmulatorKeyboardView.keyNormalFont.pointSize, height:EmulatorKeyboardView.keyNormalFont.pointSize)
      slider.setThumbImage(UIImage.dot(size:size, color:EmulatorKeyboardView.keyNormalTextColor), for: .normal)
      return slider
   }
   @objc func adjustKeyboardAlpha(_ sender: UISlider) {
      keyboardView?.delegate?.updateTransparency(toAlpha: sender.value)
   }
}

