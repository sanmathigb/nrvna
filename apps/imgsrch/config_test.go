package main

import (
	"os"
	"path/filepath"
	"testing"
)

func TestResolveModelDoesNotDependOnWorkingDirectory(t *testing.T) {
	project := t.TempDir()
	projectModel := filepath.Join(project, "models", "custom.gguf")
	if err := os.MkdirAll(filepath.Dir(projectModel), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(projectModel, []byte("project"), 0o644); err != nil {
		t.Fatal(err)
	}

	other := t.TempDir()
	if err := os.MkdirAll(filepath.Join(other, "models"), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(other, "models", "custom.gguf"), []byte("cwd"), 0o644); err != nil {
		t.Fatal(err)
	}
	old, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	if err := os.Chdir(other); err != nil {
		t.Fatal(err)
	}
	defer os.Chdir(old)

	got := resolveModel(project, "models/custom.gguf")
	if got != projectModel {
		t.Fatalf("resolveModel() = %q, want project model %q", got, projectModel)
	}
}
