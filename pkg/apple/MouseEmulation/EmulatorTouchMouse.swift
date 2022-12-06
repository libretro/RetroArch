//
//  EmulatorTouchMouse.swift
//  RetroArchiOS
//
//  Created by Yoshi Sugawara on 12/27/21.
//  Copyright © 2021 RetroArch. All rights reserved.
//

/**
 Touch mouse behavior:
 - Mouse movement: Pan finger around screen
 - Left click: Tap with one finger
 - Right click: Tap with two fingers (or hold with one finger and tap with another)
 - Click-and-drag: Double tap and hold for 1 second, then pan finger around screen to drag mouse
 
 Code adapted from iDOS/dospad: https://github.com/litchie/dospad
 */

import Combine
import UIKit

@objc public protocol EmulatorTouchMouseHandlerDelegate: AnyObject {
   func handleMouseClick(isLeftClick: Bool, isPressed: Bool)
   func handleMouseMove(x: CGFloat, y: CGFloat)
   func handlePointerMove(x: CGFloat, y: CGFloat)
}

@objcMembers public class EmulatorTouchMouseHandler: NSObject, UIPointerInteractionDelegate {
   enum MouseHoldState {
      case notHeld, wait, held
   }

   struct MouseClick {
      var isRightClick = false
      var isPressed = false
   }
   
   struct TouchInfo {
      let touch: UITouch
      let origin: CGPoint
      let holdState: MouseHoldState
   }

   var enabled = false
   
   let view: UIView
   weak var delegate: EmulatorTouchMouseHandlerDelegate?
   
   private let positionChangeThreshold: CGFloat = 20.0
   private let mouseHoldInterval: TimeInterval = 1.0
   
   private var pendingMouseEvents = [MouseClick]()
   private var mouseEventPublisher: AnyPublisher<MouseClick, Never> {
      mouseEventSubject.eraseToAnyPublisher()
   }
   private let mouseEventSubject = PassthroughSubject<MouseClick, Never>()
   private var subscription: AnyCancellable?
   
   private var primaryTouch: TouchInfo?
   private var secondaryTouch: TouchInfo?
   
   private let mediumHaptic = UIImpactFeedbackGenerator(style: .medium)
   
   public init(view: UIView, delegate: EmulatorTouchMouseHandlerDelegate? = nil) {
      self.view = view
      self.delegate = delegate
      super.init()
      setup()
   }
   
   private func setup() {
      subscription = mouseEventPublisher
         .sink(receiveValue: {[weak self] value in
            self?.pendingMouseEvents.append(value)
            self?.processMouseEvents()
         })
      if #available(iOS 13.4, *) {
         // get pointer interactions
         let pointerInteraction = UIPointerInteraction(delegate: self)
         self.view.addInteraction(pointerInteraction)
         self.view.isUserInteractionEnabled=true
      }
   }
   
   private func processMouseEvents() {
      guard let event = pendingMouseEvents.first else {
         return
      }
      delegate?.handleMouseClick(isLeftClick: !event.isRightClick, isPressed: event.isPressed)
      if event.isPressed {
         DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) { [weak self] in
            self?.mouseEventSubject.send(MouseClick(isRightClick: event.isRightClick, isPressed: false))
         }
      }
      pendingMouseEvents.removeFirst()
      processMouseEvents()
   }
   
   @objc private func beginHold() {
      guard let primaryTouch = primaryTouch, primaryTouch.holdState == .wait else {
         return
      }
      self.primaryTouch = TouchInfo(touch: primaryTouch.touch, origin: primaryTouch.origin, holdState: .held)
      mediumHaptic.impactOccurred()
      delegate?.handleMouseClick(isLeftClick: true, isPressed: true)
   }
   
   private func endHold() {
      guard let primaryTouch = primaryTouch else { return }
      if primaryTouch.holdState == .notHeld {
         return
      }
      if primaryTouch.holdState == .wait {
         Thread.cancelPreviousPerformRequests(withTarget: self, selector: #selector(beginHold), object: self)
      } else {
         delegate?.handleMouseClick(isLeftClick: true, isPressed: false)
      }
      self.primaryTouch = TouchInfo(touch: primaryTouch.touch, origin: primaryTouch.origin, holdState: .notHeld)
   }
   
   public func touchesBegan(touches: Set<UITouch>, event: UIEvent?) {
      guard enabled, let touch = touches.first else {
         if #available(iOS 13.4, *), let _ = touches.first {
            let isLeftClick=(event?.buttonMask == UIEvent.ButtonMask.button(1))
            delegate?.handleMouseClick(isLeftClick: isLeftClick, isPressed: true)
         }
         return
      }
      if primaryTouch == nil {
         primaryTouch = TouchInfo(touch: touch, origin: touch.location(in: view), holdState: .wait)
         if touch.tapCount == 2 {
            self.perform(#selector(beginHold), with: nil, afterDelay: mouseHoldInterval)
         }
      } else if secondaryTouch == nil {
         secondaryTouch = TouchInfo(touch: touch, origin: touch.location(in: view), holdState: .notHeld)
      }
   }
   
   public func touchesEnded(touches: Set<UITouch>, event: UIEvent?) {
      guard enabled else {
         if #available(iOS 13.4, *) {
            let isLeftClick=(event?.buttonMask == UIEvent.ButtonMask.button(1))
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) { [weak self] in
               self?.delegate?.handleMouseClick(isLeftClick: isLeftClick, isPressed: false)
            }
         }
         return
      }
      for touch in touches {
         if touch == primaryTouch?.touch {
            if touch.tapCount > 0 {
               for _ in 1...touch.tapCount {
                  mouseEventSubject.send(MouseClick(isRightClick: false, isPressed: true))
               }
            }
            endHold()
            primaryTouch = nil
            secondaryTouch = nil
         } else if touch == secondaryTouch?.touch {
            if touch.tapCount > 0 {
               mouseEventSubject.send(MouseClick(isRightClick: true, isPressed: true))
               endHold()
            }
            secondaryTouch = nil
         }
      }
      delegate?.handleMouseMove(x: 0, y: 0)
   }
   
   public func touchesMoved(touches: Set<UITouch>) {
      guard enabled else { return }
      for touch in touches {
         if touch == primaryTouch?.touch {
            let a = touch.previousLocation(in: view)
            let b = touch.location(in: view)
            if primaryTouch?.holdState == .wait && (distanceBetween(pointA: a, pointB: b) > positionChangeThreshold) {
               endHold()
            }
            delegate?.handleMouseMove(x: b.x-a.x, y: b.y-a.y)
         }
      }
   }
   
   public func touchesCancelled(touches: Set<UITouch>, event: UIEvent?) {
      guard enabled else {
         if #available(iOS 13.4, *) {
            let isLeftClick=(event?.buttonMask == UIEvent.ButtonMask.button(1))
            delegate?.handleMouseClick(isLeftClick: isLeftClick, isPressed: false)
         }
         return
      }
      for touch in touches {
         if touch == primaryTouch?.touch {
            endHold()
         }
      }
      primaryTouch = nil
      secondaryTouch = nil
   }
   
   func distanceBetween(pointA: CGPoint, pointB: CGPoint) -> CGFloat {
      let dx = pointA.x - pointB.x
      let dy = pointA.y - pointB.y
      return sqrt(dx*dx*dy*dy)
   }

   @available(iOS 13.4, *)
   public func pointerInteraction(
       _ interaction: UIPointerInteraction,
       regionFor request: UIPointerRegionRequest,
       defaultRegion: UIPointerRegion
     ) -> UIPointerRegion? {
        guard !enabled else { return defaultRegion }
        let location = request.location;
        delegate?.handlePointerMove(x: location.x, y: location.y)
        return defaultRegion
   }
}
