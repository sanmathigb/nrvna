import Foundation

/// Turns a prompt into a friendly, collision-safe filename stem.
/// The user never names anything — bckbrnr derives it from what they typed.
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
        return cleaned.isEmpty ? "prompt" : cleaned
    }

    /// A stem unique within `dir` for `ext`, appending -2, -3… if taken.
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
