//
//  KeyboardButton.swift
//  RetroArchiOS
//
//  Created by Yoshi Sugawara on 3/3/22.
//  Copyright © 2022 RetroArch. All rights reserved.
//

import UIKit

class EmulatorKeyboardButton: UIButton {
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

   private func updateColors() {
        backgroundColor = isHighlighted ? EmulatorKeyboardView.keyPressedBackgroundColor : isSelected ? EmulatorKeyboardView.keySelectedBackgroundColor : EmulatorKeyboardView.keyNormalBackgroundColor
        layer.borderColor = (isHighlighted ? EmulatorKeyboardView.keyPressedBorderColor : isSelected ? EmulatorKeyboardView.keySelectedBorderColor : EmulatorKeyboardView.keyNormalBorderColor).cgColor
        titleLabel?.textColor = isHighlighted ? EmulatorKeyboardView.keyPressedTextColor : isSelected ? EmulatorKeyboardView.keySelectedTextColor : EmulatorKeyboardView.keyNormalTextColor
        titleLabel?.tintColor = titleLabel?.textColor
    }
   
    override open var isHighlighted: Bool {
        didSet {
           updateColors()
        }
    }
    
    override open var isSelected: Bool {
        didSet {
           updateColors()
        }
    }
    
    required init(key: KeyCoded) {
       self.key = key
       super.init(frame: .zero)
       updateColors()
    }
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
}
