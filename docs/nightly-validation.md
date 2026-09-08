# Nightly extended validation

Phase 11.7 moves expensive stress, soak, sanitizer, fuzz, and trend collection out of normal
pull-request CI while keeping them repeatable and bounded.

The workflow is `.github/workflows/nightly-extended.yml`.

## Schedule

GitHub Actions runs the workflow from the default branch every day at:

- `02:17 UTC` via cron `17 2 * * *`.

The non-round minute is intentional so the workflow does not join the common top-of-hour scheduled
load. GitHub scheduled workflows use UTC. The workflow can also be started manually with
`workflow_dispatch`.

Normal pull requests do **not** execute the expensive validation jobs. When this workflow file or
this document changes, a lightweight pull-request job runs so GitHub parses the workflow and verifies
that the scheduled validation sources are present.

## Jobs

### Release stress, soak, and trend snapshot

The Release job builds only the required stress and benchmark targets, then runs:

1. `Lorenzo2DStressValidation --profile standard`;
2. `Lorenzo2DStressValidation --profile soak`;
3. `Lorenzo2DBenchmarks` with JSON and CSV output.

The stress reports are correctness gates. Crashes, invariant failures, deterministic replay
divergence, handle/counter drift, failed persistence, or other stress-runner failures fail the job.

The benchmark executable must run successfully, but its wall-clock values are **not** compared
against a pass/fail threshold. The JSON/CSV output is retained as trend evidence so regressions can
be investigated across runs without treating noisy hosted-runner timing as a correctness contract.

### ASan/UBSan standard stress

A separate Clang Debug build enables AddressSanitizer and UndefinedBehaviorSanitizer and executes the
full `standard` stress profile.

The sanitizer job uses the same strict options as pull-request CI:

- leak detection enabled;
- halt on the first ASan error;
- initialization-order checking;
- UBSan stack traces;
- halt on the first undefined-behavior report.

The full soak profile is intentionally kept in the Release job rather than duplicated under
sanitizers. The standard profile already reaches the representative 10k+ tile, 1k+ renderable,
500+ collider, and 250+ navigation-client scale while keeping nightly sanitizer runtime bounded.

### Extended deterministic fuzz

The fuzz job builds `Lorenzo2DFuzzValidation` in Release and runs all registered scenarios with
2,048 cases per scenario for each of four fixed seeds:

- `0x4c324446555a5a31`;
- `0x9e3779b97f4a7c15`;
- `0xd1b54a32d192ed03`;
- `0x94d049bb133111eb`.

That is 8,192 deterministic cases per scenario per nightly run while staying below the runner's
hard 4,096-case-per-invocation bound. Failures remain reproducible from the retained seed/case log.

## Artifacts

Every heavy job uploads diagnostic artifacts even when its validation step fails.

Release artifacts contain:

- workflow/commit metadata;
- standard stress JSON and console log;
- soak stress JSON and console log;
- benchmark JSON;
- benchmark CSV;
- benchmark console log.

Sanitizer artifacts contain the standard stress JSON/log and provenance metadata.

Fuzz artifacts contain one log per fixed seed plus provenance metadata.

Artifacts are retained for 21 days. They are diagnostic evidence, not a permanent release archive.

## Security and repository permissions

The workflow declares only:

```yaml
permissions:
  contents: read
```

It needs no repository secrets and cannot write source, releases, issues, or pull requests through
the implicit `GITHUB_TOKEN`. Artifact upload is handled by the Actions service for the current run.

Checkout and artifact-upload actions are pinned to immutable commit SHAs, matching the repository's
existing CI hardening policy.

## Failure policy

A nightly run fails when:

- the Release standard or soak stress profile fails;
- the benchmark executable itself cannot complete or emit its reports;
- ASan/UBSan standard stress fails;
- any fixed-seed extended fuzz pass fails.

A nightly run does **not** fail because a benchmark took a particular number of milliseconds.
Performance timing should only become blocking after stable hardware-specific variance and an
explicit budget have been established.

## Relationship to pull-request CI

The existing seven required CI gates remain the merge contract for ordinary changes. Nightly
validation adds depth rather than making every pull request pay the cost of multi-hour-equivalent
simulation, repeated larger fuzz passes, and trend collection.

When a nightly failure occurs, reproduce the relevant command locally from the retained artifact
before changing a correctness contract or weakening a test.
