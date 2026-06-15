package main

import (
	"errors"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"time"
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

func pidFile(ws string) string  { return filepath.Join(ws, ".nrvnad.pid") }
func metaFile(ws string) string { return filepath.Join(ws, ".nrvnad.start") }

func readPid(ws string) (int, bool) {
	data, err := os.ReadFile(pidFile(ws))
	if err != nil {
		return 0, false
	}
	pid, err := strconv.Atoi(strings.TrimSpace(string(data)))
	if err != nil || pid <= 0 {
		return 0, false
	}
	return pid, true
}

func pidAlive(pid int) bool { return syscall.Kill(pid, 0) == nil }

func processIsNrvnad(pid int) bool {
	out, err := exec.Command("ps", "-p", strconv.Itoa(pid), "-o", "comm=").Output()
	if err != nil {
		return false
	}
	return filepath.Base(strings.TrimSpace(string(out))) == "nrvnad"
}

func workspaceLockHeld(ws string) bool {
	f, err := os.OpenFile(filepath.Join(ws, ".nrvnad.lock"), os.O_RDWR|os.O_CREATE, 0o644)
	if err != nil {
		return false
	}
	defer f.Close()
	err = syscall.Flock(int(f.Fd()), syscall.LOCK_EX|syscall.LOCK_NB)
	if err == nil {
		syscall.Flock(int(f.Fd()), syscall.LOCK_UN)
		return false
	}
	return errors.Is(err, syscall.EWOULDBLOCK) || errors.Is(err, syscall.EAGAIN)
}

func daemonOwnsWorkspace(pid int, ws string) bool {
	return pidAlive(pid) && processIsNrvnad(pid) && workspaceLockHeld(ws)
}

// daemonRunning reports whether the workspace has a live worker; it clears a
// stale pidfile as a side effect, same as the bash spec.
func daemonRunning(ws string) bool {
	pid, ok := readPid(ws)
	if !ok {
		return false
	}
	if daemonOwnsWorkspace(pid, ws) {
		return true
	}
	os.Remove(pidFile(ws))
	os.Remove(metaFile(ws))
	return false
}

// runtimeMeta captures everything that requires a worker restart when it
// changes: model, args, and all environment read when the daemon starts.
func runtimeMeta(model string, args []string, env map[string]string) string {
	get := func(k, def string) string {
		if v, ok := env[k]; ok {
			return v
		}
		return envOr(k, def)
	}
	return "model=" + model + "\n" +
		"args=" + strings.Join(args, "\x1f") + "\n" +
		"env.NRVNA_WORKERS=" + get("NRVNA_WORKERS", "4") + "\n" +
		"env.NRVNA_GPU_LAYERS=" + get("NRVNA_GPU_LAYERS", "0") + "\n" +
		"env.NRVNA_MAX_CTX=" + get("NRVNA_MAX_CTX", "8192") + "\n" +
		"env.NRVNA_BATCH=" + get("NRVNA_BATCH", "2048") + "\n" +
		"env.NRVNA_UBATCH=" + get("NRVNA_UBATCH", get("NRVNA_BATCH", "2048")) + "\n" +
		"env.NRVNA_PREDICT=" + get("NRVNA_PREDICT", envOr("NRVNA_N_PREDICT", "2048")) + "\n" +
		"env.NRVNA_TEMP=" + get("NRVNA_TEMP", "0.8") + "\n" +
		"env.NRVNA_THINKING=" + get("NRVNA_THINKING", "1") + "\n" +
		"env.NRVNA_IMAGE_MAX_TOKENS=" + get("NRVNA_IMAGE_MAX_TOKENS", "0") + "\n" +
		"env.NRVNA_CHAT_TEMPLATE_FILE=" + get("NRVNA_CHAT_TEMPLATE_FILE", "") + "\n"
}

// startWorker ensures a worker for ws is running with the wanted runtime.
// Output voice: the user sees "worker", never daemon internals.
func startWorker(label, model, ws string, env map[string]string, args ...string) error {
	daemon := binDaemon()
	if daemon == "" {
		return fmt.Errorf("inference engine not found (set NRVNA_BUILD_DIR or NRVNA_DAEMON_BIN)")
	}
	if err := os.MkdirAll(ws, 0o755); err != nil {
		return err
	}

	wantMeta := runtimeMeta(model, args, env)
	if daemonRunning(ws) {
		saved, _ := os.ReadFile(metaFile(ws))
		if string(saved) == wantMeta {
			return nil // already running with the right config
		}
		note("%s worker config changed; restarting", label)
		if err := stopWorker(label, ws); err != nil {
			return err
		}
	}

	logPath := filepath.Join(filepath.Dir(filepath.Dir(ws)), "logs", label+".log")
	logF, err := os.Create(logPath)
	if err != nil {
		return err
	}
	defer logF.Close()

	cmd := exec.Command(daemon, append([]string{model, ws}, args...)...)
	cmd.Stdout, cmd.Stderr = logF, logF
	cmd.Env = os.Environ()
	for k, v := range env {
		cmd.Env = append(cmd.Env, k+"="+v)
	}
	if err := cmd.Start(); err != nil {
		return fmt.Errorf("starting %s worker: %w", label, err)
	}
	launcherPid := cmd.Process.Pid
	go cmd.Wait() // reap when it exits; lifetime is tracked via the pidfile

	note("starting %s worker (%s)", label, filepath.Base(model))
	deadline := time.Now().Add(startTimeout())
	for {
		if pid, ok := readPid(ws); ok && daemonOwnsWorkspace(pid, ws) {
			if err := os.WriteFile(metaFile(ws), []byte(wantMeta), 0o644); err != nil {
				return err
			}
			note("%s worker ready", label)
			return nil
		}
		if !pidAlive(launcherPid) {
			return fmt.Errorf("%s worker exited during startup; log: %s\n%s", label, logPath, tailFile(logPath, 20))
		}
		if time.Now().After(deadline) {
			return fmt.Errorf("timed out waiting for %s worker; log: %s\n%s", label, logPath, tailFile(logPath, 20))
		}
		time.Sleep(200 * time.Millisecond)
	}
}

func startTimeout() time.Duration {
	if v := os.Getenv("NRVNA_START_TIMEOUT"); v != "" {
		if s, err := strconv.Atoi(v); err == nil {
			return time.Duration(s) * time.Second
		}
	}
	return 120 * time.Second
}

func stopTimeout() time.Duration {
	if v := os.Getenv("NRVNA_STOP_TIMEOUT"); v != "" {
		if s, err := strconv.Atoi(v); err == nil && s > 0 {
			return time.Duration(s) * time.Second
		}
	}
	return 20 * time.Second
}

func waitForExit(pid int, timeout time.Duration, alive func(int) bool) bool {
	deadline := time.Now().Add(timeout)
	for alive(pid) {
		if time.Now().After(deadline) {
			return false
		}
		time.Sleep(20 * time.Millisecond)
	}
	return true
}

func stopPID(
	pid int,
	gracefulTimeout time.Duration,
	alive func(int) bool,
	signal func(int, syscall.Signal) error,
) error {
	if err := signal(pid, syscall.SIGTERM); err != nil && alive(pid) {
		return err
	}
	if waitForExit(pid, gracefulTimeout, alive) {
		return nil
	}
	if err := signal(pid, syscall.SIGTERM); err != nil && alive(pid) {
		return err
	}
	if waitForExit(pid, 2*time.Second, alive) {
		return nil
	}
	if err := signal(pid, syscall.SIGKILL); err != nil && alive(pid) {
		return err
	}
	if waitForExit(pid, 2*time.Second, alive) {
		return nil
	}
	return fmt.Errorf("process %d did not stop", pid)
}

func stopWorker(label, ws string) error {
	pid, ok := readPid(ws)
	if !ok {
		return nil
	}
	if !daemonOwnsWorkspace(pid, ws) {
		os.Remove(pidFile(ws))
		os.Remove(metaFile(ws))
		return nil
	}
	if err := stopPID(pid, stopTimeout(), pidAlive, syscall.Kill); err != nil {
		return fmt.Errorf("stopping %s worker: %w", label, err)
	}
	os.Remove(pidFile(ws))
	os.Remove(metaFile(ws))
	note("%s worker stopped", label)
	return nil
}

func tailFile(path string, lines int) string {
	data, err := os.ReadFile(path)
	if err != nil {
		return ""
	}
	all := strings.Split(strings.TrimRight(string(data), "\n"), "\n")
	if len(all) > lines {
		all = all[len(all)-lines:]
	}
	return strings.Join(all, "\n")
}
