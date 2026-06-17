package main

import (
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"time"
)

// The blessed model set. Every entry is pinned to an exact repo revision and
// SHA-256 — these are byte-identical to the files the pipeline was validated
// with. Sources are the original author/org repos (no redistribution).
type modelSpec struct {
	Name     string // local filename under the models home
	Label    string
	Repo     string // HuggingFace repo
	Revision string // pinned repo commit
	Path     string // file path inside the repo
	Sha256   string
	Size     int64
}

var modelManifest = []modelSpec{
	{"LFM2.5-VL-1.6B-Q8_0.gguf", "caption", "LiquidAI/LFM2.5-VL-1.6B-GGUF",
		"48c6a306939241d1ddc99b090df552cb47a066c6", "LFM2.5-VL-1.6B-Q8_0.gguf",
		"a34bd1506a298d7ff07902e69baeac48c7c20bb85162e61218b743dc10be7c67", 1246254880},
	{"mmproj-LFM2.5-VL-1.6b-Q8_0.gguf", "caption-mmproj", "LiquidAI/LFM2.5-VL-1.6B-GGUF",
		"48c6a306939241d1ddc99b090df552cb47a066c6", "mmproj-LFM2.5-VL-1.6b-Q8_0.gguf",
		"2ce89e610c56f3198ece2b86cf61743a08b9307279c89125eb2412ebb908689d", 583109888},
	{"GLM-OCR-Q8_0.gguf", "ocr", "ggml-org/GLM-OCR-GGUF",
		"65a42de1148dbed2297e922b5dbc7d9b70c36578", "GLM-OCR-Q8_0.gguf",
		"45bc244a6446aff850521dc41f18bc8d7105ad5f0c2c8c28af04e7cc4f4d50b1", 950433408},
	{"mmproj-GLM-OCR-Q8_0.gguf", "ocr-mmproj", "ggml-org/GLM-OCR-GGUF",
		"65a42de1148dbed2297e922b5dbc7d9b70c36578", "mmproj-GLM-OCR-Q8_0.gguf",
		"9c4b58e33e316ed142eb5dcb41abec3844d3e6e5dc361ffb782c3fa9d175141f", 484403648},
	{"nomic-embed-text-v1.5.Q8_0.gguf", "embed", "nomic-ai/nomic-embed-text-v1.5-GGUF",
		"0188c9bf409793f810680a5a431e7b899c46104c", "nomic-embed-text-v1.5.Q8_0.gguf",
		"3e24342164b3d94991ba9692fdc0dd08e3fd7362e0aacc396a9a5c54a544c3b7", 146146432},
}

func newDownloadClient(timeout time.Duration) *http.Client {
	return &http.Client{Timeout: timeout}
}

var downloadClient = newDownloadClient(2 * time.Hour)

// modelsHome is where setup installs models and where defaults resolve as a
// fallback after ./models — one fixed, predictable place per user.
func modelsHome() string {
	if v := os.Getenv("IMGSRCH_MODELS_DIR"); v != "" {
		return v
	}
	home, err := os.UserHomeDir()
	if err != nil {
		return ".imgsrch-models"
	}
	return filepath.Join(home, ".imgsrch", "models")
}

func cmdSetup() error {
	dir := modelsHome()
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return err
	}
	var total int64
	for _, m := range modelManifest {
		total += m.Size
	}
	note("model home: %s", dir)

	for _, m := range modelManifest {
		dest := filepath.Join(dir, m.Name)
		if exists(dest) {
			sum, err := fileSha256(dest)
			if err != nil {
				return err
			}
			if sum == m.Sha256 {
				fmt.Printf("  ✓ %-15s %s (verified)\n", m.Label, m.Name)
				continue
			}
			note("%s exists but checksum differs; re-downloading", m.Name)
		}
		if err := download(m, dest); err != nil {
			return fmt.Errorf("%s: %w", m.Label, err)
		}
		fmt.Printf("  ✓ %-15s %s (downloaded)\n", m.Label, m.Name)
	}
	fmt.Println()
	note("all models ready. next: imgsrch index <project>")
	return nil
}

func download(m modelSpec, dest string) error {
	url := fmt.Sprintf("https://huggingface.co/%s/resolve/%s/%s", m.Repo, m.Revision, m.Path)
	fmt.Printf("  ↓ %-15s %s (%.1f GB)\n", m.Label, m.Name, float64(m.Size)/1e9)

	resp, err := downloadClient.Get(url)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	if resp.StatusCode != 200 {
		return fmt.Errorf("download failed: HTTP %d from %s", resp.StatusCode, url)
	}

	tmp := dest + ".partial"
	out, err := os.Create(tmp)
	if err != nil {
		return err
	}
	h := sha256.New()
	var written int64
	lastReport := time.Now()
	buf := make([]byte, 1<<20)
	for {
		n, rerr := resp.Body.Read(buf)
		if n > 0 {
			if _, werr := out.Write(buf[:n]); werr != nil {
				out.Close()
				return werr
			}
			h.Write(buf[:n])
			written += int64(n)
			if time.Since(lastReport) > 5*time.Second {
				fmt.Printf("    %d%% (%.0f / %.0f MB)\n", written*100/m.Size, float64(written)/1e6, float64(m.Size)/1e6)
				lastReport = time.Now()
			}
		}
		if rerr == io.EOF {
			break
		}
		if rerr != nil {
			out.Close()
			return rerr
		}
	}
	if err := out.Close(); err != nil {
		return err
	}
	if sum := hex.EncodeToString(h.Sum(nil)); sum != m.Sha256 {
		os.Remove(tmp)
		return fmt.Errorf("checksum mismatch after download (got %s)", sum[:16])
	}
	return os.Rename(tmp, dest)
}

func fileSha256(path string) (string, error) {
	f, err := os.Open(path)
	if err != nil {
		return "", err
	}
	defer f.Close()
	h := sha256.New()
	if _, err := io.Copy(h, f); err != nil {
		return "", err
	}
	return hex.EncodeToString(h.Sum(nil)), nil
}
