package main

import (
	"encoding/json"
	"fmt"
	"math"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
)

// Hybrid search: cosine similarity over embeddings fused 50/50 with
// BM25 over the combined caption+OCR text. Faithful port of the validated
// scorer (k1=1.5, b=0.75, idf log((N-df+0.5)/(df+0.5)+1), min-max BM25 norm).

var tokenRe = regexp.MustCompile(`[a-z0-9_+.#/-]+`)

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
	Key, Path, Text  string
	Dense, B25, Score float64
}

func cmdSearch(project, query string, topN int) error {
	pr, err := advance(project, false)
	if err != nil {
		return err
	}
	if pr.Indexed == 0 {
		return fmt.Errorf("nothing indexed yet; run 'imgsrch status %s' and try again later", project)
	}
	c := loadConfig(project)
	if missing := checkModels(c); len(missing) > 0 {
		return fmt.Errorf("missing models:\n  %s", strings.Join(missing, "\n  "))
	}
	if err := startEmbed(project, c); err != nil {
		return err
	}

	// Embed the query through the same model that embedded the documents.
	job, err := submitStdin(embedWs(project), c.QueryPrefix+query, "-", "--embed")
	if err != nil {
		return err
	}
	flw := binFlw()
	if flw == "" {
		return fmt.Errorf("engine result binary not found")
	}
	out, err := exec.Command(flw, embedWs(project), "-w", job, "--json").Output()
	if err != nil {
		return fmt.Errorf("waiting for query embedding: %w", err)
	}
	qvec, err := parseVector(out)
	if err != nil {
		return fmt.Errorf("query embedding: %w", err)
	}

	// Score every indexed document.
	idxData, err := os.ReadFile(indexFile(project))
	if err != nil {
		return err
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
			continue
		}
		vec, err := loadVector(embFile)
		if err != nil {
			continue
		}
		hits = append(hits, hit{Key: key, Path: path, Text: string(text), Dense: cosine(qvec, vec)})
	}

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
	maxB := 0.0
	for i := range hits {
		hits[i].B25 = bm25(qt, docTokens[i], avgDl, df, len(hits))
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
	sort.Slice(hits, func(i, j int) bool { return hits[i].Score > hits[j].Score })
	if len(hits) > topN {
		hits = hits[:topN]
	}

	// Print to stdout and write search-results.md.
	var md strings.Builder
	fmt.Fprintf(&md, "# Search: %s\n\n", query)
	for i, h := range hits {
		snippet := strings.Join(strings.Fields(h.Text), " ")
		if len(snippet) > 320 {
			cut := snippet[:320]
			if j := strings.LastIndex(cut, " "); j > 0 {
				cut = cut[:j]
			}
			snippet = cut + "..."
		}
		fmt.Printf("[%.3f] %s  (dense=%.3f bm25=%.3f)\n%s\n\n", h.Score, h.Path, h.Dense, h.B25, snippet)
		fmt.Fprintf(&md, "## %d. %s\n\nScore: `%.3f`  Dense: `%.3f`  BM25: `%.3f`\n\nImage: `%s`\n\n![](%s)\n\n> %s\n\n",
			i+1, filepath.Base(h.Path), h.Score, h.Dense, h.B25, h.Path, h.Path, snippet)
	}
	outMd := filepath.Join(project, "search-results.md")
	if err := os.WriteFile(outMd, []byte(md.String()), 0o644); err != nil {
		return err
	}
	note("wrote %s", outMd)
	return nil
}
