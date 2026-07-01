package main

import "testing"

func TestLoadEvalCasesAcceptsObjectHardset(t *testing.T) {
	cases, err := loadEvalCases("testdata/hardset.example.json")
	if err != nil {
		t.Fatal(err)
	}
	if len(cases) != 3 {
		t.Fatalf("len(cases) = %d, want 3", len(cases))
	}
	if cases[0].Query != "docker permission denied error" {
		t.Fatalf("first query = %q", cases[0].Query)
	}
	if cases[0].Expected[0] != "screenshots/docker-permission-denied.png" {
		t.Fatalf("first expected = %v", cases[0].Expected)
	}
}

func TestEvalMetricsRecallPrecisionAndMRR(t *testing.T) {
	cases := []evalCase{
		{Query: "first", Expected: []string{"a.png"}},
		{Query: "second", Expected: []string{"b.png"}},
	}
	results := [][]hit{
		{{Path: "images/a.png"}, {Path: "images/x.png"}},
		{{Path: "images/y.png"}, {Path: "images/b.png"}},
	}

	s := summarizeEval(scorerSimple, cases, results, []int{1, 2})
	if s.RecallAt[1] != 0.5 {
		t.Fatalf("recall@1 = %.3f, want 0.500", s.RecallAt[1])
	}
	if s.PrecisionAt[1] != 0.5 {
		t.Fatalf("precision@1 = %.3f, want 0.500", s.PrecisionAt[1])
	}
	if s.RecallAt[2] != 1.0 {
		t.Fatalf("recall@2 = %.3f, want 1.000", s.RecallAt[2])
	}
	if s.PrecisionAt[2] != 0.5 {
		t.Fatalf("precision@2 = %.3f, want 0.500", s.PrecisionAt[2])
	}
	if s.MeanReciprocalR != 0.75 {
		t.Fatalf("mrr = %.3f, want 0.750", s.MeanReciprocalR)
	}
}

func TestExpectedImagesCanMatchByBasenamePathOrKey(t *testing.T) {
	h := hit{Key: "abc123", Path: "images/final.png"}
	for _, expected := range [][]string{
		{"abc123"},
		{"images/final.png"},
		{"final.png"},
	} {
		if matchingExpected(h, expected) == "" {
			t.Fatalf("expected %v to match %+v", expected, h)
		}
	}
}

func TestEvalKValuesIncludesTopKOnce(t *testing.T) {
	got := evalKValues(4)
	want := []int{1, 3, 4}
	if len(got) != len(want) {
		t.Fatalf("evalKValues length = %v, want %v", got, want)
	}
	for i := range want {
		if got[i] != want[i] {
			t.Fatalf("evalKValues = %v, want %v", got, want)
		}
	}
}
