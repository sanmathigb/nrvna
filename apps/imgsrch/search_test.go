package main

import "testing"

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

func TestParseScorer(t *testing.T) {
	for _, tc := range []struct {
		input string
		want  scorer
		ok    bool
	}{
		{"simple", scorerSimple, true},
		{"rrf", scorerRRF, true},
		{"RRF", scorerRRF, true},
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
