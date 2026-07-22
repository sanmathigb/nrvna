package main

import (
	"encoding/json"
	"errors"
	"fmt"
	"math"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
)

// Hybrid search over the current MVP index: cosine similarity over the
// combined caption+OCR embedding plus BM25 over the same combined text.
// The default scorer is RRF, promoted after the local hard set showed better
// top-1 and top-3 recall than the original 50/50 blend.

var tokenRe = regexp.MustCompile(`[a-z0-9_+.#/-]+`)

type scorer string

const (
	scorerSimple scorer = "simple"
	scorerRRF    scorer = "rrf"
	scorerDense  scorer = "dense"
	scorerBM25   scorer = "bm25"
)

func parseScorer(s string) (scorer, error) {
	switch scorer(strings.ToLower(s)) {
	case scorerSimple:
		return scorerSimple, nil
	case scorerRRF:
		return scorerRRF, nil
	case scorerDense:
		return scorerDense, nil
	case scorerBM25:
		return scorerBM25, nil
	default:
		return "", fmt.Errorf("unknown scorer %q (want simple, rrf, dense, or bm25)", s)
	}
}

func tokenize(s string) []string { return tokenRe.FindAllString(strings.ToLower(s), -1) }

func cosine(a, b []float64) float64 {
	if len(a) == 0 || len(b) == 0 {
		return 0
	}
	var dot, na, nb float64
	n := len(a)
	if len(b) < n {
		n = len(b)
	}
	for i := 0; i < n; i++ {
		dot += a[i] * b[i]
	}
	for _, x := range a {
		na += x * x
	}
	for _, x := range b {
		nb += x * x
	}
	if na == 0 || nb == 0 {
		return 0
	}
	return dot / (math.Sqrt(na) * math.Sqrt(nb))
}

func bm25(queryTokens, docTokens []string, avgDl float64, df map[string]int, n int) float64 {
	const k1, b = 1.5, 0.75
	if len(queryTokens) == 0 || len(docTokens) == 0 {
		return 0
	}
	tf := map[string]int{}
	for _, t := range docTokens {
		tf[t]++
	}
	dl := float64(len(docTokens))
	if avgDl < 1 {
		avgDl = 1
	}
	score := 0.0
	for _, term := range queryTokens {
		f, ok := tf[term]
		if !ok {
			continue
		}
		idf := math.Log((float64(n)-float64(df[term])+0.5)/(float64(df[term])+0.5) + 1.0)
		score += idf * (float64(f) * (k1 + 1)) / (float64(f) + k1*(1-b+b*dl/avgDl))
	}
	return score
}

// loadVector reads an embedding.json in any of the shapes the engine emits:
// a bare array, {"vector": [...]}, or {"embedding": {"vector": [...]}}.
func loadVector(path string) ([]float64, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	return parseVector(data)
}

func parseVector(data []byte) ([]float64, error) {
	var raw any
	if err := json.Unmarshal(data, &raw); err != nil {
		return nil, err
	}
	for i := 0; i < 3; i++ {
		switch v := raw.(type) {
		case []any:
			vec := make([]float64, 0, len(v))
			for _, x := range v {
				f, ok := x.(float64)
				if !ok {
					return nil, fmt.Errorf("non-numeric value in embedding vector")
				}
				vec = append(vec, f)
			}
			return vec, nil
		case map[string]any:
			if inner, ok := v["vector"]; ok {
				raw = inner
			} else if inner, ok := v["embedding"]; ok {
				raw = inner
			} else {
				return nil, fmt.Errorf("no vector found in embedding json")
			}
		default:
			return nil, fmt.Errorf("unexpected embedding json shape")
		}
	}
	return nil, fmt.Errorf("embedding json nested too deeply")
}

type hit struct {
	Key, Path, Text    string
	Vec                []float64
	Dense, B25, Score  float64
	DenseRank, B25Rank int
}

func submitQuery(project string, c config, query string) (string, error) {
	return submitStdin(embedWs(project), c.QueryPrefix+query, "-", "--embed")
}

func collectQuery(project, job string) ([]float64, error) {
	flw := binFlw()
	if flw == "" {
		return nil, fmt.Errorf("engine result binary not found")
	}
	out, err := exec.Command(flw, embedWs(project), job, "--json").Output()
	if err != nil {
		return nil, fmt.Errorf("collecting query embedding: %w", err)
	}
	qvec, err := parseVector(out)
	if err != nil {
		return nil, fmt.Errorf("query embedding: %w", err)
	}
	return qvec, nil
}

func embedQuery(project string, c config, query string) ([]float64, error) {
	job, err := submitQuery(project, c, query)
	if err != nil {
		return nil, err
	}
	drainErr := drainWorker("embed", c.EmbedModel, embedWs(project), embedEnv(), "-w", "1")
	qvec, collectErr := collectQuery(project, job)
	if collectErr != nil {
		return nil, errors.Join(drainErr, collectErr)
	}
	return qvec, nil
}

// loadCorpus reads the index and every hit's text and embedding once.
// Query-independent; eval reuses one corpus across all its queries.
func loadCorpus(project string) ([]hit, error) {
	idxData, err := os.ReadFile(indexFile(project))
	if err != nil {
		return nil, err
	}
	var hits []hit
	for i, line := range strings.Split(string(idxData), "\n") {
		if i == 0 || strings.TrimSpace(line) == "" {
			continue
		}
		f := strings.Split(line, "\t")
		if len(f) < 3 {
			continue
		}
		key, path := f[0], f[1]
		combined := filepath.Join(artifactsDir(project), key, "combined.md")
		embFile := filepath.Join(artifactsDir(project), key, "embedding.json")
		text, err := os.ReadFile(combined)
		if err != nil {
			return nil, fmt.Errorf("reading indexed text for %s: %w", path, err)
		}
		vec, err := loadVector(embFile)
		if err != nil {
			return nil, fmt.Errorf("reading indexed embedding for %s: %w", path, err)
		}
		hits = append(hits, hit{Key: key, Path: path, Text: string(text), Vec: vec})
	}
	return hits, nil
}

func denseHits(corpus []hit, qvec []float64) ([]hit, error) {
	hits := append([]hit(nil), corpus...)
	for i := range hits {
		if len(hits[i].Vec) != len(qvec) {
			return nil, fmt.Errorf("embedding dimension changed (%d query vs %d index for %s); reindex the project", len(qvec), len(hits[i].Vec), hits[i].Path)
		}
		hits[i].Dense = cosine(qvec, hits[i].Vec)
	}
	return hits, nil
}

func scoreHits(hits []hit, query string, sc scorer) []hit {
	hits = append([]hit(nil), hits...)
	for i := range hits {
		hits[i].Score = 0
		hits[i].DenseRank = 0
		hits[i].B25Rank = 0
	}
	computeBM25(hits, query)
	assignRanks(hits)
	switch sc {
	case scorerDense:
		for i := range hits {
			hits[i].Score = hits[i].Dense
		}
	case scorerBM25:
		for i := range hits {
			hits[i].Score = hits[i].B25
		}
	case scorerRRF:
		applyRRF(hits)
	case scorerSimple:
		applySimpleBlend(hits)
	}
	sort.Slice(hits, func(i, j int) bool {
		if hits[i].Score == hits[j].Score {
			return hits[i].Path < hits[j].Path
		}
		return hits[i].Score > hits[j].Score
	})
	return hits
}

func computeBM25(hits []hit, query string) {
	df := map[string]int{}
	totalLen := 0
	docTokens := make([][]string, len(hits))
	for i := range hits {
		docTokens[i] = tokenize(hits[i].Text)
		totalLen += len(docTokens[i])
		seen := map[string]bool{}
		for _, t := range docTokens[i] {
			if !seen[t] {
				df[t]++
				seen[t] = true
			}
		}
	}
	avgDl := 1.0
	if len(hits) > 0 {
		avgDl = float64(totalLen) / float64(len(hits))
	}
	qt := tokenize(query)
	for i := range hits {
		hits[i].B25 = bm25(qt, docTokens[i], avgDl, df, len(hits))
	}
}

func assignRanks(hits []hit) {
	denseOrder := make([]int, len(hits))
	bm25Order := make([]int, len(hits))
	for i := range hits {
		denseOrder[i] = i
		bm25Order[i] = i
	}
	sort.Slice(denseOrder, func(i, j int) bool {
		a, b := hits[denseOrder[i]], hits[denseOrder[j]]
		if a.Dense == b.Dense {
			return a.Path < b.Path
		}
		return a.Dense > b.Dense
	})
	sort.Slice(bm25Order, func(i, j int) bool {
		a, b := hits[bm25Order[i]], hits[bm25Order[j]]
		if a.B25 == b.B25 {
			return a.Path < b.Path
		}
		return a.B25 > b.B25
	})
	for rank, idx := range denseOrder {
		hits[idx].DenseRank = rank + 1
	}
	for rank, idx := range bm25Order {
		if hits[idx].B25 > 0 {
			hits[idx].B25Rank = rank + 1
		}
	}
}

func applySimpleBlend(hits []hit) {
	maxB := 0.0
	for i := range hits {
		if hits[i].B25 > maxB {
			maxB = hits[i].B25
		}
	}
	for i := range hits {
		bNorm := 0.0
		if maxB > 0 {
			bNorm = hits[i].B25 / maxB
		}
		hits[i].Score = 0.5*hits[i].Dense + 0.5*bNorm
	}
}

func applyRRF(hits []hit) {
	const rrfK = 60.0
	const denseWeight = 1.0
	const bm25Weight = 1.0
	for i := range hits {
		hits[i].Score = 0
		if hits[i].DenseRank > 0 {
			hits[i].Score += denseWeight / (rrfK + float64(hits[i].DenseRank))
		}
		if hits[i].B25Rank > 0 {
			hits[i].Score += bm25Weight / (rrfK + float64(hits[i].B25Rank))
		}
	}
}

// ensureSearchReady does query-independent setup without driving indexing.
func ensureSearchReady(project string) (config, error) {
	pr, err := readProgress(project)
	if err != nil {
		return config{}, err
	}
	if pr.Indexed == 0 {
		return config{}, fmt.Errorf("nothing indexed yet; indexing may still be in progress")
	}
	c := loadConfig(project)
	if missing := checkModels(c); len(missing) > 0 {
		return config{}, fmt.Errorf("missing models:\n  %s", strings.Join(missing, "\n  "))
	}
	return c, nil
}

func searchBaseHits(project, query string) ([]hit, error) {
	c, err := ensureSearchReady(project)
	if err != nil {
		return nil, err
	}
	qvec, err := embedQuery(project, c, query)
	if err != nil {
		return nil, err
	}
	corpus, err := loadCorpus(project)
	if err != nil {
		return nil, err
	}
	return denseHits(corpus, qvec)
}

func searchProject(project, query string, topN int, sc scorer) ([]hit, error) {
	hits, err := searchBaseHits(project, query)
	if err != nil {
		return nil, err
	}
	hits = scoreHits(hits, query, sc)
	if len(hits) > topN {
		hits = hits[:topN]
	}
	return hits, nil
}

func cmdSearch(project, query string, topN int, sc scorer) error {
	hits, err := searchProject(project, query, topN, sc)
	if err != nil {
		return err
	}

	var md strings.Builder
	fmt.Fprintf(&md, "# Search: %s\n\nScorer: `%s`\n\n", query, sc)
	for i, h := range hits {
		snippet := strings.Join(strings.Fields(h.Text), " ")
		snippet = truncateAtWord(snippet, 320)
		// Terminal shows what the user acts on: rank, path, evidence.
		// Scores and per-channel ranks stay in search-results.md below.
		fmt.Printf("%d  %s\n   %s\n\n", i+1, h.Path, snippet)
		fmt.Fprintf(&md, "## %d. %s\n\nScore: `%.3f`  Dense: `%.3f`  Dense rank: `%d`  BM25: `%.3f`  BM25 rank: `%d`\n\nImage: `%s`\n\n[![](%s)](%s)\n\n> %s\n\n",
			i+1, filepath.Base(h.Path), h.Score, h.Dense, h.DenseRank, h.B25, h.B25Rank, h.Path, h.Path, h.Path, snippet)
	}
	outMd := filepath.Join(project, "search-results.md")
	if err := os.WriteFile(outMd, []byte(md.String()), 0o644); err != nil {
		return err
	}
	note("wrote %s", outMd)
	return nil
}
