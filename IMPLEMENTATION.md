# bedbug — Step-by-Step Implementation Plan

Execution companion to [`PLAN.md`](PLAN.md). `PLAN.md` is the *what and why*; this file is
the *in what order and how do I know it's done*.

**Status:** not started. Prerequisite: PLAN.md §1 decisions are settled.

---

## How to use this document

- Work milestones **M0 → M9 in order**. M0–M2 are fully headless; the riskiest logic
  (type mapping, FK ordering, JSON round-trip) is proven before any pixel is drawn.
- Each step lists **Do**, **Files**, and **Done when**. A step is finished only when its
  *Done when* check passes.
- Commit at the end of every step (or every few small steps) so history stays bisectable.
- Nothing in M0–M9 is optional for the thin slice. Everything after M9 is.

**Global definition of done for any step**

1. Builds clean with no new warnings.
2. `ctest` green.
3. Relevant new logic is covered by a test (core + sqlgen: mandatory).
4. The step's own *Done when* check passes.

**Standard commands**

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/src/app/bedbug            # run the app
```

---

## M0 — Toolchain and skeleton

> Goal: prove Qt6 builds and an empty window launches. No product logic.

### 0.1 Install Qt6 development packages
- **Do:** `sudo dnf install qt6-qtbase-devel qt6-qtsvg-devel`.
- **Done when:** `qmake6 --version` works and `/usr/include/qt6/QtWidgets` exists.

### 0.2 Top-level CMake project
- **Do:** create `CMakeLists.txt` with `cmake_minimum_required(3.28)`,
  `project(bedbug LANGUAGES CXX)`, `find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets
  PrintSupport Sql Test)`, `qt_standard_project_setup()`.
- **Files:** `CMakeLists.txt`.
- **Done when:** `cmake -S . -B build -G Ninja` configures with no errors.

### 0.3 `bb_core` static library + empty app
- **Do:** add `src/core/CMakeLists.txt` (one placeholder `.cpp`) and
  `src/app/CMakeLists.txt` with an executable that creates an empty `QMainWindow` and
  shows it. Set `AUTOMOC ON`.
- **Files:** `src/core/CMakeLists.txt`, `src/core/Version.{h,cpp}`,
  `src/app/CMakeLists.txt`, `src/app/main.cpp`, `src/app/MainWindow.{h,cpp}`.
- **Done when:** `./build/src/app/bedbug` opens a window that can be closed.

### 0.4 Test wiring
- **Do:** add `enable_testing()`, a `tests/` tree, one trivial `QTest` case, and register
  it with `add_test`.
- **Files:** `tests/CMakeLists.txt`, `tests/core/tst_smoke.cpp`.
- **Done when:** `ctest --test-dir build` reports 1 passing test.

---

## M1 — Core model and JSON persistence

> Goal: the schema model exists, is engine-neutral, and survives a save/load round trip.

### 1.1 Identity and primitive types
- **Do:** `Id` (wrapping `QUuid`, with `isNull()`, `toString()`, `fromString()`);
  `CanonicalType` with the 16 `Kind` values plus `length`, `precision`, `scale`,
  `enumValues`; a `dialectOverrides` map and a `DialectHint` struct on the column.
- **Files:** `src/core/Id.h`, `src/core/CanonicalType.{h,cpp}`.
- **Done when:** a unit test constructs every `CanonicalType::Kind` and compares equality.

### 1.2 Entities, columns, indexes, relationships, model
- **Do:** the structs from PLAN.md §4, with `operator==` for each (needed by the round-trip
  test) and a `Model` holding `formatVersion` and `preferredDialect`.
- **Files:** `src/core/Column.h`, `src/core/Entity.h`, `src/core/Index.h`,
  `src/core/Relationship.h`, `src/core/Model.h`.
- **Done when:** `bb_core` builds and a test can assemble a two-table model in memory.

### 1.3 Domain helpers
- **Do:** `Model::findEntity(id)`, `Entity::findColumn(id)`, `Model::relationshipsFor(entity)`,
  and equality helpers. Keep these pure and header-visible where cheap.
- **Files:** `src/core/Model.{h,cpp}`.
- **Done when:** tests cover lookup of present and absent ids.

### 1.4 JSON serialization and parsing
- **Do:** `JsonSerializer::toJson(Model)` / `fromJson(QJsonDocument)` covering every field
  in PLAN.md §5, including `diagramOverrides`/`nativeType` and diagrams/nodes.
- **Files:** `src/core/JsonSerializer.{h,cpp}`.
- **Done when:** tests cover well-formed round trip and malformed/missing-field input.

### 1.5 Round-trip and version tests
- **Do:** build a fixture model exercising every type kind, nullability, PK/unique,
  auto-increment, a composite FK, and two diagrams. Assert `fromJson(toJson(m)) == m`.
  Assert unknown `formatVersion` is rejected with a clear error.
- **Files:** `tests/core/tst_model_json.cpp`, `tests/fixtures/sample_model.json`.
- **Done when:** `ctest` passes both.

---

## M2 — SQL generation (PostgreSQL) with golden tests

> Goal: correct DDL from the model, proven headlessly. This is the highest-value test suite.

### 2.1 Dialect interface and type map skeleton
- **Do:** `SqlDialect` abstract class (PLAN.md §6) and `TypeMap::map(canonical, dialect)`
  returning `TypeMapping { sqlType, fidelity, note, suggestedOverride }`.
- **Files:** `src/sqlgen/SqlDialect.h`, `src/sqlgen/TypeMap.{h,cpp}`.
- **Done when:** `bb_sqlgen` builds and links against `bb_core`.

### 2.2 PostgreSQL dialect
- **Do:** `quoteIdent` (double quotes + escaping), `mapType` for all 16 kinds,
  `columnDefinition` (PK, unique, not null, default, identity), `dropTable`.
- **Files:** `src/sqlgen/dialects/Postgres.{h,cpp}`, register in a dialect factory.
- **Done when:** unit tests assert the mapping for every `Kind`, including
  auto-increment → `GENERATED BY DEFAULT AS IDENTITY`.

### 2.3 Generator: ordering, tables, indexes
- **Do:** `SqlGenerator::generate(model, dialect, options)` implementing: preamble, `CREATE
  TABLE` in topological order, unique/plain `CREATE INDEX`. Implement `SqlGenOptions`
  (`emitDrop`, `ifExists`, `wrapInTransaction`, `emitIndexes`, `qualifySchema`,
  `includeComments`).
- **Files:** `src/sqlgen/SqlGenerator.{h,cpp}`.
- **Done when:** a two-table parent/child fixture emits the child's `CREATE TABLE` after
  the parent's.

### 2.4 Foreign keys and cycle handling
- **Do:** emit FKs as `ALTER TABLE ... ADD CONSTRAINT`; detect cycles and defer the
  back-edge. This is required even in M2 so cycles never produce invalid SQL.
- **Files:** `src/sqlgen/SqlGenerator.cpp`.
- **Done when:** a deliberately cyclic three-table fixture generates SQL with no
  forward reference.

### 2.5 Golden-file test harness
- **Do:** a test helper that loads `tests/golden/<dialect>/<case>.model.json`, generates,
  and byte-compares against `<case>.sql`; prints a unified diff on failure. Provide an
  env-var/flag to regenerate fixtures.
- **Files:** `tests/sqlgen/tst_postgres.cpp`, `tests/golden/postgres/*.sql`,
  `tests/golden/postgres/*.model.json`.
- **Done when:** at least three golden cases (`simple`, `composite_fk`, `cyclic`) pass, and
  the diff output is readable when deliberately broken.

---

## M3 — Canvas basics: entities on screen

> Goal: create, move, and delete tables visually. First user-visible value.

### 3.1 Scene and view scaffolding
- **Do:** `DiagramScene : QGraphicsScene` and `DiagramView : QGraphicsView` with a grid
  background, rubber-band selection, zoom (wheel + Ctrl), pan (space/middle drag), and
  snap-to-grid toggle.
- **Files:** `src/canvas/DiagramScene.{h,cpp}`, `src/canvas/DiagramView.{h,cpp}`.
- **Done when:** an empty grid renders and zooms/pans.

### 3.2 `EntityItem`
- **Do:** paint a header bar (table name) and one row per column (PK/FK badge, name, type,
  `NOT NULL`). Flags `Movable | Selectable | SendsGeometryChanges`. Size derives from
  content; expose `rowAt(y)` and `portFor(columnId)` helpers.
- **Files:** `src/canvas/EntityItem.{h,cpp}`.
- **Done when:** an entity added programmatically displays its columns with correct sizes.

### 3.3 Scene ↔ model binding
- **Do:** `DiagramScene::syncFrom(const Model&, const Diagram&)` and an
  `entityId -> EntityItem` map. `ItemPositionChange` writes back to the diagram node.
- **Files:** `src/canvas/DiagramScene.cpp`.
- **Done when:** moving an `EntityItem` updates the in-memory diagram node position.

### 3.4 Shell integration
- **Do:** `MainWindow` hosts the `DiagramView`, a menu/toolbar, and a stubbed
  `DocumentController` that owns a `Model` with one empty diagram. Add-entity creates an
  `Entity` and refreshes the scene.
- **Files:** `src/app/MainWindow.{h,cpp}`, `src/document/DocumentController.{h,cpp}`.
- **Done when:** the user can add a table, drag it, and see it persist across a scene
  refresh.

---

## M4 — Relationships and crow's-foot edges

> Goal: draw FKs visually. The domain's genuinely hard part.

### 4.1 `CrowsFootPainter`
- **Do:** `paint(painter, anchor, direction, cardinality, optional)` — "many" as three
  diverging strokes, "one" as a perpendicular tick, optional as a circle, mandatory as a
  double tick. Unit-testable by rendering to a small `QImage`.
- **Files:** `src/canvas/CrowsFootPainter.{h,cpp}`, `tests/canvas/tst_crowsfoot.cpp`.
- **Done when:** snapshot renders for one/many × optional/mandatory all match expectations.

### 4.2 `RelationshipItem`
- **Do:** orthogonal polyline route from child port to parent port, stepping around entity
  bounding boxes; crow's-foot at both ends; selectable.
- **Files:** `src/canvas/RelationshipItem.{h,cpp}`.
- **Done when:** a relationship between two placed tables draws with markers at both ends
  and re-routes when either box moves.

### 4.3 Link mode
- **Do:** a scene mode where clicking a source column then a target column creates a
  `Relationship` (and the implied FK) and re-renders the edge. Esc cancels.
- **Files:** `src/canvas/DiagramScene.cpp`, `src/app/MainWindow.cpp`.
- **Done when:** the user can create an FK by clicking two columns, and it appears as an edge.

### 4.4 Deletion and cascade rules
- **Do:** deleting an entity removes its attached relationships; deleting a column removes
  relationships that reference it; guard against dangling ids.
- **Files:** `src/document/` + `src/canvas/DiagramScene.cpp`.
- **Done when:** tests assert no relationship references a missing entity/column after
  deletion.

---

## M5 — Undo/redo

> Goal: every mutation is reversible; nothing bypasses the stack.

### 5.1 Command infrastructure
- **Do:** `DocumentController` owns a `QUndoStack`; `cleanChanged` drives the window's
  modified indicator. Define the command set from PLAN.md §9.
- **Files:** `src/document/DocumentController.{h,cpp}`, `src/document/commands/*.{h,cpp}`.
- **Done when:** a stubbed command increments/decrements the stack predictably.

### 5.2 Route all existing mutations through commands
- **Do:** convert M3/M4 add / remove / rename / move / create-relationship paths to
  commands. `MoveEntityNode` must **merge** consecutive drags into one undo step.
- **Files:** the `commands/` tree and call sites in `MainWindow`/`DiagramScene`.
- **Done when:** Ctrl+Z after adding a table removes it; Ctrl+Z after a drag returns the box
  in one step (not one per pixel).

### 5.3 Redo and re-entrancy tests
- **Do:** tests that apply a chain of commands, undo all, redo all, and compare the model to
  the original.
- **Files:** `tests/document/tst_undo.cpp`.
- **Done when:** undo-all-then-redo-all yields an identical model, and the stack is clean.

---

## M6 — File lifecycle

> Goal: the document is real — open, save, dirty tracking.

### 6.1 JSON document I/O
- **Do:** `DocumentController::load(path)` / `save(path)` / `saveAs()` using
  `JsonSerializer`; track `filePath`, `isModified`.
- **Files:** `src/document/DocumentController.cpp`.
- **Done when:** round-tripping through disk preserves model and diagram positions.

### 6.2 Menus, shortcuts, and prompts
- **Do:** File → New / Open / Save / Save-As with `Ctrl+N/O/S/Shift+S`; the
  "save before close?" prompt on modified documents; window title shows name + modified.
- **Files:** `src/app/MainWindow.{h,cpp}`.
- **Done when:** editing, closing, and cancelling/rejecting the prompt behave correctly.

### 6.3 Recent files and error handling
- **Do:** a recent-files list in `QSettings`; friendly errors on unreadable/invalid JSON
  (with the file path and a short reason).
- **Files:** `src/app/MainWindow.cpp`.
- **Done when:** opening a corrupt file reports an error and leaves the current document
  intact.

---

## M7 — SQL preview and one-click engine switching

> Goal: deliver the seamless switching UX from PLAN.md §7.

### 7.1 `SqlPreviewPanel`
- **Do:** a dock showing generated SQL in a read-only, syntax-highlighted widget, with
  Copy and Save-to-file actions; re-generates on any model change.
- **Files:** `src/app/SqlPreviewPanel.{h,cpp}`.
- **Done when:** editing a table updates the preview live.

### 7.2 `DialectSelector` and live switch
- **Do:** a toolbar control listing the registered dialects (Postgres first), plus
  shortcuts. Switching re-runs `generate()` and updates the preview and the canvas type
  labels. **No model mutation.**
- **Files:** `src/app/DialectSelector.{h,cpp}`, `src/app/MainWindow.cpp`,
  `src/canvas/EntityItem.cpp`.
- **Done when:** switching engines changes labels and SQL instantly and leaves the model
  unchanged (assert in a test).

### 7.3 Advisories and fidelity chip
- **Do:** collect `TypeMapping` results during generation; show a status-bar fidelity chip
  (`PostgreSQL ✓` / `SQL Server ⚠ 3 types adapted`) and a marker on affected columns; map
  `Unsupported` to an engine default rather than blocking.
- **Files:** `src/app/SqlPreviewPanel.cpp`, `src/app/MainWindow.cpp`.
- **Done when:** a model using `Json`/`Uuid` shows a non-zero adapted count on SQL Server and
  ✓ on PostgreSQL.

### 7.4 `Resolve all`
- **Do:** one action that writes `dialectOverrides` for every advisory as a **single
  undoable command**, after which the chip reads ✓.
- **Files:** `src/document/commands/ResolveTypesCommand.{h,cpp}`, `src/app/MainWindow.cpp`.
- **Done when:** after `Resolve all`, the chip is ✓, one Ctrl+Z restores the advisory state,
  and the SQL is unchanged (the mapped types now come from overrides).

### 7.5 Persistence of accepted decisions
- **Do:** ensure resolved overrides serialize and reload; add a golden case that
  round-trips a model with overrides.
- **Files:** `src/core/JsonSerializer.cpp`, `tests/golden/*`.
- **Done when:** reopening a file with overrides reproduces identical SQL.

---

## M8 — PDF export

> Goal: vector PDF of the diagram, on the layout the user chooses.

### 8.1 `PdfExporter` core
- **Do:** render a `DiagramScene` to a `QPdfWriter` via `QPainter`; fit-to-page single-page
  mode using `sourceRect`/`targetRect`.
- **Files:** `src/export/PdfExporter.{h,cpp}`.
- **Done when:** exporting produces a valid, vector PDF that opens with crisp zoomed text.

### 8.2 Layout modes
- **Do:** tiled N×M (A4/Letter) with overlap and crop marks; poster mode. Document scaling
  and margins.
- **Files:** `src/export/PdfExporter.cpp`.
- **Done when:** a large diagram tiles without clipping content across page seams.

### 8.3 Export dialog and options
- **Do:** an Export → PDF dialog (page size, orientation, layout mode, title block, legend)
  wired into the File menu.
- **Files:** `src/app/MainWindow.cpp`, a `PdfExportDialog.{h,cpp}`.
- **Done when:** the user exports a diagram end-to-end from the running app.

---

## M9 — Thin-slice hardening and first commit

> Goal: everything promised by the thin slice actually works together.

### 9.1 End-to-end walkthrough
- **Do:** create two related tables, save, reopen, switch engine, `Resolve all`, export SQL,
  export PDF — as one scripted manual pass.
- **Done when:** the whole chain works from a fresh launch with no console warnings.

### 9.2 Regression pass
- **Do:** ensure `ctest` covers core round-trip, all Postgres goldens, crow's-foot snapshots,
  and undo/redo. Fix any gaps.
- **Done when:** `ctest` is green and no core/sqlgen code is untested.

### 9.3 Documentation and commit
- **Do:** `README.md` with build/run/test instructions and a screenshot placeholder; a
  short `docs/file-format.md` mirroring PLAN.md §5.
- **Files:** `README.md`, `docs/file-format.md`.
- **Done when:** a clean checkout builds and tests from the README alone. First release
  commit tagged `v0.1-thin-slice`.

---

## After the thin slice (not scheduled here)

Ordered by value, each with the PLAN.md section that specifies it:

1. **Additional dialects** — MySQL, MariaDB, SQL Server, SQLite, Oracle, each with golden
   tests (PLAN.md §6). Add the curated per-dialect type candidate lists (§13).
2. **Grouped `TypeResolutionPanel`** — the per-issue audit UI with `Accept all` /
   `Choose another type…` (PLAN.md §7.8).
3. **Property inspector** — `EntityEditorPanel` for editing columns, indexes, constraints
   in place instead of inventing tables via dialogs.
4. **Indexes and constraint editors** — composite PKs, unique, check constraints.
5. **Auto-layout** — Graphviz (`dot`) integration or a layered (Sugiyama) implementation
   (PLAN.md §8).
6. **Reverse engineering** — connect via `QtSql`, import schema, populate `nativeType` for
   exact round-trips (PLAN.md §7.6, §13).
7. **Validation panel** — reserved words, index prefix limits, row-size limits per engine
   (PLAN.md §13).
8. **Packaging** — CPack, `windeployqt`, `macdeployqt`, AppImage/Flatpak (PLAN.md §11).

---

## Critical path and dependencies

```
M0 toolchain
 └─ M1 model ── M2 sqlgen ──────────────────┐
      └─ M3 canvas ── M4 relationships ── M5 undo ── M6 files ── M7 switching ── M8 pdf
                                                                   └──────────────┴─ M9
```

- M1 blocks everything.
- M2 is independent of the UI and can proceed in parallel with M3–M5 if desired.
- M7 needs M2 (generation) and M6 (persistence of overrides).
- M9 requires all of the above.

## Watchpoints

- **`MoveEntityNode` merging** (5.2) — the most common source of "why did undo take 400
  steps". Get it right before M6.
- **Cycle handling** (2.4) — never let it produce invalid SQL; it must be in M2, not deferred.
- **Advisories must not mutate the model** (7.2/7.3) — assert this in a test, or the
  canonical-type design silently erodes.
- **Edge routing** (4.2) — budget more time than the boxes; it is the real complexity.
- **Golden fixtures** (2.5) — keep them small and readable; they double as documentation.
