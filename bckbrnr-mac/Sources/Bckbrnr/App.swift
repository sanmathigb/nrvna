import AppKit
import SwiftUI

@main
struct BckbrnrApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate
    @StateObject private var controller = BckbrnrController()

    var body: some Scene {
        MenuBarExtra {
            PopoverView(controller: controller)
        } label: {
            Image(systemName: controller.isRunning ? "circle.inset.filled" : "circle")
        }
        .menuBarExtraStyle(.window)
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate, NSUserNotificationCenterDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
        NSUserNotificationCenter.default.delegate = self
    }

    func userNotificationCenter(
        _ center: NSUserNotificationCenter,
        didActivate notification: NSUserNotification
    ) {
        if let path = notification.userInfo?["path"] as? String {
            NSWorkspace.shared.open(URL(fileURLWithPath: path))
        }
    }

    func userNotificationCenter(
        _ center: NSUserNotificationCenter,
        shouldPresent notification: NSUserNotification
    ) -> Bool {
        true
    }
}

/// Warm paper canvas. Adapts to the menu bar's light/dark backing so the
/// instrument feels like aged paper rather than a system panel.
private struct Paper {
    static func color(_ scheme: ColorScheme) -> Color {
        scheme == .dark
            ? Color(red: 0.14, green: 0.13, blue: 0.12)   // warm graphite
            : Color(red: 0.96, green: 0.94, blue: 0.89)   // warm cream
    }

    static func ink(_ scheme: ColorScheme) -> Color {
        scheme == .dark
            ? Color(red: 0.92, green: 0.90, blue: 0.85)
            : Color(red: 0.18, green: 0.16, blue: 0.13)
    }
}

/// On/off + status signal. Breathes a soft green pulse while the engine is
/// running; sits as a still, dim dot when stopped.
private struct BreathingOrb: View {
    let running: Bool
    @State private var breathe = false

    var body: some View {
        let core = running ? Color(red: 0.30, green: 0.69, blue: 0.42) : Color.secondary

        ZStack {
            if running {
                Circle()
                    .fill(core)
                    .frame(width: 10, height: 10)
                    .blur(radius: 4)
                    .opacity(breathe ? 0.55 : 0.15)
                    .scaleEffect(breathe ? 1.9 : 1.1)
            }
            Circle()
                .fill(core)
                .frame(width: 10, height: 10)
                .opacity(running ? (breathe ? 1.0 : 0.78) : 0.45)
        }
        .frame(width: 22, height: 22)
        .onChange(of: running) { _ in restart() }
        .onAppear { restart() }
    }

    private func restart() {
        breathe = false
        guard running else { return }
        withAnimation(.easeInOut(duration: 1.8).repeatForever(autoreverses: true)) {
            breathe = true
        }
    }
}

struct PopoverView: View {
    @ObservedObject var controller: BckbrnrController
    @State private var prompt = ""
    @Environment(\.colorScheme) private var scheme

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            header
            Divider().opacity(0.4)
            promptArea
            Divider().opacity(0.4)
            modelRow
            controls
        }
        .padding(18)
        .frame(width: 310)
        .background(Paper.color(scheme))
        .onAppear { controller.refresh() }
    }

    private var header: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack(alignment: .center) {
                Text("bckbrnr")
                    .font(.system(.title2, design: .serif))
                    .fontWeight(.semibold)
                    .tracking(0.5)
                    .foregroundStyle(Paper.ink(scheme))
                Spacer()
                BreathingOrb(running: controller.isRunning)
            }
            Text("prompts & inference")
                .font(.system(.caption, design: .serif))
                .italic()
                .tracking(0.3)
                .foregroundStyle(.secondary)
        }
    }

    @ViewBuilder
    private var promptArea: some View {
        if controller.isRunning {
            TextField("Type your prompt", text: $prompt)
                .textFieldStyle(.roundedBorder)
                .onSubmit(submit)
            HStack {
                Text("Enter to run. Answer lands in response/.")
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                Spacer()
                Button("Run", action: submit)
                    .disabled(prompt.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
            }
        } else {
            Text(controller.statusText)
                .font(.callout)
                .foregroundStyle(.secondary)
        }
    }

    private var modelRow: some View {
        HStack(alignment: .bottom) {
            VStack(alignment: .leading, spacing: 2) {
                Text("MODEL")
                    .font(.system(size: 9))
                    .foregroundStyle(.tertiary)
                Text(controller.modelName)
                    .font(.system(.caption, design: .monospaced))
                    .lineLimit(1)
                    .truncationMode(.middle)
            }
            Spacer()
            Button("Change", action: controller.chooseModel)
                .buttonStyle(.link)
                .font(.caption)
        }
    }

    private var controls: some View {
        HStack {
            Button(controller.isRunning ? "Stop" : "Start") {
                if controller.isRunning {
                    controller.stop()
                } else {
                    controller.start()
                }
            }
            Spacer()
            Button("Quit") {
                NSApp.terminate(nil)
            }
            .buttonStyle(.link)
            .font(.caption)
        }
    }

    private func submit() {
        let text = prompt.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !text.isEmpty else { return }
        controller.submit(text)
        prompt = ""
    }
}
