//
//  RetroArchWidgetExtension.swift
//  RetroArchWidgetExtension
//
//  Created by Yoshi Sugawara on 10/10/22.
//  Copyright © 2022 RetroArch. All rights reserved.
//

import WidgetKit
import SwiftUI
import Intents

struct DummyProvider: TimelineProvider {
   func getSnapshot(in context: Context, completion: @escaping (DummyEntry) -> Void) {
      completion(DummyEntry())
   }

   func getTimeline(in context: Context, completion: @escaping (Timeline<DummyEntry>) -> Void) {
      completion(Timeline(entries: [DummyEntry()], policy: .never))
   }
   
   func placeholder(in context: Context) -> DummyEntry {
      DummyEntry()
   }
}

struct DummyEntry: TimelineEntry {
   let date = Date()
}

struct SimpleEntry: TimelineEntry {
    let date: Date
}

struct RetroArchImageView : View {
    var body: some View {
       ZStack {
          AccessoryWidgetBackground()
          Image("logo")
       }
    }
}

@main
struct RetroArchWidgetExtension: Widget {
    let kind: String = "RetroArchWidgetExtension"

    var body: some WidgetConfiguration {
       if #available(iOSApplicationExtension 16.0, *) {
          return StaticConfiguration(kind: kind, provider: DummyProvider()) { _ in
             RetroArchImageView()
          }
          .configurationDisplayName("Icon")
          .description("Launch RetroArch.")
          .supportedFamilies([.accessoryCircular])
       } else {
          return EmptyWidgetConfiguration()
       }
    }
}

struct RetroArchWidgetExtension_Previews: PreviewProvider {
    static var previews: some View {
       RetroArchImageView()           .previewContext(WidgetPreviewContext(family: .accessoryCircular))
    }
}
