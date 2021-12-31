//
//  EmulatorTouchMouse.swift
//  RetroArchiOS
//
//  Created by Yoshi Sugawara on 12/27/21.
//  Copyright © 2021 RetroArch. All rights reserved.
//

import Combine
import UIKit

@objc protocol EmulatorTouchMouseHandlerDelegate: AnyObject {
   func handleMouseClick(isLeftClick: Bool, isPressed: Bool)
   func handleMouseMove(x: CGFloat, y: CGFloat)
}

@objcMembers public class EmulatorTouchMouseHandler: NSObject {
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

   let view: UIView
   weak var delegate: EmulatorTouchMouseHandlerDelegate?
   
   private let positionChangeThreshold: CGFloat = 20.0
   private let mouseHoldInterval: TimeInterval = 1.5
   
   private var pendingMouseEvents = [MouseClick]()
   private var mouseEventPublisher: AnyPublisher<MouseClick, Never> {
      mouseEventSubject.eraseToAnyPublisher()
   }
   private let mouseEventSubject = PassthroughSubject<MouseClick, Never>()
   private var subscription: AnyCancellable?
   
   private var primaryTouch: TouchInfo?
   private var secondaryTouch: TouchInfo?
   
   private let mediumHaptic = UIImpactFeedbackGenerator(style: .medium)
   private let lightHaptic = UIImpactFeedbackGenerator(style: .light)
   
   public init(view: UIView) {
      self.view = view
      super.init()
      setup()
   }
   
   private func setup() {
      subscription = mouseEventPublisher
         .sink(receiveValue: {[weak self] value in
//            print("mouseEventPublisher processing event: \(value)")
            self?.pendingMouseEvents.append(value)
            self?.processMouseEvents()
         })
   }
   
   private func processMouseEvents() {
//      print("processMouseEvents: mouse events = \(pendingMouseEvents)")
      guard let event = pendingMouseEvents.first else {
         return
      }
//      print("processing mouse event: \(event)")
      delegate?.handleMouseClick(isLeftClick: !event.isRightClick, isPressed: event.isPressed)
      if event.isPressed {
         DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) { [weak self] in
//            print("scheduling mouse event after 0.1 sec")
            self?.mouseEventSubject.send(MouseClick(isRightClick: event.isRightClick, isPressed: false))
//            self?.pendingMouseEvents.append(MouseClick(isRightClick: event.isRightClick, isPressed: false))
         }
      }
      pendingMouseEvents.removeFirst()
      processMouseEvents()
   }
   
   @objc private func beginHold() {
      guard let primaryTouch = primaryTouch, primaryTouch.holdState == .wait else {
         return
      }
      print("beginHold: hold down")
      self.primaryTouch = TouchInfo(touch: primaryTouch.touch, origin: primaryTouch.origin, holdState: .held)
      mediumHaptic.impactOccurred()
      delegate?.handleMouseClick(isLeftClick: true, isPressed: true)
   }
   
   private func endHold() {
      guard let primaryTouch = primaryTouch else { return }
      print("endhold: holdState = \(primaryTouch.holdState)")
      if primaryTouch.holdState == .notHeld {
         return
      }
      if primaryTouch.holdState == .wait {
         print("endHold: cancel hold")
         Thread.cancelPreviousPerformRequests(withTarget: self, selector: #selector(beginHold), object: self)
      } else {
         print("endHold: hold up")
         delegate?.handleMouseClick(isLeftClick: true, isPressed: false)
      }
      self.primaryTouch = TouchInfo(touch: primaryTouch.touch, origin: primaryTouch.origin, holdState: .notHeld)
//      lightHaptic.impactOccurred()
   }
   
   public func touchesBegan(touches: Set<UITouch>) {
      guard let touch = touches.first else {
         return
      }
      if primaryTouch == nil {
         print("primary touch began")
         primaryTouch = TouchInfo(touch: touch, origin: touch.location(in: view), holdState: .wait)
         self.perform(#selector(beginHold), with: nil, afterDelay: mouseHoldInterval)
      } else if secondaryTouch == nil {
         print("secondary touch began")
         secondaryTouch = TouchInfo(touch: touch, origin: touch.location(in: view), holdState: .notHeld)
      }
   }
   
   public func touchesEnded(touches: Set<UITouch>) {
      for touch in touches {
         if touch == primaryTouch?.touch {
            print("primary ended, tap count = \(touch.tapCount)")
            if touch.tapCount > 0 {
               for _ in 1...touch.tapCount {
                  mouseEventSubject.send(MouseClick(isRightClick: false, isPressed: true))
               }
//               pendingMouseEvents.append(MouseClick(isRightClick: false, isPressed: true))
            }
            endHold()
            primaryTouch = nil
            secondaryTouch = nil
         } else if touch == secondaryTouch?.touch {
            print("secondary ended, tap count = \(touch.tapCount)")
            if touch.tapCount > 0 {
//               pendingMouseEvents.append(MouseClick(isRightClick: true, isPressed: true))
               mouseEventSubject.send(MouseClick(isRightClick: true, isPressed: true))
               endHold()
            }
            secondaryTouch = nil
         }
      }
      delegate?.handleMouseMove(x: 0, y: 0)
   }
   
   public func touchesMoved(touches: Set<UITouch>) {
      for touch in touches {
         if touch == primaryTouch?.touch {
            let a = touch.previousLocation(in: view)
            let b = touch.location(in: view)
//            print("touchesMoved: primary move: \(b.x-a.x) \(b.y-a.y)")
            if primaryTouch?.holdState == .wait && (distanceBetween(pointA: a, pointB: b) > positionChangeThreshold) {
               endHold()
            }
            if primaryTouch?.holdState == .held {
//               print("mouse is held down...")
            }
            delegate?.handleMouseMove(x: b.x-a.x, y: b.y-a.y)
         }
      }
   }
   
   public func touchesCancelled(touches: Set<UITouch>) {
      print("touchesCancelled...")
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
}
