package main

import (
	"strings"
	"testing"
)

func TestUsageListsPublicCommandsAndFlags(t *testing.T) {
	for _, want := range []string{
		"Usage:",
		"imgsrch <command> [arguments]",
		"setup",
		"search",
		"eval",
		"--scorer",
		"--help",
		"--version",
	} {
		if !strings.Contains(usageText, want) {
			t.Errorf("usageText missing %q", want)
		}
	}
}

func TestParseSearchArgsDefaultsToSimpleScorer(t *testing.T) {
	got, err := parseSearchArgs([]string{"project", "query text"})
	if err != nil {
		t.Fatal(err)
	}
	if got.Project != "project" || got.Query != "query text" || got.TopN != 5 || got.Scorer != scorerSimple {
		t.Fatalf("parseSearchArgs defaults = %+v", got)
	}
}

func TestParseSearchArgsAcceptsRRFScorer(t *testing.T) {
	got, err := parseSearchArgs([]string{"project", "query text", "10", "--scorer", "rrf"})
	if err != nil {
		t.Fatal(err)
	}
	if got.TopN != 10 || got.Scorer != scorerRRF {
		t.Fatalf("parseSearchArgs = %+v, want top 10 rrf", got)
	}
}

func TestParseEvalArgsDefaultsToAllScorers(t *testing.T) {
	got, err := parseEvalArgs([]string{"project", "hardset.json"})
	if err != nil {
		t.Fatal(err)
	}
	if got.TopK != 5 || len(got.Scorers) != 2 || got.Scorers[0] != scorerSimple || got.Scorers[1] != scorerRRF {
		t.Fatalf("parseEvalArgs defaults = %+v", got)
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
