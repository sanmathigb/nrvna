package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
)

// Worker configurations — the exact validated settings from the bash spec.
// imgsrch's MVP keeps inference on CPU for predictable portability.
func captionEnv() map[string]string {
	return map[string]string{
		"NRVNA_GPU_LAYERS":       "0",
		"NRVNA_TEMP":             envOr("NRVNA_TEMP", "0.1"),
		"NRVNA_PREDICT":          envOr("NRVNA_PREDICT", "256"),
		"NRVNA_THINKING":         envOr("NRVNA_THINKING", "0"),
		"NRVNA_BATCH":            envOr("NRVNA_BATCH", "512"),
		"NRVNA_UBATCH":           envOr("NRVNA_UBATCH", "512"),
		"NRVNA_IMAGE_MAX_TOKENS": envOr("NRVNA_IMAGE_MAX_TOKENS", "512"),
	}
}

func ocrEnv() map[string]string {
	return map[string]string{
		"NRVNA_GPU_LAYERS": "0",
		"NRVNA_TEMP":       envOr("NRVNA_TEMP", "0.1"),
		"NRVNA_PREDICT":    envOr("NRVNA_PREDICT", "512"),
		"NRVNA_THINKING":   envOr("NRVNA_THINKING", "0"),
		"NRVNA_BATCH":      envOr("NRVNA_BATCH", "512"),
		"NRVNA_UBATCH":     envOr("NRVNA_UBATCH", "512"),
	}
}

func embedEnv() map[string]string {
	return map[string]string{
		"NRVNA_GPU_LAYERS": "0",
		"NRVNA_BATCH":      envOr("NRVNA_BATCH", "512"),
		"NRVNA_UBATCH":     envOr("NRVNA_UBATCH", "512"),
	}
}

func startCaption(project string, c config) error {
	return startWorker("caption", c.CaptionModel, captionWs(project), captionEnv(), "--mmproj", c.CaptionMmproj, "-w", "1")
}
func startOcr(project string, c config) error {
	return startWorker("ocr", c.OcrModel, ocrWs(project), ocrEnv(), "--mmproj", c.OcrMmproj, "-w", "1")
}
func startEmbed(project string, c config) error {
	return startWorker("embed", c.EmbedModel, embedWs(project), embedEnv(), "-w", "1")
}

// submit runs wrk and returns the job id from stdout.
func submit(ws string, args ...string) (string, error) {
	wrk := binWrk()
	if wrk == "" {
		return "", fmt.Errorf("engine submit binary not found")
	}
	out, err := exec.Command(wrk, append([]string{ws}, args...)...).Output()
	if err != nil {
		return "", fmt.Errorf("submitting job to %s: %w", filepath.Base(ws), err)
	}
	return strings.TrimSpace(string(out)), nil
}

// submitStdin pipes a payload to wrk (used for embeddings).
func submitStdin(ws, payload string, args ...string) (string, error) {
	wrk := binWrk()
	if wrk == "" {
		return "", fmt.Errorf("engine submit binary not found")
	}
	cmd := exec.Command(wrk, append([]string{ws}, args...)...)
	cmd.Stdin = strings.NewReader(payload + "\n")
	out, err := cmd.Output()
	if err != nil {
		return "", fmt.Errorf("submitting embed job: %w", err)
	}
	return strings.TrimSpace(string(out)), nil
}

type submitJobFunc func(string, ...string) (string, error)
type persistItemFunc func(item) error

func submitMissingImageJobs(
	it item,
	capWs, ocrWs, captionPrompt, ocrPrompt, image string,
	submitFn submitJobFunc,
	persistFn persistItemFunc,
) (item, error) {
	if it.CapJob == "" {
		job, err := submitFn(capWs, captionPrompt, "--image", image)
		if err != nil {
			return it, err
		}
		it.CapJob = job
		if err := persistFn(it); err != nil {
			return it, err
		}
	}
	if it.OcrJob == "" {
		job, err := submitFn(ocrWs, ocrPrompt, "--image", image)
		if err != nil {
			return it, err
		}
		it.OcrJob = job
		if err := persistFn(it); err != nil {
			return it, err
		}
	}
	return it, nil
}

func persistEmbedJob(items []item, index int, job string, persist func([]item) error) error {
	items[index].EmbJob = job
	return persist(items)
}

func jobOutput(ws, job, name string) (string, bool) {
	p := filepath.Join(ws, "output", job, name)
	if _, err := os.Stat(p); err == nil {
		return p, true
	}
	return "", false
}

// jobError returns the failure reason if the job landed in failed/.
func jobError(ws, job string) (string, bool) {
	data, err := os.ReadFile(filepath.Join(ws, "failed", job, "error.txt"))
	if err != nil {
		return "", false
	}
	return strings.TrimSpace(string(data)), true
}

type jobFailedFunc func(string, string) bool

func resetFailedImageJobs(it item, failed jobFailedFunc, capWs, ocrWs string) (item, bool) {
	changed := false
	if it.CapJob != "" && failed(capWs, it.CapJob) {
		it.CapJob = ""
		changed = true
	}
	if it.OcrJob != "" && failed(ocrWs, it.OcrJob) {
		it.OcrJob = ""
		changed = true
	}
	return it, changed
}

func resetFailedEmbedJob(it item, failed jobFailedFunc, embedWs string) (item, bool) {
	if it.EmbJob != "" && failed(embedWs, it.EmbJob) {
		it.EmbJob = ""
		return it, true
	}
	return it, false
}

func jobFailed(ws, job string) bool {
	_, failed := jobError(ws, job)
	return failed
}

func cmdIndex(project string, newImages []string) error {
	if err := requireProject(project); err != nil {
		return err
	}
	c := loadConfig(project)
	if missing := checkModels(c); len(missing) > 0 {
		return fmt.Errorf("missing models:\n  %s\nrun 'imgsrch doctor' for details", strings.Join(missing, "\n  "))
	}
	if len(newImages) > 0 {
		if err := addImages(project, newImages); err != nil {
			return err
		}
	}
	if err := startCaption(project, c); err != nil {
		return err
	}
	if err := startOcr(project, c); err != nil {
		return err
	}

	accepted, unsupported, err := collectImages(project)
	if err != nil {
		return err
	}
	for _, f := range unsupported {
		note("skipped %s (format not supported by the inference engine)", filepath.Base(f))
	}

	// Lock around the submit loop: persist each image's job ids immediately so an
	// interrupt never orphans them, and serialize against a concurrent status/search.
	unlock, err := lockProject(project)
	if err != nil {
		return err
	}
	defer unlock()

	items, err := readItems(project)
	if err != nil {
		return err
	}
	known := map[string]int{}
	for i, it := range items {
		known[it.Key] = i
	}

	absProject, _ := filepath.Abs(project)
	queued, skipped := 0, 0
	for _, img := range accepted {
		key, err := contentKey(img)
		if err != nil {
			return err
		}
		absImg, _ := filepath.Abs(img)
		rel, err := filepath.Rel(absProject, absImg)
		if err != nil {
			rel = img
		}
		idx, exists := known[key]
		it := item{Key: key, Path: rel}
		if exists {
			it = items[idx]
			var retry bool
			it, retry = resetFailedImageJobs(it, jobFailed, captionWs(project), ocrWs(project))
			if retry {
				items[idx] = it
				if err := writeItems(project, items); err != nil {
					return err
				}
			}
			if it.CapJob != "" && it.OcrJob != "" {
				skipped++
				continue
			}
		}
		persist := func(updated item) error {
			if !exists {
				items = append(items, updated)
				idx = len(items) - 1
				known[key] = idx
				exists = true
			} else {
				items[idx] = updated
			}
			return writeItems(project, items)
		}
		if _, err := submitMissingImageJobs(
			it, captionWs(project), ocrWs(project),
			c.CaptionPrompt, c.OcrPrompt, absImg,
			submit, persist,
		); err != nil {
			return err
		}
		queued++
	}

	fmt.Printf("queued:  %d image(s)\n", queued)
	fmt.Printf("skipped: %d already indexed\n", skipped)
	fmt.Println()
	fmt.Println("indexing runs in the background. come back later:")
	fmt.Printf("  imgsrch status %s\n", project)
	fmt.Printf("  imgsrch search %s \"your query\"\n", project)
	return nil
}

type failure struct{ Path, Stage, Reason string }

type progress struct {
	Total, Caption, Ocr, Combined, Embed, Indexed int
	Failures                                      []failure
}

// advance is the single-pass pipeline driver: move every item as far as it
// can go right now, then return. No polling — the filesystem holds the state
// between runs. It is called implicitly by status and search.
func advance(project string, verbose bool) (progress, error) {
	var pr progress
	if err := requireProject(project); err != nil {
		return pr, err
	}
	if err := ensureProject(project); err != nil {
		return pr, err
	}
	c := loadConfig(project)

	// Serialize the manifest read-modify-write against a concurrent index run.
	unlock, err := lockProject(project)
	if err != nil {
		return pr, err
	}
	defer unlock()

	items, err := readItems(project)
	if err != nil {
		return pr, err
	}
	indexed, err := readIndexKeys(project)
	if err != nil {
		return pr, err
	}

	capWs, oWs, eWs := captionWs(project), ocrWs(project), embedWs(project)
	embedStarted := false
	var nCap, nOcr, nComb, nEmb, nQueued, nIdx int

	for i := range items {
		it := &items[i]
		adir := filepath.Join(artifactsDir(project), it.Key)
		if err := os.MkdirAll(adir, 0o755); err != nil {
			return pr, err
		}
		capFile := filepath.Join(adir, "caption.txt")
		ocrFile := filepath.Join(adir, "ocr.txt")
		combFile := filepath.Join(adir, "combined.md")
		embFile := filepath.Join(adir, "embedding.json")

		if updated, retry := resetFailedEmbedJob(*it, jobFailed, eWs); retry {
			*it = updated
			if err := writeItems(project, items); err != nil {
				return pr, err
			}
		}

		if it.CapJob != "" && !exists(capFile) {
			if src, ok := jobOutput(capWs, it.CapJob, "result.txt"); ok {
				if err := copyFile(src, capFile); err != nil {
					return pr, err
				}
				nCap++
			} else if reason, failed := jobError(capWs, it.CapJob); failed {
				pr.Failures = append(pr.Failures, failure{it.Path, "caption", reason})
			}
		}
		if it.OcrJob != "" && !exists(ocrFile) {
			if src, ok := jobOutput(oWs, it.OcrJob, "result.txt"); ok {
				if err := copyFile(src, ocrFile); err != nil {
					return pr, err
				}
				nOcr++
			} else if reason, failed := jobError(oWs, it.OcrJob); failed {
				pr.Failures = append(pr.Failures, failure{it.Path, "ocr", reason})
			}
		}
		if exists(capFile) && exists(ocrFile) && !exists(combFile) {
			if err := writeCombined(capFile, ocrFile, combFile); err != nil {
				return pr, err
			}
			nComb++
		}
		if exists(combFile) && it.EmbJob == "" {
			if !embedStarted {
				if err := startEmbed(project, c); err != nil {
					return pr, err
				}
				embedStarted = true
			}
			payload, err := os.ReadFile(combFile)
			if err != nil {
				return pr, err
			}
			flat := strings.ReplaceAll(string(payload), "\n", " ")
			job, err := submitStdin(eWs, c.DocPrefix+flat, "-", "--embed")
			if err != nil {
				return pr, err
			}
			if err := persistEmbedJob(items, i, job, func(updated []item) error {
				return writeItems(project, updated)
			}); err != nil {
				return pr, err
			}
			nQueued++
		}
		if it.EmbJob != "" && !exists(embFile) {
			if src, ok := jobOutput(eWs, it.EmbJob, "embedding.json"); ok {
				if err := copyFile(src, embFile); err != nil {
					return pr, err
				}
				nEmb++
			} else if reason, failed := jobError(eWs, it.EmbJob); failed {
				pr.Failures = append(pr.Failures, failure{it.Path, "embed", reason})
			}
		}
		if exists(embFile) && !indexed[it.Key] {
			if err := appendIndexRow(project, it.Key, it.Path); err != nil {
				return pr, err
			}
			indexed[it.Key] = true
			nIdx++
		}
	}

	if err := writeItems(project, items); err != nil {
		return pr, err
	}

	pr.Total = len(items)
	pr.Caption = countArtifacts(project, "caption.txt")
	pr.Ocr = countArtifacts(project, "ocr.txt")
	pr.Combined = countArtifacts(project, "combined.md")
	pr.Embed = countArtifacts(project, "embedding.json")
	pr.Indexed = len(indexed)

	if verbose {
		fmt.Printf("advanced: caption +%d | ocr +%d | combined +%d | embed +%d (queued %d) | searchable %d/%d\n",
			nCap, nOcr, nComb, nEmb, nQueued, pr.Indexed, pr.Total)
	}
	return pr, nil
}

func exists(p string) bool { _, err := os.Stat(p); return err == nil }

func countArtifacts(project, name string) int {
	n := 0
	entries, _ := os.ReadDir(artifactsDir(project))
	for _, e := range entries {
		if e.IsDir() && exists(filepath.Join(artifactsDir(project), e.Name(), name)) {
			n++
		}
	}
	return n
}

// writeCombined merges caption + OCR into the embedding source document.
// Caption is whitespace-collapsed and capped at 900 chars on a word boundary
// (verbose captions dilute embeddings — proven by A/B in the spec's history).
func writeCombined(capFile, ocrFile, outFile string) error {
	capData, err := os.ReadFile(capFile)
	if err != nil {
		return err
	}
	ocrData, err := os.ReadFile(ocrFile)
	if err != nil {
		return err
	}
	cap := strings.Join(strings.Fields(string(capData)), " ")
	if len(cap) > 900 {
		cut := cap[:900]
		if i := strings.LastIndex(cut, " "); i > 0 {
			cut = cut[:i]
		}
		cap = cut + "..."
	}
	text := cap
	ocr := strings.TrimSpace(string(ocrData))
	if ocr != "" {
		text += "\n\nVisible text:\n" + ocr
	}
	return os.WriteFile(outFile, []byte(strings.TrimSpace(text)+"\n"), 0o644)
}

func cmdStatus(project string) error {
	pr, err := advance(project, false)
	if err != nil {
		return err
	}
	fmt.Printf("images:   %d\n", pr.Total)
	fmt.Printf("caption:  %d done / %d\n", pr.Caption, pr.Total)
	fmt.Printf("ocr:      %d done / %d\n", pr.Ocr, pr.Total)
	fmt.Printf("combined: %d ready\n", pr.Combined)
	fmt.Printf("embed:    %d done\n", pr.Embed)
	fmt.Printf("search:   %d indexed\n", pr.Indexed)
	for _, f := range pr.Failures {
		fmt.Printf("failed:   %s (%s: %s)\n", f.Path, f.Stage, f.Reason)
	}
	return nil
}

func cmdStop(project string) error {
	var firstErr error
	for _, w := range []struct{ label, ws string }{
		{"caption", captionWs(project)}, {"ocr", ocrWs(project)}, {"embed", embedWs(project)},
	} {
		if err := stopWorker(w.label, w.ws); err != nil && firstErr == nil {
			firstErr = err
		}
	}
	return firstErr
}
