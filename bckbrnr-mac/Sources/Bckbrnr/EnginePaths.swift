import AppKit
import Foundation

struct EnginePaths {
    let nrvnad: URL
    let wrk: URL
    let flw: URL

    static func discover() -> EnginePaths? {
        let env = ProcessInfo.processInfo.environment
        let candidates = [
            Bundle.main.resourceURL?.appendingPathComponent("bin").path,
            env["BCKBRNR_ENGINE_DIR"],
            env["NRVNA_BUILD_DIR"],
            URL(fileURLWithPath: FileManager.default.currentDirectoryPath).appendingPathComponent("build").path
        ].compactMap { $0 }

        for dir in candidates {
            let root = URL(fileURLWithPath: NSString(string: dir).expandingTildeInPath)
            let paths = EnginePaths(
                nrvnad: root.appendingPathComponent("nrvnad"),
                wrk: root.appendingPathComponent("wrk"),
                flw: root.appendingPathComponent("flw")
            )
            if paths.allExecutable {
                return paths
            }
        }
        return nil
    }

    var allExecutable: Bool {
        let fm = FileManager.default
        return fm.isExecutableFile(atPath: nrvnad.path)
            && fm.isExecutableFile(atPath: wrk.path)
            && fm.isExecutableFile(atPath: flw.path)
    }
}
