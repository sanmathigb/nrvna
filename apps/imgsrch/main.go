// imgsrch — search your screenshots and images by what's in them. Fully local.
//
// Built with nrvna ai: three background workers (caption, OCR, embedding)
// process images as durable jobs; every artifact is a plain file you can read.
package main

import (
	"fmt"
	"os"
	"strconv"
)

const version = "0.1.0"

const usageText = `Local semantic search for images.

Usage:
  imgsrch <command> [arguments]

Commands:
  setup                                  Download the default models
  init <project>                         Create a project
  add <project> <image...>               Add images to a project
  index <project>                        Index project images
  status <project>                       Show indexing status
  search <project> <query> [top_n]       Search a project
  stop <project>                         Stop background indexing
  doctor [project]                       Check the installation

Options:
  -h, --help       Show help
  -v, --version    Show version
`

func usage() { fmt.Fprint(os.Stderr, usageText) }

func note(format string, a ...any) { fmt.Fprintf(os.Stderr, "imgsrch: "+format+"\n", a...) }

func fail(format string, a ...any) {
	note(format, a...)
	os.Exit(1)
}

func parseTopN(s string) (int, error) {
	n, err := strconv.Atoi(s)
	if err != nil || n < 1 {
		return 0, fmt.Errorf("top_n must be a positive integer")
	}
	return n, nil
}

func main() {
	args := os.Args[1:]
	if len(args) == 0 || args[0] == "-h" || args[0] == "--help" {
		usage()
		return
	}
	if args[0] == "-v" || args[0] == "--version" || args[0] == "version" {
		fmt.Printf("imgsrch %s\n", version)
		return
	}
	cmd, rest := args[0], args[1:]

	var err error
	switch cmd {
	case "setup":
		if len(rest) != 0 {
			usage()
			os.Exit(1)
		}
		err = cmdSetup()
	case "init":
		if len(rest) != 1 {
			usage()
			os.Exit(1)
		}
		err = cmdInit(rest[0])
	case "add":
		if len(rest) < 2 {
			usage()
			os.Exit(1)
		}
		err = cmdAdd(rest[0], rest[1:])
	case "index":
		if len(rest) != 1 {
			usage()
			os.Exit(1)
		}
		err = cmdIndex(rest[0])
	case "status":
		if len(rest) != 1 {
			usage()
			os.Exit(1)
		}
		err = cmdStatus(rest[0])
	case "search":
		if len(rest) < 2 || len(rest) > 3 {
			usage()
			os.Exit(1)
		}
		topN := 5
		if len(rest) == 3 {
			topN, err = parseTopN(rest[2])
			if err != nil {
				break
			}
		}
		err = cmdSearch(rest[0], rest[1], topN)
	case "stop":
		if len(rest) != 1 {
			usage()
			os.Exit(1)
		}
		err = cmdStop(rest[0])
	case "collect": // hidden maintenance verb: one advance pass, prints the delta
		if len(rest) != 1 {
			usage()
			os.Exit(1)
		}
		_, err = advance(rest[0], true)
	case "doctor":
		if len(rest) > 1 {
			usage()
			os.Exit(1)
		}
		p := ""
		if len(rest) > 0 {
			p = rest[0]
		}
		err = cmdDoctor(p)
	default:
		usage()
		os.Exit(1)
	}
	if err != nil {
		fail("%v", err)
	}
}
