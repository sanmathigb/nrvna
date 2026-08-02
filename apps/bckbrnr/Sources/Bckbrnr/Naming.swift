import Foundation

/// Turns a prompt into a friendly, collision-safe filename stem.
/// bckbrnr derives the file name from the prompt.
enum Naming {
    /// First non-empty line, cleaned of path-illegal characters,
    /// whitespace-collapsed, capped at 40 characters. Empty → "prompt".
    static func deriveStem(from prompt: String) -> String {
        let firstLine = prompt
            .split(whereSeparator: \.isNewline)
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .first(where: { !$0.isEmpty }) ?? ""

        var cleaned = firstLine
        for bad in ["/", ":", "\\"] {
            cleaned = cleaned.replacingOccurrences(of: bad, with: " ")
        }
        cleaned = cleaned
            .components(separatedBy: .whitespacesAndNewlines)
            .filter { !$0.isEmpty }
            .joined(separator: " ")

        if cleaned.count > 40 {
            let capped = String(cleaned.prefix(40))
            // Cut back to the last whole word so we don't end mid-word.
            if let lastSpace = capped.lastIndex(of: " ") {
                cleaned = String(capped[..<lastSpace])
            } else {
                cleaned = capped
            }
            cleaned = cleaned.trimmingCharacters(in: .whitespaces)
        }
        // A leading dot would hide the answer file in response/.
        // The answer would be delivered but invisible in Finder.
        while cleaned.hasPrefix(".") { cleaned.removeFirst() }
        cleaned = cleaned.trimmingCharacters(in: .whitespaces)
        return cleaned.isEmpty ? "prompt" : cleaned
    }

    /// Return a unique stem. Add -2, -3, and later numbers when needed.
    /// Never overwrites an existing file (durability).
    static func uniqueStem(_ base: String, in dir: URL, ext: String) -> String {
        let fm = FileManager.default
        var candidate = base
        var n = 2
        while fm.fileExists(atPath: dir.appendingPathComponent("\(candidate).\(ext)").path) {
            candidate = "\(base)-\(n)"
            n += 1
        }
        return candidate
    }
}
