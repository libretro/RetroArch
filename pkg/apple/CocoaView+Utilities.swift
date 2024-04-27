//
//  CocoaView+Utilities.swift
//  RetroArchiOS
//
//  Created by Yoshi Sugawara on 3/2/22.
//  Copyright © 2022 RetroArch. All rights reserved.
//

extension CocoaView {
   
   // A native swift wrapper around displaying notifications
   @objc func showRetroArchNotification(
      title: String? = nil,
      message: String,
      icon: message_queue_icon = MESSAGE_QUEUE_ICON_DEFAULT,
      category: message_queue_category = MESSAGE_QUEUE_CATEGORY_INFO
   ) {
      guard let messageCString = message.cString(using: .utf8) else { return }
      let titleCString: UnsafeMutablePointer<CChar>? = {
         if let title = title {
            let str = title as NSString
            return UnsafeMutablePointer<CChar>(mutating: str.utf8String)
         }
         return nil
      }()
      runloop_msg_queue_push(messageCString, 1, 100, true, titleCString, icon, category)
   }
}
