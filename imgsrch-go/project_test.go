package main

import (
	"os"
	"path/filepath"
	"testing"
)

func TestCmdAddRejectsExistingBasename(t *testing.T) {
	project := filepath.Join(t.TempDir(), "project")
	firstDir := filepath.Join(t.TempDir(), "first")
	secondDir := filepath.Join(t.TempDir(), "second")
	if err := os.MkdirAll(firstDir, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.MkdirAll(secondDir, 0o755); err != nil {
		t.Fatal(err)
	}
	first := filepath.Join(firstDir, "same.png")
	second := filepath.Join(secondDir, "same.png")
	if err := os.WriteFile(first, []byte("first"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(second, []byte("second"), 0o644); err != nil {
		t.Fatal(err)
	}

	if err := cmdAdd(project, []string{first}); err != nil {
		t.Fatal(err)
	}
	if err := cmdAdd(project, []string{second}); err == nil {
		t.Fatal("expected duplicate basename to be rejected")
	}
	got, err := os.ReadFile(filepath.Join(imgDir(project), "same.png"))
	if err != nil {
		t.Fatal(err)
	}
	if string(got) != "first" {
		t.Fatalf("existing image was overwritten: %q", got)
	}
}
