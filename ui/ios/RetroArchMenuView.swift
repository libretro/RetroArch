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
   let submenu: Bool
   let inputKind: Int
   let inputValue: String
   let inputLimit: Int
   let minimum: Double?
   let maximum: Double?

   var editable: Bool { inputKind != Int(IOS_MENU_VALUE_NONE.rawValue) }

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
      submenu = data["submenu"] as? Bool ?? false
      inputKind = (data["inputKind"] as? NSNumber)?.intValue ?? 0
      inputValue = data["inputValue"] as? String ?? ""
      inputLimit = (data["inputLimit"] as? NSNumber)?.intValue ?? 0
      minimum = (data["minimum"] as? NSNumber)?.doubleValue
      maximum = (data["maximum"] as? NSNumber)?.doubleValue
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
private struct NativeMenuEdit: Identifiable {
   let row: NativeMenuRow
   let revision: UInt64
   var id: Int { row.id }
}

@available(iOS 16.0, *)
private final class NativeMenuModel: ObservableObject {
   let session: UInt64
   @Published var revision: UInt64 = 0
   @Published var pages: [NativeMenuPage] = []
   @Published var path: [Int] = []
   @Published var selection = 0
   @Published private(set) var focusRevision: UInt64 = 0
   @Published var busy = true
   @Published var dialog = ""
   @Published private(set) var message = ""
   @Published var labels: [String: String] = [:]
   @Published var tab = Int(IOS_MENU_TAB_MAIN.rawValue)
   @Published var tabs = 7
   @Published var running = false
   private(set) var depth = 0
   private var returnDepth: Int?
   private var nativeActionPending = false
   private var messageRevision: UInt64 = 0

   init(session: UInt64) { self.session = session }

   func label(_ key: String, fallback: String) -> String {
      labels[key] ?? fallback
   }

   func showMessage(_ message: String) {
      guard self.message != message else { return }
      self.message = message
      messageRevision &+= 1
      let revision = messageRevision
      DispatchQueue.main.asyncAfter(deadline: .now() + 4) { [weak self] in
         guard let self = self, self.messageRevision == revision else { return }
         self.message = ""
      }
   }

   @discardableResult
   func send(_ action: ios_menu_action, row: NativeMenuRow? = nil, index: Int = 0,
         revision: UInt64? = nil) -> Bool {
      guard !busy else { return false }
      if ios_menu_request(session, revision ?? self.revision, row?.id ?? index,
            row?.type ?? 0, action.rawValue) {
         busy = true
         nativeActionPending = true
         return true
      }
      showMessage("The menu has changed. Please try again.")
      return false
   }

   func setValue(_ value: String, row: NativeMenuRow, revision: UInt64) -> Bool {
      guard !busy else { return false }
      if ios_menu_set_value(session, revision, row.id, row.type, value) {
         busy = true
         nativeActionPending = true
         return true
      }
      showMessage("The menu has changed. Please try again.")
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
      if self.selection != selection {
         self.selection = selection
         if !busy && !nativeActionPending { focusRevision &+= 1 }
      }
      if self.dialog != dialog { self.dialog = dialog }
      if self.busy && !busy && returnDepth != nil {
         returnDepth = nil
         path = enginePath
      }
      if self.busy != busy { self.busy = busy }
      if !busy { nativeActionPending = false }
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
private struct NativeMenuValueEditor: View {
   @ObservedObject var model: NativeMenuModel
   let row: NativeMenuRow
   let revision: UInt64
   @Environment(\.dismiss) private var dismiss
   @State private var value: String
   @State private var color: Color
   @FocusState private var focused: Bool

   init(model: NativeMenuModel, row: NativeMenuRow, revision: UInt64) {
      self.model = model
      self.row = row
      self.revision = revision
      _value = State(initialValue: row.inputValue)
      let rgb = UInt32(row.inputValue) ?? 0
      _color = State(initialValue: Color(red: Double((rgb >> 16) & 255) / 255,
         green: Double((rgb >> 8) & 255) / 255, blue: Double(rgb & 255) / 255))
   }

   private var submittedValue: String? {
      var result = value
      var number: Double?
      switch row.inputKind {
         case Int(IOS_MENU_VALUE_INT.rawValue):
            guard let parsed = Int32(value) else { return nil }
            result = String(parsed)
            number = Double(parsed)
         case Int(IOS_MENU_VALUE_UINT.rawValue):
            guard let parsed = UInt32(value) else { return nil }
            result = String(parsed)
            number = Double(parsed)
         case Int(IOS_MENU_VALUE_SIZE.rawValue):
            guard let parsed = UInt64(value) else { return nil }
            result = String(parsed)
            number = Double(parsed)
         case Int(IOS_MENU_VALUE_FLOAT.rawValue):
            guard let parsed = Float(value.replacingOccurrences(of: ",", with: ".")),
                  parsed.isFinite else { return nil }
            result = String(parsed)
            number = Double(parsed)
         case Int(IOS_MENU_VALUE_COLOR.rawValue):
            var red: CGFloat = 0
            var green: CGFloat = 0
            var blue: CGFloat = 0
            UIColor(color).getRed(&red, green: &green, blue: &blue, alpha: nil)
            return String((UInt32((red * 255).rounded()) << 16)
               | (UInt32((green * 255).rounded()) << 8) | UInt32((blue * 255).rounded()))
         default:
            if row.inputLimit > 0 && value.utf8.count >= row.inputLimit { return nil }
      }
      if let number = number {
         if let minimum = row.minimum, number < minimum { return nil }
         if let maximum = row.maximum, number > maximum { return nil }
      }
      return result
   }

   private var keyboard: UIKeyboardType {
      switch row.inputKind {
         case Int(IOS_MENU_VALUE_UINT.rawValue), Int(IOS_MENU_VALUE_SIZE.rawValue):
            return .numberPad
         case Int(IOS_MENU_VALUE_INT.rawValue), Int(IOS_MENU_VALUE_FLOAT.rawValue):
            return .numbersAndPunctuation
         default: return .default
      }
   }

   var body: some View {
      NavigationStack {
         Form {
            Section(footer: Text(row.sublabel)) {
               if row.inputKind == Int(IOS_MENU_VALUE_COLOR.rawValue) {
                  ColorPicker(row.title, selection: $color, supportsOpacity: false)
               } else if row.inputKind == Int(IOS_MENU_VALUE_PASSWORD.rawValue) {
                  SecureField(row.title, text: $value).focused($focused)
               } else {
                  TextField(row.title, text: $value)
                     .keyboardType(keyboard)
                     .textInputAutocapitalization(.never)
                     .autocorrectionDisabled()
                     .focused($focused)
               }
            }
            if row.minimum != nil || row.maximum != nil {
               Section {
                  if let minimum = row.minimum { Text("≥ \(minimum.formatted())") }
                  if let maximum = row.maximum { Text("≤ \(maximum.formatted())") }
               }.foregroundColor(.secondary)
            }
         }
         .navigationTitle(row.title)
         .navigationBarTitleDisplayMode(.inline)
         .toolbar {
            ToolbarItem(placement: .cancellationAction) {
               Button(model.label("cancel", fallback: "Cancel"), role: .cancel) { dismiss() }
            }
            ToolbarItem(placement: .confirmationAction) {
               Button(model.label("apply", fallback: "Apply")) {
                  if let value = submittedValue,
                        model.setValue(value, row: row, revision: revision) { dismiss() }
               }.disabled(submittedValue == nil || model.busy)
            }
         }
         .onAppear { focused = row.inputKind != Int(IOS_MENU_VALUE_COLOR.rawValue) }
      }
   }
}

@available(iOS 16.0, *)
private struct NativeMenuPageView: View {
   @ObservedObject var model: NativeMenuModel
   let page: NativeMenuPage
   @State private var editing: NativeMenuEdit?

   private var active: Bool { page.depth == model.depth && page.revision == model.revision }

   private func title(_ entry: NativeMenuRow) -> some View {
      VStack(alignment: .leading, spacing: 4) {
         Text(entry.title)
            .foregroundColor(entry.enabled && !entry.boolean && !entry.editable
               && !entry.submenu && !entry.checked ? .accentColor : .primary)
         if !entry.sublabel.isEmpty {
            Text(entry.sublabel).font(.caption).foregroundColor(.secondary)
         }
      }
      .fixedSize(horizontal: false, vertical: true)
   }

   private func contents(_ entry: NativeMenuRow) -> some View {
      HStack {
         title(entry)
         Spacer(minLength: 12)
         if entry.inputKind == Int(IOS_MENU_VALUE_COLOR.rawValue),
               let rgb = UInt32(entry.inputValue) {
            Circle()
               .fill(Color(red: Double((rgb >> 16) & 255) / 255,
                  green: Double((rgb >> 8) & 255) / 255, blue: Double(rgb & 255) / 255))
               .frame(width: 24, height: 24)
               .overlay(Circle().strokeBorder(Color.primary.opacity(0.15)))
               .accessibilityHidden(true)
         }
         if !entry.value.isEmpty && entry.value != "..." {
            Text(entry.value)
               .foregroundColor(entry.editable ? .accentColor : .secondary)
               .multilineTextAlignment(.trailing)
         }
         if entry.checked {
            Image(systemName: "checkmark").foregroundColor(.accentColor)
         } else if entry.submenu {
            Image(systemName: "chevron.right").font(.caption)
               .foregroundColor(Color(UIColor.tertiaryLabel))
               .accessibilityHidden(true)
         }
      }
      .padding(.vertical, 4)
      .contentShape(Rectangle())
   }

   @ViewBuilder
   private func row(_ entry: NativeMenuRow) -> some View {
      if entry.boolean {
         Toggle(isOn: Binding(
            get: { entry.checked },
            set: { _ in self.model.send(IOS_MENU_RIGHT, row: entry, revision: self.page.revision) })) {
            title(entry)
         }
         .tint(.green)
         .padding(.vertical, 4)
         .disabled(!entry.enabled)
      } else if entry.enabled {
         Button(action: {
            if entry.editable {
               self.editing = NativeMenuEdit(row: entry, revision: self.page.revision)
            }
            else { self.model.send(IOS_MENU_OK, row: entry, revision: self.page.revision) }
         }) {
            contents(entry)
         }
         .buttonStyle(.plain)
         .accessibilityAddTraits(entry.checked ? .isSelected : [])
      } else {
         contents(entry).accessibilityElement(children: .combine)
      }
   }

   var body: some View {
      List {
         if active && !model.dialog.isEmpty {
            Section {
               Text(model.dialog)
               Button(model.label("confirm", fallback: "Continue")) { self.model.send(IOS_MENU_CONFIRM) }
               Button(model.label("back", fallback: "Back")) { self.model.send(IOS_MENU_BACK) }
            }
         } else {
            ForEach(page.rows) { entry in
               self.row(entry).id(entry.id)
                  .disabled(!active || self.model.busy)
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
      .listStyle(.insetGrouped)
      .navigationTitle(page.title)
      .navigationBarTitleDisplayMode(.inline)
      .toolbarBackground(Color(UIColor.systemGroupedBackground), for: .navigationBar)
      .toolbarBackground(.visible, for: .navigationBar)
      .sheet(item: $editing) { edit in
         NativeMenuValueEditor(model: model, row: edit.row, revision: edit.revision)
            .presentationDetents([.medium, .large])
            .presentationDragIndicator(.visible)
      }
      .toolbar {
         ToolbarItem(placement: .navigationBarTrailing) {
            if model.running {
               Button(model.label("resume", fallback: "Resume")) {
                  self.model.send(IOS_MENU_RESUME)
               }.disabled(model.busy || !model.dialog.isEmpty)
            }
         }
      }
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
               .onChange(of: model.focusRevision) { _ in
                  if depth == self.model.depth { proxy.scrollTo(self.model.selection, anchor: .center) }
               }
               .onChange(of: model.pages[depth].offset) { _ in
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
      .overlay(alignment: .bottom) {
         if !model.message.isEmpty {
            Text(model.message)
               .font(.callout)
               .padding()
               .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 12))
               .padding(.horizontal)
               .padding(.bottom, 90)
               .allowsHitTesting(false)
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
      model.showMessage(message)
   }
}
#endif
