# Why nrvna

Chat works well when a person is present for each turn. Many local inference
tasks do not need that interaction. A model can caption images, transcribe
audio, embed text, or process a batch of independent prompts in the background.

nrvna gives each task its own durable job. You can submit work before a model
starts, run it later, and read the saved result when you return. The workspace
keeps the job files. The model does not keep a conversation between jobs.

This is useful when the work matters more than a live reply: batch processing,
repeatable pipelines, and apps that need local inference as one step. nrvna
provides the job contract. The caller chooses the model, supplies context,
connects steps, and decides what to do with failures.
