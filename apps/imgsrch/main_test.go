package main

import (
	"strings"
	"testing"
)

func TestUsageListsPublicCommandsAndFlags(t *testing.T) {
	for _, want := range []string{
		"Search local images by visible text and meaning.",
		"Usage:",
		"imgsrch <command> [arguments]",
		"setup",
		"search",
		"eval",
		"--scorer",
		"IMGSRCH_MODELS_DIR",
		"CONFIGURATION.md",
		"--help",
		"--version",
	} {
		if !strings.Contains(usageText, want) {
			t.Errorf("usageText missing %q", want)
		}
	}
}

func TestParseSearchArgsDefaultsToRRFScorer(t *testing.T) {
	got, err := parseSearchArgs([]string{"project", "query text"})
	if err != nil {
		t.Fatal(err)
	}
	if got.Project != "project" || got.Query != "query text" || got.TopN != 5 || got.Scorer != scorerRRF {
		t.Fatalf("parseSearchArgs defaults = %+v", got)
	}
}

func TestParseSearchArgsAcceptsTopKAndScorer(t *testing.T) {
	got, err := parseSearchArgs([]string{"project", "query text", "--top-k", "10", "--scorer", "dense"})
	if err != nil {
		t.Fatal(err)
	}
	if got.TopN != 10 || got.Scorer != scorerDense {
		t.Fatalf("parseSearchArgs = %+v, want top 10 dense", got)
	}
}

func TestParseSearchArgsRejectsPositionalTopN(t *testing.T) {
	if _, err := parseSearchArgs([]string{"project", "query text", "10"}); err == nil {
		t.Fatal("bare positional top_n should be rejected; use --top-k")
	}
}

func TestParseEvalArgsDefaultsToAllScorers(t *testing.T) {
	got, err := parseEvalArgs([]string{"project", "hardset.json"})
	if err != nil {
		t.Fatal(err)
	}
	want := []scorer{scorerSimple, scorerRRF, scorerDense, scorerBM25}
	if got.TopK != 5 || len(got.Scorers) != len(want) {
		t.Fatalf("parseEvalArgs defaults = %+v", got)
	}
	for i := range want {
		if got.Scorers[i] != want[i] {
			t.Fatalf("parseEvalArgs scorers = %v, want %v", got.Scorers, want)
		}
	}
}

func TestParseTopN(t *testing.T) {
	for _, tc := range []struct {
		input string
		want  int
		ok    bool
	}{
		{"1", 1, true},
		{"10", 10, true},
		{"0", 0, false},
		{"-1", 0, false},
		{"many", 0, false},
	} {
		got, err := parseTopN(tc.input)
		if tc.ok && (err != nil || got != tc.want) {
			t.Fatalf("parseTopN(%q) = %d, %v; want %d, nil", tc.input, got, err, tc.want)
		}
		if !tc.ok && err == nil {
			t.Fatalf("parseTopN(%q) unexpectedly succeeded with %d", tc.input, got)
		}
	}
}
