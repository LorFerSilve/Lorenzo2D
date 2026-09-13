from pathlib import Path

path = Path("docs/post-1.0-roadmap.md")
text = path.read_text(encoding="utf-8")

start = text.index("- **13.4 Asset browser/picking foundation:**")
end = text.index("\n\n### Initial editor scope", start)
section = text[start:end]
status_start = section.index("- **Phase 13 status:")
prefix = section[:status_start]

phase_13_5 = """- **13.5 Bounded tilemap authoring foundation:** the standalone editor now provides an editor-only\n  `TilemapAuthoringModel` over the installed public `TileMapData` and `TiledJsonImporter` contracts.\n  Tiled JSON publication is transactional and bounded by explicit input-byte, map-cell, layer, tile\n  definition, object, and aggregate tile-slot limits. Layer selection, brush selection, and the tile\n  palette are deterministic; tile IDs and Tiled horizontal/vertical/diagonal flip flags are validated\n  through the existing runtime tilemap contract. Single-cell paint/erase and continuous paint\n  gestures use bounded cell-delta history rather than whole-map snapshots, so a successful stroke is\n  one undoable command while cancellation restores the gesture-start state and preserves pre-existing\n  redo history. The slice deliberately adds no editor-private serializer or second tilemap persistence\n  format: Tiled import remains the canonical content boundary until a future public persistence/export\n  contract exists. Focused installed-package regressions cover import publication, workload limits,\n  deterministic selection/palette behavior, painting, erase, coalesced strokes, cancellation,\n  undo/redo, redo preservation, and history eviction. PR #53 and the merged `master` implementation\n  commit `0522fcf34d6a919444644e48cc353f610810df7f` both pass all seven required CI gates, including\n  Windows MSVC, ASan/UBSan, coverage, clang-tidy, and the installed-package editor consumer path.\n"""

status = """- **Phase 13 status: in progress.** Slices 13.1 through 13.5 establish the editor/runtime dependency\n  boundary, document/history foundation, interactive hierarchy, validated component inspection,\n  coalesced viewport translation, validated bounded asset browsing/picking, and bounded tilemap\n  authoring with transactional Tiled import and gesture-coalesced cell-delta history. Collider and\n  navigation visualization/editing, animation preview, play/test workflow, richer component-specific\n  controls, rotation/scale gizmos, and the end-to-end authoring tutorial remain future Phase 13\n  slices. The next planned dependency is **Phase 13.6 — collider/navigation visualization and editing\n  foundation**.\n"""

updated = text[:start] + prefix + phase_13_5 + status + text[end:]
path.write_text(updated, encoding="utf-8")
