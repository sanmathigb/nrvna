package main

import (
	"fmt"
	"os/exec"
)

func engineEnvVar(name string) string {
	switch name {
	case "nrvnad":
		return "NRVNA_DAEMON_BIN"
	case "wrk":
		return "NRVNA_WRK_BIN"
	case "flw":
		return "NRVNA_FLW_BIN"
	default:
		return ""
	}
}

// doctor verifies the engine binaries and models are present and runnable.
// It is the one place engine internals are named — that's its job.
func cmdDoctor(project string) error {
	if project != "" {
		if err := requireProject(project); err != nil {
			return err
		}
	}
	c := loadConfig(project)
	bad := false

	fmt.Println("engine:")
	for _, b := range []struct{ name, path string }{
		{"nrvnad", binDaemon()}, {"wrk", binWrk()}, {"flw", binFlw()},
	} {
		switch {
		case b.path == "":
			fmt.Printf("  ✗ %s missing (set NRVNA_BUILD_DIR or %s)\n", b.name, engineEnvVar(b.name))
			bad = true
		case exec.Command(b.path, "--version").Run() != nil:
			fmt.Printf("  ✗ %s won't run: %s\n", b.name, b.path)
			bad = true
		default:
			fmt.Printf("  ✓ %s %s\n", b.name, b.path)
		}
	}

	fmt.Println()
	fmt.Println("models:")
	for _, m := range []struct{ name, path string }{
		{"caption", c.CaptionModel}, {"caption-mmproj", c.CaptionMmproj},
		{"ocr", c.OcrModel}, {"ocr-mmproj", c.OcrMmproj},
		{"embed", c.EmbedModel},
	} {
		if exists(m.path) {
			fmt.Printf("  ✓ %s %s\n", m.name, m.path)
		} else {
			fmt.Printf("  ✗ %s %s\n", m.name, m.path)
			bad = true
		}
	}

	if bad {
		return fmt.Errorf("doctor found problems (see above); missing models can be fetched with 'imgsrch setup'")
	}
	return nil
}
