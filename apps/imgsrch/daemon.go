package main

import (
	"encoding/json"
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

// The engine owns lifecycle truth; imgsrch just asks it.
type workerStatus struct {
	Running bool   `json:"running"`
	Ready   bool   `json:"ready"`
	Model   string `json:"model"`
}

// queryWorker asks the engine. An error means "no trustworthy answer"
// (missing binary, unparseable output) — callers must not read that as
// "not running".
func queryWorker(ws string) (workerStatus, error) {
	bin := binDaemon()
	if bin == "" {
		return workerStatus{}, fmt.Errorf("inference engine not found (set NRVNA_BUILD_DIR or NRVNA_DAEMON_BIN)")
	}
	// status --json prints a JSON object on every exit code (0/2/1);
	// Output() still captures stdout when the exit code is non-zero.
	out, _ := exec.Command(bin, "status", ws, "--json").Output()
	var st workerStatus
	if err := json.Unmarshal(out, &st); err != nil {
		return workerStatus{}, fmt.Errorf("engine gave no valid status for %s", ws)
	}
	return st, nil
}

// startWorker ensures a worker for ws is running and ready with the wanted
// model. A worker that is still loading (running, not ready) is adopted and
// waited on — its model is compared only once it is ready, since the engine
// publishes model info at readiness.
// Output voice: the user sees "worker", never daemon internals.
func startWorker(label, model, ws string, env map[string]string, args ...string) error {
	daemon := binDaemon()
	if daemon == "" {
		return fmt.Errorf("inference engine not found (set NRVNA_BUILD_DIR or NRVNA_DAEMON_BIN)")
	}
	if err := os.MkdirAll(ws, 0o755); err != nil {
		return err
	}

	st, err := queryWorker(ws)
	if err != nil {
		return err
	}
	adopted := st.Running && !st.Ready
	if st.Running && st.Ready {
		if st.Model == model {
			return nil // already running with the right model
		}
		note("%s worker model changed; restarting", label)
		if err := stopWorker(label, ws); err != nil {
			return err
		}
	}

	logPath := filepath.Join(filepath.Dir(filepath.Dir(ws)), "logs", label+".log")
	launcherPid := 0
	if !adopted {
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
		launcherPid = cmd.Process.Pid
		go cmd.Wait() // reap when it exits; lifetime is tracked by the engine's lock
		note("starting %s worker (%s)", label, filepath.Base(model))
	} else {
		note("%s worker already starting; waiting for it", label)
	}

	deadline := time.Now().Add(startTimeout())
	for {
		st, err := queryWorker(ws)
		if err != nil {
			return err
		}
		if st.Ready {
			if st.Model != model {
				// An adopted worker turned out to hold the wrong model.
				note("%s worker has a different model; restarting", label)
				if err := stopWorker(label, ws); err != nil {
					return err
				}
				return startWorker(label, model, ws, env, args...)
			}
			note("%s worker ready", label)
			return nil
		}
		alive := st.Running
		if launcherPid != 0 {
			alive = syscall.Kill(launcherPid, 0) == nil
		}
		if !alive {
			return fmt.Errorf("%s worker exited during startup; log: %s\n%s", label, logPath, tailFile(logPath, 20))
		}
		if time.Now().After(deadline) {
			return fmt.Errorf("timed out waiting for %s worker; log: %s\n%s", label, logPath, tailFile(logPath, 20))
		}
		time.Sleep(500 * time.Millisecond)
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
	st, err := queryWorker(ws)
	if err != nil {
		return fmt.Errorf("stopping %s worker: %w", label, err)
	}
	if !st.Running {
		return nil
	}
	out, err := exec.Command(binDaemon(), "stop", ws).CombinedOutput()
	if err != nil {
		return fmt.Errorf("stopping %s worker: %s", label, strings.TrimSpace(string(out)))
	}
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
