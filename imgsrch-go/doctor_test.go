package main

import "testing"

func TestEngineEnvVarNames(t *testing.T) {
	want := map[string]string{
		"nrvnad": "NRVNA_DAEMON_BIN",
		"wrk":    "NRVNA_WRK_BIN",
		"flw":    "NRVNA_FLW_BIN",
	}
	for name, env := range want {
		if got := engineEnvVar(name); got != env {
			t.Fatalf("engineEnvVar(%q) = %q, want %q", name, got, env)
		}
	}
}
