// imgsrch searches screenshots and images by their content. It runs locally.
//
// Built with nrvna: three background workers (caption, OCR, embedding)
// process images as durable jobs; every artifact is a plain file you can read.
package main

import (
	"fmt"
	"io"
	"os"
	"strconv"
)

const version = "0.1.1"

const usageText = `Search image projects by visible text and meaning.

Usage:
  imgsrch <command> [arguments]

Commands:
  setup                              Download default models
  init <project>                     Create a project
  add <project> <image...>           Stage images
  index <project> [image...]         Index staged or named images
  status <project>                   Show indexing progress
  search <project> <query> [options] Find matching images
  eval <project> <hardset.json>      Compare ranking methods
  doctor [project]                   Check setup

Examples:
  imgsrch init ~/Pictures/shots
  imgsrch index ~/Pictures/shots ~/Desktop/diagram.png
  imgsrch search ~/Pictures/shots "prefill and decode"

Options:
  --top-k n                   Results to return; default 5
  --scorer rrf|simple|dense|bm25  Ranking method; default rrf
  --scorer all                Eval only; compare methods
  -h, --help                  Show this help
  -v, --version               Show version

Indexing runs in the background. Check progress with status.

Configuration:
  Models default to ~/.imgsrch/models; override with IMGSRCH_MODELS_DIR.
  Run 'imgsrch doctor' to check model and engine paths.
  Full reference: https://github.com/sanmathigb/nrvna/blob/main/CONFIGURATION.md
`

func usage(w io.Writer) { fmt.Fprint(w, usageText) }

func note(format string, a ...any) { fmt.Fprintf(os.Stderr, "imgsrch: "+format+"\n", a...) }

func fail(format string, a ...any) {
	note(format, a...)
	os.Exit(1)
}

func parseTopN(s string) (int, error) {
	n, err := strconv.Atoi(s)
	if err != nil || n < 1 {
		return 0, fmt.Errorf("--top-k must be a positive integer")
	}
	return n, nil
}

type searchArgs struct {
	Project string
	Query   string
	TopN    int
	Scorer  scorer
}

func parseSearchArgs(rest []string) (searchArgs, error) {
	if len(rest) < 2 {
		return searchArgs{}, fmt.Errorf("search requires <project> and <query>")
	}
	args := searchArgs{Project: rest[0], Query: rest[1], TopN: 5, Scorer: scorerRRF}
	for i := 2; i < len(rest); i++ {
		switch rest[i] {
		case "--scorer":
			if i+1 >= len(rest) {
				return args, fmt.Errorf("--scorer requires simple, rrf, dense, or bm25")
			}
			sc, err := parseScorer(rest[i+1])
			if err != nil {
				return args, err
			}
			args.Scorer = sc
			i++
		case "--top-k":
			if i+1 >= len(rest) {
				return args, fmt.Errorf("--top-k requires a positive integer")
			}
			topN, err := parseTopN(rest[i+1])
			if err != nil {
				return args, err
			}
			args.TopN = topN
			i++
		default:
			return args, fmt.Errorf("unexpected search argument %q", rest[i])
		}
	}
	return args, nil
}

type evalArgs struct {
	Project string
	SetPath string
	TopK    int
	Scorers []scorer
}

func parseEvalArgs(rest []string) (evalArgs, error) {
	if len(rest) < 2 {
		return evalArgs{}, fmt.Errorf("eval requires <project> and <hardset.json>")
	}
	args := evalArgs{Project: rest[0], SetPath: rest[1], TopK: 5, Scorers: []scorer{scorerSimple, scorerRRF, scorerDense, scorerBM25}}
	for i := 2; i < len(rest); i++ {
		switch rest[i] {
		case "--top-k":
			if i+1 >= len(rest) {
				return args, fmt.Errorf("--top-k requires a positive integer")
			}
			topK, err := parseTopN(rest[i+1])
			if err != nil {
				return args, err
			}
			args.TopK = topK
			i++
		case "--scorer":
			if i+1 >= len(rest) {
				return args, fmt.Errorf("--scorer requires simple, rrf, dense, bm25, or all")
			}
			if rest[i+1] == "all" {
				args.Scorers = []scorer{scorerSimple, scorerRRF, scorerDense, scorerBM25}
			} else {
				sc, err := parseScorer(rest[i+1])
				if err != nil {
					return args, err
				}
				args.Scorers = []scorer{sc}
			}
			i++
		default:
			return args, fmt.Errorf("unexpected eval argument %q", rest[i])
		}
	}
	return args, nil
}

func run(args []string, stdout io.Writer) error {
	if len(args) == 0 || args[0] == "-h" || args[0] == "--help" {
		usage(stdout)
		return nil
	}
	if args[0] == "-v" || args[0] == "--version" || args[0] == "version" {
		fmt.Fprintf(stdout, "imgsrch %s\n", version)
		return nil
	}
	cmd, rest := args[0], args[1:]

	var err error
	switch cmd {
	case "setup":
		if len(rest) != 0 {
			return fmt.Errorf("usage: imgsrch setup")
		}
		err = cmdSetup()
	case "init":
		if len(rest) != 1 {
			return fmt.Errorf("usage: imgsrch init <project>")
		}
		err = cmdInit(rest[0])
	case "add":
		if len(rest) < 2 {
			return fmt.Errorf("usage: imgsrch add <project> <image...>")
		}
		err = cmdAdd(rest[0], rest[1:])
	case "index":
		if len(rest) < 1 {
			return fmt.Errorf("usage: imgsrch index <project> [image...]")
		}
		err = cmdIndex(rest[0], rest[1:])
	case "status":
		if len(rest) != 1 {
			return fmt.Errorf("usage: imgsrch status <project>")
		}
		err = cmdStatus(rest[0])
	case "search":
		var a searchArgs
		a, err = parseSearchArgs(rest)
		if err == nil {
			err = cmdSearch(a.Project, a.Query, a.TopN, a.Scorer)
		}
	case "eval":
		var a evalArgs
		a, err = parseEvalArgs(rest)
		if err == nil {
			err = cmdEval(a.Project, a.SetPath, a.TopK, a.Scorers)
		}
	case "__finish": // internal detached continuation launched by index
		if len(rest) != 1 {
			return fmt.Errorf("internal finish requires one project")
		}
		err = finishPipeline(rest[0])
	case "doctor":
		if len(rest) > 1 {
			return fmt.Errorf("usage: imgsrch doctor [project]")
		}
		p := ""
		if len(rest) > 0 {
			p = rest[0]
		}
		err = cmdDoctor(p)
	default:
		return fmt.Errorf("unknown command %q; run 'imgsrch --help'", cmd)
	}
	return err
}

func main() {
	if err := run(os.Args[1:], os.Stdout); err != nil {
		fail("%v", err)
	}
}
