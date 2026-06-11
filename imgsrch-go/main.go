// imgsrch — search your screenshots and images by what's in them. Fully local.
//
// Built with nrvna ai: three background workers (caption, OCR, embedding)
// process images as durable jobs; every artifact is a plain file you can read.
package main

import (
	"fmt"
	"os"
)

const usageText = `Usage:
  imgsrch init   <project>
  imgsrch add    <project> <image...>
  imgsrch index  <project>
  imgsrch status <project>
  imgsrch search <project> <query> [top_n]
  imgsrch stop   <project>
  imgsrch doctor [project]

imgsrch turns a folder of images into a searchable local index.
Indexing is asynchronous: run 'index', walk away, come back to 'status'
and 'search'. Supported formats: png, jpg, jpeg, gif.
`

func usage() { fmt.Fprint(os.Stderr, usageText) }

func note(format string, a ...any) { fmt.Fprintf(os.Stderr, "imgsrch: "+format+"\n", a...) }

func fail(format string, a ...any) {
	note(format, a...)
	os.Exit(1)
}

func main() {
	args := os.Args[1:]
	if len(args) == 0 || args[0] == "-h" || args[0] == "--help" {
		usage()
		return
	}
	cmd, rest := args[0], args[1:]

	var err error
	switch cmd {
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
			fmt.Sscanf(rest[2], "%d", &topN)
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
