import AppKit
import Foundation

final class BckbrnrController: ObservableObject {
    @Published var isRunning = false
    @Published var statusText = restingHint
    static let restingHint = "start model. submit prompts. async inference."
    @Published var modelName = "No model chosen"
    @Published var rootDisplay = ""

    private let queue = DispatchQueue(label: "ai.nrvna.bckbrnr.jobs")
    private let defaults = UserDefaults.standard

    private var desk: URL
    private var promptDir: URL
    private var responseDir: URL
    private var workspace: URL

    private var engine: EnginePaths?
    private var daemon: Process?

    init() {
        // bckbrnr is the umbrella; each utility is a modality-named workspace
        // beneath it. Text ships now; vision/voice/embed are siblings later.
        let defaultDesk = FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("bckbrnr", isDirectory: true)
            .appendingPathComponent("text", isDirectory: true)
        let saved = defaults.string(forKey: "deskPath")
        let desk = saved.map { URL(fileURLWithPath: NSString(string: $0).expandingTildeInPath) }
            ?? defaultDesk
        self.desk = desk
        self.promptDir = desk.appendingPathComponent("prompt", isDirectory: true)
        self.responseDir = desk.appendingPathComponent("response", isDirectory: true)
        self.workspace = desk.appendingPathComponent(".ws", isDirectory: true)
        self.rootDisplay = Self.collapseTilde(desk.path)
        self.engine = EnginePaths.discover()
        if let model = resolveModel() { modelName = model.lastPathComponent }
    }

    // MARK: lifecycle

    func start() {
        do {
            try ensureFolders()
            guard let model = resolveModel() else { setStatus("Choose a model to begin"); return }
            guard let engine = EnginePaths.discover() else {
                setStatus("Engine binaries not found (set BCKBRNR_ENGINE_DIR)"); return
            }
            self.engine = engine
            if workspaceDaemonPid() != nil {
                daemon = nil                       // adopt an already-running daemon
            } else if daemon?.isRunning != true {
                try startDaemon(model: model, engine: engine)
            }
            DispatchQueue.main.async {
                self.isRunning = true
                self.statusText = "Ready"
            }
        } catch {
            setStatus("Start failed: \(error.localizedDescription)")
        }
    }

    /// Re-check whether a daemon is actually alive and flip the UI to match.
    /// Called when the popover opens so the prompt box only shows for a live
    /// daemon — you can never send a prompt into nothing.
    func refresh() {
        let alive = daemon?.isRunning == true || workspaceDaemonPid() != nil
        DispatchQueue.main.async {
            if self.isRunning != alive {
                self.isRunning = alive
                self.statusText = alive ? "Ready" : Self.restingHint
            }
        }
    }

    func stop() {
        if daemon?.isRunning == true { daemon?.terminate() }
        else if let pid = workspaceDaemonPid() { kill(pid, SIGTERM) }
        daemon = nil
        DispatchQueue.main.async {
            self.isRunning = false
            self.statusText = Self.restingHint
        }
    }

    // MARK: submit

    func submit(_ text: String) {
        // Never send into a dead workspace: require a live daemon, not just
        // that Start was pressed at some point.
        let alive = daemon?.isRunning == true || workspaceDaemonPid() != nil
        guard alive, let engine else { refresh(); return }
        setStatus("Working…")
        queue.async { [weak self] in
            guard let self else { return }
            let stem = Naming.uniqueStem(Naming.deriveStem(from: text), in: self.promptDir, ext: "txt")
            let promptFile = self.promptDir.appendingPathComponent("\(stem).txt")
            let responseFile = self.responseDir.appendingPathComponent("\(stem).txt")
            do {
                try text.write(to: promptFile, atomically: true, encoding: .utf8)
                let jobId = try self.runProcess(engine.wrk, arguments: [self.workspace.path, "-"], input: text)
                    .trimmingCharacters(in: .whitespacesAndNewlines)
                let result = try self.runProcess(engine.flw, arguments: [self.workspace.path, "-w", jobId])
                try result.write(to: responseFile, atomically: true, encoding: .utf8)
                self.setStatus("Ready")
                self.notify(title: "bckbrnr — your answer is ready", body: stem, path: responseFile.path)
            } catch {
                self.setStatus("Ready")
                self.notify(title: "bckbrnr — couldn’t finish", body: stem, path: nil)
            }
        }
    }

    // MARK: model

    func chooseModel() {
        let panel = NSOpenPanel()
        panel.title = "Choose a GGUF text model"
        panel.allowsMultipleSelection = false
        panel.canChooseDirectories = false
        panel.canChooseFiles = true
        if panel.runModal() == .OK, let url = panel.url {
            defaults.set(url.path, forKey: "textModelPath")
            modelName = url.lastPathComponent
            setStatus(isRunning ? "Restart to use the new model" : "Model selected — press Start")
        }
    }

    // MARK: root directory

    /// Point the daemon at a different root. Only allowed while stopped — a
    /// running daemon is bound to the old workspace, so changing under it
    /// would split the queue. Takes effect on the next Start.
    func chooseRoot() {
        guard !isRunning else { setStatus("Stop before changing the root"); return }
        let panel = NSOpenPanel()
        panel.title = "Choose a folder for this utility"
        panel.canChooseDirectories = true
        panel.canChooseFiles = false
        panel.canCreateDirectories = true
        panel.allowsMultipleSelection = false
        if panel.runModal() == .OK, let url = panel.url { setRoot(url.path) }
    }

    func setRoot(_ raw: String) {
        guard !isRunning else { setStatus("Stop before changing the root"); return }
        let trimmed = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { rootDisplay = Self.collapseTilde(desk.path); return }
        let expanded = NSString(string: trimmed).expandingTildeInPath
        applyDesk(URL(fileURLWithPath: expanded, isDirectory: true))
        defaults.set(desk.path, forKey: "deskPath")
        if let model = resolveModel() { modelName = model.lastPathComponent }
        setStatus("Root set — press Start")
    }

    private func applyDesk(_ url: URL) {
        desk = url
        promptDir = url.appendingPathComponent("prompt", isDirectory: true)
        responseDir = url.appendingPathComponent("response", isDirectory: true)
        workspace = url.appendingPathComponent(".ws", isDirectory: true)
        rootDisplay = Self.collapseTilde(url.path)
    }

    private static func collapseTilde(_ path: String) -> String {
        let home = FileManager.default.homeDirectoryForCurrentUser.path
        return path.hasPrefix(home) ? "~" + path.dropFirst(home.count) : path
    }

    // MARK: helpers (carried over from the original app)

    private func ensureFolders() throws {
        for url in [desk, promptDir, responseDir, workspace] {
            try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        }
    }

    private func resolveModel() -> URL? {
        let env = ProcessInfo.processInfo.environment
        let candidates = [env["BCKBRNR_TEXT_MODEL"], defaults.string(forKey: "textModelPath")].compactMap { $0 }
        for path in candidates {
            let url = URL(fileURLWithPath: NSString(string: path).expandingTildeInPath)
            if FileManager.default.fileExists(atPath: url.path) { return url }
        }
        return nil
    }

    private func startDaemon(model: URL, engine: EnginePaths) throws {
        let process = Process()
        process.executableURL = engine.nrvnad
        process.arguments = [model.path, workspace.path, "-w", "1"]
        process.environment = ProcessInfo.processInfo.environment.merging([
            "NRVNA_GPU_LAYERS": "0",
            "NRVNA_TEMP": "0.3",
            "NRVNA_PREDICT": "1024",
            "NRVNA_MAX_CTX": "4096",
            "NRVNA_THINKING": "0"
        ]) { _, new in new }
        try process.run()
        daemon = process
    }

    private func workspaceDaemonPid() -> pid_t? {
        let pidFile = workspace.appendingPathComponent(".nrvnad.pid")
        guard let raw = try? String(contentsOf: pidFile).trimmingCharacters(in: .whitespacesAndNewlines),
              let value = Int32(raw), value > 0 else { return nil }
        return kill(value, 0) == 0 ? value : nil
    }

    private func runProcess(_ executable: URL, arguments: [String], input: String? = nil) throws -> String {
        let process = Process(); let stdout = Pipe(); let stderr = Pipe(); let stdin = Pipe()
        process.executableURL = executable
        process.arguments = arguments
        process.standardOutput = stdout
        process.standardError = stderr
        if input != nil { process.standardInput = stdin }
        try process.run()
        if let input {
            stdin.fileHandleForWriting.write(input.data(using: .utf8) ?? Data())
            try? stdin.fileHandleForWriting.close()
        }
        process.waitUntilExit()
        let output = String(data: stdout.fileHandleForReading.readDataToEndOfFile(), encoding: .utf8) ?? ""
        let errOut = String(data: stderr.fileHandleForReading.readDataToEndOfFile(), encoding: .utf8) ?? ""
        guard process.terminationStatus == 0 else {
            throw NSError(domain: "bckbrnr", code: Int(process.terminationStatus),
                          userInfo: [NSLocalizedDescriptionKey: errOut.isEmpty ? output : errOut])
        }
        return output
    }

    private func notify(title: String, body: String, path: String?) {
        DispatchQueue.main.async {
            let n = NSUserNotification()
            n.title = title
            n.informativeText = body
            if let path { n.userInfo = ["path": path] }
            NSUserNotificationCenter.default.deliver(n)
        }
    }

    private func setStatus(_ value: String) {
        DispatchQueue.main.async { self.statusText = value }
    }
}
