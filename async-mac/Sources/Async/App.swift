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

struct PopoverView: View {
    @ObservedObject var controller: BckbrnrController
    @State private var prompt = ""

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            header
            Divider()
            promptArea
            Divider()
            modelRow
            controls
        }
        .padding(16)
        .frame(width: 310)
    }

    private var header: some View {
        VStack(alignment: .leading, spacing: 3) {
            HStack {
                Text("bckbrnr")
                    .font(.system(.title3, design: .serif))
                    .bold()
                Spacer()
                Circle()
                    .fill(controller.isRunning ? Color.green : Color.secondary)
                    .frame(width: 10, height: 10)
            }
            Text("prompts & inference")
                .font(.caption)
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
