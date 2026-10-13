# Execution path

`src/Main.cc` builds a Tcl interpreter and calls `initOpenRoad`.
`OpenRoad::init` in `src/OpenRoad.cc` constructs every tool, then loads its
Tcl. The order that matters for timing is:

1. `odb::dbDatabase` holds the physical netlist.
2. `sta::dbSta` is the OpenSTA engine bound to that database (`src/dbSta`).
3. `stt::SteinerTreeBuilder` owns the FLUTE tables (`src/stt`).
4. `est::EstimateParasitics` writes wire RC onto STA nets (`src/est`).
5. `rsz::Resizer` repairs slew, cap, setup, and hold (`src/rsz`).
6. `Tcl_Eval` of `sta::define_sta_cmds` publishes `report_checks`,
   `report_worst_slack`, and the SDC commands.

A gate-level timing run, the one used for the callgrind numbers, is:

```
read_lef -> read_liberty -> read_def | (read_verilog + link_design)
-> read_sdc -> set_wire_rc -> estimate_parasitics -placement
-> report_checks -> repair_timing -setup
```

`read_def` after `link_design` fails: the chip already has a block. A placed
design is LEF, liberty, then DEF. A gate netlist with no placement is
Verilog plus `link_design`. `gcd_nangate45.sdc` calls
`set_all_input_output_delays`, which is defined in `test/flow_helpers.tcl`,
not in OpenSTA.

`report_checks` and `report_worst_slack` enter OpenSTA
(`search/Search.cc`, `search/ReportPath.cc`). They do not resize gates.

`repair_timing` enters `rsz::Resizer`. One setup pass:

- `findRequireds()` once, then `dbSta::vertexSlack` on endpoints and drivers
- `RepairTargetCollector` walks violating endpoints in slack order
- a repair that inserts a buffer copies `levelizedDrvrVertices()` first,
  because the insert drops that cache

`estimate_parasitics -placement` asks Steiner for a tree per net, then
annotates parasitics. The first net whose pin count exceeds the initial
FLUTE degree calls `Flute::ensureLUT`, which used to decode the tables again.

`check_sanity` is `dbSta::checkSanityNetlistConsistency`. It compares driver
wire edges in OpenDB with the STA graph. Instance inputs and top-level
outputs are not drivers and are not visited.

Other tools are constructed in `OpenRoad::init` but are not on this timing
path: `gpl` (global place), `dpl` (detail place), `grt` (global route),
`drt` (detailed route), `cts`, `rcx`, `rmp` (ABC resynthesis), `cgt`
(clock gating through ABC). Profile those flows separately. Do not assume a
timing-run hotspot is theirs.
