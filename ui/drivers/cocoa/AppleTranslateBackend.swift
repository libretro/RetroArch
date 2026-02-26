/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2025 - RetroArch
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  See the COPYING file at the root of the source tree for the full license text.
 */

import Foundation
import Vision
import AVFoundation
import ImageIO

#if os(macOS)
import AppKit
#elseif os(iOS) || os(tvOS)
import UIKit
#endif

#if canImport(SwiftUI)
import SwiftUI
#endif

#if canImport(Translation)
import Translation
#endif

// MARK: - Logging

@_silgen_name("apple_translate_log")
private func raLog(_ message: UnsafePointer<CChar>)

private func logTranslation(_ message: String) {
    message.withCString { raLog($0) }
}

// MARK: - Translation Manager (SwiftUI-based)

#if canImport(Translation) && !os(tvOS)
@available(macOS 15.0, iOS 18.0, *)
@MainActor
private class TranslationManager {
    static let shared = TranslationManager()

    private var pendingCompletion: ((String?, String?) -> Void)?
    private var pendingText: String?

    #if os(iOS)
    private var hostingController: UIHostingController<AnyView>?
    private weak var parentVC: UIViewController?
    private weak var containerView: UIView?
    #endif

    func translate(text: String,
                   sourceLanguage: String?,
                   targetLanguage: String,
                   completion: @escaping (String?, String?) -> Void) {
        pendingText = text
        pendingCompletion = completion

        #if os(iOS)
        // Remove old hosting controller if any
        hostingController?.willMove(toParent: nil)
        hostingController?.view.removeFromSuperview()
        hostingController?.removeFromParent()
        hostingController = nil

        guard let cocoaView = CocoaView.get() else {
            logTranslation("[Translation] ERROR: CocoaView.get() returned nil")
            completion(nil, "CocoaView not available")
            return
        }

        // Find parent view controller
        if parentVC == nil {
            var responder: UIResponder? = cocoaView
            while let r = responder {
                if let vc = r as? UIViewController {
                    parentVC = vc
                    break
                }
                responder = r.next
            }
        }
        containerView = cocoaView.view

        // Create configuration
        let config = TranslationSession.Configuration(
            source: sourceLanguage.map { Locale.Language(identifier: $0) },
            target: Locale.Language(identifier: targetLanguage)
        )

        // Create a fresh SwiftUI view with the configuration already set
        let translationView = TranslationTaskView(
            configuration: config,
            onSession: { [weak self] session in
                await self?.handleSession(session)
            }
        )

        let controller = UIHostingController(rootView: AnyView(translationView))
        controller.view.frame = CGRect(x: 0, y: 0, width: 10, height: 10)
        controller.view.backgroundColor = .clear
        controller.view.isUserInteractionEnabled = false

        if let parent = parentVC {
            parent.addChild(controller)
            containerView?.addSubview(controller.view)
            controller.didMove(toParent: parent)
        } else {
            containerView?.addSubview(controller.view)
        }

        hostingController = controller
        #else
        // macOS fallback
        completion(nil, "Translation not implemented for macOS")
        #endif
    }

    private func handleSession(_ session: TranslationSession) async {
        guard let text = pendingText, let completion = pendingCompletion else {
            return
        }

        do {
            let response = try await session.translate(text)
            completion(response.targetText, nil)
        } catch {
            logTranslation("[Translation] Translation error: \(error.localizedDescription)")
            completion(nil, error.localizedDescription)
        }

        pendingText = nil
        pendingCompletion = nil
    }
}

@available(macOS 15.0, iOS 18.0, *)
private struct TranslationTaskView: View {
    let configuration: TranslationSession.Configuration
    let onSession: (TranslationSession) async -> Void

    var body: some View {
        Color.clear
            .frame(width: 10, height: 10)
            .translationTask(configuration) { session in
                await onSession(session)
            }
    }
}
#endif

// MARK: - OCR Implementation

private struct TextBlock {
    let text: String
    let boundingBox: CGRect  // Normalized coordinates (0-1), origin at bottom-left
}

@available(macOS 10.15, iOS 13.0, tvOS 13.0, *)
private func performOCR(cgImage: CGImage, sourceLanguage: String?) throws -> [TextBlock] {
    var results: [TextBlock] = []
    var ocrError: Error?

    logTranslation("[Translation] OCR input: \(cgImage.width)x\(cgImage.height), sourceLanguage=\(sourceLanguage ?? "auto")")

    let request = VNRecognizeTextRequest { request, error in
        if let error = error {
            ocrError = error
            logTranslation("[Translation] OCR request error: \(error.localizedDescription)")
            return
        }

        guard let observations = request.results as? [VNRecognizedTextObservation] else {
            logTranslation("[Translation] OCR: no observations returned")
            return
        }

        logTranslation("[Translation] OCR found \(observations.count) observations")
        for observation in observations {
            if let candidate = observation.topCandidates(1).first {
                logTranslation("[Translation] OCR text: '\(candidate.string)' (confidence: \(String(format: "%.2f", candidate.confidence)))")
                results.append(TextBlock(
                    text: candidate.string,
                    boundingBox: observation.boundingBox
                ))
            }
        }
    }

    request.recognitionLevel = .accurate
    request.usesLanguageCorrection = true

    if let lang = sourceLanguage {
        request.recognitionLanguages = [lang]
    }

    let handler = VNImageRequestHandler(cgImage: cgImage, options: [:])
    try handler.perform([request])

    if let error = ocrError {
        throw error
    }

    return results
}

private struct OCRResult {
    let blocks: [TextBlock]
    let cgImage: CGImage
    let width: Int
    let height: Int
}

@available(macOS 10.15, iOS 13.0, tvOS 13.0, *)
private func performOCR(imageData: UnsafePointer<UInt8>, width: Int, height: Int,
                        pitch: Int, sourceLanguage: String?) throws -> OCRResult {
    // Convert BGR24 to RGBX and flip vertically (framebuffer is bottom-up)
    let rgbxData = UnsafeMutablePointer<UInt8>.allocate(capacity: width * height * 4)
    defer { rgbxData.deallocate() }

    for y in 0..<height {
        let srcY = height - 1 - y  // Flip vertically
        for x in 0..<width {
            let srcOffset = srcY * pitch + x * 3
            let dstOffset = (y * width + x) * 4
            rgbxData[dstOffset + 0] = imageData[srcOffset + 2]  // R <- B
            rgbxData[dstOffset + 1] = imageData[srcOffset + 1]  // G <- G
            rgbxData[dstOffset + 2] = imageData[srcOffset + 0]  // B <- R
            rgbxData[dstOffset + 3] = 255                       // X (unused alpha)
        }
    }

    let colorSpace = CGColorSpaceCreateDeviceRGB()
    guard let context = CGContext(
        data: rgbxData,
        width: width,
        height: height,
        bitsPerComponent: 8,
        bytesPerRow: width * 4,
        space: colorSpace,
        bitmapInfo: CGImageAlphaInfo.noneSkipLast.rawValue
    ) else {
        throw NSError(domain: "AppleTranslate", code: 1,
                      userInfo: [NSLocalizedDescriptionKey: "Failed to create graphics context"])
    }

    guard let cgImage = context.makeImage() else {
        throw NSError(domain: "AppleTranslate", code: 2,
                      userInfo: [NSLocalizedDescriptionKey: "Failed to create CGImage"])
    }

    let blocks = try performOCR(cgImage: cgImage, sourceLanguage: sourceLanguage)
    return OCRResult(blocks: blocks, cgImage: cgImage, width: width, height: height)
}

// MARK: - Speech Synthesis

@available(macOS 10.15, iOS 13.0, tvOS 13.0, *)
private class SpeechSynthesizer: NSObject, AVSpeechSynthesizerDelegate {
    private let synthesizer = AVSpeechSynthesizer()
    private var audioBuffers: [AVAudioBuffer] = []
    private var completion: ((Data?, String?) -> Void)?
    private var audioFormat: AVAudioFormat?

    override init() {
        super.init()
        synthesizer.delegate = self
    }

    func synthesize(text: String, language: String?, completion: @escaping (Data?, String?) -> Void) {
        self.completion = completion
        self.audioBuffers = []

        let utterance = AVSpeechUtterance(string: text)
        if let lang = language {
            utterance.voice = AVSpeechSynthesisVoice(language: lang)
        }

        synthesizer.write(utterance) { [weak self] buffer in
            guard let self = self else { return }
            if let pcmBuffer = buffer as? AVAudioPCMBuffer, pcmBuffer.frameLength > 0 {
                self.audioFormat = pcmBuffer.format
                self.audioBuffers.append(pcmBuffer)
            }
        }
    }

    func speechSynthesizer(_ synthesizer: AVSpeechSynthesizer, didFinish utterance: AVSpeechUtterance) {
        guard let format = audioFormat else {
            completion?(nil, "No audio format available")
            return
        }

        // Combine all buffers and convert to WAV
        let wavData = createWAVData(from: audioBuffers, format: format)
        completion?(wavData, nil)
        completion = nil
        audioBuffers = []
    }

    func speechSynthesizer(_ synthesizer: AVSpeechSynthesizer, didCancel utterance: AVSpeechUtterance) {
        completion?(nil, "Speech synthesis cancelled")
        completion = nil
        audioBuffers = []
    }

    private func createWAVData(from buffers: [AVAudioBuffer], format: AVAudioFormat) -> Data? {
        // Calculate total frame count
        var totalFrames: AVAudioFrameCount = 0
        for buffer in buffers {
            if let pcm = buffer as? AVAudioPCMBuffer {
                totalFrames += pcm.frameLength
            }
        }

        guard totalFrames > 0 else { return nil }

        let channels = format.channelCount
        let sampleRate = UInt32(format.sampleRate)
        let bitsPerSample: UInt16 = 16
        let bytesPerSample = bitsPerSample / 8
        let dataSize = UInt32(totalFrames) * UInt32(channels) * UInt32(bytesPerSample)

        var wavData = Data()

        // WAV header
        wavData.append(contentsOf: "RIFF".utf8)
        wavData.append(UInt32(36 + dataSize).littleEndianBytes)
        wavData.append(contentsOf: "WAVE".utf8)
        wavData.append(contentsOf: "fmt ".utf8)
        wavData.append(UInt32(16).littleEndianBytes)  // Subchunk1Size
        wavData.append(UInt16(1).littleEndianBytes)   // AudioFormat (PCM)
        wavData.append(UInt16(channels).littleEndianBytes)
        wavData.append(sampleRate.littleEndianBytes)
        wavData.append((sampleRate * UInt32(channels) * UInt32(bytesPerSample)).littleEndianBytes)
        wavData.append(UInt16(channels * UInt32(bytesPerSample)).littleEndianBytes)
        wavData.append(bitsPerSample.littleEndianBytes)
        wavData.append(contentsOf: "data".utf8)
        wavData.append(dataSize.littleEndianBytes)

        // Audio data - convert float32 to int16
        for buffer in buffers {
            guard let pcm = buffer as? AVAudioPCMBuffer,
                  let floatData = pcm.floatChannelData else { continue }

            for frame in 0..<Int(pcm.frameLength) {
                for channel in 0..<Int(channels) {
                    let sample = floatData[channel][frame]
                    let clipped = max(-1.0, min(1.0, sample))
                    let int16Sample = Int16(clipped * 32767.0)
                    wavData.append(int16Sample.littleEndianBytes)
                }
            }
        }

        return wavData
    }
}

// Keep synthesizer alive during async operation
private var activeSynthesizer: AnyObject?

// MARK: - TTS Text Processing

/// Cleans up text for better speech synthesis output.
/// Handles common OCR errors and improves pronunciation.
/// - Parameter text: The raw text from OCR or translation
/// - Returns: Cleaned text optimized for TTS
private func cleanTextForSpeech(_ text: String) -> String {
    var result = text

    // Replace ellipsis with a pause (period works well for AVSpeechSynthesizer)
    result = result.replacingOccurrences(of: "...", with: ".")

    // Fix common OCR contraction errors (spaces around apostrophes)
    result = result.replacingOccurrences(of: " ' s ", with: "'s ")
    result = result.replacingOccurrences(of: " ' t ", with: "'t ")
    result = result.replacingOccurrences(of: " ' re ", with: "'re ")
    result = result.replacingOccurrences(of: " ' ve ", with: "'ve ")
    result = result.replacingOccurrences(of: " ' ll ", with: "'ll ")
    result = result.replacingOccurrences(of: " ' d ", with: "'d ")
    result = result.replacingOccurrences(of: " ' m ", with: "'m ")

    // Also handle case at end of string (no trailing space)
    result = result.replacingOccurrences(of: " ' s$", with: "'s", options: .regularExpression)
    result = result.replacingOccurrences(of: " ' t$", with: "'t", options: .regularExpression)
    result = result.replacingOccurrences(of: " ' re$", with: "'re", options: .regularExpression)
    result = result.replacingOccurrences(of: " ' ve$", with: "'ve", options: .regularExpression)
    result = result.replacingOccurrences(of: " ' ll$", with: "'ll", options: .regularExpression)
    result = result.replacingOccurrences(of: " ' d$", with: "'d", options: .regularExpression)
    result = result.replacingOccurrences(of: " ' m$", with: "'m", options: .regularExpression)

    return result
}

/// Formats multiple text blocks for speech synthesis with textbox announcements.
/// - Parameter blocks: Array of text strings, one per textbox
/// - Returns: Formatted text with "Textbox N:" prefixes and pauses between blocks
private func formatTextForSpeech(_ blocks: [String]) -> String {
    var result = ""

    for (index, block) in blocks.enumerated() {
        // Add textbox announcement
        result += "Textbox \(index + 1): "

        // Add a brief pause after announcement (period creates natural pause)
        result += ". "

        // Add cleaned block text
        result += cleanTextForSpeech(block)

        // Add longer pause between blocks (multiple periods)
        if index < blocks.count - 1 {
            result += " .. "
        }
    }

    return result
}

private extension FixedWidthInteger {
    var littleEndianBytes: Data {
        var value = self.littleEndian
        return Data(bytes: &value, count: MemoryLayout<Self>.size)
    }
}

// MARK: - Image Overlay Generation

private struct TranslatedBlock {
    let text: String
    let boundingBox: CGRect
}

// MARK: - CJK Language Support

/// Languages that require CJK font support and character-by-character wrapping
private let cjkLanguages: Set<String> = ["ja", "zh", "zh-cn", "zh-tw", "ko"]

/// Determines if a language code represents a CJK language
private func isCJKLanguage(_ language: String?) -> Bool {
    guard let lang = language?.lowercased() else { return false }
    return cjkLanguages.contains(lang)
}

/// Returns the appropriate font name for the given language.
/// For CJK languages, uses system fonts that support CJK characters.
/// For other languages, uses Helvetica-Bold.
private func fontName(for language: String?) -> CFString {
    if isCJKLanguage(language) {
        // Try CJK-capable system fonts in order of preference
        // These are available on all Apple platforms
        #if os(macOS)
        // On macOS, Hiragino Sans is the primary CJK font
        return "Hiragino Sans" as CFString
        #else
        // On iOS/tvOS, PingFang SC is widely available
        return "PingFang SC" as CFString
        #endif
    }
    return "Helvetica-Bold" as CFString
}

/// Wraps text to fit within a given width.
/// - Parameters:
///   - text: The text to wrap
///   - font: The font to use for measurement
///   - maxWidth: Maximum width for each line
///   - characterWrap: If true, wraps character-by-character (for CJK languages).
///                    If false, wraps word-by-word (for Western languages).
/// - Returns: An array of lines that fit within the specified width.
private func wrapText(_ text: String, font: CTFont, maxWidth: CGFloat, characterWrap: Bool = false) -> [String] {
    let attributes: [NSAttributedString.Key: Any] = [.font: font]

    if characterWrap {
        // Character-by-character wrapping for CJK languages
        var lines: [String] = []
        var currentLine = ""

        for char in text {
            // Skip newlines, treat them as line breaks
            if char == "\n" {
                if !currentLine.isEmpty {
                    lines.append(currentLine)
                    currentLine = ""
                }
                continue
            }

            let testLine = currentLine + String(char)
            let attrStr = NSAttributedString(string: testLine, attributes: attributes)
            let line = CTLineCreateWithAttributedString(attrStr)
            let lineWidth = CTLineGetTypographicBounds(line, nil, nil, nil)

            if lineWidth < Double(maxWidth) {
                currentLine = testLine
            } else {
                if !currentLine.isEmpty {
                    lines.append(currentLine)
                }
                currentLine = String(char)
            }
        }

        if !currentLine.isEmpty {
            lines.append(currentLine)
        }

        return lines
    } else {
        // Word-by-word wrapping for Western languages
        let words = text.split(separator: " ", omittingEmptySubsequences: false)
        var lines: [String] = []
        var currentLine = ""

        for word in words {
            let wordStr = String(word)
            let testLine = currentLine.isEmpty ? wordStr : currentLine + " " + wordStr
            let attrStr = NSAttributedString(string: testLine, attributes: attributes)
            let line = CTLineCreateWithAttributedString(attrStr)
            let lineWidth = CTLineGetTypographicBounds(line, nil, nil, nil)

            if lineWidth < Double(maxWidth) {
                currentLine = testLine
            } else {
                if !currentLine.isEmpty {
                    lines.append(currentLine)
                }
                currentLine = wordStr
            }
        }

        if !currentLine.isEmpty {
            lines.append(currentLine)
        }

        return lines
    }
}

/// Calculates the line height for a given font.
private func getLineHeight(font: CTFont) -> CGFloat {
    let ascent = CTFontGetAscent(font)
    let descent = CTFontGetDescent(font)
    let leading = CTFontGetLeading(font)
    return ascent + descent + leading
}

/// Checks if wrapped text fits within the given bounding box dimensions.
/// Returns the wrapped lines if they fit, or nil if they don't.
private func textFitsInBox(_ text: String, font: CTFont, maxWidth: CGFloat, maxHeight: CGFloat, characterWrap: Bool = false) -> [String]? {
    let padding: CGFloat = 4
    let availableWidth = maxWidth - padding
    let availableHeight = maxHeight - padding

    let lines = wrapText(text, font: font, maxWidth: availableWidth, characterWrap: characterWrap)
    let lineHeight = getLineHeight(font: font)
    let totalHeight = CGFloat(lines.count) * lineHeight

    if totalHeight <= availableHeight {
        return lines
    }
    return nil
}

/// Finds the largest font size (within the given range) where the text fits in the bounding box.
/// Uses an upward iteration approach similar to vgtranslate's algorithm - iterates through
/// increasing font sizes until text no longer fits, then returns the last size that worked.
///
/// - Parameters:
///   - text: The text to fit
///   - maxWidth: Maximum width of the bounding box
///   - maxHeight: Maximum height of the bounding box
///   - language: The target language (used to select appropriate font and wrapping mode)
///   - minSize: Minimum font size to try (default: 8pt)
///   - maxSize: Maximum font size to try (default: 32pt)
/// - Returns: A tuple containing the best font, the wrapped lines, and the line height
private func findLargestFittingFont(
    text: String,
    maxWidth: CGFloat,
    maxHeight: CGFloat,
    language: String? = nil,
    minSize: CGFloat = 8,
    maxSize: CGFloat = 32
) -> (font: CTFont, lines: [String], lineHeight: CGFloat) {
    let selectedFontName = fontName(for: language)
    let characterWrap = isCJKLanguage(language)

    // Start with minimum size as the fallback
    var bestFont = CTFontCreateWithName(selectedFontName, minSize, nil)
    var bestLines = wrapText(text, font: bestFont, maxWidth: maxWidth - 4, characterWrap: characterWrap)
    var bestLineHeight = getLineHeight(font: bestFont)

    // Iterate upward through font sizes, keeping the last one that fits
    // This guarantees we find the LARGEST font that works
    var size = minSize
    while size <= maxSize {
        let font = CTFontCreateWithName(selectedFontName, size, nil)

        if let lines = textFitsInBox(text, font: font, maxWidth: maxWidth, maxHeight: maxHeight, characterWrap: characterWrap) {
            // This size fits - save it as our best so far
            bestFont = font
            bestLines = lines
            bestLineHeight = getLineHeight(font: font)
            size += 1
        } else {
            // This size doesn't fit - stop searching, use the previous best
            break
        }
    }

    return (bestFont, bestLines, bestLineHeight)
}

@available(macOS 10.15, iOS 13.0, tvOS 13.0, *)
private func renderTransparentOverlay(width: Int, height: Int, blocks: [TranslatedBlock], targetLanguage: String? = nil) -> Data? {
    // Create a fully transparent RGBA context (like vgtranslate's png-a mode)
    let colorSpace = CGColorSpaceCreateDeviceRGB()
    guard let context = CGContext(
        data: nil,
        width: width,
        height: height,
        bitsPerComponent: 8,
        bytesPerRow: width * 4,
        space: colorSpace,
        bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue
    ) else { return nil }

    // Start with fully transparent background (RGBA: 0,0,0,0)
    context.clear(CGRect(x: 0, y: 0, width: width, height: height))

    // Draw text blocks on transparent background
    for block in blocks {
        // Convert normalized coordinates to pixel coordinates
        // Vision uses bottom-left origin, Core Graphics also uses bottom-left
        let x = block.boundingBox.origin.x * CGFloat(width)
        let y = block.boundingBox.origin.y * CGFloat(height)
        let w = block.boundingBox.size.width * CGFloat(width)
        var h = block.boundingBox.size.height * CGFloat(height)

        // Enforce minimum box height for readability (matches vgtranslate)
        if h < 18 {
            h = 18
        }

        // Draw black background rectangle (fully opaque like vgtranslate)
        context.setFillColor(CGColor(red: 0, green: 0, blue: 0, alpha: 1.0))
        context.fill(CGRect(x: x, y: y, width: w, height: h))

        // Find the largest font size that fits within the bounding box
        // Use target language to select appropriate font and wrapping mode
        let (finalFont, finalLines, finalLineHeight) = findLargestFittingFont(
            text: block.text,
            maxWidth: w,
            maxHeight: h,
            language: targetLanguage
        )

        let padding: CGFloat = 4
        let attributes: [NSAttributedString.Key: Any] = [
            .font: finalFont,
            .foregroundColor: CGColor(red: 1, green: 1, blue: 1, alpha: 1)
        ]

        // Draw each line from top to bottom
        // Core Graphics origin is bottom-left, so we draw from (y + h - lineHeight) downward
        let descent = CTFontGetDescent(finalFont)
        for (index, lineText) in finalLines.enumerated() {
            let lineY = y + h - (CGFloat(index + 1) * finalLineHeight) + descent
            let attrStr = NSAttributedString(string: lineText, attributes: attributes)
            let ctLine = CTLineCreateWithAttributedString(attrStr)
            context.textPosition = CGPoint(x: x + padding / 2, y: lineY)
            CTLineDraw(ctLine, context)
        }
    }

    guard let outputImage = context.makeImage() else { return nil }

    // Convert to PNG format with alpha channel preserved
    return createPNG(from: outputImage)
}

private func createPNG(from cgImage: CGImage) -> Data? {
    // Use ImageIO to create PNG data with alpha channel preserved
    let mutableData = NSMutableData()
    guard let destination = CGImageDestinationCreateWithData(
        mutableData as CFMutableData,
        "public.png" as CFString,
        1,
        nil
    ) else { return nil }

    CGImageDestinationAddImage(destination, cgImage, nil)

    guard CGImageDestinationFinalize(destination) else { return nil }

    return mutableData as Data
}

// MARK: - C Bridge (minimal async API)

private var lastError: String?

@_cdecl("apple_translate_init")
public func appleTranslateInit() -> Bool {
    if #available(macOS 10.15, iOS 13.0, tvOS 13.0, *) {
        return true
    }
    return false
}

@_cdecl("apple_translate_can_translate")
public func appleTranslateCanTranslate() -> Bool {
    #if canImport(Translation) && !os(tvOS)
    if #available(macOS 15.0, iOS 18.0, *) {
        return true
    }
    #endif
    return false
}

@_cdecl("apple_translate_image")
public func appleTranslateImage(
    _ imageData: UnsafePointer<UInt8>?,
    _ width: UInt32,
    _ height: UInt32,
    _ pitch: UInt32,
    _ sourceLang: UnsafePointer<CChar>?,
    _ targetLang: UnsafePointer<CChar>?,
    _ mode: UInt32,
    _ callback: (@convention(c) (
        UnsafeMutablePointer<CChar>?,  /* text */
        UnsafeMutableRawPointer?,      /* image_data */
        Int,                           /* image_size */
        UnsafeMutableRawPointer?,      /* sound_data */
        Int,                           /* sound_size */
        UnsafePointer<CChar>?,         /* error */
        UnsafeMutableRawPointer?       /* userdata */
    ) -> Void)?,
    _ userdata: UnsafeMutableRawPointer?
) {
    guard let callback = callback else { return }

    guard let imageData = imageData else {
        lastError = "No image data"
        callback(nil, nil, 0, nil, 0, (lastError! as NSString).utf8String, userdata)
        return
    }

    let sourceLanguage = sourceLang.map { String(cString: $0) }
    let targetLanguage = targetLang.map { String(cString: $0) }

    // Check if translation is needed (target set and differs from source)
    let translationNeeded = targetLanguage != nil && targetLanguage != sourceLanguage

    // If translation is needed but not available, error out
    if translationNeeded && !appleTranslateCanTranslate() {
        lastError = "On-device translation requires macOS 15.0+ / iOS 18.0+"
        callback(nil, nil, 0, nil, 0, (lastError! as NSString).utf8String, userdata)
        return
    }

    let modeStr: String
    switch mode {
    case 0: modeStr = "image overlay"
    case 1: modeStr = "speech"
    case 2: modeStr = "narrator"
    case 3: modeStr = "image+speech"
    default: modeStr = "unknown"
    }
    NSLog("[AppleTranslate] Mode: %@ (%@)", modeStr, translationNeeded ? "with translation" : "OCR only")

    // Copy image data since we're going async
    let size = Int(height) * Int(pitch)
    let imageCopy = UnsafeMutablePointer<UInt8>.allocate(capacity: size)
    imageCopy.initialize(from: imageData, count: size)

    let w = Int(width)
    let h = Int(height)
    let p = Int(pitch)
    let outputMode = mode

    // Do OCR on background thread, then translation on main thread
    DispatchQueue.global(qos: .userInitiated).async {
        guard #available(macOS 10.15, iOS 13.0, tvOS 13.0, *) else {
            imageCopy.deallocate()
            DispatchQueue.main.async {
                lastError = "Requires macOS 10.15+ / iOS 13.0+ / tvOS 13.0+"
                callback(nil, nil, 0, nil, 0, (lastError! as NSString).utf8String, userdata)
            }
            return
        }

        // Step 1: Perform OCR (can run on background thread)
        let ocrResult: OCRResult
        do {
            ocrResult = try performOCR(
                imageData: imageCopy,
                width: w,
                height: h,
                pitch: p,
                sourceLanguage: sourceLanguage
            )
        } catch {
            imageCopy.deallocate()
            let errMsg = "OCR failed: \(error.localizedDescription)"
            DispatchQueue.main.async {
                lastError = errMsg
                callback(nil, nil, 0, nil, 0, (lastError! as NSString).utf8String, userdata)
            }
            return
        }

        imageCopy.deallocate()

        if ocrResult.blocks.isEmpty {
            DispatchQueue.main.async {
                lastError = "No text found"
                callback(nil, nil, 0, nil, 0, (lastError! as NSString).utf8String, userdata)
            }
            return
        }

        // Helper to deliver results with translated text blocks
        // The renderLanguage parameter determines which font/wrapping to use for rendering
        // blockTexts is an array of individual block texts for speech formatting
        let deliverResult: ([TranslatedBlock], [String], String?) -> Void = { translatedBlocks, blockTexts, renderLanguage in
            DispatchQueue.main.async {
                var imagePtr: UnsafeMutableRawPointer? = nil
                var imageSize = 0
                var soundPtr: UnsafeMutableRawPointer? = nil
                var soundSize = 0
                var textPtr: UnsafeMutablePointer<CChar>? = nil

                // Plain text version (for text output modes)
                let plainText = blockTexts.joined(separator: "\n")

                // Mode 0 or 3: Generate transparent image overlay (PNG with alpha)
                if outputMode == 0 || outputMode == 3 {
                    if let pngData = renderTransparentOverlay(width: ocrResult.width, height: ocrResult.height, blocks: translatedBlocks, targetLanguage: renderLanguage) {
                        imagePtr = malloc(pngData.count)
                        pngData.copyBytes(to: imagePtr!.assumingMemoryBound(to: UInt8.self), count: pngData.count)
                        imageSize = pngData.count
                    } else {
                        logTranslation("[Translation] ERROR: Overlay rendering failed")
                    }
                }

                // Mode 1 or 3: Generate speech
                if outputMode == 1 || outputMode == 3 {
                    let synthLanguage = targetLanguage ?? sourceLanguage
                    let synth = SpeechSynthesizer()
                    activeSynthesizer = synth

                    // Format text for speech with textbox announcements and cleanup
                    let speechText = formatTextForSpeech(blockTexts)

                    synth.synthesize(text: speechText, language: synthLanguage) { wavData, error in
                        activeSynthesizer = nil

                        if let wavData = wavData {
                            soundPtr = malloc(wavData.count)
                            wavData.copyBytes(to: soundPtr!.assumingMemoryBound(to: UInt8.self), count: wavData.count)
                            soundSize = wavData.count
                        } else {
                            NSLog("[AppleTranslate] Speech synthesis failed: %@", error ?? "unknown")
                        }

                        // For mode 3, also include text (plain version, not speech-formatted)
                        if outputMode == 3 {
                            textPtr = strdup(plainText)
                        }

                        callback(textPtr, imagePtr, imageSize, soundPtr, soundSize, nil, userdata)
                    }
                    return
                }

                // Mode 2: Just text for narrator
                if outputMode == 2 {
                    textPtr = strdup(plainText)
                }

                callback(textPtr, imagePtr, imageSize, soundPtr, soundSize, nil, userdata)
            }
        }

        // Step 2: Translate if needed
        if let target = targetLanguage, sourceLanguage != target {
            logTranslation("[Translation] Translating \(sourceLanguage ?? "auto") -> \(target)")
            #if canImport(Translation) && !os(tvOS)
            if #available(macOS 15.0, iOS 18.0, *) {
                Task { @MainActor in
                    // Translate all blocks sequentially
                    var translatedBlocks: [TranslatedBlock] = []
                    var allTranslatedText: [String] = []

                    for (i, block) in ocrResult.blocks.enumerated() {
                        await withCheckedContinuation { continuation in
                            TranslationManager.shared.translate(
                                text: block.text,
                                sourceLanguage: sourceLanguage,
                                targetLanguage: target
                            ) { translated, error in
                                if let error = error {
                                    logTranslation("[Translation] ERROR: Block \(i+1) failed: \(error)")
                                }
                                let finalText = translated ?? block.text
                                translatedBlocks.append(TranslatedBlock(
                                    text: finalText,
                                    boundingBox: block.boundingBox
                                ))
                                allTranslatedText.append(finalText)
                                continuation.resume()
                            }
                        }
                    }

                    deliverResult(translatedBlocks, allTranslatedText, target)
                }
                return
            } else {
                logTranslation("[Translation] On-device translation not available (requires macOS 15.0+ / iOS 18.0+)")
            }
            #else
            logTranslation("[Translation] Translation framework not available")
            #endif
        }

        // No translation needed - use OCR text directly
        // Use source language for font/wrapping since we're displaying original text
        let blocks = ocrResult.blocks.map { TranslatedBlock(text: $0.text, boundingBox: $0.boundingBox) }
        let blockTexts = ocrResult.blocks.map { $0.text }
        deliverResult(blocks, blockTexts, sourceLanguage)
    }
}

@_cdecl("apple_translate_free_string")
public func appleTranslateFreeString(_ str: UnsafeMutablePointer<CChar>?) {
    free(str)
}

@_cdecl("apple_translate_free_data")
public func appleTranslateFreeData(_ data: UnsafeMutableRawPointer?) {
    free(data)
}
