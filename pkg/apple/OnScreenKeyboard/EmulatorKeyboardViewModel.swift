//
//  EmulatorKeyboardViewModel.swift
//  RetroArchiOS
//
//  Created by Yoshi Sugawara on 3/3/22.
//  Copyright © 2022 RetroArch. All rights reserved.
//

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
