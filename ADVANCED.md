# Examples

These examples use the three commands directly. Install nrvna and get a compatible
model (see [INSTALL.md](INSTALL.md)) first. Run commands from the folder
that contains `models/`. Use a new workspace for each example.

Stop if submission or a drain fails. Inspect the workspace with `flw` before
continuing. Generated answers vary; check their content before using them.

## Submit a batch before loading the model

Submit two independent triage jobs. Each carries its own text:

```sh
wrk ./batch-workspace "Label as bug, billing, or account: I was charged twice this month." --tag triage
wrk ./batch-workspace "Label as bug, billing, or account: The app crashes on launch." --tag triage
flw ./batch-workspace --json
```

Both jobs are queued. Submission does not require a running model.

```sh
nrvnad ./batch-workspace ./models/your-model.gguf --drain
flw ./batch-workspace --tag triage --json
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
nrvnad ./live-workspace ./models/your-model.gguf
```

Leave that terminal open. In another terminal, submit work as it arrives:

```sh
wrk ./live-workspace "Label as urgent or routine: the production database is down." --tag triage
wrk ./live-workspace "Label as urgent or routine: update the footer copyright year." --tag triage
```

Each submission returns immediately after publishing its job. Continue other
work. When you need both results, wait for the tagged batch, then collect it:

```sh
flw ./live-workspace -W --tag triage
flw ./live-workspace --tag triage --json
```

The batch wait prints no results. Collection reports results and failures.
After collecting, stop the daemon you started:

```sh
nrvnad stop ./live-workspace
```

## Use a completed result in another job

Submit an extraction job and save its ID:

```sh
first=$(wrk ./followup-workspace "List the action items with owners: Priya ships the docs Wednesday; Raj migrates billing; the designer role is unassigned.")
nrvnad ./followup-workspace ./models/your-model.gguf --drain
items=$(flw ./followup-workspace "$first")
```

Continue only if retrieval succeeded. Check the result against the original
note. Then submit the next job with those items in the prompt:

```sh
wrk ./followup-workspace "Which of these action items has no owner? $items" --parent "$first" --tag followup
```

That job is queued independently. Process it when ready:

```sh
nrvnad ./followup-workspace ./models/your-model.gguf --drain
flw ./followup-workspace --tag followup --json
```

The prompt includes the earlier result. `--parent` records lineage only. It
neither reads the earlier result nor waits for it. A file path in a prompt does
not read the file; include the text you need, as shown.

An application can use the same sequence. It owns the dependency between stages
and must check each stage before submitting the next one. The job files remain
after the application exits. See [AGENTS.md](AGENTS.md) for the machine contract.
