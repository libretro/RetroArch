//
//  RetroArchAppShortcuts.swift
//  RetroArch
//
//  App Shortcuts provider and bridge for RetroArch App Intents
//

import Foundation
import AppIntents

@available(macOS 13.0, iOS 16.0, tvOS 16.0, *)
@objc(RetroArchAppShortcuts)
public final class RetroArchAppShortcuts: NSObject, AppShortcutsProvider {
   @AppShortcutsBuilder
   public static var appShortcuts: [AppShortcut] {
      AppShortcut(
         intent: PlayGameIntent(),
         phrases: [
            "Play \(\.$game) in \(.applicationName)",
            "In \(.applicationName) play \(\.$game)",
            "Play the game \(\.$game) in \(.applicationName)",
            "Use \(.applicationName) to play \(\.$game)"
         ],
         shortTitle: "Play Game",
         systemImageName: "gamecontroller"
      )
   }

   @objc public static func updateAppShortcuts() {
      Task {
         RetroArchAppShortcuts.updateAppShortcutParameters()
      }
   }

   @objc public static func contentLoaded() {
      Task {
         await donateGameLaunchIntent()
      }
   }

   private static func donateGameLaunchIntent() async {
      // Get the current content path from RetroArch
      guard let contentPath = getContentPath(),
            !contentPath.isEmpty else {
         return
      }

      // Extract filename from path
      let filename = (contentPath as NSString).lastPathComponent

      // TODO: maybe don't limit to playlists
      guard let playlistGame = RetroArchPlaylistManager.findGame(byFilename: filename) else {
         return
      }

      // Convert the playlist game to a GameEntity
      let systemName = extractSystemName(from: playlistGame.gameId)
      let gameEntity = GameEntity(
         id: playlistGame.title,
         title: playlistGame.title,
         filename: playlistGame.filename,
         core: playlistGame.corePath,
         coreName: playlistGame.coreName,
         systemName: systemName
      )

      // Create and donate the intent
      let intent = PlayGameIntent()
      intent.game = gameEntity

      do {
         try await IntentDonationManager.shared.donate(intent: intent)
      } catch {
      }
   }

   // Helper method to extract system name from game ID (copied from GameEntityQuery)
   private static func extractSystemName(from gameId: String) -> String {
      // gameId format is "playlistName:index"
      let components = gameId.components(separatedBy: ":")
      guard let playlistName = components.first else { return "Unknown" }

      // Convert playlist filename to user-friendly system name
      let systemName = playlistName
         .replacingOccurrences(of: ".lpl", with: "")
         .replacingOccurrences(of: "^[^-]+- ", with: "", options: .regularExpression)

      return systemName
   }

   // C bridge function to get content path
   private static func getContentPath() -> String? {
      guard let cString = path_get(RARCH_PATH_CONTENT) else {
         return nil
      }
      return String(cString: cString)
   }
}

@available(macOS 13.0, iOS 16.0, tvOS 16.0, *)
struct PlayGameIntent: AppIntent {
   static var title: LocalizedStringResource = "Play Game"
   static var description = IntentDescription("Launch a game in RetroArch")
   static var openAppWhenRun = true

   @Parameter(title: "Game", description: "The game to launch")
   var game: GameEntity

   @MainActor
   func perform() throws -> some IntentResult {
      let success = cocoa_launch_game_by_filename(game.filename)

      if success {
         return .result()
      } else {
         throw PlayGameIntentError.gameNotFound
      }
   }

   enum PlayGameIntentError: Swift.Error, LocalizedError {
      case gameNotFound

      var errorDescription: String? {
         switch self {
         case .gameNotFound:
            return "Game could not be found or launched"
         }
      }
   }
}

@available(macOS 13.0, iOS 16.0, tvOS 16.0, *)
struct GameEntity: AppEntity, Identifiable {
   static var typeDisplayRepresentation = TypeDisplayRepresentation(name: "Game")
   static let defaultQuery = GameEntityQuery()

   var id: String
   var displayRepresentation: DisplayRepresentation
   var filename: String
   var core: String?
   var coreName: String?
   var systemName: String?
   var synonyms: [String]

   init(id: String, title: String, filename: String, core: String? = nil, coreName: String? = nil, systemName: String? = nil) {
      self.id = id
      self.filename = filename
      self.core = core
      self.coreName = coreName
      self.systemName = systemName

      // Create display representation with system context for disambiguation
      let subtitle: String
      if let systemName = systemName {
         subtitle = systemName
      } else if let coreName = coreName {
         subtitle = coreName
      } else {
         subtitle = "Unknown System"
      }

      // Create comprehensive synonyms for better Siri recognition
      self.synonyms = GameSynonymGenerator.generateSynonyms(for: title)

      if #available(macOS 14.0, iOS 17.0, tvOS 17.0, *) {
         self.displayRepresentation = DisplayRepresentation(
            title: "\(title)",
            subtitle: LocalizedStringResource(stringLiteral: subtitle),
            synonyms: self.synonyms.map { LocalizedStringResource(stringLiteral: $0) }
         )
      } else {
         self.displayRepresentation = DisplayRepresentation(
            title: "\(title)",
            subtitle: LocalizedStringResource(stringLiteral: subtitle)
         )
      }
   }
}

@available(macOS 13.0, iOS 16.0, tvOS 16.0, *)
struct GameEntityQuery: EntityQuery, EntityStringQuery {
   func entities(for identifiers: [GameEntity.ID]) -> [GameEntity] {
      return getAllGames().filter { identifiers.contains($0.id) }
   }

   func suggestedEntities() -> [GameEntity] {
      var suggestions: [GameEntity] = []

      // Get up to 7 games from history (recently played)
      let historyGames = RetroArchPlaylistManager.getHistoryGames()
      let historyEntities = convertToGameEntities(historyGames)
      suggestions.append(contentsOf: Array(historyEntities.prefix(7)))

      // Fill remaining slots with favorites (up to 10 total)
      let favoritesGames = RetroArchPlaylistManager.getFavoriteGames()
      let favoritesEntities = convertToGameEntities(favoritesGames)
      let remainingSlots = max(0, 10 - suggestions.count)
      suggestions.append(contentsOf: Array(favoritesEntities.prefix(remainingSlots)))

      // Fall back to regular games if we don't have enough suggestions
      if suggestions.count < 5 {
         let regularGames = Array(getAllGames().prefix(10 - suggestions.count))
         suggestions.append(contentsOf: regularGames)
      }

      return Array(suggestions.prefix(10))
   }

   func entities(matching string: String) -> [GameEntity] {
      return GameMatchingEngine.findBestMatches(for: string, in: getAllGames())
   }

   private func getAllGames() -> [GameEntity] {
      let objcGames = RetroArchPlaylistManager.getAllGames()

      // Return empty array if no games found (RetroArch not initialized)
      guard !objcGames.isEmpty else {
         return []
      }

      // Create a map to track duplicate game titles for smart disambiguation
      var gamesByTitle: [String: [RetroArchPlaylistGame]] = [:]
      for game in objcGames {
         let cleanTitle = game.title.lowercased().trimmingCharacters(in: .whitespacesAndNewlines)
         if gamesByTitle[cleanTitle] == nil {
            gamesByTitle[cleanTitle] = []
         }
         gamesByTitle[cleanTitle]?.append(game)
      }

      return objcGames.map { objcGame in
         let systemName = extractSystemName(from: objcGame.gameId)
         let cleanTitle = objcGame.title.lowercased().trimmingCharacters(in: .whitespacesAndNewlines)

         // Create unique ID that includes system for disambiguation when there are duplicates
         let uniqueId: String
         if let duplicates = gamesByTitle[cleanTitle], duplicates.count > 1 {
            uniqueId = "\(objcGame.title) (\(systemName))"
         } else {
            uniqueId = objcGame.title
         }

         return GameEntity(
            id: uniqueId,
            title: objcGame.title,
            filename: objcGame.filename,
            core: objcGame.corePath,
            coreName: objcGame.coreName,
            systemName: systemName
         )
      }
   }

   // Extract a user-friendly system name from the playlist game ID
   private func extractSystemName(from gameId: String) -> String {
      // gameId format is "playlistName:index"
      let components = gameId.components(separatedBy: ":")
      guard let playlistName = components.first else { return "Unknown" }

      // Convert playlist filename to user-friendly system name
      let systemName = playlistName
         .replacingOccurrences(of: ".lpl", with: "")
         .replacingOccurrences(of: "^[^-]+- ", with: "", options: .regularExpression)

      return systemName
   }

   // Helper method to convert RetroArchPlaylistGame objects to GameEntity objects
   private func convertToGameEntities(_ objcGames: [RetroArchPlaylistGame]) -> [GameEntity] {
      guard !objcGames.isEmpty else {
         return []
      }

      // Create a map to track duplicate game titles for smart disambiguation
      var gamesByTitle: [String: [RetroArchPlaylistGame]] = [:]
      for game in objcGames {
         let cleanTitle = game.title.lowercased().trimmingCharacters(in: .whitespacesAndNewlines)
         if gamesByTitle[cleanTitle] == nil {
            gamesByTitle[cleanTitle] = []
         }
         gamesByTitle[cleanTitle]?.append(game)
      }

      return objcGames.map { objcGame in
         let systemName = extractSystemName(from: objcGame.gameId)
         let cleanTitle = objcGame.title.lowercased().trimmingCharacters(in: .whitespacesAndNewlines)

         // Create unique ID that includes system for disambiguation when there are duplicates
         let uniqueId: String
         if let duplicates = gamesByTitle[cleanTitle], duplicates.count > 1 {
            uniqueId = "\(objcGame.title) (\(systemName))"
         } else {
            uniqueId = objcGame.title
         }

         return GameEntity(
            id: uniqueId,
            title: objcGame.title,
            filename: objcGame.filename,
            core: objcGame.corePath,
            coreName: objcGame.coreName,
            systemName: systemName
         )
      }
   }
}

// MARK: - Game Synonym Generator

struct GameSynonymGenerator {
   static func generateSynonyms(for title: String) -> [String] {
      var synonyms = Set<String>()

      // Always include the original title
      synonyms.insert(title)
      // Add explicit ampersand variants so Siri can match spoken “and” vs “&”
      if title.contains("&") {
         let tightAnd = title.replacingOccurrences(of: "&", with: "and")
         let spacedAnd = title.replacingOccurrences(of: "&", with: " and ")
         synonyms.insert(tightAnd)
         synonyms.insert(spacedAnd)
      }

      // Basic normalization - remove regions and common tags
      let cleanTitle = normalizeTitle(title)
      if cleanTitle != title {
         synonyms.insert(cleanTitle)
      }

      // Add punctuation-free version
      let noPunctuationTitle = removePunctuation(cleanTitle)
      if noPunctuationTitle != cleanTitle {
         synonyms.insert(noPunctuationTitle)
      }

      // Add no-space variant for titles that commonly get typed without spaces
      let collapsed = noSpaceVariant(of: noPunctuationTitle)
      if collapsed.count > 0 && collapsed != noPunctuationTitle.lowercased() {
         synonyms.insert(collapsed)
      }

      return Array(synonyms).filter { !$0.isEmpty }
   }

   private static func normalizeTitle(_ title: String) -> String {
      var normalized = title

      // Remove region tags and ROM metadata with regex patterns
      let regionPatterns = [
         "\\s*\\(\\s*([A-Za-z, ]+)\\s*\\)", // Region/language tags
         "\\s*\\[!\\]", // Good dump indicator
         "\\s*\\(\\s*Rev\\s*\\d+\\s*\\)", // Revision tags
         "\\s*\\(\\s*V\\d+\\.\\d+\\s*\\)", // Version tags
         "\\s*\\[\\s*T[-+]?[^\\]]*\\]", // Translation/hack tags
         "\\s*\\([^)]*(?:Beta|Proto|Sample|Demo)[^)]*\\)", // Development versions
      ]

      for pattern in regionPatterns {
         normalized = normalized.replacingOccurrences(
            of: pattern,
            with: "",
            options: [.regularExpression, .caseInsensitive]
         )
      }

      return normalized.trimmingCharacters(in: .whitespacesAndNewlines)
   }

   private static func removePunctuation(_ title: String) -> String {
      return title
         .replacingOccurrences(of: "&", with: " and ")
         .replacingOccurrences(of: ":", with: " ")
         .replacingOccurrences(of: "-", with: " ")
         .replacingOccurrences(of: "_", with: " ")
         .replacingOccurrences(of: "'", with: "")
         .replacingOccurrences(of: "!", with: " ")
         .replacingOccurrences(of: "?", with: " ")
         .replacingOccurrences(of: ".", with: "")
         .replacingOccurrences(of: "\\s+", with: " ", options: .regularExpression)
         .trimmingCharacters(in: .whitespacesAndNewlines)
   }

   private static func noSpaceVariant(of title: String) -> String {
      return title
         .lowercased()
         .replacingOccurrences(of: " ", with: "")
   }
}

// MARK: - Game Matching Engine

@available(macOS 13.0, iOS 16.0, tvOS 16.0, *)
struct GameMatchingEngine {
   struct GameMatch {
      let game: GameEntity
      let score: Int
      let matchType: MatchType

      enum MatchType {
          case exactTitle
          case exactSynonym
          case wordMatch
          case prefixTitle
          case prefixSynonym
          case fuzzyTitle
          case fuzzySynonym
          case partialWord
          case substringTitle
          case substringSynonym
      }
   }

   private static func foldDiacritics(_ s: String) -> String {
      return s.folding(options: [.diacriticInsensitive, .caseInsensitive], locale: .current)
   }

   static func findBestMatches(for searchString: String, in games: [GameEntity]) -> [GameEntity] {
      let normalizedSearch = normalizeSearchString(searchString)

      var matches: [GameMatch] = []

      games.forEach { game in
         if let result = findBestMatch(searchString: normalizedSearch, game: game) {
            matches.append(result)
         }
      }

      // Sort by score (descending), then by title length (ascending for cleaner titles)
      matches.sort { first, second in
         if first.score != second.score {
            return first.score > second.score
         }
         let firstTitle = String(localized: first.game.displayRepresentation.title)
         let secondTitle = String(localized: second.game.displayRepresentation.title)
         return firstTitle.count < secondTitle.count
      }

      return matches.map { $0.game }
   }

   private static func findBestMatch(searchString: String, game: GameEntity) -> GameMatch? {
      let gameTitle = String(localized: game.displayRepresentation.title)
      let normalizedTitle = normalizeSearchString(gameTitle)

      // Get all synonyms for this game
      let synonyms = game.synonyms
      let normalizedSynonyms = synonyms.map { normalizeSearchString($0) }

      // Test exact matches first (highest score)
      if normalizedTitle == searchString {
         return GameMatch(game: game, score: 100, matchType: .exactTitle)
      }

      for synonym in normalizedSynonyms {
         if synonym == searchString {
            return GameMatch(game: game, score: 95, matchType: .exactSynonym)
         }
      }

      let nospaceTitle = normalizedTitle.replacingOccurrences(of: " ", with: "")
      let nospaceSearch = searchString.replacingOccurrences(of: " ", with: "")

      if nospaceTitle == nospaceSearch {
         return GameMatch(game: game, score: 93, matchType: .exactTitle)
      }

      // Test word-level exact matches
      let searchWords = Set(searchString.split(separator: " ").map(String.init))
      let titleWords = Set(normalizedTitle.split(separator: " ").map(String.init))

      if !searchWords.isEmpty && searchWords.isSubset(of: titleWords) {
         return GameMatch(game: game, score: 90, matchType: .wordMatch)
      }

      // Test prefix matches
      if normalizedTitle.hasPrefix(searchString) {
         return GameMatch(game: game, score: 85, matchType: .prefixTitle)
      }

      for synonym in normalizedSynonyms {
         if synonym.hasPrefix(searchString) {
            return GameMatch(game: game, score: 80, matchType: .prefixSynonym)
         }
      }

      // Fuzzy matching with Levenshtein distance
      let titleDistance = levenshteinDistance(searchString, normalizedTitle)
      let maxLength = max(searchString.count, normalizedTitle.count)

      if maxLength > 0 {
         let similarity = Double(maxLength - titleDistance) / Double(maxLength)
         if similarity > 0.7 {
            let score = Int(60 + (similarity - 0.7) * 60) // 60-79 range
            return GameMatch(game: game, score: score, matchType: .fuzzyTitle)
         }
      }

      // Check fuzzy matches against synonyms
      for synonym in normalizedSynonyms {
         let synonymDistance = levenshteinDistance(searchString, synonym)
         let synonymMaxLength = max(searchString.count, synonym.count)

         if synonymMaxLength > 0 {
            let synonymSimilarity = Double(synonymMaxLength - synonymDistance) / Double(synonymMaxLength)
            if synonymSimilarity > 0.7 {
               let score = Int(55 + (synonymSimilarity - 0.7) * 60)
               return GameMatch(game: game, score: score, matchType: .fuzzySynonym)
            }
         }
      }

      // Partial word matching
      for searchWord in searchWords {
         if searchWord.count >= 3 {
            for titleWord in titleWords {
               if titleWord.count > 1 && (titleWord.contains(searchWord) || searchWord.contains(titleWord)) {
                  return GameMatch(game: game, score: 50, matchType: .partialWord)
               }
            }
         }
      }

      // Substring matching (lowest priority)
      if normalizedTitle.contains(searchString) {
         return GameMatch(game: game, score: 30, matchType: .substringTitle)
      }

      if nospaceTitle.contains(nospaceSearch) {
         return GameMatch(game: game, score: 27, matchType: .substringTitle)
      }

      for synonym in normalizedSynonyms {
         if synonym.contains(searchString) {
            return GameMatch(game: game, score: 25, matchType: .substringSynonym)
         }
      }

      return nil
   }

   private static func normalizeSearchString(_ string: String) -> String {
      let base = foldDiacritics(string)
      return base.lowercased()
         .replacingOccurrences(of: "&", with: " and ")
         .replacingOccurrences(of: ".", with: "")
         .replacingOccurrences(of: ":", with: "")
         .replacingOccurrences(of: "-", with: "")
         .replacingOccurrences(of: "_", with: " ")
         .replacingOccurrences(of: "  ", with: " ")
         .replacingOccurrences(of: "\\s+", with: " ", options: .regularExpression)
         .trimmingCharacters(in: .whitespacesAndNewlines)
   }

   private static func levenshteinDistance(_ s1: String, _ s2: String) -> Int {
      let s1Array = Array(s1)
      let s2Array = Array(s2)
      let s1Count = s1Array.count
      let s2Count = s2Array.count

      var matrix = Array(repeating: Array(repeating: 0, count: s2Count + 1), count: s1Count + 1)

      for i in 0...s1Count {
         matrix[i][0] = i
      }

      for j in 0...s2Count {
         matrix[0][j] = j
      }

      for i in 1...s1Count {
         for j in 1...s2Count {
            let cost = s1Array[i-1] == s2Array[j-1] ? 0 : 1
            matrix[i][j] = min(
               matrix[i-1][j] + 1,
               matrix[i][j-1] + 1,
               matrix[i-1][j-1] + cost
            )
         }
      }

      return matrix[s1Count][s2Count]
   }
}
