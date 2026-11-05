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
      NativeMenuTab(id: Int(IOS_MENU_TAB_MAIN.rawValue), key: "home", title: "Main Menu", icon: "house"),
      NativeMenuTab(id: Int(IOS_MENU_TAB_LIBRARY.rawValue), key: "library", title: "Game Library", icon: "square.stack"),
      NativeMenuTab(id: Int(IOS_MENU_TAB_SETTINGS.rawValue), key: "settings", title: "Settings", icon: "gearshape")
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
private struct NativeMenuPage {
   let depth: Int
   let revision: UInt64
   let title: String
   let rows: [NativeMenuRow]
   let offset: Int
   let total: Int
}

@available(iOS 16.0, *)
private final class NativeMenuModel: ObservableObject {
   let session: UInt64
   @Published var revision: UInt64 = 0
   @Published var pages: [NativeMenuPage] = []
   @Published var path: [Int] = []
   @Published var selection = 0
   @Published var busy = true
   @Published var dialog = ""
   @Published var message = ""
   @Published var labels: [String: String] = [:]
   @Published var tab = Int(IOS_MENU_TAB_MAIN.rawValue)
   @Published var tabs = 7
   @Published var running = false
   private(set) var depth = 0
   private var returnDepth: Int?

   init(session: UInt64) { self.session = session }

   func label(_ key: String, fallback: String) -> String {
      labels[key] ?? fallback
   }

   @discardableResult
   func send(_ action: ios_menu_action, row: NativeMenuRow? = nil, index: Int = 0,
         revision: UInt64? = nil) -> Bool {
      guard !busy else { return false }
      if ios_menu_request(session, revision ?? self.revision, row?.id ?? index,
            row?.type ?? 0, action.rawValue) {
         busy = true
         return true
      }
      message = "The menu has changed. Please try again."
      return false
   }

   func selectTab(_ tab: Int) {
      guard !busy, self.tab != tab else { return }
      send(IOS_MENU_TAB, index: tab)
   }

   private var enginePath: [Int] {
      depth > 0 ? Array(1...depth) : []
   }

   private func returnToPage() {
      guard let target = returnDepth, depth > target else { return }
      if !send(IOS_MENU_BACK) {
         returnDepth = nil
         path = enginePath
      }
   }

   func navigate(_ path: [Int]) {
      guard !busy else { return }
      let target = path.last ?? 0
      guard target <= depth else { return }
      self.path = path
      if target < depth {
         returnDepth = target
         returnToPage()
      }
   }

   func update(selection: Int, busy: Bool, dialog: String) {
      if self.selection != selection { self.selection = selection }
      if self.dialog != dialog { self.dialog = dialog }
      if self.busy && !busy && returnDepth != nil {
         returnDepth = nil
         path = enginePath
      }
      if self.busy != busy { self.busy = busy }
   }

   func publish(_ data: NSDictionary) {
      guard (data["session"] as? NSNumber)?.uint64Value == session else { return }
      let oldDepth = depth
      let newTab = (data["tab"] as? NSNumber)?.intValue ?? 0
      let changedTab = tab != newTab
      if changedTab {
         path = []
         pages = []
         returnDepth = nil
      }
      revision = (data["revision"] as? NSNumber)?.uint64Value ?? 0
      depth = (data["depth"] as? NSNumber)?.intValue ?? 0
      labels = data["labels"] as? [String: String] ?? [:]
      tab = newTab
      tabs = (data["tabs"] as? NSNumber)?.intValue ?? 7
      running = data["running"] as? Bool ?? false
      var title = data["title"] as? String ?? "RetroArch"
      if depth == 0, tab == Int(IOS_MENU_TAB_LIBRARY.rawValue) {
         title = label("library", fallback: "Game Library")
      } else if depth == 0, tab == Int(IOS_MENU_TAB_MAIN.rawValue) {
         title = label("home", fallback: "Main Menu")
      }
      if pages.count > depth { pages.removeSubrange(depth...) }
      while pages.count < depth {
         pages.append(NativeMenuPage(depth: pages.count, revision: 0,
            title: "RetroArch", rows: [], offset: 0, total: 0))
      }
      pages.append(NativeMenuPage(depth: depth, revision: revision, title: title,
         rows: (data["rows"] as? [[String: Any]] ?? []).compactMap(NativeMenuRow.init),
         offset: (data["offset"] as? NSNumber)?.intValue ?? 0,
         total: (data["total"] as? NSNumber)?.intValue ?? 0))
      busy = false
      if let target = returnDepth {
         if depth <= target {
            returnDepth = nil
            path = enginePath
         } else if depth < oldDepth {
            returnToPage()
         } else {
            returnDepth = nil
            path = enginePath
         }
      } else {
         path = enginePath
      }
   }
}

@available(iOS 16.0, *)
private struct NativeMenuPageView: View {
   @ObservedObject var model: NativeMenuModel
   let page: NativeMenuPage

   private var active: Bool { page.depth == model.depth && page.revision == model.revision }

   private func row(_ entry: NativeMenuRow) -> some View {
      HStack {
         Button(action: { self.model.send(IOS_MENU_OK, row: entry, revision: self.page.revision) }) {
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
               set: { _ in self.model.send(IOS_MENU_RIGHT, row: entry, revision: self.page.revision) }))
               .labelsHidden()
         } else {
            if entry.adjustable {
               Button(action: { self.model.send(IOS_MENU_LEFT, row: entry, revision: self.page.revision) }) {
                  Image(systemName: "minus.circle")
               }.buttonStyle(BorderlessButtonStyle())
               Button(action: { self.model.send(IOS_MENU_RIGHT, row: entry, revision: self.page.revision) }) {
                  Image(systemName: "plus.circle")
               }.buttonStyle(BorderlessButtonStyle())
            }
         }
      }
      .padding(.vertical, 4)
      .listRowBackground(entry.id == model.selection
         ? Color.accentColor.opacity(0.12) : Color.clear)
   }

   var body: some View {
      List {
         if !model.message.isEmpty {
            Section {
               Text(model.message).font(.callout)
               Button("Dismiss") { self.model.message = "" }
            }
         }
         if active && !model.dialog.isEmpty {
            Section {
               Text(model.dialog)
               Button(model.label("confirm", fallback: "Continue")) { self.model.send(IOS_MENU_CONFIRM) }
               Button(model.label("back", fallback: "Back")) { self.model.send(IOS_MENU_BACK) }
            }
         } else {
            if page.depth == 0 && model.tab == Int(IOS_MENU_TAB_MAIN.rawValue) && model.running {
               Section {
                  Button(action: { self.model.send(IOS_MENU_RESUME, revision: self.page.revision) }) {
                     Label(model.label("resume", fallback: "Resume"), systemImage: "play.fill")
                  }.disabled(!active || model.busy)
               }
            }
            ForEach(page.rows) { entry in
               self.row(entry).id(entry.id)
                  .disabled(!entry.enabled || !active || self.model.busy)
            }
            if page.total > page.rows.count {
               Section(footer: Text("\(page.offset + 1)–\(page.offset + page.rows.count) / \(page.total)")) {
                  if page.offset > 0 {
                     Button("Previous Page") { self.model.send(IOS_MENU_PREVIOUS_PAGE, revision: self.page.revision) }
                  }
                  if page.offset + page.rows.count < page.total {
                     Button("Next Page") { self.model.send(IOS_MENU_NEXT_PAGE, revision: self.page.revision) }
                  }
               }.disabled(!active || model.busy)
            }
         }
      }
      .listStyle(GroupedListStyle())
      .navigationTitle(page.title)
      .navigationBarTitleDisplayMode(.inline)
      .toolbarBackground(Color(UIColor.systemGroupedBackground), for: .navigationBar)
      .toolbarBackground(.visible, for: .navigationBar)
   }
}

@available(iOS 16.0, *)
private struct NativeMenuView: View {
   @ObservedObject var model: NativeMenuModel

   @ViewBuilder
   private func page(_ depth: Int) -> some View {
      if model.pages.indices.contains(depth) {
         ScrollViewReader { proxy in
            NativeMenuPageView(model: model, page: model.pages[depth])
               .onChange(of: model.selection) { index in
                  if depth == self.model.depth { proxy.scrollTo(index, anchor: .center) }
               }
               .onChange(of: model.revision) { _ in
                  if depth == self.model.depth { proxy.scrollTo(self.model.selection, anchor: .center) }
               }
         }
      } else {
         ProgressView()
      }
   }

   private var navigationView: some View {
      NavigationStack(path: Binding(get: { self.model.path }, set: { self.model.navigate($0) })) {
         page(0)
            .navigationDestination(for: Int.self) { depth in
               self.page(depth)
            }
      }
      .background(Color(UIColor.systemGroupedBackground).ignoresSafeArea())
      .tint(.accentColor)
   }

   var body: some View {
      TabView(selection: Binding(get: { self.model.tab }, set: { self.model.selectTab($0) })) {
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
      model.update(selection: selection, busy: busy, dialog: dialog)
   }

   @objc func showMessage(_ message: String) {
      if model.message != message { model.message = message }
   }
}
#endif
