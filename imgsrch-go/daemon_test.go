package main

import (
	"os"
	"path/filepath"
	"reflect"
	"syscall"
	"testing"
	"time"
)

func TestRuntimeMetaChangesForStartupEnvironment(t *testing.T) {
	keys := []string{
		"NRVNA_MAX_CTX",
		"NRVNA_BATCH",
		"NRVNA_UBATCH",
		"NRVNA_PREDICT",
		"NRVNA_TEMP",
		"NRVNA_THINKING",
		"NRVNA_IMAGE_MAX_TOKENS",
		"NRVNA_CHAT_TEMPLATE_FILE",
	}
	base := runtimeMeta("model.gguf", []string{"-w", "1"}, map[string]string{})

	for _, key := range keys {
		t.Run(key, func(t *testing.T) {
			got := runtimeMeta("model.gguf", []string{"-w", "1"}, map[string]string{key: "changed"})
			if got == base {
				t.Fatalf("%s did not change runtime metadata", key)
			}
		})
	}
}

func TestProcessIsNrvnadRejectsCurrentTestProcess(t *testing.T) {
	if processIsNrvnad(os.Getpid()) {
		t.Fatal("test process was mistaken for nrvnad")
	}
}

func TestWorkspaceLockHeld(t *testing.T) {
	ws := t.TempDir()
	path := filepath.Join(ws, ".nrvnad.lock")
	f, err := os.OpenFile(path, os.O_RDWR|os.O_CREATE, 0o644)
	if err != nil {
		t.Fatal(err)
	}
	defer f.Close()
	if err := syscall.Flock(int(f.Fd()), syscall.LOCK_EX|syscall.LOCK_NB); err != nil {
		t.Fatal(err)
	}
	defer syscall.Flock(int(f.Fd()), syscall.LOCK_UN)

	if !workspaceLockHeld(ws) {
		t.Fatal("held daemon lock was reported as free")
	}
}

func TestDaemonRunningRejectsUnrelatedLivePID(t *testing.T) {
	ws := t.TempDir()
	if err := os.WriteFile(pidFile(ws), []byte("1\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(metaFile(ws), []byte("stale"), 0o644); err != nil {
		t.Fatal(err)
	}
	if daemonRunning(ws) {
		t.Fatal("unrelated PID was accepted as the workspace daemon")
	}
	if _, err := os.Stat(pidFile(ws)); !os.IsNotExist(err) {
		t.Fatalf("stale pidfile was not removed: %v", err)
	}
	if _, err := os.Stat(metaFile(ws)); !os.IsNotExist(err) {
		t.Fatalf("stale metafile was not removed: %v", err)
	}
}

func TestStopPIDEscalatesWithSecondTerm(t *testing.T) {
	alive := true
	var signals []syscall.Signal
	err := stopPID(
		123,
		time.Millisecond,
		func(int) bool { return alive },
		func(_ int, sig syscall.Signal) error {
			signals = append(signals, sig)
			if len(signals) == 2 {
				alive = false
			}
			return nil
		},
	)
	if err != nil {
		t.Fatal(err)
	}
	want := []syscall.Signal{syscall.SIGTERM, syscall.SIGTERM}
	if !reflect.DeepEqual(signals, want) {
		t.Fatalf("signals = %v, want %v", signals, want)
	}
}
