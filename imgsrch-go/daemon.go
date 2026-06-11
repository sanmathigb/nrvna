package main

import (
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
		if p := filepath.Join(filepath.Dir(exe), name); isExecutable(p) {
			return p
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

// daemonRunning reports whether the workspace has a live worker; it clears a
// stale pidfile as a side effect, same as the bash spec.
func daemonRunning(ws string) bool {
	pid, ok := readPid(ws)
	if !ok {
		return false
	}
	if pidAlive(pid) {
		return true
	}
	os.Remove(pidFile(ws))
	return false
}

// runtimeMeta captures everything that requires a worker restart when it
// changes: model, args, and reload-required env. Sampling env vars are read
// per job by the engine and deliberately excluded.
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
		"env.NRVNA_IMAGE_MAX_TOKENS=" + get("NRVNA_IMAGE_MAX_TOKENS", "0") + "\n"
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
		if pid, ok := readPid(ws); ok && pidAlive(pid) {
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

func stopWorker(label, ws string) error {
	pid, ok := readPid(ws)
	if !ok {
		return nil
	}
	if !pidAlive(pid) {
		os.Remove(pidFile(ws))
		os.Remove(metaFile(ws))
		return nil
	}
	syscall.Kill(pid, syscall.SIGTERM)
	deadline := time.Now().Add(20 * time.Second)
	for pidAlive(pid) {
		if time.Now().After(deadline) {
			return fmt.Errorf("timed out stopping %s worker (pid %d)", label, pid)
		}
		time.Sleep(200 * time.Millisecond)
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
