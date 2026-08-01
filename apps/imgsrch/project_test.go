package main

import (
	"image"
	"image/color"
	"image/png"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"testing"
)

func writeTestPNG(t *testing.T, path string, c color.Color) {
	t.Helper()
	f, err := os.Create(path)
	if err != nil {
		t.Fatal(err)
	}
	img := image.NewRGBA(image.Rect(0, 0, 2, 2))
	for y := 0; y < 2; y++ {
		for x := 0; x < 2; x++ {
			img.Set(x, y, c)
		}
	}
	if err := png.Encode(f, img); err != nil {
		f.Close()
		t.Fatal(err)
	}
	if err := f.Close(); err != nil {
		t.Fatal(err)
	}
}

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
	writeTestPNG(t, img, color.Black)

	err := cmdIndex(project, []string{img})
	if err == nil {
		t.Fatal("expected index to fail without engine binaries")
	}
	if _, statErr := os.Stat(filepath.Join(imgDir(project), "shot.png")); statErr != nil {
		t.Fatalf("image should be staged even though the engine is unavailable: %v", statErr)
	}
}

func TestCmdAddDisambiguatesExistingBasename(t *testing.T) {
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
	writeTestPNG(t, first, color.Black)
	writeTestPNG(t, second, color.White)

	if err := cmdAdd(project, []string{first}); err != nil {
		t.Fatal(err)
	}
	if err := cmdAdd(project, []string{second}); err != nil {
		t.Fatal(err)
	}
	secondKey, err := contentKey(second)
	if err != nil {
		t.Fatal(err)
	}
	disambiguated := filepath.Join(imgDir(project), "same-"+secondKey[:8]+".png")
	if _, err := os.Stat(disambiguated); err != nil {
		t.Fatalf("disambiguated image missing: %v", err)
	}
	firstKey, err := contentKey(filepath.Join(imgDir(project), "same.png"))
	if err != nil {
		t.Fatal(err)
	}
	wantFirstKey, err := contentKey(first)
	if err != nil {
		t.Fatal(err)
	}
	if firstKey != wantFirstKey {
		t.Fatal("existing image was overwritten")
	}
}

func TestCmdAddSkipsIdenticalExistingImage(t *testing.T) {
	project := filepath.Join(t.TempDir(), "project")
	if err := cmdInit(project); err != nil {
		t.Fatal(err)
	}
	sourceDir := t.TempDir()
	image := filepath.Join(sourceDir, "same.png")
	writeTestPNG(t, image, color.Black)

	if err := cmdAdd(project, []string{image}); err != nil {
		t.Fatal(err)
	}
	if err := cmdAdd(project, []string{image}); err != nil {
		t.Fatalf("adding the same image twice should be idempotent: %v", err)
	}
}

func TestCmdAddSkipsIdenticalContentWithDifferentName(t *testing.T) {
	project := filepath.Join(t.TempDir(), "project")
	if err := cmdInit(project); err != nil {
		t.Fatal(err)
	}
	first := filepath.Join(t.TempDir(), "first.png")
	second := filepath.Join(t.TempDir(), "copy.png")
	writeTestPNG(t, first, color.Black)
	data, err := os.ReadFile(first)
	if err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(second, data, 0o644); err != nil {
		t.Fatal(err)
	}
	if err := cmdAdd(project, []string{first}); err != nil {
		t.Fatal(err)
	}
	if err := cmdAdd(project, []string{second}); err != nil {
		t.Fatal(err)
	}
	entries, err := os.ReadDir(imgDir(project))
	if err != nil {
		t.Fatal(err)
	}
	if len(entries) != 1 {
		t.Fatalf("project contains %d files, want one exact-content copy", len(entries))
	}
}

func TestCmdAddRejectsCorruptImageBeforeCopy(t *testing.T) {
	project := filepath.Join(t.TempDir(), "project")
	if err := cmdInit(project); err != nil {
		t.Fatal(err)
	}
	image := filepath.Join(t.TempDir(), "broken.png")
	if err := os.WriteFile(image, []byte("not a png"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := cmdAdd(project, []string{image}); err == nil {
		t.Fatal("expected corrupt image to be rejected")
	}
	if _, err := os.Stat(filepath.Join(imgDir(project), "broken.png")); !os.IsNotExist(err) {
		t.Fatalf("corrupt image was copied: %v", err)
	}
}

func TestValidateImageDimensions(t *testing.T) {
	if err := validateImageDimensions(8_000, 6_000); err != nil {
		t.Fatalf("48MP phone image should be accepted: %v", err)
	}
	if err := validateImageDimensions(1_290, 30_000); err != nil {
		t.Fatalf("very tall screenshot should be accepted: %v", err)
	}
	if err := validateImageDimensions(8_000, 8_000); err != nil {
		t.Fatalf("64MP image should be accepted: %v", err)
	}
	if err := validateImageDimensions(8_001, 8_001); err == nil {
		t.Fatal("image above decoded-pixel limit should be rejected")
	}
	if err := validateImageDimensions(0, 10); err == nil {
		t.Fatal("zero-width image should be rejected")
	}
}

func TestCmdAddRejectsFilenameThatBreaksManifest(t *testing.T) {
	project := filepath.Join(t.TempDir(), "project")
	if err := cmdInit(project); err != nil {
		t.Fatal(err)
	}
	image := filepath.Join(t.TempDir(), "bad\tname.png")
	if err := os.WriteFile(image, []byte("image"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := cmdAdd(project, []string{image}); err == nil {
		t.Fatal("expected manifest-unsafe filename to be rejected")
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

func TestAtomicWriteFileReplacesWholeFile(t *testing.T) {
	path := filepath.Join(t.TempDir(), "state.tsv")
	if err := os.WriteFile(path, []byte("old"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := atomicWriteFile(path, []byte("new state\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	got, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	if string(got) != "new state\n" {
		t.Fatalf("atomicWriteFile wrote %q", got)
	}
}

func TestCopyFileUsesReadableArtifactMode(t *testing.T) {
	dir := t.TempDir()
	src := filepath.Join(dir, "src")
	dst := filepath.Join(dir, "dst")
	if err := os.WriteFile(src, []byte("artifact"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := copyFile(src, dst); err != nil {
		t.Fatal(err)
	}
	info, err := os.Stat(dst)
	if err != nil {
		t.Fatal(err)
	}
	if got := info.Mode().Perm(); got != 0o644 {
		t.Fatalf("copy mode = %o, want 644", got)
	}
}

func TestAppendIndexRowsPreservesExistingData(t *testing.T) {
	project := filepath.Join(t.TempDir(), "project")
	if err := cmdInit(project); err != nil {
		t.Fatal(err)
	}
	before, err := os.ReadFile(indexFile(project))
	if err != nil {
		t.Fatal(err)
	}
	rows := []string{"key-a\ta.png\ta.json\n", "key-b\tb.png\tb.json\n"}
	if err := appendIndexRows(project, rows); err != nil {
		t.Fatal(err)
	}
	after, err := os.ReadFile(indexFile(project))
	if err != nil {
		t.Fatal(err)
	}
	want := string(before) + strings.Join(rows, "")
	if string(after) != want {
		t.Fatalf("index = %q, want %q", after, want)
	}
}
