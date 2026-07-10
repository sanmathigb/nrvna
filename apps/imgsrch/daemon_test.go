package main

import (
	"encoding/json"
	"os"
	"path/filepath"
	"testing"
)

// The engine's documented status --json shape; imgsrch depends on these keys.
func TestWorkerStatusParsesEngineJSON(t *testing.T) {
	payload := `{"running":true,"ready":true,"pid":42,"model":"/models/x.gguf","workers":1,"started_at":"t"}`
	var st workerStatus
	if err := json.Unmarshal([]byte(payload), &st); err != nil {
		t.Fatal(err)
	}
	if !st.Running || !st.Ready || st.Model != "/models/x.gguf" {
		t.Fatalf("parsed status wrong: %+v", st)
	}

	var down workerStatus
	if err := json.Unmarshal([]byte(`{"running":false,"ready":false}`), &down); err != nil {
		t.Fatal(err)
	}
	if down.Running || down.Ready {
		t.Fatalf("not-running status parsed wrong: %+v", down)
	}
}

func TestFindBinPrefersEnvVar(t *testing.T) {
	dir := t.TempDir()
	fake := filepath.Join(dir, "fakebin")
	if err := os.WriteFile(fake, []byte("#!/bin/sh\n"), 0o755); err != nil {
		t.Fatal(err)
	}
	t.Setenv("TEST_BIN_OVERRIDE", fake)
	if got := findBin("TEST_BIN_OVERRIDE", "does-not-exist-anywhere"); got != fake {
		t.Fatalf("findBin ignored env override: %q", got)
	}
}
