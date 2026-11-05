// RetroArch - A frontend for libretro.
// SPDX-License-Identifier: GPL-3.0-or-later

#if os(iOS) && HAVE_IOS_MENU
import SwiftUI
import UIKit

@available(iOS 16.0, *)
private struct NativeMenuTab: Identifiable {
   let id: Int
   let key: String
   let title: String
   let icon: String

   static let all = [
      NativeMenuTab(id: Int(IOS_MENU_TAB_LIBRARY.rawValue), key: "library", title: "Library", icon: "square.stack"),
      NativeMenuTab(id: Int(IOS_MENU_TAB_HISTORY.rawValue), key: "history", title: "History", icon: "clock"),
      NativeMenuTab(id: Int(IOS_MENU_TAB_FAVORITES.rawValue), key: "favorites", title: "Favorites", icon: "star"),
      NativeMenuTab(id: Int(IOS_MENU_TAB_SETTINGS.rawValue), key: "settings", title: "Settings", icon: "gear"),
      NativeMenuTab(id: Int(IOS_MENU_TAB_QUICK.rawValue), key: "quick", title: "Quick Menu", icon: "gamecontroller")
   ]
}

@available(iOS 16.0, *)
private struct NativeMenuRow: Identifiable {
   let id: Int
   let type: UInt32
   let title: String
   let value: String
   let sublabel: String
   let enabled: Bool
   let boolean: Bool
   let checked: Bool
   let adjustable: Bool

   init?(_ data: [String: Any]) {
      guard let index = data["index"] as? NSNumber,
            let type = data["type"] as? NSNumber,
            let title = data["title"] as? String else { return nil }
      id = index.intValue
      self.type = type.uint32Value
      self.title = title
      value = data["value"] as? String ?? ""
      sublabel = data["sublabel"] as? String ?? ""
      enabled = data["enabled"] as? Bool ?? false
      boolean = data["boolean"] as? Bool ?? false
      checked = data["checked"] as? Bool ?? false
      adjustable = data["adjustable"] as? Bool ?? false
   }
}

@available(iOS 16.0, *)
private final class NativeMenuModel: ObservableObject {
   let session: UInt64
   @Published var revision: UInt64 = 0
   @Published var title = "RetroArch"
   @Published var rows: [NativeMenuRow] = []
   @Published var drivers: [(index: Int, name: String)] = []
   @Published var driver = "ios"
   @Published var pointer = false
   @Published var canGoBack = false
   @Published var offset = 0
   @Published var total = 0
   @Published var selection = 0
   @Published var busy = true
   @Published var dialog = ""
   @Published var message = ""
   @Published var settingsOpen = false
   @Published var labels: [String: String] = [:]
   @Published var tab = Int(IOS_MENU_TAB_LIBRARY.rawValue)
   @Published var tabs = 15

   init(session: UInt64) { self.session = session }

   func label(_ key: String, fallback: String) -> String {
      labels[key] ?? fallback
   }

   func send(_ action: ios_menu_action, row: NativeMenuRow? = nil, index: Int = 0) {
      guard !busy else { return }
      if ios_menu_request(session, revision, row?.id ?? index, row?.type ?? 0,
            action.rawValue) {
         busy = true
      } else {
         message = "The menu has changed. Please try again."
      }
   }

   func selectTab(_ tab: Int) {
      guard !busy else { return }
      settingsOpen = false
      send(IOS_MENU_TAB, index: tab)
   }

   func publish(_ data: NSDictionary) {
      guard (data["session"] as? NSNumber)?.uint64Value == session else { return }
      revision = (data["revision"] as? NSNumber)?.uint64Value ?? 0
      title = data["title"] as? String ?? "RetroArch"
      rows = (data["rows"] as? [[String: Any]] ?? []).compactMap(NativeMenuRow.init)
      labels = data["labels"] as? [String: String] ?? [:]
      let newTab = (data["tab"] as? NSNumber)?.intValue ?? 0
      if tab != newTab { settingsOpen = false }
      tab = newTab
      tabs = (data["tabs"] as? NSNumber)?.intValue ?? 1
      drivers = (data["drivers"] as? [[String: Any]] ?? []).compactMap {
         guard let index = $0["index"] as? NSNumber,
               let name = $0["name"] as? String else { return nil }
         return (index.intValue, name)
      }
      driver = data["driver"] as? String ?? "ios"
      pointer = data["pointer"] as? Bool ?? false
      canGoBack = data["back"] as? Bool ?? false
      offset = (data["offset"] as? NSNumber)?.intValue ?? 0
      total = (data["total"] as? NSNumber)?.intValue ?? 0
      busy = false
   }
}

@available(iOS 16.0, *)
private struct NativeMenuSettingsView: View {
   @ObservedObject var model: NativeMenuModel

   var body: some View {
      Form {
         Section(header: Text(model.label("driver", fallback: "Menu Driver")),
               footer: Text("Save the configuration, then close and reopen RetroArch to use the selected menu.")) {
            ForEach(model.drivers, id: \.index) { driver in
               Button(action: { self.model.send(IOS_MENU_SET_DRIVER, index: driver.index) }) {
                  HStack {
                     Text(driver.name)
                     Spacer()
                     if self.model.driver == driver.name {
                        Image(systemName: "checkmark")
                     }
                  }
               }
            }
         }
         Section {
            Toggle(model.label("touch", fallback: "Touch Support"), isOn: Binding(
               get: { self.model.pointer },
               set: { _ in self.model.send(IOS_MENU_POINTER) }))
            Button(model.label("save", fallback: "Save Current Configuration")) {
               self.model.send(IOS_MENU_SAVE_CONFIG)
            }
         }
         if !model.message.isEmpty {
            Section { Text(model.message) }
         }
      }
      .disabled(model.busy)
      .navigationTitle(model.label("settings", fallback: "Menu Settings"))
      .navigationBarTitleDisplayMode(.inline)
      .toolbarBackground(Color(UIColor.systemGroupedBackground), for: .navigationBar)
      .toolbarBackground(.visible, for: .navigationBar)
      .onAppear { ios_menu_settings_visible(self.model.session, true) }
      .onDisappear { ios_menu_settings_visible(self.model.session, false) }
   }
}

@available(iOS 16.0, *)
private struct NativeMenuView: View {
   @ObservedObject var model: NativeMenuModel

   private func row(_ entry: NativeMenuRow) -> some View {
      HStack {
         Button(action: { self.model.send(IOS_MENU_OK, row: entry) }) {
            HStack {
               VStack(alignment: .leading, spacing: 4) {
                  Text(entry.title).foregroundColor(.primary)
                  if !entry.sublabel.isEmpty {
                     Text(entry.sublabel).font(.caption).foregroundColor(.secondary)
                  }
               }
               Spacer()
               if !entry.boolean {
                  if !entry.value.isEmpty && entry.value != "..." {
                     Text(entry.value).foregroundColor(.secondary).multilineTextAlignment(.trailing)
                  }
                  if entry.checked { Image(systemName: "checkmark") }
                  if entry.enabled && !entry.adjustable && !entry.checked {
                     Image(systemName: "chevron.right").font(.caption).foregroundColor(.secondary)
                  }
               }
            }
         }.buttonStyle(BorderlessButtonStyle())
         if entry.boolean {
            Toggle(entry.title, isOn: Binding(
               get: { entry.checked },
               set: { _ in self.model.send(IOS_MENU_RIGHT, row: entry) }))
               .labelsHidden()
         } else {
            if entry.adjustable {
               Button(action: { self.model.send(IOS_MENU_LEFT, row: entry) }) {
                  Image(systemName: "minus.circle")
               }.buttonStyle(BorderlessButtonStyle())
               Button(action: { self.model.send(IOS_MENU_RIGHT, row: entry) }) {
                  Image(systemName: "plus.circle")
               }.buttonStyle(BorderlessButtonStyle())
            }
         }
      }
      .padding(.vertical, 4)
      .listRowBackground(entry.id == model.selection
         ? Color.accentColor.opacity(0.12) : Color.clear)
   }

   private var menuList: some View {
      List {
         if !model.message.isEmpty {
            Section {
               Text(model.message).font(.callout)
               Button("Dismiss") { self.model.message = "" }
            }
         }
         if !model.dialog.isEmpty {
            Section {
               Text(model.dialog)
               Button(model.label("confirm", fallback: "Continue")) { self.model.send(IOS_MENU_CONFIRM) }
               Button(model.label("back", fallback: "Back")) { self.model.send(IOS_MENU_BACK) }
            }
         } else {
            ForEach(model.rows) { entry in
               self.row(entry).id(entry.id)
                  .disabled(!entry.enabled || self.model.busy)
            }
            if model.total > model.rows.count {
               Section(footer: Text("\(model.offset + 1)–\(model.offset + model.rows.count) / \(model.total)")) {
                  if model.offset > 0 {
                     Button("Previous Page") { self.model.send(IOS_MENU_PREVIOUS_PAGE) }
                  }
                  if model.offset + model.rows.count < model.total {
                     Button("Next Page") { self.model.send(IOS_MENU_NEXT_PAGE) }
                  }
               }.disabled(model.busy)
            }
         }
      }
         .listStyle(GroupedListStyle())
         .navigationTitle(model.title)
         .navigationBarTitleDisplayMode(.inline)
         .toolbarBackground(Color(UIColor.systemGroupedBackground), for: .navigationBar)
         .toolbarBackground(.visible, for: .navigationBar)
         .toolbar {
            ToolbarItem(placement: .navigationBarLeading) {
               Button(action: {
                  self.model.send(self.model.canGoBack ? IOS_MENU_BACK : IOS_MENU_HOME)
               }) {
                  if model.canGoBack {
                     Label(model.label("back", fallback: "Back"), systemImage: "chevron.backward")
                        .labelStyle(.titleAndIcon)
                  } else {
                     Label(model.label("home", fallback: "Main Menu"), systemImage: "line.3.horizontal")
                        .labelStyle(.iconOnly)
                  }
               }.disabled(model.busy)
            }
            ToolbarItem(placement: .navigationBarTrailing) {
               Button(action: { self.model.settingsOpen = true }) {
                  Label(model.label("settings", fallback: "Menu Settings"), systemImage: "gearshape")
                     .labelStyle(.iconOnly)
               }
            }
         }
         .navigationDestination(isPresented: $model.settingsOpen) {
            NativeMenuSettingsView(model: model)
         }
   }

   private var navigationView: some View {
      NavigationStack {
         ScrollViewReader { proxy in
            menuList
               .onChange(of: model.selection) { index in
                  proxy.scrollTo(index, anchor: .center)
               }
               .onChange(of: model.revision) { _ in
                  proxy.scrollTo(model.selection, anchor: .center)
               }
         }
      }
      .background(Color(UIColor.systemGroupedBackground).ignoresSafeArea())
      .tint(.accentColor)
   }

   var body: some View {
      TabView(selection: Binding(get: { self.model.tab },
            set: { self.model.selectTab($0) })) {
         ForEach(NativeMenuTab.all.filter { model.tabs & (1 << $0.id) != 0 }) { tab in
            Group {
               if self.model.tab == tab.id {
                  self.navigationView
               } else {
                  Color(UIColor.systemGroupedBackground)
               }
            }
            .tabItem {
               Image(systemName: tab.icon)
               Text(self.model.label(tab.key, fallback: tab.title))
            }
            .tag(tab.id)
         }
      }
   }
}

@available(iOS 16.0, *)
@objc(RetroArchNativeMenuController)
final class RetroArchNativeMenuController: UIViewController {
   private let model: NativeMenuModel

   @objc(initWithSession:)
   init(session: UInt64) {
      model = NativeMenuModel(session: session)
      super.init(nibName: nil, bundle: nil)
   }

   required init?(coder: NSCoder) { return nil }

   override func viewDidLoad() {
      super.viewDidLoad()
      view.backgroundColor = .systemGroupedBackground
      let host = UIHostingController(rootView: NativeMenuView(model: model))
      addChild(host)
      host.view.frame = view.bounds
      host.view.autoresizingMask = [.flexibleWidth, .flexibleHeight]
      view.addSubview(host.view)
      host.didMove(toParent: self)
   }

   @objc func publish(_ snapshot: NSDictionary) { model.publish(snapshot) }

   @objc(updateSelection:busy:dialog:)
   func update(selection: Int, busy: Bool, dialog: String) {
      if model.selection != selection { model.selection = selection }
      if model.busy != busy { model.busy = busy }
      if model.dialog != dialog { model.dialog = dialog }
   }

   @objc func showMessage(_ message: String) {
      if model.message != message { model.message = message }
   }

   @objc func closeSettings() { model.settingsOpen = false }
}
#endif
