import importlib.machinery
import importlib.util
import unittest
from pathlib import Path


SCRIPT = Path(__file__).with_name("ask")
loader = importlib.machinery.SourceFileLoader("ask", str(SCRIPT))
spec = importlib.util.spec_from_loader(loader.name, loader)
ask = importlib.util.module_from_spec(spec)
loader.exec_module(ask)


class EvidenceTests(unittest.TestCase):
    def test_qmd_evidence_keeps_source_and_snippet(self):
        rows = [
            {"file": "qmd://notes/lab.md", "line": 12, "snippet": "Decode reads weights."},
            {"file": "qmd://notes/empty.md", "snippet": "  "},
        ]
        evidence = ask.format_qmd_rows(rows)
        self.assertEqual(evidence, ["[1] qmd://notes/lab.md:12\nDecode reads weights."])

    def test_qmd_evidence_ignores_invalid_rows(self):
        self.assertEqual(ask.format_qmd_rows([None, {"snippet": ""}]), [])

    def test_qmd_labels_only_count_results_with_snippets(self):
        rows = [None, {"snippet": ""}, {"file": "qmd://notes/a.md", "snippet": "A"}]
        self.assertTrue(ask.format_qmd_rows(rows)[0].startswith("[1] "))

    def test_image_citation_uses_ranked_result_filename(self):
        evidence = "## Result 1 — images/diagram.png (confident)\ntext"
        self.assertEqual(ask.image_citations(evidence), ["images/diagram.png"])

    def test_final_answer_always_lists_sources(self):
        self.assertEqual(ask.append_sources("Answer.\n", ["qmd://notes/a.md:4"]),
                         "Answer.\n\nSources:\n- qmd://notes/a.md:4")


if __name__ == "__main__":
    unittest.main()
