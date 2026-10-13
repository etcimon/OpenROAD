# OpenROAD architecture notes

These notes are for inferring execution paths and speed work. They describe the
`g6lc` tree. They are not a frequency claim and they are not a place-and-route
tutorial.

| Note | What it answers |
|---|---|
| [EXECUTION.md](EXECUTION.md) | Which C++ object a Tcl command reaches |
| [DATA.md](DATA.md) | Which structure is O(1) and which is a walk |
| [OPPORTUNITIES.md](OPPORTUNITIES.md) | Measured costs and the rules that still apply |

Nested checkouts have their own notes:

- `src/sta/AGENTS-opensta/architecture/` — the timing engine
- `third-party/abc/AGENTS-abc/architecture/` — logic networks used by cut and clock gating
- `third-party/slang-elab/AGENTS-sv-elab/architecture/` — SystemVerilog elaboration into RTLIL
- that tree's `third_party/slang/AGENTS-slang/architecture/` and `third_party/fmt/AGENTS-fmt/architecture/`

The sign-off timing script is `build-platform/scripts/openroad-timing.tcl` in the
CVA6 repo. Structural FO4 in `sv-timing` is a different number.
