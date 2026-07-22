import AppKit
import Foundation
import UserNotifications

private final class ProcessOutputBox {
    var data = Data()
}

private struct WaitCancelled: Error {}

private struct CollectedJob: Decodable {
    let status: String
    let tags: [String]?
    let result: String?
    let error: String?
}

final class BckbrnrController: ObservableObject {
    @Published var isRunning = false
    @Published private(set) var isStopping = false
    @Published var statusText = restingHint
    static let restingHint = ""   // nothing when idle; errors still surface
    @Published var modelName = "No model chosen"
    @Published var rootDisplay = ""

    private let queue = DispatchQueue(label: "ai.nrvna.bckbrnr.jobs")
    private let collectQueue = DispatchQueue(
        label: "ai.nrvna.bckbrnr.results", attributes: .concurrent
    )
    private let defaults = UserDefaults.standard

    private var desk: URL
    private var promptDir: URL
    private var mappingDir: URL
    private var responseDir: URL
    private var workspace: URL

    private var engine: EnginePaths?
    private var daemon: Process?
    private var daemonLog: FileHandle?
    private let lifecycleLock = NSLock()
    private let waiterLock = NSLock()
    private var acceptingWaiters = true
    private var waiters: [ObjectIdentifier: Process] = [:]

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
        self.mappingDir = self.promptDir.appendingPathComponent(".jobs", isDirectory: true)
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
        }
        recoverCompletedResponses()
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
        guard !isStopping else { return }
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
            isRunning = true
            statusText = "Warming up…"
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
                if self.stopping() { return }
                switch self.engineStatusCode() {
                case 0:
                    if self.stopping() { return }
                    self.setStatus("Ready")
                    return
                case 2:
                    break // still loading
                default:
                    if launched == nil || launched?.isRunning != true {
                        if self.stopping() { return }
                        DispatchQueue.main.async { self.isRunning = false }
                        self.setStatus("Engine stopped during startup — see bckbrnr-engine.log")
                        return
                    }
                }
                Thread.sleep(forTimeInterval: 1)
            }
            if self.stopping() { return }
            self.setStatus("Engine is taking unusually long to load")
        }
    }

    /// Re-check whether a daemon is actually alive and flip the UI to match.
    /// Called when the popover opens so the prompt box reflects daemon state.
    /// If the daemon disappears after a prompt is accepted, wrk still queues it
    /// durably for the next Start.
    func refresh() {
        guard !isStopping else { return }
        // Status is a subprocess call; keep it off the main thread so opening
        // the popover never hitches on it.
        queue.async { [weak self] in
            guard let self else { return }
            let code = self.engineStatusCode()
            let alive = Self.isLive(code)
            DispatchQueue.main.async {
                guard !self.isStopping else { return }
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
        cancelWaiters()
        engineStop()
        daemon = nil
    }

    func stop() {
        guard !isStopping else { return }
        isStopping = true
        statusText = "Stopping…"
        DispatchQueue.global(qos: .userInitiated).async { [self] in
            self.engineStop()
            DispatchQueue.main.async {
                self.daemon = nil
                self.isRunning = false
                self.isStopping = false
                self.statusText = Self.restingHint
            }
        }
    }

    /// Reveal the answers in Finder. Prompt copies live in a hidden .prompt
    /// sibling, so the folder shows only responses.
    func openResponses() {
        try? ensureFolders()
        NSWorkspace.shared.open(responseDir)
    }

    // MARK: submit

    @discardableResult
    func submit(_ text: String) -> Bool {
        guard let engine = engine ?? EnginePaths.discover() else {
            setStatus("Engine binaries not found (set BCKBRNR_ENGINE_DIR)")
            return false
        }
        setStatus("Working…")
        let stem = Naming.uniqueStem(Naming.deriveStem(from: text), in: promptDir, ext: "txt")
        let promptFile = promptDir.appendingPathComponent("\(stem).txt")
        let responseFile = responseDir.appendingPathComponent("\(stem).txt")
        let errorFile = responseDir.appendingPathComponent("\(stem).error.txt")
        let workspacePath = workspace.path
        let token = "bckbrnr-" + UUID().uuidString.replacingOccurrences(of: "-", with: "")
        let mappingFile = mappingDir.appendingPathComponent(token)
        do {
            try text.write(to: promptFile, atomically: true, encoding: .utf8)
            try stem.write(to: mappingFile, atomically: true, encoding: .utf8)
            // Publish before returning so Return followed by Quit cannot lose
            // accepted work. wrk only performs small filesystem operations.
            let jobId = try runProcess(
                engine.wrk,
                arguments: [workspacePath, "-", "--tag", "bckbrnr", "--tag", token],
                input: text
            ).trimmingCharacters(in: .whitespacesAndNewlines)
            collectQueue.async { [weak self] in
                guard let self else { return }
                do {
                    let result = try self.runProcess(
                        engine.flw, arguments: [workspacePath, "-w", jobId], trackWaiter: true
                    )
                    try result.write(to: responseFile, atomically: true, encoding: .utf8)
                    self.setStatus("Ready")
                    self.notify(title: "bckbrnr — your answer is ready", body: stem, path: responseFile.path)
                } catch is WaitCancelled {
                    return
                } catch {
                    self.writeFailure(stem: stem, prompt: text, error: error, errorFile: errorFile)
                }
            }
        } catch {
            try? FileManager.default.removeItem(at: mappingFile)
            writeFailure(stem: stem, prompt: text, error: error, errorFile: errorFile)
            return false
        }
        return true
    }

    private func writeFailure(stem: String, prompt: String, error: Error, errorFile: URL) {
        // Failure is durable too: leave a readable artifact beside where the
        // answer would have been.
        let body = """
        bckbrnr couldn’t finish this prompt.

        PROMPT:
        \(prompt)

        ERROR:
        \(error.localizedDescription)
        """
        do {
            try body.write(to: errorFile, atomically: true, encoding: .utf8)
        } catch {
            setStatus("Couldn’t save failure: \(error.localizedDescription)")
            return
        }
        // Don't claim "Ready": a dead daemon is a likely cause of the
        // failure, so re-derive the status from the engine.
        refresh()
        notify(title: "bckbrnr — couldn’t finish", body: stem, path: errorFile.path)
    }

    private func recoverCompletedResponses() {
        // Root changes are allowed while stopped. Snapshot one complete desk
        // before dispatch so recovery can never read from one root and write
        // into another.
        guard let flw = (engine ?? EnginePaths.discover())?.flw else { return }
        let directories = [desk, promptDir, mappingDir, responseDir, workspace]
        let workspace = self.workspace
        let mappingDir = self.mappingDir
        let responseDir = self.responseDir
        queue.async { [weak self] in
            guard let self else { return }
            for directory in directories {
                try? FileManager.default.createDirectory(
                    at: directory, withIntermediateDirectories: true
                )
            }
            guard let output = try? self.runProcess(
                flw,
                arguments: [workspace.path, "--tag", "bckbrnr", "--json"],
                acceptedExitCodes: [0, 1]
            ) else { return }
            let decoder = JSONDecoder()
            for line in output.split(separator: "\n") {
                guard let job = try? decoder.decode(CollectedJob.self, from: Data(line.utf8)),
                      job.status == "done" || job.status == "failed",
                      let token = job.tags?.first(where: { $0.hasPrefix("bckbrnr-") }),
                      let stem = try? String(contentsOf: mappingDir.appendingPathComponent(token)),
                      let content = job.status == "done" ? job.result : job.error
                else { continue }
                let failed = job.status == "failed"
                let ext = failed ? ".error.txt" : ".txt"
                let target = responseDir.appendingPathComponent("\(stem)\(ext)")
                guard !FileManager.default.fileExists(atPath: target.path) else { continue }
                do {
                    try content.write(to: target, atomically: true, encoding: .utf8)
                } catch {
                    self.setStatus("Couldn’t recover answer: \(error.localizedDescription)")
                    continue
                }
                self.notify(
                    title: failed ? "bckbrnr — couldn’t finish" : "bckbrnr — your answer is ready",
                    body: stem,
                    path: target.path
                )
            }
        }
    }

    // MARK: model

    func chooseModel() {
        guard !isRunning && !isStopping else { setStatus("Stop before changing the model"); return }
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
        guard !isRunning && !isStopping else { setStatus("Stop before changing the root"); return }
        let panel = NSOpenPanel()
        panel.title = "Choose a folder for this utility"
        panel.canChooseDirectories = true
        panel.canChooseFiles = false
        panel.canCreateDirectories = true
        panel.allowsMultipleSelection = false
        if panel.runModal() == .OK, let url = panel.url { setRoot(url.path) }
    }

    func setRoot(_ raw: String) {
        guard !isRunning && !isStopping else { setStatus("Stop before changing the root"); return }
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
        mappingDir = promptDir.appendingPathComponent(".jobs", isDirectory: true)
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
        for url in [desk, promptDir, mappingDir, responseDir, workspace] {
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
        let logURL = responseDir.appendingPathComponent("bckbrnr-engine.log")
        if !FileManager.default.fileExists(atPath: logURL.path) {
            FileManager.default.createFile(atPath: logURL.path, contents: nil)
        }
        let log = try FileHandle(forWritingTo: logURL)
        try log.seekToEnd()
        log.write("\n--- starting \(model.lastPathComponent) at \(Date()) ---\n".data(using: .utf8)!)
        process.standardOutput = log
        process.standardError = log
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
        ]) { current, _ in current }
        do {
            try process.run()
        } catch {
            try? log.close()
            throw error
        }
        try? daemonLog?.close()
        daemonLog = log
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
        lifecycleLock.lock()
        defer { lifecycleLock.unlock() }
        guard let engine = engine ?? EnginePaths.discover() else { return }
        let process = Process()
        process.executableURL = engine.nrvnad
        process.arguments = ["stop", workspace.path, "--timeout", "5"]
        process.standardOutput = Pipe()
        process.standardError = Pipe()
        guard (try? process.run()) != nil else {
            try? daemonLog?.close()
            daemonLog = nil
            return
        }
        process.waitUntilExit()
        try? daemonLog?.close()
        daemonLog = nil
    }

    private func stopping() -> Bool {
        if Thread.isMainThread { return isStopping }
        return DispatchQueue.main.sync { isStopping }
    }

    private func cancelWaiters() {
        waiterLock.lock()
        acceptingWaiters = false
        let running = Array(waiters.values)
        waiters.removeAll()
        waiterLock.unlock()
        for process in running where process.isRunning { process.terminate() }
    }

    private func runProcess(
        _ executable: URL,
        arguments: [String],
        input: String? = nil,
        acceptedExitCodes: Set<Int32> = [0],
        trackWaiter: Bool = false
    ) throws -> String {
        let process = Process(); let stdout = Pipe(); let stderr = Pipe(); let stdin = Pipe()
        process.executableURL = executable
        process.arguments = arguments
        process.standardOutput = stdout
        process.standardError = stderr
        if input != nil { process.standardInput = stdin }

        let waiterID = ObjectIdentifier(process)
        if trackWaiter {
            waiterLock.lock()
            guard acceptingWaiters else {
                waiterLock.unlock()
                throw WaitCancelled()
            }
            do {
                // Launch and registration are one operation with respect to
                // cancelWaiters(), so quit cannot miss a newly-started waiter.
                try process.run()
                waiters[waiterID] = process
            } catch {
                waiterLock.unlock()
                throw error
            }
            waiterLock.unlock()
        } else {
            try process.run()
        }
        defer {
            if trackWaiter {
                waiterLock.lock()
                waiters.removeValue(forKey: waiterID)
                waiterLock.unlock()
            }
        }

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
        if trackWaiter && process.terminationStatus == SIGTERM { throw WaitCancelled() }
        guard acceptedExitCodes.contains(process.terminationStatus) else {
            throw NSError(domain: "bckbrnr", code: Int(process.terminationStatus),
                          userInfo: [NSLocalizedDescriptionKey: errOut.isEmpty ? output : errOut])
        }
        return output
    }

    private func notify(title: String, body: String, path: String?) {
        guard Bundle.main.bundleIdentifier != nil else { return }
        let content = UNMutableNotificationContent()
        content.title = title
        content.body = body
        if let path { content.userInfo = ["path": path] }
        let request = UNNotificationRequest(
            identifier: UUID().uuidString, content: content, trigger: nil
        )
        UNUserNotificationCenter.current().add(request)
    }

    private func setStatus(_ value: String) {
        DispatchQueue.main.async { self.statusText = value }
    }
}
