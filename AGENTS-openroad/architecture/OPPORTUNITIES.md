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
| pin maps use pointer hash | 15.37e9 | same | same |
| degree 9 built when a net asks | 15.35e9 | same | same |
| liberty port read from the master term | 14.96e9 | same | same |
| debug flag tested before the lookup | 14.64e9 | same | same |

The placed `gcd` netlist at its own clock (period 0.485 ns) finished in
0.61 s with slack +0.01 ns. TinyRocket at its own 2.03 ns clock finished
in 3.51 s with slack +0.33 ns. Those two runs do not exercise repair.

## Callers of `dbNetwork::id` on the 16.29e9 profile

The 744 million instructions in `id(Pin*)` were not a hierarchical walk.
`hasHierarchy()` from that function was 53 million. The callers were
`PinIdLess` on `std::map` nodes: output-delay pins, input-delay pins, and
`LoadPinIndexMap` (`Pin*` to `size_t`). Each tree comparison called
`Network::id` twice. `PinIdHash` did the same for clock-pin unordered maps.

Those maps are lookups. The stored index or delay set does not depend on
key order. They now hash the pin pointer. `PinSet` stays ordered by
database id, so report order of pin sets does not move.

After that change the same run is 15.37 billion instructions. The largest
named function was `base64_decode` at 2.9 percent (445 million), which is
the one-shot Steiner table and Python-init decode, not a per-gate loop.
No function is a dense float loop above 5 percent, so AVX was not added.

A per-byte Steiner decoder, which stopped before degree 9 when that degree
was not asked for yet, was measured on the same run. Instructions rose from
15.37 billion to 15.83 billion. Slack and TNS did not change. That decoder
was removed.

The kept cut uses the same 4-character decode loop and stops after degree
8 until a net asks for degree 9. One thread does not call
`prepareForThreads` before the parasitic pass. Raising the table from
degree 8 to 9 continues the parse instead of reading degrees 4–8 again.
TinyRocket still asks for degree 9, so that part of the table is still
decoded. Instructions fell from 15.37 billion to 15,348,915,691. The
largest named function is `appendBase64` at 2.56 percent (393 million).
Slack and TNS did not change.

`Network::libertyPort(pin)` calls `port(pin)`, and `dbNetwork::port`
decodes the pin that `libertyPort` already decoded. For an instance term
the master term holds the concrete port, so the pin overload now reads
that port. `port(pin)` fell from 324 million instructions to 52 million,
which is the callers that still need the port. The same run is
14,956,314,051 instructions. Slack and TNS did not change.

`debugPrint` called `Debug::check` on every arrival edge. With no debug
category enabled, that call only read a false flag. The macro now tests
the flag first, and the category lookup runs only when debug is on.
Instructions fell to 14,637,298,787. Slack and TNS did not change.
Instruction-cache misses rose from 840,994 to 897,883. `appendBase64` is
still 393 million, because TinyRocket still asks for degree 9.
`dbNetwork::instance(pin)` is still 236 million.

## Still open, in this repo

- `dbNetwork::id` remains for ordered `PinSet` comparisons. Inlining it
  does not remove the virtual call through `Network*`. Do not skip
  `hasHierarchy()` until a profile shows that branch.
- `levelizedDrvrVertices` rebuilds the full sorted vector after one level
  change. An incremental insert is only safe if every caller that holds the
  vector is identified. `repairDesign` must keep its snapshot.
- FLUTE degree 9 is still decoded on TinyRocket because a net asks for it.
  The bytes are decoded once. Do not replace the 4-character loop with a
  per-byte reader.
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
