# Examples

These examples use the three commands directly. Install nrvna and download the
example model from [INSTALL.md](INSTALL.md) first. Run commands from the folder
that contains `models/`. Use a new workspace for each example.

Stop if submission or a drain fails. Inspect the workspace with `flw` before
continuing. Generated answers vary; check their content before using them.

## Submit a batch before loading the model

Submit two requests about the same report. Each receives its own evidence:

```sh
wrk ./batch-workspace "Summarize: Sales rose 8%. Costs rose 3%." --tag report
wrk ./batch-workspace "Name one unanswered question: Sales rose 8%. Costs rose 3%." --tag report
flw ./batch-workspace --json
```

Both jobs are queued. Submission does not require a running model.

```sh
nrvnad ./batch-workspace ./models/smollm2-1.7b.gguf --workers 1 --drain
flw ./batch-workspace --tag report --json
```

The daemon loads once, processes both jobs, then exits. Collection emits one
JSON object per line. Tags select jobs; they do not share context.

## Submit to a running daemon and collect later

In the first terminal, check daemon state:

```sh
nrvnad status ./live-workspace
```

Start a daemon only if status reports that none is running. Exit code `1`
means not running; `0` means ready; `2` means starting.

```sh
nrvnad ./live-workspace ./models/smollm2-1.7b.gguf --workers 1
```

Leave that terminal open. In another terminal, submit work:

```sh
wrk ./live-workspace "Extract the deadline: Please send the draft by Friday." --tag deadlines
wrk ./live-workspace "Extract the deadline: Review comments are due Monday." --tag deadlines
```

Each submission returns immediately after publishing its job. Continue other
work. When you need both results, wait for the tagged batch, then collect it:

```sh
flw ./live-workspace -W --tag deadlines
flw ./live-workspace --tag deadlines --json
```

The batch wait prints no results. Collection reports results and failures.
After collecting, stop the daemon you started:

```sh
nrvnad stop ./live-workspace
```

## Use a completed result in another job

Submit a fact extraction job and save its ID:

```sh
first=$(wrk ./followup-workspace "Extract the facts: Sales rose 8%. Costs rose 3%.")
nrvnad ./followup-workspace ./models/smollm2-1.7b.gguf --workers 1 --drain
flw ./followup-workspace "$first" > facts.txt
```

Continue only if retrieval succeeded. Read `facts.txt` and check it against the
original evidence. Then submit the next job with those facts:

```sh
{ printf '%s\n' 'Name one unanswered question about these facts:'; cat facts.txt; } | wrk ./followup-workspace - --parent "$first" --tag followup
```

That job is queued independently. Process it when ready:

```sh
nrvnad ./followup-workspace ./models/smollm2-1.7b.gguf --workers 1 --drain
flw ./followup-workspace --tag followup --json
```

The pipe supplies context. `--parent` records lineage only. It neither reads
the earlier result nor waits for it. A file path in a prompt does not read the
file; pipe its contents to `wrk` as shown above.

An application can use the same sequence. It owns the dependency between stages
and must check each stage before submitting the next one. The job files remain
after the application exits. See [AGENTS.md](AGENTS.md) for the machine contract.
