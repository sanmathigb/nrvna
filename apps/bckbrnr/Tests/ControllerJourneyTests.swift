import Foundation

@main
enum ControllerJourneyTests {
    static func waitUntil(_ timeout: TimeInterval, _ condition: () -> Bool) -> Bool {
        let deadline = Date().addingTimeInterval(timeout)
        while Date() < deadline {
            if condition() { return true }
            RunLoop.current.run(until: Date().addingTimeInterval(0.1))
        }
        return condition()
    }

    static func require(_ condition: Bool, _ message: String) throws {
        guard condition else {
            throw NSError(domain: "bckbrnr-tests", code: 1,
                          userInfo: [NSLocalizedDescriptionKey: message])
        }
    }

    static func answerFiles(in directory: URL) -> [URL] {
        let files = (try? FileManager.default.contentsOfDirectory(
            at: directory, includingPropertiesForKeys: nil
        )) ?? []
        return files.filter {
            $0.pathExtension == "txt" && !$0.lastPathComponent.hasSuffix(".error.txt")
        }
    }

    static func main() throws {
        let fm = FileManager.default
        let root = fm.temporaryDirectory.appendingPathComponent(
            "bckbrnr-journey-\(ProcessInfo.processInfo.processIdentifier)", isDirectory: true
        )
        try? fm.removeItem(at: root)
        defer { try? fm.removeItem(at: root) }

        let defaults = UserDefaults.standard
        let previousRoot = defaults.string(forKey: "deskPath")
        defaults.set(root.path, forKey: "deskPath")
        defer {
            if let previousRoot { defaults.set(previousRoot, forKey: "deskPath") }
            else { defaults.removeObject(forKey: "deskPath") }
        }

        let controller = BckbrnrController()
        controller.start()
        try require(waitUntil(60) { controller.statusText == "Ready" },
                    "engine did not become ready: \(controller.statusText)")

        controller.submit("Reply briefly: first journey answer")
        controller.submit("Reply briefly: second journey answer")

        let mappings = root.appendingPathComponent(".prompt/.jobs", isDirectory: true)
        try require(waitUntil(3) {
            ((try? fm.contentsOfDirectory(at: mappings, includingPropertiesForKeys: nil)) ?? []).count == 2
        }, "rapid prompts were not assigned durable identities")

        let responses = root.appendingPathComponent("response", isDirectory: true)
        try require(waitUntil(120) { answerFiles(in: responses).count == 2 },
                    "two submitted prompts did not produce two answers")

        controller.stop()
        try require(waitUntil(10) { !controller.isRunning }, "engine did not stop")

        let removed = answerFiles(in: responses)[0]
        try fm.removeItem(at: removed)
        let reopened = BckbrnrController()
        try withExtendedLifetime(reopened) {
            try require(waitUntil(5) { fm.fileExists(atPath: removed.path) },
                        "cold recovery did not restore \(removed.lastPathComponent)")
        }

        print("controller journey tests: all passed")
    }
}
