# bedbug — Database Design Studio

A cross-platform desktop application (Windows / Linux / macOS) for visual database
design in the style of MySQL Workbench: Entity-Relationship diagrams with crow's-foot
notation, saveable/loadable diagram files, PDF export, and SQL DDL generation for
multiple engines (PostgreSQL, MySQL, MariaDB, SQL Server, SQLite, ...).

**Status:** planning. No code written yet.

---

## 1. Decisions

| Decision | Choice |
|---|---|
| Language / UI toolkit | C++ with Qt 6 Widgets |
| Diagram canvas | `QGraphicsScene` / `QGraphicsView` |
| Relationship notation | Crow's foot / IE |
| File format | JSON document (one file per model) |
| First milestone | End-to-end thin slice (table → relationship → save → SQL → PDF) |
| Relationship model | A relationship **is** a foreign key (single source of truth) |
| Type storage | Canonical, engine-neutral types in the model; per-engine mapping at export |
| Target engine | An export-time parameter, not baked into the file |
| Engine-switching UX | One click to switch, never blocks, review is optional and grouped |

### Why Qt 6 Widgets + `QGraphicsScene`

The diagram canvas is the core of the product, and `QGraphicsScene` already provides the
item/scene graph an editor needs: selection, z-order, transforms, hit-testing, grouping,
and dirty-rect repaint. Building this on Qt Widgets avoids re-implementing an editor
framework and yields a genuinely native, lightweight, fast binary.

Alternatives considered:

| Option | Canvas fit for ERD | Native feel | Dev speed | Packaging |
|---|---|---|---|---|
| **C++ + Qt Widgets** (chosen) | Excellent — built for it | Excellent | Medium | Good |
| C++ + Qt Quick/QML | Good, but custom item model | Excellent | Medium | Good |
| Python + PySide6 | Excellent (same Qt API) | Good | Fastest | Heavy |
| Rust + Slint | Good, declarative | Very good | Medium | Excellent (single binary) |
| Rust + egui/iced | Fair — immediate-mode, weak for persistent editors | Good | Medium | Excellent |
| C# + Avalonia | Very good | Good | Fast | Good |

### Licensing note

Qt is used under **LGPLv3 with dynamic linking**, which keeps a closed-source commercial
release possible provided the user can relink against a modified Qt. If static linking is
ever desired, that decision must be made before shipping (it triggers GPL/commercial
obligations).

---

## 2. Environment prerequisites

Verified on Fedora 44: Qt **6.11.2 runtime** is present but the development packages are
not (`moc` missing, `/usr/include/qt6` empty, only `Qt6Core/Gui/WaylandClient` CMake
configs installed). CMake 4.3, Ninja 1.13, and GCC 16.2 are already available.

```bash
sudo dnf install qt6-qtbase-devel qt6-qtsvg-devel
```

This provides the headers, `moc`, and the `Qt6Widgets`, `Qt6Sql`, `Qt6PrintSupport`, and
`Qt6Test` CMake packages.

---

## 3. Repository layout

```
bedbug/
├── CMakeLists.txt              # top-level, qt_standard_project_setup()
├── src/
│   ├── core/                   # static lib — QtCore ONLY, zero UI
│   │   ├── CanonicalType.{h,cpp}
│   │   ├── Column.h  Entity.h  Relationship.h  Index.h  Model.h
│   │   ├── Id.h                # stable opaque ids (QUuid)
│   │   └── JsonSerializer.{h,cpp}
│   ├── sqlgen/
│   │   ├── SqlDialect.h        # abstract interface
│   │   ├── dialects/Postgres.cpp  MySql.cpp  MariaDb.cpp  SqlServer.cpp  Sqlite.cpp
│   │   ├── TypeMap.{h,cpp}     # canonical -> dialect type + fidelity
│   │   └── SqlGenerator.{h,cpp}  # topological sort, options, assembly
│   ├── canvas/                 # QtWidgets
│   │   ├── DiagramScene.{h,cpp}  DiagramView.{h,cpp}
│   │   ├── EntityItem.{h,cpp}  RelationshipItem.{h,cpp}
│   │   └── CrowsFootPainter.{h,cpp}
│   ├── document/
│   │   ├── DocumentController.{h,cpp}   # model + QUndoStack + dirty + file path
│   │   └── commands/*.cpp
│   ├── export/
│   │   └── PdfExporter.{h,cpp}
│   └── app/
│       ├── MainWindow.{h,cpp}
│       ├── EntityEditorPanel.{h,cpp}     # property inspector
│       ├── SqlPreviewPanel.{h,cpp}       # live preview + fidelity chip
│       ├── DialectSelector.{h,cpp}       # one-click engine switch
│       └── TypeResolutionPanel.{h,cpp}   # grouped, one-click fixes
└── tests/
    ├── core/tst_model_json.cpp
    ├── sqlgen/tst_postgres.cpp
    └── golden/postgres/*.sql          # expected output fixtures
```

Dependency direction is strictly one-way:

```
app/  ->  canvas/  ->  core/
  \->  document/  ->  core/
  \->  export/
  \->  sqlgen/  ->  core/
```

`core` has **zero UI dependency**. It is the source of truth for the schema and is what
the SQL generator consumes. This is what makes headless testing and golden-file
comparison possible.

Build targets:

- `bb_core` (static lib, QtCore only)
- `bb_sqlgen` (links `bb_core`)
- `bb_canvas` (QtWidgets)
- `bedbug` (executable; Widgets + PrintSupport)
- `bb_core_tests`, `bb_sqlgen_tests` (QTest, run via CTest)

---

## 4. Core model

The single most important design call: **a relationship *is* a foreign key.** MySQL
Workbench allows the relationship and the FK to diverge, which is a persistent source of
confusion. Collapsing them gives one source of truth and makes SQL generation trivial.

```cpp
struct CanonicalType {                 // engine-neutral
  enum Kind { Int, BigInt, SmallInt, Varchar, Char, Text, Boolean,
              Date, Time, Timestamp, Decimal, Float, Double,
              Blob, Json, Uuid, Enum };
  Kind kind; int length, precision, scale; QStringList enumValues;
};

struct Column {
  Id id; QString name; CanonicalType type;
  bool nullable, primaryKey, unique, autoIncrement;
  QString defaultValue, comment;       // defaultValue = raw SQL expression

  // Engine adaptation (see §7):
  QHash<QString, QString> dialectOverrides;  // "sqlserver" -> raw type string
  DialectHint nativeType;                     // raw type + engine it was authored in
};

struct Entity {
  Id id; QString name, schema, comment;
  QList<Column> columns;
  QList<Index> indexes;
};

struct Relationship {                  // == exactly one foreign key
  Id id; QString name;
  Id childEntity;  QList<Id> childColumns;
  Id parentEntity; QList<Id> parentColumns;
  bool identifying;                    // weak entity / ON DELETE CASCADE default
  QString onDelete, onUpdate;          // "CASCADE" | "RESTRICT" | ...
};

struct Model {
  QString formatVersion;               // "1"
  QString preferredDialect;            // convenience only; never gates generation (see §7)
  QList<Entity> entities;
  QList<Relationship> relationships;
  QList<Diagram> diagrams;
};

struct DiagramNode { Id entity; QPointF pos; };
struct Diagram {
  Id id; QString name;
  QList<DiagramNode> nodes;
  double zoom;
};
```

### Structure vs. presentation split

Structure is owned by the model; **positions are owned by the diagram**. Rearranging boxes
never dirties the schema. One model can contain N diagrams from the start — cheap to
support now, and it avoids a format migration later (matching MySQL Workbench's model of
multiple EER diagrams over one schema).

---

## 5. JSON file format

One file per model. Human-readable, git-diffable, versioned via `formatVersion`.

```json
{
  "formatVersion": "1",
  "model": { "name": "Sales", "preferredDialect": "postgres" },
  "entities": [{
    "id": "8f3c...", "name": "customer", "schema": "public",
    "columns": [
      { "id": "a1...", "name": "id", "type": {"kind": "BigInt"},
        "nullable": false, "primaryKey": true, "autoIncrement": true },
      { "id": "a2...", "name": "email", "type": {"kind": "Varchar", "length": 255},
        "nullable": false, "unique": true }
    ],
    "indexes": []
  }],
  "relationships": [{
    "id": "r1...", "name": "fk_order_customer",
    "childEntity": "...", "childColumns": ["..."],
    "parentEntity": "...", "parentColumns": ["..."],
    "onDelete": "CASCADE"
  }],
  "diagrams": [{
    "id": "d1...", "name": "Main",
    "nodes": [{ "entity": "8f3c...", "pos": [40, 80] }],
    "zoom": 1.0
  }]
}
```

Optional per-column fields `dialectOverrides` (map of engine id → raw type) and
`nativeType` (raw type + source engine) are omitted above for brevity; see §7.

A `Model -> JSON -> Model` round-trip equality test is the first real test written.

---

## 6. SQL generation

### Dialect interface

```cpp
class SqlDialect {
public:
  virtual QString id() const = 0;                             // "postgres"
  virtual QString displayName() const = 0;
  virtual QString quoteIdent(const QString&) const = 0;
  virtual QString mapType(const CanonicalType&) const = 0;
  virtual QString columnDefinition(const Column&) const = 0;  // auto-inc / PK syntax
  virtual QString dropTable(const Entity&, bool ifExists) const = 0;
  virtual QStringList preTableStatements(const Model&) const; // e.g. extensions
};
```

### Generator assembly

`SqlGenerator::generate(model, dialect, options)`:

1. Preamble (drops, session `SET` statements).
2. `CREATE TABLE` statements in **topological order** (children after parents).
3. `ALTER TABLE ... ADD CONSTRAINT` for foreign keys, including back-edges that form cycles.
4. `CREATE INDEX` / unique indexes.

`SqlGenOptions`: `emitDrop`, `ifExists`, `wrapInTransaction`, `emitIndexes`,
`qualifySchema`, `includeComments`, `useAlterForForeignKeys`.

The `dialect` argument is the *only* place the target engine enters generation — the model
is never rewritten. See §7 for how that is surfaced to the user.

### Canonical → dialect type map

| Canonical | PostgreSQL | MySQL | MariaDB | SQL Server | SQLite |
|---|---|---|---|---|---|
| `BigInt` auto-inc | `bigint GENERATED BY DEFAULT AS IDENTITY` | `bigint AUTO_INCREMENT` | same | `bigint IDENTITY(1,1)` | `INTEGER PRIMARY KEY AUTOINCREMENT` |
| `Varchar(n)` | `varchar(n)` | `varchar(n)` | `varchar(n)` | `nvarchar(n)` | `TEXT` |
| `Text` | `text` | `text` | `text` | `nvarchar(max)` | `TEXT` |
| `Boolean` | `boolean` | `tinyint(1)` | `tinyint(1)` | `bit` | `INTEGER` |
| `Timestamp` | `timestamptz` | `datetime` | `datetime` | `datetime2` | `TEXT` |
| `Uuid` | `uuid` | `char(36)` | `char(36)` | `uniqueidentifier` | `TEXT` |
| `Json` | `jsonb` | `json` | `json` | `nvarchar(max)` | `TEXT` |
| `Decimal(p,s)` | `numeric(p,s)` | `decimal(p,s)` | `decimal(p,s)` | `decimal(p,s)` | `NUMERIC` |

`GENERATED BY DEFAULT AS IDENTITY` is used rather than `ALWAYS` so that explicit inserts
remain possible.

Each dialect gets a **golden-file test**: a fixture model plus the exact expected `.sql`,
diffed automatically.

---

## 7. Switching the target engine and how types adapt

### 7.1 The target engine is an export-time parameter, not a model property

Because the model stores **canonical** types, the target engine is not baked into the
file. Switching engines is a non-destructive view/export parameter:

- The toolbar and the SQL preview dock expose a dialect selector (`DialectSelector`).
- Changing it re-runs `SqlGenerator::generate(...)` and re-renders the preview and the
  type labels shown on the canvas; **nothing in the model changes**.
- The document may store a `preferredDialect` for convenience on reopen, but it never
  gates generation.

This is the payoff of the canonical type layer: switching PostgreSQL → SQL Server requires
no migration pass, no rewrite of column definitions, and no risk of the model drifting
toward one engine.

### 7.2 Where adaptation happens

`TypeMap::map(canonical, dialect)` returns a result object rather than a bare string, so
the UI can reason about fidelity:

```cpp
enum class Fidelity { Exact, Widened, Narrowed, Lossy, Unsupported };

struct TypeMapping {
  QString sqlType;            // rendered type for the dialect
  Fidelity fidelity;
  QString note;               // human-readable explanation
  QString suggestedOverride;  // e.g. "nvarchar(max)"; empty when none applies
};
```

Each dialect's `columnDefinition()` calls `TypeMap` and embeds the result.

### 7.3 Fidelity levels and what they mean

| Fidelity | Meaning | Example | UX |
|---|---|---|---|
| `Exact` | 1:1, no semantic change | `Int` → `integer` | silent |
| `Widened` | Lossless superset | `Text` → `nvarchar(max)` (SQL Server) | silent; noted in legend |
| `Narrowed` | Parameter clamped to fit target | `Varchar(10000)` → `varchar(4000)` (SQL Server) | warning badge |
| `Lossy` | Semantics change | `Uuid` → `char(36)` (MySQL); `timestamptz` → `datetime2` (no tz, SQL Server) | warning badge |
| `Unsupported` | No native type; auto-mapped to an engine default | `Json` on an engine without it | badge; blocks only in strict mode |

These levels drive the seamless flow in §7.7–7.9: by default nothing blocks, the status-bar
fidelity chip summarizes the state, and the resolution panel groups the fixes so one
decision covers every affected column.

### 7.4 Custom / raw types are engine-bound

The `Unknown` escape hatch (§13) holds a raw SQL string. A raw type is authored in one
engine's syntax and cannot be translated automatically, so it carries the dialect it was
written for. When the target engine differs:

1. if a per-engine override exists, use it;
2. otherwise emit the raw string verbatim and mark it `Unsupported` with a note.

### 7.5 Per-engine overrides

Two optional `Column` fields (§4) allow hand-tuning a schema that must be engine-specific
while keeping it single-sourced:

- `dialectOverrides` — map of dialect id → raw type string, e.g.
  `{ "sqlserver": "nvarchar(4000)" }`.
- `nativeType` — the original raw type plus the engine it was authored in.

Resolution order for a given target dialect:

1. explicit `dialectOverrides[dialect]`;
2. `nativeType` when the target equals the hint's source engine (exact round-trip);
3. `TypeMap` mapping of the canonical type;
4. error / `Unsupported`.

### 7.6 Re-export to the same engine stays exact

When a model is later imported from a live database (future reverse engineering), the
original native type is preserved in `nativeType` (raw string + source engine).
Re-generating for that same engine returns the original definition verbatim instead of a
re-mapped approximation. This makes "import → tweak → export" **lossless for the engine of
origin**, while every other engine still receives the canonical → dialect mapping. Without
this, a Postgres `jsonb` round-tripped through the canonical `Json` kind and back would
silently become a look-alike rather than the original.

### 7.7 The default experience: zero clicks to switch

Switching engines must feel like changing a filter, not like running a migration. The app
**never blocks or interrogates you on switch**; it produces valid SQL immediately and makes
review optional.

- A single control does it: the `DialectSelector` (toolbar combo / segmented buttons), with
  shortcuts for the common engines.
- On switch, every column is mapped automatically using this policy:

  | Fidelity | Automatic action |
  |---|---|
  | `Exact`, `Widened` | map and stay silent |
  | `Narrowed` | map to the widest valid parameter for the engine |
  | `Lossy` | map to the closest native type |
  | `Unsupported` | map to a sensible engine default (usually its text type) |

- These automatic choices are **advisories, not edits** — they are pure presentation and are
  never written to the file. The canvas labels and the SQL preview show the real generated
  type, with a small marker on affected columns.
- A **fidelity chip** in the status bar summarizes the state at a glance:
  `PostgreSQL ✓` · `SQL Server ⚠ 3 types adapted` · `MySQL ✓`.
- Result: one click changes engine, and the script is already valid. Review is opt-in.

### 7.8 The resolution panel: fixing anything in a few clicks

Clicking the fidelity chip (or a column's marker) opens the `TypeResolutionPanel`. It groups
by **issue, not by column**, so a decision applies to every affected column at once:

```
⚠ UNSUPPORTED   jsonb (SQL Server)              4 columns
    jsonb → nvarchar(max)
    [ Accept all ]   [ Choose another type… ]

⚠ LOSSY         uuid → char(36) (MySQL)         2 columns
    [ Accept all ]   [ Choose another type… ]

⚠ NARROWED      varchar(10000) → varchar(4000) (SQL Server)   1 column
    [ Accept all ]   [ Keep original ]
```

- **One group = one decision**, no matter how many columns it covers.
- `Accept all` writes `dialectOverrides` for the affected columns as a **single undoable
  command**.
- `Choose another type…` opens a curated list of that engine's native types with the
  recommendation pre-selected — so the common path is one click and the escape hatch is two.
- A single `Resolve all` button applies every recommendation at once.
- Because accepted decisions are stored per engine (§7.5), the panel is **empty** the next
  time you switch back to that engine.

### 7.9 Persistence and reversibility

- Accepted fixes are per-engine overrides: saved with the model and stable across reopens.
- Unaccepted advisories are transient and never touch the file.
- `Resolve all` is one undo step; each per-group `Accept all` is one step.
- An optional **strict mode** (off by default) turns `Unsupported` into a blocking prompt
  before export, for users who want to be forced to review. The default stays seamless.

---

## 8. Canvas

- **`EntityItem : QGraphicsItem`** — paints a header bar (table name) and column rows
  (`PK`/`FK` badge, name, mapped type, `NOT NULL`). Flags:
  `Movable | Selectable | SendsGeometryChanges`. Size derives from content; rows are
  painted directly (fast) with an explicit `rowAt(y)` hit-test for port math.
- **`RelationshipItem : QGraphicsPathItem`** — routes an **orthogonal** polyline from the
  child-row port to the parent-row port, stepping around box edges, and paints crow's-foot
  at both ends.
- **`CrowsFootPainter`** — `paint(painter, anchor, direction, cardinality, optional)`:
  three diverging strokes for "many", a perpendicular tick for "one", a circle for
  zero/optional, a double tick for mandatory.
- **`DiagramScene`** — owns items, maintains an `entityId -> EntityItem` map, handles
  rubber-band selection and a link mode (click source column → click target column →
  creates `Relationship` + `AddRelationshipCommand`).
- **`DiagramView : QGraphicsView`** — zoom (wheel + ctrl), pan (space-drag / middle-drag),
  grid and snap.

The type label rendered on a column follows the currently selected target engine (§7.1),
so the canvas is a live preview of what would be generated.

### Known hard problems

1. **Edge routing** — orthogonal connectors that avoid entity boxes, with cardinality
   markers. More work than the boxes themselves.
2. **Auto-layout** — integrate Graphviz (`dot`) or implement a layered (Sugiyama) layout.
   Deferrable.
3. **Type fidelity across engines** — mapping is imperfect in reverse; needs the
   conservative fidelity model described in §7.3 and §7.4.

---

## 9. Undo/redo and document lifecycle

`DocumentController` owns the `Model`, a `QUndoStack`, the current file path, and the dirty
flag. Every mutation goes through a command so nothing bypasses undo:

`AddEntity`, `RemoveEntity`, `RenameEntity`, `AddColumn`, `RemoveColumn`, `MoveColumns`,
`CreateRelationship`, `RemoveRelationship`, `MoveEntityNode` (merges during a drag).

`QUndoStack::cleanChanged` drives the window's modified indicator and the
"save before close?" prompt.

Note that selecting a target engine (§7) is **not** an undoable model mutation — it only
changes what is displayed and generated.

---

## 10. PDF export

`PdfExporter` uses `QPdfWriter` + `QPainter`, producing true **vector** output (crisp at
any zoom). It reuses the item `paint()` routines via `scene->render()`.

Layout modes:

- Fit-to-page single page.
- Tiled N×M (A4 / Letter) with overlap and crop marks.
- Poster.

Options: title block, legend, and a table-list appendix (optionally including the
generated DDL).

---

## 11. Build and test

- CMake with `qt_standard_project_setup()` and `AUTOMOC`.
- CTest wired so `ctest` runs the model round-trip test and every dialect golden file.
- Later: CPack plus `windeployqt` / `macdeployqt` / AppImage / Flatpak for distribution.

---

## 12. Thin-slice task order

1. Install the Qt6 development packages (section 2).
2. CMake skeleton + an empty `QMainWindow` that launches — proves the toolchain.
3. `core` model + JSON round-trip test green.
4. `sqlgen` PostgreSQL dialect + golden test green (headless, no UI).
5. `EntityItem` plus create / move / delete tables on the canvas.
6. Relationship creation and crow's-foot edges.
7. Undo/redo wired through all mutations.
8. File → Open / Save / Save-As with a dirty prompt.
9. SQL preview dock: one-click engine switch with live re-generation, fidelity chip,
   `Resolve all`, view / copy / save `.sql`. (The grouped per-issue audit panel of §7.8 is
   the next step, not the thin slice.)
10. Export → PDF dialog.
11. First commit; `ctest` covers core and PostgreSQL.

Steps 3–4 are fully headless, so the riskiest logic (type mapping, FK ordering) is proven
before any pixel is drawn. Step 9 is where the engine-switching behavior from §7 first
becomes user-visible.

---

## 13. Open items / risks

- **Relationship ≡ FK** — the one irreversible model decision. Chosen deliberately.
- **v1 canonical type set** — start with the 16 kinds above; expose an explicit
  `Unknown` / custom-type escape hatch carrying a raw SQL string for anything unmapped,
  tagged with its source engine so §7.4 applies.
- **Reverse engineering from a live DB** — deliberately out of the thin slice. QtSql plus
  driver plugins make it relatively cheap to add immediately afterward, and it is what
  populates `nativeType` for exact round-trips (§7.6).
- **Queryable dialect differences** — index prefix limits, row-size limits, and reserved
  words differ per engine. v1 handles reserved words (quoting) and the type map; the rest
  is a follow-up validation pass.
- **Curated type candidates per engine** — the seamless UX (§7.7–7.8) depends on each
  dialect exposing a recommended default and a short list of native alternatives for every
  canonical kind. This list is small but must be hand-reviewed per engine; it is what keeps
  switching automatic and fixes to one click.
- **Qt licensing** — see section 1.

## 14. Non-goals for the first milestone

- Live database connections / reverse engineering.
- Automatic per-engine schema optimization beyond type mapping.
- Auto-layout.
- Multi-user or cloud sync.
- Non-ERD diagram kinds.
