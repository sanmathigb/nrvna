package main

import (
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"syscall"
)

// Accepted formats = what the inference engine (llama.cpp/stb_image) can
// actually decode, intersected with what screenshots are. Everything else is
// loudly skipped — never silently dropped, never accepted-then-failed.
var acceptExt = map[string]bool{".png": true, ".jpg": true, ".jpeg": true, ".gif": true}

func imgDir(p string) string       { return filepath.Join(p, "images") }
func rootDir(p string) string      { return filepath.Join(p, ".imgsrch") }
func itemsFile(p string) string    { return filepath.Join(p, ".imgsrch", "items.tsv") }
func artifactsDir(p string) string { return filepath.Join(p, ".imgsrch", "artifacts") }
func indexFile(p string) string    { return filepath.Join(p, ".imgsrch", "index", "index.tsv") }
func logsDir(p string) string      { return filepath.Join(p, ".imgsrch", "logs") }
func captionWs(p string) string    { return filepath.Join(p, ".imgsrch", "workspaces", "caption") }
func ocrWs(p string) string        { return filepath.Join(p, ".imgsrch", "workspaces", "ocr") }
func embedWs(p string) string      { return filepath.Join(p, ".imgsrch", "workspaces", "embed") }

type item struct {
	Key, Path, CapJob, OcrJob, EmbJob string
}

// requireProject refuses to operate on a directory that was never initialized —
// a typo in the project name must not silently create a new project.
func requireProject(project string) error {
	if st, err := os.Stat(rootDir(project)); err == nil && st.IsDir() {
		return nil
	}
	return fmt.Errorf("no project at %s — create it first: imgsrch init %s", project, project)
}

func ensureProject(project string) error {
	for _, d := range []string{
		imgDir(project), artifactsDir(project),
		filepath.Dir(indexFile(project)), logsDir(project),
		filepath.Join(rootDir(project), "workspaces"),
	} {
		if err := os.MkdirAll(d, 0o755); err != nil {
			return err
		}
	}
	if _, err := os.Stat(itemsFile(project)); err != nil {
		if err := os.WriteFile(itemsFile(project), []byte("key\tpath\tcaption_job\tocr_job\tembed_job\n"), 0o644); err != nil {
			return err
		}
	}
	if _, err := os.Stat(indexFile(project)); err != nil {
		if err := os.WriteFile(indexFile(project), []byte("key\tpath\tembedding_path\n"), 0o644); err != nil {
			return err
		}
	}
	return nil
}

func readItems(project string) ([]item, error) {
	data, err := os.ReadFile(itemsFile(project))
	if err != nil {
		return nil, err
	}
	var items []item
	lines := strings.Split(string(data), "\n")
	for i, line := range lines {
		if i == 0 || strings.TrimSpace(line) == "" {
			continue
		}
		f := strings.Split(line, "\t")
		for len(f) < 5 {
			f = append(f, "")
		}
		items = append(items, item{f[0], f[1], f[2], f[3], f[4]})
	}
	return items, nil
}

// writeItems replaces the manifest atomically (temp file + rename).
func writeItems(project string, items []item) error {
	var b strings.Builder
	b.WriteString("key\tpath\tcaption_job\tocr_job\tembed_job\n")
	for _, it := range items {
		fmt.Fprintf(&b, "%s\t%s\t%s\t%s\t%s\n", it.Key, it.Path, it.CapJob, it.OcrJob, it.EmbJob)
	}
	return atomicWriteFile(itemsFile(project), []byte(b.String()), 0o644)
}

func readIndexKeys(project string) (map[string]bool, error) {
	keys := map[string]bool{}
	data, err := os.ReadFile(indexFile(project))
	if err != nil {
		return keys, err
	}
	for i, line := range strings.Split(string(data), "\n") {
		if i == 0 || strings.TrimSpace(line) == "" {
			continue
		}
		if f := strings.Split(line, "\t"); len(f) >= 1 {
			keys[f[0]] = true
		}
	}
	return keys, nil
}

// lockProject takes an exclusive flock on .imgsrch/.lock and returns an unlock
// func. It serializes manifest mutations so a concurrent `index` and
// `status`/`search` can't clobber each other's job ids. The lock auto-releases
// if the process dies (flock semantics), so a crash never deadlocks the next run.
func lockProject(project string) (func(), error) {
	f, err := os.OpenFile(filepath.Join(rootDir(project), ".lock"), os.O_RDWR|os.O_CREATE, 0o644)
	if err != nil {
		return nil, err
	}
	if err := syscall.Flock(int(f.Fd()), syscall.LOCK_EX); err != nil {
		f.Close()
		return nil, err
	}
	return func() {
		syscall.Flock(int(f.Fd()), syscall.LOCK_UN)
		f.Close()
	}, nil
}

func appendIndexRows(project string, rows []string) error {
	if len(rows) == 0 {
		return nil
	}
	data, err := os.ReadFile(indexFile(project))
	if err != nil {
		return err
	}
	var added strings.Builder
	for _, row := range rows {
		added.WriteString(row)
	}
	return atomicWriteFile(indexFile(project), append(data, []byte(added.String())...), 0o644)
}

// contentKey is the first 16 hex chars of the file's SHA-256 — identical to
// the bash spec, so existing project indexes stay valid.
func contentKey(path string) (string, error) {
	f, err := os.Open(path)
	if err != nil {
		return "", err
	}
	defer f.Close()
	h := sha256.New()
	if _, err := io.Copy(h, f); err != nil {
		return "", err
	}
	return hex.EncodeToString(h.Sum(nil))[:16], nil
}

// collectImages lists accepted images in the project, sorted. Files with
// unsupported extensions are returned separately so callers can be loud.
func collectImages(project string) (accepted, skipped []string, err error) {
	dir := imgDir(project)
	err = filepath.Walk(dir, func(path string, info os.FileInfo, werr error) error {
		if werr != nil || info.IsDir() {
			return nil
		}
		if acceptExt[strings.ToLower(filepath.Ext(path))] {
			accepted = append(accepted, path)
		} else {
			skipped = append(skipped, path)
		}
		return nil
	})
	sort.Strings(accepted)
	sort.Strings(skipped)
	return accepted, skipped, err
}

func cmdInit(project string) error {
	if err := os.MkdirAll(project, 0o755); err != nil {
		return err
	}
	if err := ensureProject(project); err != nil {
		return err
	}
	if err := writeDefaultConfig(project); err != nil {
		return err
	}
	note("initialized %s", project)
	return nil
}

func cmdAdd(project string, images []string) error {
	if err := requireProject(project); err != nil {
		return err
	}
	return addImages(project, images)
}

// addImages copies images into the project; shared by add and index.
func addImages(project string, images []string) error {
	if err := ensureProject(project); err != nil {
		return err
	}
	copied, skipped, existing := 0, 0, 0
	for _, img := range images {
		st, err := os.Stat(img)
		if err != nil || st.IsDir() {
			note("not a file: %s", img)
			continue
		}
		if !acceptExt[strings.ToLower(filepath.Ext(img))] {
			note("skipped %s (format not supported by the inference engine; use png/jpg/jpeg/gif)", filepath.Base(img))
			skipped++
			continue
		}
		dst := filepath.Join(imgDir(project), filepath.Base(img))
		if err := copyFileNoClobber(img, dst); err != nil {
			if os.IsExist(err) {
				srcKey, srcErr := contentKey(img)
				dstKey, dstErr := contentKey(dst)
				if srcErr == nil && dstErr == nil && srcKey == dstKey {
					existing++
					continue
				}
				return fmt.Errorf("%s already exists in the project; rename the new image first", filepath.Base(img))
			}
			return fmt.Errorf("copying %s: %w", img, err)
		}
		copied++
	}
	note("added %d image(s) to %s", copied, imgDir(project))
	if skipped > 0 {
		note("skipped %d unsupported file(s)", skipped)
	}
	if existing > 0 {
		note("skipped %d image(s) already in the project", existing)
	}
	return nil
}

func copyFile(src, dst string) error {
	in, err := os.Open(src)
	if err != nil {
		return err
	}
	defer in.Close()
	out, err := os.CreateTemp(filepath.Dir(dst), ".imgsrch-copy-*")
	if err != nil {
		return err
	}
	tmp := out.Name()
	defer os.Remove(tmp)
	if err := out.Chmod(0o644); err != nil {
		out.Close()
		return err
	}
	if _, err := io.Copy(out, in); err != nil {
		out.Close()
		return err
	}
	if err := out.Close(); err != nil {
		return err
	}
	return os.Rename(tmp, dst)
}

func copyFileNoClobber(src, dst string) error {
	in, err := os.Open(src)
	if err != nil {
		return err
	}
	defer in.Close()
	out, err := os.OpenFile(dst, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0o644)
	if err != nil {
		return err
	}
	ok := false
	defer func() {
		out.Close()
		if !ok {
			os.Remove(dst)
		}
	}()
	if _, err = io.Copy(out, in); err != nil {
		return err
	}
	if err = out.Close(); err != nil {
		return err
	}
	ok = true
	return nil
}

func atomicWriteFile(dst string, data []byte, mode os.FileMode) error {
	tmp, err := os.CreateTemp(filepath.Dir(dst), ".imgsrch-write-*")
	if err != nil {
		return err
	}
	name := tmp.Name()
	defer os.Remove(name)
	if err := tmp.Chmod(mode); err != nil {
		tmp.Close()
		return err
	}
	if _, err := tmp.Write(data); err != nil {
		tmp.Close()
		return err
	}
	if err := tmp.Close(); err != nil {
		return err
	}
	return os.Rename(name, dst)
}
