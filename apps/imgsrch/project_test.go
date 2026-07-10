package main

import (
	"os"
	"path/filepath"
	"strings"
	"sync"
	"testing"
)

func TestAddRefusesUninitializedProject(t *testing.T) {
	project := filepath.Join(t.TempDir(), "typo")
	img := filepath.Join(t.TempDir(), "a.png")
	if err := os.WriteFile(img, []byte("img"), 0o644); err != nil {
		t.Fatal(err)
	}
	err := cmdAdd(project, []string{img})
	if err == nil {
		t.Fatal("expected add to refuse an uninitialized project")
	}
	if !strings.Contains(err.Error(), "imgsrch init") {
		t.Fatalf("error should point at init, got: %v", err)
	}
	if _, statErr := os.Stat(project); !os.IsNotExist(statErr) {
		t.Fatal("refusing add must not create the project directory")
	}
}

func TestIndexStagesImagesBeforeStartingWorkers(t *testing.T) {
	// Force engine-binary resolution to fail so the test stops after staging.
	t.Setenv("PATH", t.TempDir())
	t.Setenv("NRVNA_BUILD_DIR", "")
	t.Setenv("NRVNA_DAEMON_BIN", "")
	models := t.TempDir()
	for _, v := range []string{"CAPTION_MODEL", "CAPTION_MMPROJ", "OCR_MODEL", "OCR_MMPROJ", "EMBED_MODEL"} {
		p := filepath.Join(models, v+".gguf")
		if err := os.WriteFile(p, []byte("stub"), 0o644); err != nil {
			t.Fatal(err)
		}
		t.Setenv(v, p)
	}

	project := filepath.Join(t.TempDir(), "project")
	if err := cmdInit(project); err != nil {
		t.Fatal(err)
	}
	img := filepath.Join(t.TempDir(), "shot.png")
	if err := os.WriteFile(img, []byte("img"), 0o644); err != nil {
		t.Fatal(err)
	}

	err := cmdIndex(project, []string{img})
	if err == nil {
		t.Fatal("expected index to fail without engine binaries")
	}
	if _, statErr := os.Stat(filepath.Join(imgDir(project), "shot.png")); statErr != nil {
		t.Fatalf("image should be staged even though the engine is unavailable: %v", statErr)
	}
}

func TestCmdAddRejectsExistingBasename(t *testing.T) {
	project := filepath.Join(t.TempDir(), "project")
	if err := cmdInit(project); err != nil {
		t.Fatal(err)
	}
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

func TestCopyFileNoClobberIsAtomic(t *testing.T) {
	dir := t.TempDir()
	first := filepath.Join(dir, "first.png")
	second := filepath.Join(dir, "second.png")
	dst := filepath.Join(dir, "same.png")
	if err := os.WriteFile(first, []byte("first"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(second, []byte("second"), 0o644); err != nil {
		t.Fatal(err)
	}

	start := make(chan struct{})
	errs := make(chan error, 2)
	var wg sync.WaitGroup
	for _, src := range []string{first, second} {
		wg.Add(1)
		go func() {
			defer wg.Done()
			<-start
			errs <- copyFileNoClobber(src, dst)
		}()
	}
	close(start)
	wg.Wait()
	close(errs)

	successes, exists := 0, 0
	for err := range errs {
		switch {
		case err == nil:
			successes++
		case os.IsExist(err):
			exists++
		default:
			t.Fatalf("unexpected copy error: %v", err)
		}
	}
	if successes != 1 || exists != 1 {
		t.Fatalf("successes=%d exists=%d; want one of each", successes, exists)
	}
}
