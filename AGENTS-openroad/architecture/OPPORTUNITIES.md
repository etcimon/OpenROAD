# Opportunities

Rules carried from the Verilator and Yosys work:

- If a dirty set is empty, return. Do not walk the design to discover that.
- Do one search, then read the stored result. Do not search once per gate.
- A tag or an id is O(1). A scan of every pin to recover the same id is not.
- Do not copy a vector that the next statement invalidates, and do not keep
  a reference to a vector the next statement clears.
- A faster structure that changes violator order is a different repair, not
  a speedup.
- Instruction count on one fixed netlist is the comparison. Wall-clock
  samples on a loaded host overlap. Slack and TNS must stay the same.

## Measured on TinyRocket, Nangate45, clock forced to 0.40 ns

One `repair_timing -setup` pass (`-max_passes 1 -max_iterations 1
-max_repairs_per_pass 20`). Release build, link-time optimization off.
Callgrind, instruction references:

| Binary | Instructions | Worst slack before / after | TNS before / after |
|---|---:|---|---|
| before the Steiner decode change | 18.95e9 | −1.30 / −1.25 ns | −1961.15 / −1960.65 |
| after the decode change (`6e2f924`) | 17.17e9 | same | same |
| after inlining `staToDb` (`9040699`) | 16.29e9 | same | same |

The placed `gcd` netlist at its own clock (period 0.485 ns) finished in
0.61 s with slack +0.01 ns. TinyRocket at its own 2.03 ns clock finished
in 3.51 s with slack +0.33 ns. Those two runs do not exercise repair.

## Still open, in this repo

- `dbNetwork::id` is now the largest named function. It calls `staToDb` and,
  in the hierarchical case, strips the tag again. A non-hierarchical block
  already returns `iterm->getId()` without that second strip. Confirm the
  benchmark block is non-hierarchical before special-casing further.
- `levelizedDrvrVertices` rebuilds the full sorted vector after one level
  change. An incremental insert is only safe if every caller that holds the
  vector is identified. `repairDesign` must keep its snapshot.
- FLUTE still re-parses the decoded table when the degree rises. The bytes
  are cached. The parse is the remaining cost.
- `check_sanity` still builds two `std::set`s per driver to compare loads.
  Membership can be a hash set. Warning order is the iteration order of the
  OpenDB side. Keep that order if a test goldens the message text.

## Do not do these

- Do not put `slack(vertex)` back in a loop and expect `findRequireds` to
  make it cheap. `slack(vertex)` calls `findRequired` again.
- Do not scan clocks inside `getPinArrival`. Arrival already folds clock tags.
- Do not run `check_sanity` on every iterm. Loads have no wire edge.
- Do not turn on a resynthesis or clock-gating pass to speed a timing report.
  Those enter ABC and are a different program.
