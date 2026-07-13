package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"syscall"
)

// Engine binary resolution, in order: explicit env var, NRVNA_BUILD_DIR,
// the directory of the imgsrch executable (where bundled binaries land),
// ./build relative to cwd, then PATH. doctor prints what was resolved.
func findBin(envVar, name string) string {
	if v := os.Getenv(envVar); v != "" && isExecutable(v) {
		return v
	}
	if d := os.Getenv("NRVNA_BUILD_DIR"); d != "" && isExecutable(filepath.Join(d, name)) {
		return filepath.Join(d, name)
	}
	if exe, err := os.Executable(); err == nil {
		for _, p := range []string{
			filepath.Join(filepath.Dir(exe), "bin", name), // release kit layout
			filepath.Join(filepath.Dir(exe), name),
		} {
			if isExecutable(p) {
				return p
			}
		}
	}
	if p, _ := filepath.Abs(filepath.Join("build", name)); isExecutable(p) {
		return p
	}
	if p, err := exec.LookPath(name); err == nil {
		return p
	}
	return ""
}

func isExecutable(p string) bool {
	st, err := os.Stat(p)
	return err == nil && !st.IsDir() && st.Mode()&0o111 != 0 && st.Size() > 0
}

func binDaemon() string { return findBin("NRVNA_DAEMON_BIN", "nrvnad") }
func binWrk() string    { return findBin("NRVNA_WRK_BIN", "wrk") }
func binFlw() string    { return findBin("NRVNA_FLW_BIN", "flw") }

// drainWorker runs one finite engine process. Existing workers are handled by
// nrvnad --drain itself: it waits for them, or takes over if they disappear.
func drainWorker(label, model, ws string, env map[string]string, args ...string) error {
	daemon := binDaemon()
	if daemon == "" {
		return fmt.Errorf("inference engine not found (set NRVNA_BUILD_DIR or NRVNA_DAEMON_BIN)")
	}
	if err := os.MkdirAll(ws, 0o755); err != nil {
		return err
	}
	cmdArgs := []string{model, ws}
	cmdArgs = append(cmdArgs, args...)
	cmdArgs = append(cmdArgs, "--drain")
	logPath := filepath.Join(filepath.Dir(filepath.Dir(ws)), "logs", label+".log")
	logF, err := os.OpenFile(logPath, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0o644)
	if err != nil {
		return err
	}
	defer logF.Close()
	cmd := exec.Command(daemon, cmdArgs...)
	cmd.Stdout, cmd.Stderr = logF, logF
	cmd.Env = os.Environ()
	for k, v := range env {
		cmd.Env = append(cmd.Env, k+"="+v)
	}
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("%s worker drain: %w (log: %s)", label, err, logPath)
	}
	return nil
}

// launchFinisher detaches the finite pipeline continuation from `imgsrch
// index`. It writes everything to one inspectable project log and owns no
// persistent process: each worker exits after draining its queue.
func launchFinisher(project string) error {
	exe, err := os.Executable()
	if err != nil {
		return err
	}
	absProject, err := filepath.Abs(project)
	if err != nil {
		return err
	}
	logPath := filepath.Join(logsDir(project), "pipeline.log")
	logF, err := os.OpenFile(logPath, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0o644)
	if err != nil {
		return err
	}
	defer logF.Close()

	cmd := exec.Command(exe, "__finish", absProject)
	cmd.Stdout, cmd.Stderr = logF, logF
	cmd.Env = os.Environ()
	cmd.SysProcAttr = &syscall.SysProcAttr{Setsid: true}
	if err := cmd.Start(); err != nil {
		return fmt.Errorf("starting background indexer: %w", err)
	}
	if err := cmd.Process.Release(); err != nil {
		return fmt.Errorf("releasing background indexer: %w", err)
	}
	return nil
}
