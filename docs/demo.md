# Demo transcript

This is a representative transcript of what SkiffLLM feels like in a shell.
No captured numbers are used here; every speed, token count, and timing shown
in the real app is measured on your own machine.

## Pipe a diff into a review

```bash
$ git diff | llm "review these changes"
User: review these changes

HIGH   src/server.cpp:84  Possible unchecked input in request parsing.
MEDIUM src/server.cpp:142 Error response leaks implementation detail.
LOW    tests/test_main.cpp:32 Inconsistent assertion style.

Verdict: one blocking issue, otherwise reasonable. No credential exposure
detected.
```

## Summarize a file

```bash
$ cat README.md | llm "summarize this"
```

`--json` makes the output scriptable:

```bash
$ cat README.md | llm --json "summarize this"
{"text":"...","model":"...","prompt_tokens":...,"generated_tokens":...,"prompt_ms":...,"generation_ms":...,"tokens_per_second":...,"stopped":false}
```

## Ask about a whole repository

```bash
$ llm --project . "where is authentication handled?"
```

The prompt receives a real file index plus a bounded slice of source files.

## Model manager

```bash
$ llm model list
ID                NAME                    SIZE        RAM                  INSTALLED
-------------------------------------------------------------------------------------
qwen2.5-0.5b      Qwen2.5 0.5B Instruct   468.64 MiB  ~1 GB working set    no
...
```

## Privacy check

```bash
$ llm --doctor --network
  Core inference outbound: none (generation never connects)
  Explicit network uses:   model install (Hugging Face),
                           openai subcommand (user-chosen endpoint)
  Telemetry:               disabled
  Cloud APIs:              none
  History storage:         local
  Privacy status:          ✓ LOCAL-FIRST
```

If you want an animated recording, run `asciinema` or `agg` on these commands
and replace the code blocks above with a GIF.
