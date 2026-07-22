package main

import (
	"path/filepath"
	"strings"
	"testing"
)

func TestScoreHitsRRFUsesRanksInsteadOfRawScoreScale(t *testing.T) {
	hits := []hit{
		{Key: "dense", Path: "images/dense.png", Text: "nothing lexical", Dense: 0.99},
		{Key: "lexical", Path: "images/lexical.png", Text: "docker docker error", Dense: 0.10},
	}

	ranked := scoreHits(hits, "docker error", scorerRRF)
	if ranked[0].Key != "lexical" {
		t.Fatalf("top hit = %q, want lexical BM25 hit", ranked[0].Key)
	}
	if ranked[0].B25Rank != 1 {
		t.Fatalf("lexical B25Rank = %d, want 1", ranked[0].B25Rank)
	}
	if ranked[0].DenseRank == 0 {
		t.Fatal("dense rank was not assigned")
	}
}

func TestDiagnosticScorersExposeIndependentRankings(t *testing.T) {
	hits := []hit{
		{Key: "semantic", Path: "images/semantic.png", Text: "nothing lexical", Dense: 0.99},
		{Key: "lexical", Path: "images/lexical.png", Text: "docker docker error", Dense: 0.10},
	}

	if got := scoreHits(hits, "docker error", scorerDense)[0].Key; got != "semantic" {
		t.Fatalf("dense top hit = %q, want semantic", got)
	}
	if got := scoreHits(hits, "docker error", scorerBM25)[0].Key; got != "lexical" {
		t.Fatalf("BM25 top hit = %q, want lexical", got)
	}
}

func TestParseScorer(t *testing.T) {
	for _, tc := range []struct {
		input string
		want  scorer
		ok    bool
	}{
		{"simple", scorerSimple, true},
		{"rrf", scorerRRF, true},
		{"RRF", scorerRRF, true},
		{"dense", scorerDense, true},
		{"bm25", scorerBM25, true},
		{"dbsf", "", false},
	} {
		got, err := parseScorer(tc.input)
		if tc.ok && (err != nil || got != tc.want) {
			t.Fatalf("parseScorer(%q) = %q, %v; want %q, nil", tc.input, got, err, tc.want)
		}
		if !tc.ok && err == nil {
			t.Fatalf("parseScorer(%q) unexpectedly succeeded", tc.input)
		}
	}
}

func TestDenseHitsRejectsMixedEmbeddingDimensions(t *testing.T) {
	corpus := []hit{{Path: "images/old.png", Vec: []float64{1, 2}}}
	if _, err := denseHits(corpus, []float64{1, 2, 3}); err == nil {
		t.Fatal("dimension mismatch should require a reindex")
	}
}

func TestLoadCorpusReportsBrokenIndexEntry(t *testing.T) {
	project := filepath.Join(t.TempDir(), "project")
	if err := cmdInit(project); err != nil {
		t.Fatal(err)
	}
	if err := appendIndexRows(project, []string{"missing\timages/missing.png\tmissing.json\n"}); err != nil {
		t.Fatal(err)
	}
	_, err := loadCorpus(project)
	if err == nil || !strings.Contains(err.Error(), "images/missing.png") {
		t.Fatalf("loadCorpus error = %v, want broken image path", err)
	}
}
