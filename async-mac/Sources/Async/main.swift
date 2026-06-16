import AppKit
import Darwin
import Foundation

private let appName = "Async"

private final class AsyncPaths {
    let desk: URL
    let dropoff: URL
    let inProgress: URL
    let pickup: URL
    let problems: URL
    let workspace: URL

    init(desk: URL) {
        self.desk = desk
        self.dropoff = desk.appendingPathComponent("Dropoff", isDirectory: true)
        self.inProgress = desk.appendingPathComponent("InProgress", isDirectory: true)
        self.pickup = desk.appendingPathComponent("Pickup", isDirectory: true)
        self.problems = desk.appendingPathComponent("Problems", isDirectory: true)
        self.workspace = desk.appendingPathComponent(".nrvna/workspaces/text", isDirectory: true)
    }

    func ensure() throws {
        for url in [desk, dropoff, inProgress, pickup, problems, workspace] {
            try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        }
    }
}

private final class AsyncController: NSObject {
    private let queue = DispatchQueue(label: "ai.nrvna.async.jobs")
    private let defaults = UserDefaults.standard

    private var paths: AsyncPaths
    private var engine: EnginePaths?
    private var daemon: Process?
    private var timer: Timer?
    private var activeJobs = 0

    var onStatus: ((String) -> Void)?

    override init() {
        let savedDesk = defaults.string(forKey: "deskPath")
        let desk = savedDesk.map { URL(fileURLWithPath: NSString(string: $0).expandingTildeInPath) }
            ?? FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Async", isDirectory: true)
        self.paths = AsyncPaths(desk: desk)
        self.engine = EnginePaths.discover()
        super.init()
    }

    var deskURL: URL { paths.desk }
    var dropoffURL: URL { paths.dropoff }
    var pickupURL: URL { paths.pickup }
    var problemsURL: URL { paths.problems }
    var isRunning: Bool { daemon?.isRunning == true }

    func chooseModel(from window: NSWindow) {
        let panel = NSOpenPanel()
        panel.title = "Choose a GGUF text model"
        panel.allowsMultipleSelection = false
        panel.canChooseDirectories = false
        panel.canChooseFiles = true
        panel.beginSheetModal(for: window) { [weak self] response in
            guard response == .OK, let url = panel.url else { return }
            self?.defaults.set(url.path, forKey: "textModelPath")
            self?.setStatus("Text model selected")
        }
    }

    func chooseDesk(from window: NSWindow) {
        let panel = NSOpenPanel()
        panel.title = "Choose Async desk folder"
        panel.allowsMultipleSelection = false
        panel.canChooseDirectories = true
        panel.canChooseFiles = false
        panel.canCreateDirectories = true
        panel.beginSheetModal(for: window) { [weak self] response in
            guard response == .OK, let url = panel.url else { return }
            self?.paths = AsyncPaths(desk: url)
            self?.defaults.set(url.path, forKey: "deskPath")
            try? self?.paths.ensure()
            self?.setStatus("Desk ready: \(url.path)")
        }
    }

    @discardableResult
    func start() -> Bool {
        do {
            try paths.ensure()
            guard let model = resolveModel() else {
                setStatus("Text model not found. Choose a GGUF model.")
                return false
            }
            guard let engine = EnginePaths.discover() else {
                setStatus("Engine binaries not found. Set ASYNC_ENGINE_DIR or NRVNA_BUILD_DIR.")
                return false
            }
            self.engine = engine
            if workspaceDaemonPid() != nil {
                daemon = nil
            } else if daemon?.isRunning != true {
                try startDaemon(model: model, engine: engine)
            }
            startWatching()
            setStatus("Text Utility ready")
            return true
        } catch {
            setStatus("Start failed: \(error.localizedDescription)")
            return false
        }
    }

    func stop() {
        timer?.invalidate()
        timer = nil
        if daemon?.isRunning == true {
            daemon?.terminate()
        } else if let pid = workspaceDaemonPid() {
            kill(pid, SIGTERM)
        }
        daemon = nil
        setStatus("Text Utility stopped")
    }

    func open(_ url: URL) {
        try? FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        NSWorkspace.shared.open(url)
    }

    private func resolveModel() -> URL? {
        let env = ProcessInfo.processInfo.environment
        let candidates = [
            env["ASYNC_TEXT_MODEL"],
            defaults.string(forKey: "textModelPath")
        ].compactMap { $0 }
        for path in candidates {
            let url = URL(fileURLWithPath: NSString(string: path).expandingTildeInPath)
            if FileManager.default.fileExists(atPath: url.path) {
                return url
            }
        }
        return nil
    }

    private func startDaemon(model: URL, engine: EnginePaths) throws {
        let process = Process()
        process.executableURL = engine.nrvnad
        process.arguments = [model.path, paths.workspace.path, "-w", "1"]
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
        let pidFile = paths.workspace.appendingPathComponent(".nrvnad.pid")
        guard let raw = try? String(contentsOf: pidFile).trimmingCharacters(in: .whitespacesAndNewlines),
              let value = Int32(raw),
              value > 0 else {
            return nil
        }
        if kill(value, 0) == 0 {
            return value
        }
        return nil
    }

    private func startWatching() {
        timer?.invalidate()
        timer = Timer.scheduledTimer(withTimeInterval: 2.0, repeats: true) { [weak self] _ in
            self?.scanDropoff()
        }
        scanDropoff()
    }

    private func scanDropoff() {
        guard activeJobs == 0 else { return }
        let fm = FileManager.default
        let files = (try? fm.contentsOfDirectory(
            at: paths.dropoff,
            includingPropertiesForKeys: [.contentModificationDateKey],
            options: [.skipsHiddenFiles]
        )) ?? []

        guard let file = files
            .filter({ ["txt", "md"].contains($0.pathExtension.lowercased()) })
            .sorted(by: { $0.lastPathComponent < $1.lastPathComponent })
            .first(where: isStableFile)
        else {
            return
        }

        activeJobs += 1
        queue.async { [weak self] in
            self?.process(file: file)
        }
    }

    private func isStableFile(_ url: URL) -> Bool {
        let values = try? url.resourceValues(forKeys: [.contentModificationDateKey])
        guard let modified = values?.contentModificationDate else { return true }
        return Date().timeIntervalSince(modified) > 1.0
    }

    private func process(file: URL) {
        let stem = file.deletingPathExtension().lastPathComponent
        let jobName = uniqueName(stem)
        let workDir = paths.inProgress.appendingPathComponent(jobName, isDirectory: true)
        let promptPath = workDir.appendingPathComponent("prompt.\(file.pathExtension.isEmpty ? "txt" : file.pathExtension)")
        let fm = FileManager.default
        setStatus("Processing \(file.lastPathComponent)")

        do {
            try fm.createDirectory(at: workDir, withIntermediateDirectories: true)
            try fm.moveItem(at: file, to: promptPath)
            let prompt = try String(contentsOf: promptPath)
            guard let engine else { throw AsyncError.message("Engine binaries not configured") }
            let jobId = try runProcess(engine.wrk, arguments: [paths.workspace.path, "-"], input: prompt).trimmingCharacters(in: .whitespacesAndNewlines)
            let result = try runProcess(engine.flw, arguments: [paths.workspace.path, "-w", jobId])

            let outDir = paths.pickup.appendingPathComponent(jobName, isDirectory: true)
            try fm.createDirectory(at: outDir, withIntermediateDirectories: true)
            try fm.copyItem(at: promptPath, to: outDir.appendingPathComponent("prompt.\(promptPath.pathExtension)"))
            try result.write(to: outDir.appendingPathComponent("result.txt"), atomically: true, encoding: .utf8)
            try details(jobId: jobId, status: "done").write(to: outDir.appendingPathComponent("details.json"), atomically: true, encoding: .utf8)
            try? fm.removeItem(at: workDir)
            setStatus("Ready — result in Pickup")
            notify(title: "Async result ready", body: "\(stem) is in Pickup")
        } catch {
            let failDir = paths.problems.appendingPathComponent(jobName, isDirectory: true)
            try? fm.createDirectory(at: failDir, withIntermediateDirectories: true)
            if fm.fileExists(atPath: promptPath.path) {
                try? fm.copyItem(at: promptPath, to: failDir.appendingPathComponent("prompt.\(promptPath.pathExtension)"))
            } else if fm.fileExists(atPath: file.path) {
                try? fm.copyItem(at: file, to: failDir.appendingPathComponent(file.lastPathComponent))
            }
            try? String(describing: error).write(to: failDir.appendingPathComponent("error.txt"), atomically: true, encoding: .utf8)
            try? details(jobId: "", status: "failed").write(to: failDir.appendingPathComponent("details.json"), atomically: true, encoding: .utf8)
            try? fm.removeItem(at: workDir)
            setStatus("Ready — problem saved")
            notify(title: "Async problem", body: "\(stem) failed. See Problems.")
        }

        DispatchQueue.main.async { [weak self] in
            guard let self else { return }
            self.activeJobs = max(0, self.activeJobs - 1)
            self.scanDropoff()
        }
    }

    private func runProcess(_ executable: URL, arguments: [String], input: String? = nil) throws -> String {
        let process = Process()
        let stdout = Pipe()
        let stderr = Pipe()
        let stdin = Pipe()
        process.executableURL = executable
        process.arguments = arguments
        process.standardOutput = stdout
        process.standardError = stderr
        if input != nil {
            process.standardInput = stdin
        }
        try process.run()
        if let input {
            stdin.fileHandleForWriting.write(input.data(using: .utf8) ?? Data())
            try? stdin.fileHandleForWriting.close()
        }
        process.waitUntilExit()

        let output = String(data: stdout.fileHandleForReading.readDataToEndOfFile(), encoding: .utf8) ?? ""
        let error = String(data: stderr.fileHandleForReading.readDataToEndOfFile(), encoding: .utf8) ?? ""
        guard process.terminationStatus == 0 else {
            throw AsyncError.message(error.isEmpty ? output : error)
        }
        return output
    }

    private func uniqueName(_ stem: String) -> String {
        let safe = stem.replacingOccurrences(of: "/", with: "-").replacingOccurrences(of: ":", with: "-")
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyyMMdd-HHmmss"
        return "\(safe)-\(formatter.string(from: Date()))"
    }

    private func details(jobId: String, status: String) -> String {
        """
        {
          "status": "\(status)",
          "job_id": "\(jobId)",
          "created_at": "\(ISO8601DateFormatter().string(from: Date()))",
          "engine": "nrvna"
        }
        """
    }

    private func notify(title: String, body: String) {
        DispatchQueue.main.async {
            let notification = NSUserNotification()
            notification.title = title
            notification.informativeText = body
            NSUserNotificationCenter.default.deliver(notification)
        }
    }

    private func setStatus(_ value: String) {
        DispatchQueue.main.async { self.onStatus?(value) }
    }
}

private enum AsyncError: Error, CustomStringConvertible {
    case message(String)
    var description: String {
        switch self {
        case .message(let value): return value.trimmingCharacters(in: .whitespacesAndNewlines)
        }
    }
}

private final class AppDelegate: NSObject, NSApplicationDelegate {
    private let controller = AsyncController()
    private let window = NSWindow(
        contentRect: NSRect(x: 0, y: 0, width: 460, height: 285),
        styleMask: [.titled, .closable, .miniaturizable],
        backing: .buffered,
        defer: false
    )

    private let statusLabel = NSTextField(labelWithString: "Text Utility stopped")
    private let toggle = NSSwitch()

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.regular)
        buildWindow()
        controller.onStatus = { [weak self] in self?.statusLabel.stringValue = $0 }
        window.center()
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    func applicationWillTerminate(_ notification: Notification) {
        controller.stop()
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        true
    }

    private func buildWindow() {
        window.title = appName
        let root = NSStackView()
        root.orientation = .vertical
        root.alignment = .leading
        root.spacing = 14
        root.edgeInsets = NSEdgeInsets(top: 24, left: 24, bottom: 24, right: 24)
        root.translatesAutoresizingMaskIntoConstraints = false

        let title = NSTextField(labelWithString: "Async")
        title.font = .systemFont(ofSize: 30, weight: .bold)
        let subtitle = NSTextField(labelWithString: "Turn on the Text Utility. Drop prompt files. Pick up results.")
        subtitle.textColor = .secondaryLabelColor

        let utilityRow = NSStackView()
        utilityRow.orientation = .horizontal
        utilityRow.alignment = .centerY
        utilityRow.spacing = 12
        let utilityLabel = NSTextField(labelWithString: "Text Utility")
        utilityLabel.font = .systemFont(ofSize: 18, weight: .semibold)
        toggle.target = self
        toggle.action = #selector(toggleChanged)
        utilityRow.addArrangedSubview(utilityLabel)
        utilityRow.addArrangedSubview(toggle)

        statusLabel.textColor = .secondaryLabelColor

        let folderRow = NSStackView()
        folderRow.orientation = .horizontal
        folderRow.spacing = 10
        folderRow.addArrangedSubview(button("Dropoff", #selector(openDropoff)))
        folderRow.addArrangedSubview(button("Pickup", #selector(openPickup)))
        folderRow.addArrangedSubview(button("Problems", #selector(openProblems)))

        let modelButton = NSButton(title: "Choose Model", target: self, action: #selector(chooseModel))
        modelButton.bezelStyle = .inline
        modelButton.font = .systemFont(ofSize: 13)

        let hint = NSTextField(labelWithString: "Files named input.txt or prompt.md are enough. Job IDs stay hidden in details.json.")
        hint.textColor = .tertiaryLabelColor
        hint.lineBreakMode = .byWordWrapping
        hint.maximumNumberOfLines = 2
        hint.widthAnchor.constraint(equalToConstant: 400).isActive = true

        for view in [title, subtitle, utilityRow, statusLabel, folderRow, modelButton, hint] {
            root.addArrangedSubview(view)
        }

        window.contentView = NSView()
        window.contentView?.addSubview(root)
        NSLayoutConstraint.activate([
            root.leadingAnchor.constraint(equalTo: window.contentView!.leadingAnchor),
            root.trailingAnchor.constraint(equalTo: window.contentView!.trailingAnchor),
            root.topAnchor.constraint(equalTo: window.contentView!.topAnchor),
            root.bottomAnchor.constraint(lessThanOrEqualTo: window.contentView!.bottomAnchor)
        ])
    }

    private func button(_ title: String, _ action: Selector) -> NSButton {
        let button = NSButton(title: title, target: self, action: action)
        button.bezelStyle = .rounded
        button.widthAnchor.constraint(equalToConstant: 120).isActive = true
        return button
    }

    @objc private func toggleChanged() {
        if toggle.state == .on {
            if !controller.start() {
                toggle.state = .off
            }
        } else {
            controller.stop()
        }
    }

    @objc private func openDropoff() { controller.open(controller.dropoffURL) }
    @objc private func openPickup() { controller.open(controller.pickupURL) }
    @objc private func openProblems() { controller.open(controller.problemsURL) }
    @objc private func chooseModel() { controller.chooseModel(from: window) }
}

@main
private enum AsyncApp {
    static func main() {
        let app = NSApplication.shared
        let delegate = AppDelegate()
        app.delegate = delegate
        app.run()
    }
}
