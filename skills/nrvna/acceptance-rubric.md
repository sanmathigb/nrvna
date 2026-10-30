# Local findings acceptance rubric

Score each file against an independently prepared list of expected findings.
Do not show that list to the model.

For each expected finding, record `correct`, `missed`, or `incorrect`. Count a
finding as correct only when its claim is accurate and its cited source lines
support it.

Coverage is correct expected findings divided by all expected findings. Record
failed, unreadable, oversized, and inconclusive files as gaps; do not remove
them from the denominator.

Accept an answer only when it has full coverage, every claim has a valid source
reference, and it contains no incorrect or unsupported claims. Valid JSON is
required, but it does not count as semantic correctness.

For workflow comparisons, use the same source snapshot, file set, question,
acceptance key, and frontier model settings. Record frontier input, cached
input, and output usage; elapsed time; human interventions; gaps; and failures.
Record subagent model and concurrency. Report local inference time and hardware
separately from paid-model usage.
