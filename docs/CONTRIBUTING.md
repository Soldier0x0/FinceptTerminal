# Contributing to Fincept Terminal (Soldier0x0 fork)

This guide covers **how to build and work in this repository**. Contribution
**policy** (what PRs are accepted on this fork) lives in
[`.github/CONTRIBUTING.md`](../.github/CONTRIBUTING.md).

> **Fork notice:** This is
> [Soldier0x0/FinceptTerminal](https://github.com/Soldier0x0/FinceptTerminal), a
> personal fork of
> [Fincept-Corporation/FinceptTerminal](https://github.com/Fincept-Corporation/FinceptTerminal).
> It defaults to **local-only mode** (no Fincept login, Ollama LLM, no cloud
> upsell). See [LOCAL_ONLY_MODE.md](./LOCAL_ONLY_MODE.md). Upstream's
> issue-label and scope-gate rules **do not apply here**.

---

## Contents

- [Ways to Contribute](#ways-to-contribute)
- [Tech Stack](#tech-stack)
- [Prerequisites](#prerequisites)
- [Build](#build)
- [Run](#run)
- [Project Architecture](#project-architecture)
- [Code Conventions](#code-conventions)
- [Development Workflow](#development-workflow)
- [Language-Specific Guides](#language-specific-guides)
- [Getting Help](#getting-help)

---

## Ways to Contribute

| Area          | Examples                                                          |
|---------------|-------------------------------------------------------------------|
| C++ / Qt      | Local-mode gates, screens, services, performance fixes              |
| Python        | Analytics scripts, AI agents, data fetchers                       |
| Data sources  | India-focused connectors (NSE/BSE, MFs, gold, bonds)              |
| Documentation | `LOCAL_ONLY_MODE.md`, fork policy, build notes                    |
| Testing       | Reproduce bugs, review PRs, write tests                           |

---

## Tech Stack

| Layer         | Technology                                                                           |
|---------------|--------------------------------------------------------------------------------------|
| Language      | **C++20** — MSVC 19.38 (VS 2022 17.8) / GCC 12.3 / Apple Clang 15.0                  |
| UI            | **Qt 6.8.3 EXACT** Widgets (pinned)                                                  |
| Charts        | Qt6 Charts                                                                           |
| Networking    | Qt6 Network + Qt6 WebSockets                                                         |
| Database      | Qt6 Sql (SQLite)                                                                     |
| Analytics     | Embedded **Python 3.11.9** (4000+ scripts)                                           |
| Excel I/O     | QXlsx v1.4.9 (FetchContent, pinned commit)                                           |
| Mapping       | QGeoView (pinned commit)                                                             |
| Build         | **CMake 3.27.7 + Ninja 1.11.1** (pinned, unity build)                                |

---

## Prerequisites

Pinned toolchain versions — enforced by CMake. Mismatch produces a clear fail-fast error.

| Tool          | Version                                                                                 |
|---------------|-----------------------------------------------------------------------------------------|
| C++ compiler  | MSVC 19.38 (VS 2022 17.8) / GCC 12.3 / Apple Clang 15.0 (Xcode 15.2)                    |
| CMake         | **3.27.7** — [cmake.org](https://cmake.org/download/)                                   |
| Ninja         | **1.11.1** — [releases](https://github.com/ninja-build/ninja/releases)                  |
| Qt            | **6.8.3** — [Qt Online Installer](https://www.qt.io/download-qt-installer)              |
| Python        | **3.11.9** — [python.org](https://www.python.org/downloads/release/python-3119/)        |
| Git           | latest — [git-scm.com](https://git-scm.com)                                             |

Optional (speeds up rebuilds): **ccache 4.13.4** on Windows is auto-detected.

---

## Build

### Fastest — automated setup script

```bash
git clone https://github.com/Soldier0x0/FinceptTerminal.git
cd FinceptTerminal
./setup.sh      # Linux / macOS — installs toolchain + Qt via aqtinstall, then builds
setup.bat       # Windows — run from a VS 2022 Developer Command Prompt
```

Local-only mode is **on by default** (`FINCEPT_LOCAL_ONLY=ON`). To build with
upstream cloud behaviour: `cmake --preset linux-release -DFINCEPT_LOCAL_ONLY=OFF`.

### Manual — CMake presets

All day-to-day development uses presets from `fincept-qt/CMakePresets.json`.

```bash
cd fincept-qt

# Configure (one-time, or after CMakeLists.txt changes)
cmake --preset win-release        # Windows
cmake --preset linux-release      # Linux
cmake --preset macos-release      # macOS

# Build (run after every code change — incremental)
cmake --build --preset win-release        # Windows
cmake --build --preset linux-release      # Linux
cmake --build --preset macos-release      # macOS
```

> **Older or RAM-constrained machines:** add `--parallel 4` (or any small number) to cap concurrent compile jobs. The default saturates every core, which can overheat older CPUs and slow the rest of your system. Example: `cmake --build --preset macos-release --parallel 4`.

Debug builds: replace `release` with `debug` in both commands.

Manual configure if presets can't resolve Qt:

```powershell
cmake -B build/win-release -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build build/win-release
```

---

## Run

```bash
.\build\win-release\FinceptTerminal.exe                                           # Windows
./build/linux-release/FinceptTerminal                                             # Linux
./build/macos-release/FinceptTerminal.app/Contents/MacOS/FinceptTerminal          # macOS
```

Smoke test (no GUI): `./build/linux-release/FinceptTerminal --smoke-test`

---

## Project Architecture

### Repository layout

```
finceptTerminal/
├── fincept-qt/                 # Main C++ application (all development happens here)
│   ├── src/                    # C++ source
│   ├── scripts/                # Embedded Python analytics (4000+ files)
│   ├── resources/              # Icons, assets, Qt resources
│   ├── CMakeLists.txt
│   ├── CMakePresets.json
│   ├── CLAUDE.md               # Performance/architecture rules (P1–P15, D1–D5)
│   └── DESIGN_SYSTEM.md        # Obsidian UI/UX spec
├── docs/                       # Repo-wide documentation (this file lives here)
├── .github/                    # Issue/PR templates, workflows, contribution policy
└── README.md
```

### C++ source layout (`fincept-qt/src/`)

```
src/
├── app/              # Entry point, MainWindow, routing, splash
├── core/             # Config, events, logging, Result<T>, session, LocalMode
├── ui/               # Theme, reusable widgets, tables, charts, navigation
├── network/          # http/ and websocket/ clients
├── storage/          # SQLite, cache, secure storage, 16 repositories
├── auth/             # Guest + registered auth, JWT (gated in local mode)
├── python/           # Embedded Python bridge, PythonRunner
├── datahub/          # DataHub producers/consumers (see DATAHUB_ARCHITECTURE.md)
├── services/         # 18 service domains — market data, news, agents, workflow, etc.
├── trading/          # Trading core + 18 broker integrations
├── mcp/              # Model Context Protocol infrastructure (24 tool modules)
├── ai_chat/          # AI chat UI + LlmService
└── screens/          # 50+ terminal screens, one subdirectory each
```

### Python scripts (`fincept-qt/scripts/`)

```
scripts/
├── Analytics/        # Analytics modules — equity, portfolio, derivatives,
│                     #   fixed income, economics, corporate finance
├── agents/           # AI agent frameworks (finagent_core, Geopolitics, HedgeFund, …)
├── ai_quant_lab/     # ML, factor discovery, HFT, RL trading, vision quant
├── agno_trading/     # Agno-based trading agents
└── *.py              # 100+ top-level data fetchers (market, gov, economic, alt)
```

See `fincept-qt/CLAUDE.md` for the detailed service catalog and architecture rules.

---

## Code Conventions

### C++

- **Standard:** C++20
- **Naming:** `snake_case` functions/variables, `PascalCase` types/classes
- **Namespaces:** `namespace fincept {}`, `namespace fincept::ui {}`; never `using namespace std;`
- **Qt:** `Q_OBJECT` in all QObject subclasses; pointer-to-member signal/slot syntax only (never `SIGNAL()`/`SLOT()` string macros)
- **Error handling:** `Result<T>` — no raw exception-based APIs across module boundaries
- **Logging:** `LOG_INFO` / `LOG_WARN` / `LOG_ERROR` / `LOG_DEBUG` with a context tag, e.g. `LOG_INFO("MarketData", "Fetched 12 quotes")`. Never log API keys or credentials.

### Python

- **Style:** match surrounding code. Do **not** run Black / autopep8 / isort on existing files.
- **Logging:** use the `logging` module (`logger = logging.getLogger(__name__)`, `logger.info(...)`), never `print()`. Scripts run inside an embedded Qt runtime; `print()` output never reaches the user.
- **Entry points:** C++ invokes scripts through `PythonRunner::instance().run(...)` — your script receives a JSON payload on stdin and must return JSON on stdout. See existing scripts in `scripts/` for the pattern.

### UI / design

Follow `fincept-qt/DESIGN_SYSTEM.md` (Obsidian design system). Do not introduce ad-hoc color values, typography, or spacing — use the tokens.

### Mandatory architecture rules (summary)

Full detail in `fincept-qt/CLAUDE.md`. These are **non-negotiable** — PRs that violate them will be sent back:

- **P1.** Never block the UI thread — no `waitForFinished()` on main thread.
- **P2.** Lazy screen construction — use `register_factory()` for any screen that fetches data.
- **P3.** Timers must start/stop in `showEvent()` / `hideEvent()`, never in constructors.
- **P4.** Python subprocesses go through `PythonRunner` (max 3 concurrent).
- **P6.** Screens render UI only; services do all fetching, caching, processing.
- **P14.** Logs use the `LOG_*` macros (C++) or `logger` (Python). Never `printf` / `print` / `std::cout`.
- **D1–D5.** Streaming data flows through the DataHub — never spawn Python directly from a screen. See `fincept-qt/DATAHUB_ARCHITECTURE.md`.

### Local-only mode

When adding features that touch auth, cloud sync, QuantLib API, updates, or LLM
providers, read [LOCAL_ONLY_MODE.md](./LOCAL_ONLY_MODE.md). Prefer **gating**
upstream cloud paths behind `fincept::local_mode::enabled()` rather than deleting
code — keeps future upstream merges feasible.

---

## Development Workflow

### Branch naming

```
cursor/local-only-mode-spec-5a93   # Cloud Agent branches on this fork
feat/india-mf-defaults
fix/chart-render-crash
docs/local-only-guide
```

Use a topic branch — not `main` — for active work.

### Commit style

```
type: short imperative subject line

Optional body explaining why, not what. Wrap at ~72 cols.

Types: feat, fix, docs, refactor, test, chore, perf, ci
```

### Before you open a PR

1. Read [`.github/CONTRIBUTING.md`](../.github/CONTRIBUTING.md) for this fork's policy.
2. Build locally and run the app (or `--smoke-test`) — verify the change works.
3. Keep the diff minimal. No auto-formatter churn.
4. Fill out the PR template honestly.

There is **no** upstream scope-approved issue requirement on this fork.

---

## Language-Specific Guides

| Guide                                                   | Coverage                                                  |
|---------------------------------------------------------|-----------------------------------------------------------|
| [C++ Guide](./CPP_CONTRIBUTOR_GUIDE.md)                 | Screens, services, core infrastructure, widgets, Qt patterns |
| [Python Guide](./PYTHON_CONTRIBUTOR_GUIDE.md)           | Analytics modules, data fetchers, AI agents, PythonRunner contract |
| [Architecture](./ARCHITECTURE.md)                       | System design, module boundaries, data flow               |
| [Local-only mode](./LOCAL_ONLY_MODE.md)                 | Fork-specific cloud/auth/LLM gating                       |

---

## Getting Help

| Channel      | Link                                                                          |
|--------------|-------------------------------------------------------------------------------|
| Issues       | [GitHub Issues](https://github.com/Soldier0x0/FinceptTerminal/issues) |
| Upstream     | [Fincept-Corporation/FinceptTerminal](https://github.com/Fincept-Corporation/FinceptTerminal) (separate project) |
| Local-only   | [LOCAL_ONLY_MODE.md](./LOCAL_ONLY_MODE.md)                                     |

For bugs or features in **upstream** Fincept Terminal, use their repository and
follow their contribution policy.

---

**This fork:** https://github.com/Soldier0x0/FinceptTerminal  
**Upstream:** https://github.com/Fincept-Corporation/FinceptTerminal  
**License:** AGPL-3.0-or-later
