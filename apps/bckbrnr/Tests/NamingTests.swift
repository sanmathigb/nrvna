import Foundation

// Test harness for Naming. Compiled with Sources/Bckbrnr/Naming.swift into a
// standalone binary by `make test` — no SPM, matching the app's build style.

@main
enum NamingTests {
    static var failures = 0

    static func expect(_ actual: String, _ expected: String, _ label: String) {
        if actual != expected {
            print("FAIL \(label): expected \"\(expected)\", got \"\(actual)\"")
            failures += 1
        }
    }

    static func main() {
        // deriveStem: empty and whitespace-only prompts fall back to "prompt"
        expect(Naming.deriveStem(from: ""), "prompt", "empty prompt")
        expect(Naming.deriveStem(from: "  \n\t\n  "), "prompt", "whitespace-only prompt")

        // deriveStem: first non-empty line wins
        expect(Naming.deriveStem(from: "hello world"), "hello world", "single line")
        expect(Naming.deriveStem(from: "\n\n  second line is first non-empty\nthird"),
               "second line is first non-empty", "skips empty lines")

        // deriveStem: path-illegal characters become spaces, whitespace collapses
        expect(Naming.deriveStem(from: "a/b:c\\d"), "a b c d", "illegal characters")
        expect(Naming.deriveStem(from: "a   b\t\tc"), "a b c", "whitespace collapse")

        // deriveStem: caps at 40 characters, cutting back to a whole word
        let words = Array(repeating: "abcdefghij", count: 5).joined(separator: " ")
        expect(Naming.deriveStem(from: words),
               "abcdefghij abcdefghij abcdefghij", "cap at word boundary")

        // deriveStem: leading dots would hide the answer file in Finder
        expect(Naming.deriveStem(from: ".gitignore for a python project"),
               "gitignore for a python project", "leading dot stripped")
        expect(Naming.deriveStem(from: "..."), "prompt", "dots-only prompt")
        expect(Naming.deriveStem(from: ". leading dot then words"),
               "leading dot then words", "dot then words")

        // deriveStem: a single word longer than 40 characters is hard-cut
        let longWord = String(repeating: "x", count: 50)
        expect(Naming.deriveStem(from: longWord),
               String(repeating: "x", count: 40), "long single word")

        // uniqueStem: suffixes -2, -3… instead of ever reusing a taken name
        let dir = FileManager.default.temporaryDirectory
            .appendingPathComponent("naming-tests-\(ProcessInfo.processInfo.processIdentifier)")
        try! FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: dir) }

        expect(Naming.uniqueStem("note", in: dir, ext: "txt"), "note", "unique when free")
        try! "x".write(to: dir.appendingPathComponent("note.txt"), atomically: true, encoding: .utf8)
        expect(Naming.uniqueStem("note", in: dir, ext: "txt"), "note-2", "first collision")
        try! "x".write(to: dir.appendingPathComponent("note-2.txt"), atomically: true, encoding: .utf8)
        expect(Naming.uniqueStem("note", in: dir, ext: "txt"), "note-3", "second collision")
        expect(Naming.uniqueStem("note", in: dir, ext: "md"), "note", "extension-scoped")

        if failures > 0 {
            print("\(failures) failure(s)")
            exit(1)
        }
        print("naming tests: all passed")
    }
}
