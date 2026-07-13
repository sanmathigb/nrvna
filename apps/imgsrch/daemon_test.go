package main

import (
	"os"
	"path/filepath"
	"testing"
)

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
