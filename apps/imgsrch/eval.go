package main

import (
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"
)

type evalCase struct {
	Query    string
	Expected []string
}

type rawEvalCase struct {
	Query          string   `json:"query"`
	Expected       []string `json:"expected"`
	ExpectedImages []string `json:"expected_images"`
	ExpectedImage  string   `json:"expected_image"`
}

func loadEvalCases(path string) ([]evalCase, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	var obj struct {
		Queries []rawEvalCase `json:"queries"`
	}
	if err := json.Unmarshal(data, &obj); err == nil && obj.Queries != nil {
		return normalizeEvalCases(obj.Queries)
	}
	var arr []rawEvalCase
	if err := json.Unmarshal(data, &arr); err != nil {
		return nil, fmt.Errorf("hardset must be a JSON array or an object with queries: %w", err)
	}
	return normalizeEvalCases(arr)
}

func normalizeEvalCases(raw []rawEvalCase) ([]evalCase, error) {
	cases := make([]evalCase, 0, len(raw))
	for i, r := range raw {
		q := strings.TrimSpace(r.Query)
		if q == "" {
			return nil, fmt.Errorf("eval case %d has empty query", i+1)
		}
		expected := append([]string(nil), r.Expected...)
		expected = append(expected, r.ExpectedImages...)
		if r.ExpectedImage != "" {
			expected = append(expected, r.ExpectedImage)
		}
		for j := range expected {
			expected[j] = strings.TrimSpace(expected[j])
		}
		expected = compactStrings(expected)
		if len(expected) == 0 {
			return nil, fmt.Errorf("eval case %d (%q) has no expected images", i+1, q)
		}
		cases = append(cases, evalCase{Query: q, Expected: expected})
	}
	return cases, nil
}

func compactStrings(xs []string) []string {
	out := xs[:0]
	seen := map[string]bool{}
	for _, x := range xs {
		if x == "" || seen[x] {
			continue
		}
		seen[x] = true
		out = append(out, x)
	}
	return out
}

type evalSummary struct {
	Scorer          scorer
	Queries         int
	KValues         []int
	RecallAt        map[int]float64
	PrecisionAt     map[int]float64
	MeanReciprocalR float64
}

func summarizeEval(sc scorer, cases []evalCase, results [][]hit, kValues []int) evalSummary {
	s := evalSummary{
		Scorer:      sc,
		Queries:     len(cases),
		KValues:     append([]int(nil), kValues...),
		RecallAt:    map[int]float64{},
		PrecisionAt: map[int]float64{},
	}
	if len(cases) == 0 {
		return s
	}
	for i, tc := range cases {
		hits := []hit(nil)
		if i < len(results) {
			hits = results[i]
		}
		for _, k := range kValues {
			relevant := countRelevantAtK(hits, tc.Expected, k)
			s.RecallAt[k] += float64(relevant) / float64(len(tc.Expected))
			s.PrecisionAt[k] += float64(relevant) / float64(k)
		}
		if rank := firstRelevantRank(hits, tc.Expected); rank > 0 {
			s.MeanReciprocalR += 1.0 / float64(rank)
		}
	}
	for _, k := range kValues {
		s.RecallAt[k] /= float64(len(cases))
		s.PrecisionAt[k] /= float64(len(cases))
	}
	s.MeanReciprocalR /= float64(len(cases))
	return s
}

func countRelevantAtK(hits []hit, expected []string, k int) int {
	if k > len(hits) {
		k = len(hits)
	}
	n := 0
	matchedExpected := map[string]bool{}
	for i := 0; i < k; i++ {
		if match := matchingExpected(hits[i], expected); match != "" && !matchedExpected[match] {
			matchedExpected[match] = true
			n++
		}
	}
	return n
}

func firstRelevantRank(hits []hit, expected []string) int {
	for i, h := range hits {
		if matchingExpected(h, expected) != "" {
			return i + 1
		}
	}
	return 0
}

func matchingExpected(h hit, expected []string) string {
	candidates := []string{h.Key, h.Path, filepath.Base(h.Path), filepath.Clean(h.Path)}
	for _, want := range expected {
		want = strings.TrimSpace(want)
		if want == "" {
			continue
		}
		cleanWant := filepath.Clean(want)
		baseWant := filepath.Base(want)
		for _, got := range candidates {
			if got == want || got == cleanWant || got == baseWant {
				return want
			}
		}
	}
	return ""
}

func evalKValues(topK int) []int {
	candidates := []int{1, 3, 5, topK}
	seen := map[int]bool{}
	var out []int
	for _, k := range candidates {
		if k > 0 && k <= topK && !seen[k] {
			seen[k] = true
			out = append(out, k)
		}
	}
	sort.Ints(out)
	return out
}

func cmdEval(project, setPath string, topK int, scorers []scorer) error {
	cases, err := loadEvalCases(setPath)
	if err != nil {
		return err
	}
	if len(cases) == 0 {
		return fmt.Errorf("hardset has no cases")
	}
	kValues := evalKValues(topK)
	fmt.Printf("hardset: %s\n", setPath)
	fmt.Printf("queries: %d\n", len(cases))
	fmt.Printf("top_k:   %d\n\n", topK)

	c, err := ensureSearchReady(project)
	if err != nil {
		return err
	}
	corpus, err := loadCorpus(project)
	if err != nil {
		return err
	}

	resultsByScorer := map[scorer][][]hit{}
	for _, sc := range scorers {
		resultsByScorer[sc] = make([][]hit, 0, len(cases))
	}
	jobs := make([]string, len(cases))
	for i, tc := range cases {
		jobs[i], err = submitQuery(project, c, tc.Query)
		if err != nil {
			return fmt.Errorf("query %q: %w", tc.Query, err)
		}
	}
	drainErr := drainWorker("embed", c.EmbedModel, embedWs(project), embedEnv(), "-w", "1")
	for i, tc := range cases {
		qvec, err := collectQuery(project, jobs[i])
		if err != nil {
			return fmt.Errorf("query %q: %w", tc.Query, errors.Join(drainErr, err))
		}
		baseHits, err := denseHits(corpus, qvec)
		if err != nil {
			return fmt.Errorf("query %q: %w", tc.Query, err)
		}
		for _, sc := range scorers {
			hits := scoreHits(baseHits, tc.Query, sc)
			if len(hits) > topK {
				hits = hits[:topK]
			}
			resultsByScorer[sc] = append(resultsByScorer[sc], hits)
		}
	}
	for _, sc := range scorers {
		printEvalSummary(summarizeEval(sc, cases, resultsByScorer[sc], kValues))
	}
	return nil
}

func printEvalSummary(s evalSummary) {
	fmt.Printf("scorer: %s\n", s.Scorer)
	for _, k := range s.KValues {
		fmt.Printf("  recall@%d:    %.3f\n", k, s.RecallAt[k])
		fmt.Printf("  precision@%d: %.3f\n", k, s.PrecisionAt[k])
	}
	fmt.Printf("  mrr:         %.3f\n\n", s.MeanReciprocalR)
}
