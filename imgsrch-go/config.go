package main

import (
	"bufio"
	"os"
	"path/filepath"
	"strings"
)

// Known-good local defaults, identical to the validated bash spec.
// Override precedence: environment > <project>/.imgsrch/config > defaults.
const (
	defaultCaptionModel  = "models/LFM2.5 VL 1.6B GGUF.gguf"
	defaultCaptionMmproj = "models/mmproj-LFM2.5-VL-1.6B-Q8_0.gguf"
	defaultOcrModel      = "models/GLM-OCR-Q8_0.gguf"
	defaultOcrMmproj     = "models/mmproj-GLM-OCR-Q8_0.gguf"
	defaultEmbedModel    = "models/nomic-embed-text-v1.5.Q8_0.gguf"

	defaultCaptionPrompt = "Return 2-4 short bullet points about this image for search indexing. Name specific entities, visible text, technical terms, topic, and key insight."
	defaultOcrPrompt     = "Text Recognition:" // exact GLM-OCR trigger phrase; do not change
	defaultDocPrefix     = "search_document: " // nomic-embed required prefixes
	defaultQueryPrefix   = "search_query: "
)

type config struct {
	CaptionModel, CaptionMmproj string
	OcrModel, OcrMmproj         string
	EmbedModel                  string
	CaptionPrompt, OcrPrompt    string
	DocPrefix, QueryPrefix      string
}

func envOr(key, def string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return def
}

// loadConfig resolves the model set for a project. The project config file is
// plain KEY=VALUE lines ('#' comments allowed).
func loadConfig(project string) config {
	fileVals := map[string]string{}
	if project != "" {
		if f, err := os.Open(filepath.Join(project, ".imgsrch", "config")); err == nil {
			sc := bufio.NewScanner(f)
			for sc.Scan() {
				line := strings.TrimSpace(sc.Text())
				if line == "" || strings.HasPrefix(line, "#") {
					continue
				}
				if k, v, ok := strings.Cut(line, "="); ok {
					fileVals[strings.TrimSpace(k)] = strings.TrimSpace(v)
				}
			}
			f.Close()
		}
	}
	pick := func(key, def string) string {
		if v := os.Getenv(key); v != "" {
			return v
		}
		if v, ok := fileVals[key]; ok && v != "" {
			return v
		}
		return def
	}
	c := config{
		CaptionModel:  resolvePath(pick("CAPTION_MODEL", defaultCaptionModel)),
		CaptionMmproj: resolvePath(pick("CAPTION_MMPROJ", defaultCaptionMmproj)),
		OcrModel:      resolvePath(pick("OCR_MODEL", defaultOcrModel)),
		OcrMmproj:     resolvePath(pick("OCR_MMPROJ", defaultOcrMmproj)),
		EmbedModel:    resolvePath(pick("EMBED_MODEL", defaultEmbedModel)),
		CaptionPrompt: envOr("CAPTION_PROMPT", defaultCaptionPrompt),
		OcrPrompt:     envOr("OCR_PROMPT", defaultOcrPrompt),
		DocPrefix:     envOr("META_DOC_PREFIX", defaultDocPrefix),
		QueryPrefix:   envOr("META_QUERY_PREFIX", defaultQueryPrefix),
	}
	return c
}

// resolvePath makes an existing relative path absolute; nonexistent paths pass
// through untouched so error messages show what was asked for.
func resolvePath(p string) string {
	if filepath.IsAbs(p) {
		return p
	}
	if _, err := os.Stat(p); err == nil {
		if abs, err := filepath.Abs(p); err == nil {
			return abs
		}
	}
	return p
}

func writeDefaultConfig(project string) error {
	path := filepath.Join(project, ".imgsrch", "config")
	if _, err := os.Stat(path); err == nil {
		return nil
	}
	body := `# imgsrch model configuration. Environment variables override these.
CAPTION_MODEL=` + defaultCaptionModel + `
CAPTION_MMPROJ=` + defaultCaptionMmproj + `
OCR_MODEL=` + defaultOcrModel + `
OCR_MMPROJ=` + defaultOcrMmproj + `
EMBED_MODEL=` + defaultEmbedModel + `
`
	return os.WriteFile(path, []byte(body), 0o644)
}

// checkModels returns a list of human-readable problems (empty = all good).
func checkModels(c config) []string {
	var missing []string
	for _, m := range []struct{ label, path string }{
		{"caption model", c.CaptionModel},
		{"caption mmproj", c.CaptionMmproj},
		{"OCR model", c.OcrModel},
		{"OCR mmproj", c.OcrMmproj},
		{"embedding model", c.EmbedModel},
	} {
		if _, err := os.Stat(m.path); err != nil {
			missing = append(missing, m.label+": "+m.path)
		}
	}
	return missing
}
