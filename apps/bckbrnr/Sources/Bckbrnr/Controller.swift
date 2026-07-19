import AppKit
import Foundation

private final class ProcessOutputBox {
    var data = Data()
}

final class BckbrnrController: ObservableObject {
    @Published var isRunning = false
    @Published var statusText = restingHint
    static let restingHint = ""   // nothing when idle; errors still surface
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
        self.promptDir = desk.appendingPathComponent(".prompt", isDirectory: true)
        self.responseDir = desk.appendingPathComponent("response", isDirectory: true)
        self.workspace = desk.appendingPathComponent(".ws", isDirectory: true)
        self.rootDisplay = Self.collapseTilde(desk.path)
        self.engine = EnginePaths.discover()
        if let model = resolveModel() { modelName = model.lastPathComponent }
        let code = engineStatusCode()
        if Self.isLive(code) {
            isRunning = true
            statusText = code == 0 ? "Ready" : "Warming up…"
            if code == 2 { awaitReadiness(launched: nil) }
            recoverCompletedResponses()
        }
        // Don't leave a daemon we launched running headless after the app quits.
        NotificationCenter.default.addObserver(
            forName: NSApplication.willTerminateNotification, object: nil, queue: .main
        ) { [weak self] _ in self?.terminateWorkspaceDaemon() }
    }

    deinit {
        // No dispatch here: capturing self in a queued closure during deinit
        // is object resurrection. Stop the daemon synchronously and be done.
        terminateWorkspaceDaemon()
    }

    /// 0 (ready) and 2 (starting) are the live states; any other status code
    /// means there is no daemon to talk to.
    private static func isLive(_ code: Int32) -> Bool { code == 0 || code == 2 }

    // MARK: lifecycle

    func start() {
        do {
            try ensureFolders()
            guard let model = resolveModel() else { setStatus("Choose a model to begin"); return }
            guard let engine = EnginePaths.discover() else {
                setStatus("Engine binaries not found (set BCKBRNR_ENGINE_DIR)"); return
            }
            self.engine = engine
            let code = engineStatusCode()
            if Self.isLive(code) {
                daemon = nil                       // adopt the already-running daemon
            } else if daemon?.isRunning != true {
                try startDaemon(model: model, engine: engine)
            }
            DispatchQueue.main.async {
                self.isRunning = true
                self.statusText = "Warming up…"
            }
            awaitReadiness(launched: daemon)
            recoverCompletedResponses()
        } catch {
            setStatus("Start failed: \(error.localizedDescription)")
        }
    }

    /// Poll the engine until the model is loaded. "Ready" is the engine's
    /// word, not ours: exit 0 means ready, 2 means still loading, 1 means
    /// the daemon is gone.
    private func awaitReadiness(launched: Process?) {
        queue.async { [weak self] in
            guard let self else { return }
            let deadline = Date().addingTimeInterval(180)
            while Date() < deadline {
                switch self.engineStatusCode() {
                case 0:
                    self.setStatus("Ready")
                    return
                case 2:
                    break // still loading
                default:
                    if launched == nil || launched?.isRunning != true {
                        DispatchQueue.main.async { self.isRunning = false }
                        self.setStatus("Engine stopped during startup")
                        return
                    }
                }
                Thread.sleep(forTimeInterval: 1)
            }
            self.setStatus("Engine is taking unusually long to load")
        }
    }

    /// Re-check whether a daemon is actually alive and flip the UI to match.
    /// Called when the popover opens so the prompt box only shows for a live
    /// daemon — you can never send a prompt into nothing.
    func refresh() {
        // Status is a subprocess call; keep it off the main thread so opening
        // the popover never hitches on it.
        queue.async { [weak self] in
            guard let self else { return }
            let code = self.engineStatusCode()
            let alive = Self.isLive(code)
            DispatchQueue.main.async {
                self.isRunning = alive
                // Always refresh the label: starting → ready flips the text even
                // when the running boolean hasn't changed.
                self.statusText = alive ? (code == 0 ? "Ready" : "Warming up…") : Self.restingHint
            }
            if alive { self.recoverCompletedResponses() }
        }
    }

    /// On app quit, shut down the daemon bound to this utility workspace.
    /// bckbrnr owns this workspace; leaving it headless is more surprising than
    /// stopping it. The engine identifies and stops its own process — we never
    /// signal a pid ourselves.
    func terminateWorkspaceDaemon() {
        engineStop()
        daemon = nil
    }

    func stop() {
        engineStop()
        daemon = nil
        DispatchQueue.main.async {
            self.isRunning = false
            self.statusText = Self.restingHint
        }
    }

    /// Reveal the answers in Finder. Prompt copies live in a hidden .prompt
    /// sibling, so the workspace shows only responses.
    func openResponses() {
        try? ensureFolders()
        NSWorkspace.shared.open(responseDir)
    }

    // MARK: submit

    func submit(_ text: String) {
        guard let engine else { refresh(); return }
        setStatus("Working…")
        queue.async { [weak self] in
            guard let self else { return }
            // Never send into a dead workspace: require a live daemon, not
            // just that Start was pressed at some point. (Starting counts —
            // the queue holds the prompt until the model finishes loading.)
            // Checked here so the subprocess call stays off the main thread.
            guard Self.isLive(self.engineStatusCode()) else { self.refresh(); return }
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
                // Failure is durable too: leave a readable artifact beside
                // where the answer would have been.
                let errorFile = self.responseDir.appendingPathComponent("\(stem).error.txt")
                let body = """
                bckbrnr couldn’t finish this prompt.

                PROMPT:
                \(text)

                ERROR:
                \(error.localizedDescription)
                """
                try? body.write(to: errorFile, atomically: true, encoding: .utf8)
                // Don't claim "Ready": a dead daemon is a likely cause of the
                // failure, so re-derive the status from the engine.
                self.refresh()
                self.notify(title: "bckbrnr — couldn’t finish", body: stem, path: errorFile.path)
            }
        }
    }

    private func recoverCompletedResponses() {
        queue.async { [weak self] in
            guard let self else { return }
            try? self.ensureFolders()
            // Success and failure are both durable: backfill answers from
            // completed jobs and error artifacts from failed ones.
            self.reconcile(subdir: "output", source: "result.txt",
                           ext: ".txt", title: "bckbrnr — your answer is ready")
            self.reconcile(subdir: "failed", source: "error.txt",
                           ext: ".error.txt", title: "bckbrnr — couldn’t finish")
        }
    }

    /// Mirror a finished nrvna job into the user-facing response folder, once.
    /// Runs on the work queue.
    private func reconcile(subdir: String, source: String, ext: String, title: String) {
        let dir = workspace.appendingPathComponent(subdir, isDirectory: true)
        guard let jobs = try? FileManager.default.contentsOfDirectory(
            at: dir, includingPropertiesForKeys: [.isDirectoryKey], options: [.skipsHiddenFiles]
        ) else { return }
        for job in jobs {
            guard (try? job.resourceValues(forKeys: [.isDirectoryKey]))?.isDirectory == true,
                  let prompt = try? String(contentsOf: job.appendingPathComponent("prompt.txt")),
                  let content = try? String(contentsOf: job.appendingPathComponent(source))
            else { continue }
            let base = recoveryStem(for: prompt, ext: ext)
            let target = responseDir.appendingPathComponent("\(base)\(ext)")
            guard !FileManager.default.fileExists(atPath: target.path) else { continue }
            try? content.write(to: target, atomically: true, encoding: .utf8)
            notify(title: title, body: base, path: target.path)
        }
    }

    /// Submission already chose a collision-safe prompt filename. Recover that
    /// same identity instead of deriving the unsuffixed first-line stem again.
    private func recoveryStem(for prompt: String, ext: String) -> String {
        let fallback = Naming.deriveStem(from: prompt)
        guard let files = try? FileManager.default.contentsOfDirectory(
            at: promptDir, includingPropertiesForKeys: nil, options: [.skipsHiddenFiles]
        ) else { return fallback }
        for file in files.sorted(by: { $0.lastPathComponent < $1.lastPathComponent })
        where file.pathExtension == "txt" {
            guard let saved = try? String(contentsOf: file), saved == prompt else { continue }
            let stem = file.deletingPathExtension().lastPathComponent
            let target = responseDir.appendingPathComponent("\(stem)\(ext)")
            if !FileManager.default.fileExists(atPath: target.path) { return stem }
        }
        return fallback
    }

    // MARK: model

    func chooseModel() {
        guard !isRunning else { setStatus("Stop before changing the model"); return }
        let panel = NSOpenPanel()
        panel.title = "Choose a GGUF text model"
        panel.allowsMultipleSelection = false
        panel.canChooseDirectories = false
        panel.canChooseFiles = true
        if panel.runModal() == .OK, let url = panel.url {
            defaults.set(url.path, forKey: "textModelPath")
            modelName = url.lastPathComponent
            setStatus("Model selected — press Start")
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
        promptDir = url.appendingPathComponent(".prompt", isDirectory: true)
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
        // Conservative defaults for a non-technical, single-GPU machine:
        // CPU only (0 GPU layers) avoids the discrete-GPU overflow that yields
        // gibberish; low temp + thinking-off keep small models terse and on-task;
        // modest predict/ctx bound latency and VRAM. Any of these can be
        // overridden by exporting the same NRVNA_* var before launch.
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

    /// Exit codes from `nrvnad status`: 0 ready, 2 starting, 1 not running.
    /// The engine owns lifecycle truth; bckbrnr never reads pidfiles or
    /// signals processes itself.
    private func engineStatusCode() -> Int32 {
        guard let engine = engine ?? EnginePaths.discover() else { return 1 }
        let process = Process()
        process.executableURL = engine.nrvnad
        process.arguments = ["status", workspace.path]
        process.standardOutput = Pipe()
        process.standardError = Pipe()
        guard (try? process.run()) != nil else { return 1 }
        process.waitUntilExit()
        return process.terminationStatus
    }

    /// Graceful stop, delegated to the engine (short timeout: this runs on
    /// app-quit, where macOS gives us limited time).
    private func engineStop() {
        guard let engine = engine ?? EnginePaths.discover() else { return }
        let process = Process()
        process.executableURL = engine.nrvnad
        process.arguments = ["stop", workspace.path, "--timeout", "5"]
        process.standardOutput = Pipe()
        process.standardError = Pipe()
        try? process.run()
        process.waitUntilExit()
    }

    private func runProcess(_ executable: URL, arguments: [String], input: String? = nil) throws -> String {
        let process = Process(); let stdout = Pipe(); let stderr = Pipe(); let stdin = Pipe()
        process.executableURL = executable
        process.arguments = arguments
        process.standardOutput = stdout
        process.standardError = stderr
        if input != nil { process.standardInput = stdin }
        try process.run()

        // Drain both pipes while the child runs. Waiting first can deadlock if
        // either pipe fills and the child blocks before it can exit.
        let stdoutBox = ProcessOutputBox()
        let stderrBox = ProcessOutputBox()
        let readers = DispatchGroup()
        readers.enter()
        DispatchQueue.global(qos: .utility).async {
            stdoutBox.data = stdout.fileHandleForReading.readDataToEndOfFile()
            readers.leave()
        }
        readers.enter()
        DispatchQueue.global(qos: .utility).async {
            stderrBox.data = stderr.fileHandleForReading.readDataToEndOfFile()
            readers.leave()
        }
        if let input {
            stdin.fileHandleForWriting.write(input.data(using: .utf8) ?? Data())
            try? stdin.fileHandleForWriting.close()
        }
        process.waitUntilExit()
        readers.wait()
        let output = String(data: stdoutBox.data, encoding: .utf8) ?? ""
        let errOut = String(data: stderrBox.data, encoding: .utf8) ?? ""
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
