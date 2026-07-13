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
  setup                                                Download the default models
  init <project>                                       Create a project
  index <project> [image...]                           Add images (optional) and index
  search <project> <query> [--top-k n] [--scorer s]    Search a project
  status <project>                                     Show indexing status
  add <project> <image...>                             Stage images without indexing
  eval <project> <hardset.json> [--top-k n]            Evaluate scorers on a hard set
  doctor [project]                                     Check the installation

Scorers:
  rrf       Reciprocal rank fusion over dense and BM25 rankings (default search scorer)
  simple    Original 50/50 dense + normalized BM25 blend

Options:
  --top-k n                 Results to return (search) or eval cutoff; default 5
  --scorer simple|rrf       Select search scorer
  --scorer all              Eval only; compare all scorers
  -h, --help                Show help
  -v, --version             Show version
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
				return args, fmt.Errorf("--scorer requires simple or rrf")
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
	args := evalArgs{Project: rest[0], SetPath: rest[1], TopK: 5, Scorers: []scorer{scorerSimple, scorerRRF}}
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
				return args, fmt.Errorf("--scorer requires simple, rrf, or all")
			}
			if rest[i+1] == "all" {
				args.Scorers = []scorer{scorerSimple, scorerRRF}
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
		if len(rest) < 1 {
			usage()
			os.Exit(1)
		}
		err = cmdIndex(rest[0], rest[1:])
	case "status":
		if len(rest) != 1 {
			usage()
			os.Exit(1)
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
			os.Exit(1)
		}
		err = finishPipeline(rest[0])
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
