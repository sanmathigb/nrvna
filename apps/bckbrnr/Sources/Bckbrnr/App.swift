import AppKit
import SwiftUI
import UserNotifications

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

final class AppDelegate: NSObject, NSApplicationDelegate, UNUserNotificationCenterDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
        let center = UNUserNotificationCenter.current()
        center.delegate = self
        center.requestAuthorization(options: [.alert, .sound]) { _, _ in }
    }

    func userNotificationCenter(
        _ center: UNUserNotificationCenter,
        didReceive response: UNNotificationResponse,
        withCompletionHandler completionHandler: @escaping () -> Void
    ) {
        if let path = response.notification.request.content.userInfo["path"] as? String {
            NSWorkspace.shared.open(URL(fileURLWithPath: path))
        }
        completionHandler()
    }

    func userNotificationCenter(
        _ center: UNUserNotificationCenter,
        willPresent notification: UNNotification,
        withCompletionHandler completionHandler: @escaping (UNNotificationPresentationOptions) -> Void
    ) {
        completionHandler([.banner, .sound])
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

/// Two states, nothing more: gray when off, ember when the daemon is lit.
private struct StatusOrb: View {
    let running: Bool
    var body: some View {
        Circle()
            .fill(running ? Color(red: 0.91, green: 0.45, blue: 0.23) : Color.secondary)
            .frame(width: 10, height: 10)
            .opacity(running ? 1.0 : 0.45)
    }
}

struct PopoverView: View {
    @ObservedObject var controller: BckbrnrController
    @State private var prompt = ""
    @Environment(\.colorScheme) private var scheme

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            header
            if controller.isRunning {
                Divider().opacity(0.4)
                promptArea
            } else if !controller.statusText.isEmpty {
                Divider().opacity(0.4)
                Text(controller.statusText)
                    .font(.callout)
                    .foregroundStyle(.secondary)
            }
            Divider().opacity(0.4)
            modelRow
            rootRow
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
                    .font(.system(.title, design: .serif))
                    .fontWeight(.semibold)
                    .tracking(0.5)
                    .foregroundStyle(Paper.ink(scheme))
                Spacer()
                StatusOrb(running: controller.isRunning)
            }
            Text("inference as utility")
                .font(.system(.caption, design: .serif))
                .fontWeight(.medium)
                .italic()
                .tracking(0.3)
                .foregroundStyle(.secondary)
        }
    }

    private var promptArea: some View {
        TextField("type the prompt and hit enter", text: $prompt)
            .textFieldStyle(.roundedBorder)
            .onSubmit(submit)
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
                .disabled(controller.isRunning)   // bound to a live daemon
        }
    }

    private var rootRow: some View {
        HStack(alignment: .bottom) {
            VStack(alignment: .leading, spacing: 2) {
                Text("WORKSPACE")
                    .font(.system(size: 9))
                    .foregroundStyle(.tertiary)
                Button(action: controller.openResponses) {
                    Text(controller.rootDisplay)
                        .font(.system(.caption, design: .monospaced))
                        .foregroundStyle(.secondary)
                        .lineLimit(1)
                        .truncationMode(.middle)
                }
                .buttonStyle(.plain)
                .help("Open your answers in Finder")
            }
            Spacer()
            Button("Change", action: controller.chooseRoot)
                .buttonStyle(.link)
                .font(.caption)
                .disabled(controller.isRunning)   // bound to a live daemon
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
