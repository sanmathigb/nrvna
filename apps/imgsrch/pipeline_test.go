package main

import (
	"errors"
	"os"
	"path/filepath"
	"reflect"
	"testing"
	"unicode/utf8"
)

func TestSubmitMissingImageJobsPersistsCaptionBeforeOCR(t *testing.T) {
	var events []string
	var saved item
	it := item{Key: "abc", Path: "images/a.png"}

	submitFn := func(ws string, args ...string) (string, error) {
		switch ws {
		case "caption":
			events = append(events, "submit-caption")
			return "cap-1", nil
		case "ocr":
			if saved.CapJob != "cap-1" {
				t.Fatalf("caption job was not persisted before OCR submission: %+v", saved)
			}
			events = append(events, "submit-ocr")
			return "", errors.New("OCR unavailable")
		default:
			t.Fatalf("unexpected workspace %q", ws)
			return "", nil
		}
	}
	persistFn := func(got item) error {
		saved = got
		events = append(events, "persist")
		return nil
	}

	got, err := submitMissingImageJobs(
		it, "caption", "ocr", "caption prompt", "ocr prompt", "/tmp/a.png",
		submitFn, persistFn,
	)
	if err == nil {
		t.Fatal("expected OCR submission error")
	}
	if got.CapJob != "cap-1" || got.OcrJob != "" {
		t.Fatalf("unexpected item after partial submission: %+v", got)
	}
	wantEvents := []string{"submit-caption", "persist", "submit-ocr"}
	if !reflect.DeepEqual(events, wantEvents) {
		t.Fatalf("events = %v, want %v", events, wantEvents)
	}
}

func TestSubmitMissingImageJobsResumesOnlyMissingOCR(t *testing.T) {
	it := item{Key: "abc", Path: "images/a.png", CapJob: "cap-existing"}
	var submitted []string
	var saved item

	got, err := submitMissingImageJobs(
		it, "caption", "ocr", "caption prompt", "ocr prompt", "/tmp/a.png",
		func(ws string, args ...string) (string, error) {
			submitted = append(submitted, ws)
			return "ocr-1", nil
		},
		func(got item) error {
			saved = got
			return nil
		},
	)
	if err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(submitted, []string{"ocr"}) {
		t.Fatalf("submitted = %v, want only OCR", submitted)
	}
	if got.OcrJob != "ocr-1" || saved.OcrJob != "ocr-1" {
		t.Fatalf("OCR job was not persisted: got=%+v saved=%+v", got, saved)
	}
}

func TestPersistEmbedJobWritesImmediately(t *testing.T) {
	items := []item{{Key: "a"}, {Key: "b"}}
	var writes int

	err := persistEmbedJob(items, 1, "embed-2", func(got []item) error {
		writes++
		if got[1].EmbJob != "embed-2" {
			t.Fatalf("embed job not present in persisted items: %+v", got[1])
		}
		return nil
	})
	if err != nil {
		t.Fatal(err)
	}
	if writes != 1 {
		t.Fatalf("writes = %d, want 1", writes)
	}
}

func TestResetFailedImageJobsClearsOnlyFailedStages(t *testing.T) {
	it := item{CapJob: "cap-failed", OcrJob: "ocr-running", EmbJob: "embed-done"}
	got, changed := resetFailedImageJobs(it, func(ws, job string) bool {
		return ws == "caption" && job == "cap-failed"
	}, "caption", "ocr")

	if !changed {
		t.Fatal("failed caption job was not detected")
	}
	if got.CapJob != "" {
		t.Fatalf("caption job = %q, want cleared", got.CapJob)
	}
	if got.OcrJob != "ocr-running" || got.EmbJob != "embed-done" {
		t.Fatalf("unfailed stages changed: %+v", got)
	}
}

func TestResetFailedEmbedJob(t *testing.T) {
	it := item{EmbJob: "embed-failed"}
	got, changed := resetFailedEmbedJob(it, func(ws, job string) bool {
		return ws == "embed" && job == "embed-failed"
	}, "embed")

	if !changed || got.EmbJob != "" {
		t.Fatalf("failed embed was not cleared: changed=%v item=%+v", changed, got)
	}
}

func TestVisionTokenBudgetsHaveSafeDefaults(t *testing.T) {
	t.Setenv("NRVNA_IMAGE_MAX_TOKENS", "")
	if got := captionEnv()["NRVNA_IMAGE_MAX_TOKENS"]; got != "512" {
		t.Fatalf("caption image token budget = %q, want 512", got)
	}
	if got := ocrEnv()["NRVNA_IMAGE_MAX_TOKENS"]; got != "1024" {
		t.Fatalf("OCR image token budget = %q, want 1024", got)
	}
}

func TestReadProgressObservesArtifactsAndFailures(t *testing.T) {
	project := t.TempDir()
	if err := ensureProject(project); err != nil {
		t.Fatal(err)
	}
	items := []item{
		{Key: "ready", Path: "images/ready.png", CapJob: "cap-1", OcrJob: "ocr-1", EmbJob: "emb-1"},
		{Key: "failed", Path: "images/failed.png", CapJob: "cap-2", OcrJob: "ocr-2"},
	}
	if err := writeItems(project, items); err != nil {
		t.Fatal(err)
	}
	ready := filepath.Join(artifactsDir(project), "ready")
	if err := os.MkdirAll(ready, 0o755); err != nil {
		t.Fatal(err)
	}
	for _, name := range []string{"ocr.txt", "combined.md", "embedding.json"} {
		if err := os.WriteFile(filepath.Join(ready, name), []byte("x\n"), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	captionDone := filepath.Join(captionWs(project), "output", "cap-1")
	if err := os.MkdirAll(captionDone, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(captionDone, "result.txt"), []byte("caption\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := appendIndexRows(project, []string{"ready\timages/ready.png\t.imgsrch/artifacts/ready/embedding.json\n"}); err != nil {
		t.Fatal(err)
	}
	failedJob := filepath.Join(captionWs(project), "failed", "cap-2")
	if err := os.MkdirAll(failedJob, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(failedJob, "error.txt"), []byte("vision failed\n"), 0o644); err != nil {
		t.Fatal(err)
	}

	before, err := os.ReadFile(itemsFile(project))
	if err != nil {
		t.Fatal(err)
	}
	got, err := readProgress(project)
	if err != nil {
		t.Fatal(err)
	}
	after, err := os.ReadFile(itemsFile(project))
	if err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(before, after) {
		t.Fatal("readProgress changed the manifest")
	}
	if got.Total != 2 || got.Caption != 1 || got.Ocr != 1 || got.Combined != 1 || got.Embed != 1 || got.Indexed != 1 {
		t.Fatalf("unexpected progress: %+v", got)
	}
	if len(got.Failures) != 1 || got.Failures[0].Stage != "caption" || got.Failures[0].Reason != "vision failed" {
		t.Fatalf("unexpected failures: %+v", got.Failures)
	}
}

func TestTruncateAtWordPreservesUTF8(t *testing.T) {
	got := truncateAtWord("alpha हिन्दी omega", 10)
	if !utf8.ValidString(got) {
		t.Fatalf("truncateAtWord returned invalid UTF-8: %q", got)
	}
	if got != "alpha..." {
		t.Fatalf("truncateAtWord = %q, want %q", got, "alpha...")
	}
}
