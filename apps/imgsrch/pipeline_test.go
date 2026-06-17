package main

import (
	"errors"
	"reflect"
	"testing"
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
