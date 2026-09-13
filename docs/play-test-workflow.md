# Play/test workflow foundation

Phase 13.8 adds a bounded editor-only orchestration layer for launching the current authoring state in an external Lorenzo2D game/test executable without introducing editor dependencies into runtime modules.

## Boundary

`PlayTestWorkflowModel` lives in the standalone `tools/editor` project. It consumes `EditorDocument`, which already serializes through the installed public `LevelSerializer` contract. The model does not add a second level format and does not require the engine package to know about editor process management.

Platform/application process ownership is deliberately injected through `PlayTestProcessHooks`. The editor workflow prepares a validated `PlayTestLaunchRequest`; a host-specific launcher owns process creation and termination. This keeps shell quoting, Windows process handles, POSIX process groups, terminal inheritance, and IDE-specific launch behavior outside the portable editor model instead of hiding them behind `std::system`.

## Session contract

A configured session contains:

- an external executable path;
- a working directory;
- at most 32 command-line arguments and 4 KiB of aggregate argument data;
- exactly one `{level}` placeholder occurrence across the complete argument vector.

Starting a session:

1. rejects concurrent play/test sessions on the same workflow model;
2. creates the caller-provided snapshot root when necessary;
3. atomically claims a workflow-owned per-session subdirectory beneath that root, so separate editor/workflow instances cannot overwrite or delete one another's snapshots;
4. serializes the current `EditorDocument` to a runtime-readable temporary level snapshot inside that owned directory;
5. publishes the snapshot as an absolute path and substitutes that absolute path for `{level}`;
6. publishes the launch request only if the injected launcher accepts it.

If launch is rejected, the workflow removes its owned session directory and no active session is published. While a session is active, its configuration cannot be mutated and another session cannot start on that workflow object.

`PlayTestWorkflowModel` is deliberately non-copyable and non-movable. An active model owns one process-control hook and one snapshot directory; duplicating or implicitly transferring that state would permit double-stop or cross-session cleanup bugs. Hosts should keep a stable workflow-model instance for the lifetime of a play/test session.

Stopping delegates termination to the injected stop hook before removing the workflow-owned session directory. A failed stop preserves the active session and snapshot so the editor does not claim termination or destroy input while the external process may still be running.

## Deliberate non-goals

This foundation does not define a universal game executable, command-line convention, subprocess library, hot reload, runtime-to-editor IPC, debugger attachment, process-output console, or automatic asset cooking. Phase 14 owns the future content pipeline; later editor work may add concrete platform launch adapters and richer shell controls on top of this model.

The temporary level remains the canonical Lorenzo2D runtime level format, so a play/test launch tests the current unsaved authoring state without forcing the user's working document to be overwritten.

## Validation

`Lorenzo2DEditorPlayTestWorkflowTests` is built and executed against the installed Lorenzo2D package. Regressions cover configuration limits, exact placeholder validation, absolute launch paths, distinct ownership for concurrent workflows sharing one snapshot root, runtime-readable snapshot publication, launch rollback and cleanup, active-session exclusivity, non-copyable/non-movable ownership, configuration immutability while active, stop delegation, and snapshot cleanup.
