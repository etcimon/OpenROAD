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
| top-level port read from the pin tag | 14.31e9 | same | same |
| empty disable sets return before pin decode | 13.46e9 | same | same |
| vertex id lookup inlined | 13.43e9 | same | same |
| empty pin sets are not searched | 13.27e9 | same | same |
| AVX2 Steiner prefix, on the testharness host | 12.70e9 | same | same |
| `fuzzyEqual` and the hierarchy flag inlined | 12.61e9 | same | same |

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
Instruction-cache misses rose from 840,994 to 897,883.

`isTopLevelPort` called `instance` and then `parent`. The parent is null
only for the top sentinel, and `instance` returns that sentinel only for
a block terminal that is not inside a chiplet. The predicate now reads
the pin tag. Instructions fell to 14,313,398,333. Slack and TNS did not
change. `instance(pin)` fell from 236 million to 129 million.
Instruction-cache misses rose from 897,883 to 942,849.

Every timing edge asked whether it was disabled. That query decoded the
pins and read liberty ports, then looked in the disable sets. With no
disable commands those sets are empty, so the query now returns before
the decode. Instructions fell to 13,461,175,547. Slack and TNS did not
change. `libertyPort(pin)` fell from 329 million to 183 million.
Instruction-cache misses fell from 942,849 to 886,408.

What is still large, and why it was not rewritten:

- `appendBase64` is still 393 million. TinyRocket asks for degree 9, so
  the table is decoded. A per-byte decoder was slower.
- Liberty table evaluation (`findValueOrder2`) is 166 million for about
  one million bilinear interpolations. The axes are a handful of floats.
  A vector rewrite would change the interpolated delay.
- `Tag::matchCmp` is 257 million. Tag order is part of the search.
- `malloc` and `free` are still about 330 million and 351 million. The
  calls are spread across the search, not one buffer.

`direction(pin)` reads the liberty port because a tristate liberty
direction can differ from the LEF io type. A fast path returned the ODB
io type when every signal liberty direction matched LEF. On Nangate45 a
signal port differs, so the flag stayed set. Every `direction(pin)` still
read liberty, after an extra pin decode. Instructions rose from
13,461,175,547 to 13,488,608,926. Slack and TNS did not change. That fast
path was removed.

`Graph::vertex` is an id-to-pointer. It was called 8.35 million times and
cost 150 million instructions. The body is now inline at the call. The
same run is 13,429,216,482 instructions. Slack and TNS did not change.
Instruction-cache misses fell from 886,408 to 821,743.

Path-delay and disabled-pin queries are membership tests on a pin set.
With no such constraint the set is empty, and `contains` still entered
the ordered tree. Those queries now return before the search. The same
run is 13,273,269,288 instructions. Slack and TNS did not change.
Instruction-cache misses rose from 821,743 to 913,585.

An empty derate table and an empty clock-sense map now return before the
pin is decoded. The Steiner base64 prefix is decoded with AVX2 when the
CPU has it, and the scalar loop keeps the tail. ABC's word-wide simulator
uses the same AVX2 integer AND/OR/NOT. Liberty table interpolation is
unchanged. The base64 decoder was checked against the scalar decoder for
payloads of 0 through 200 bytes, including padding. On the testharness
host that binary is 12,699,620,769 instructions. `Tag::matchCmp`,
`findValueOrder2`, and `visitFromPath` match the previous profile.
`base64DecodeAvx2` is 9.8 million instructions. `deratedDelay` did not
shrink. Slack and TNS did not change. That host has a 32KB 8-way L1I, so
its instruction-cache misses are not comparable to the earlier profiles.

`fuzzyEqual` is inline in the header. `dbNetwork::hasHierarchy` reads the
database flag in `dbNetwork.cc`, so `id` and `libertyPort` inline the
load. The same run is 12,611,721,808 instructions. `fuzzyEqual` self cost
fell from 96.5 million to 0.5 million. Slack and TNS did not change.

A four-slot thread-local cache in front of `findValueIndex`, together
with reading `sta_vertex_id_` directly from `vertexId`, raised the same
run to 12,658,492,147. Slack and TNS did not change. `findValueIndex`
rose from 119.8 million to 181.1 million. `vertexId` rose from 81.0
million to 85.0 million. Both edits were removed.

`deratedDelay` multiplies by 1 when no derate is set, and it used to
look the factor up first. It now skips that lookup. `Tag::matchCmp`
compares a key packed in the existing field order and falls through to
the clock-pin and state tail only when the keys match. The same run is
12,442,859,625 instructions. `matchCmp` fell from 256.5 million to 141.7
million. Slack and TNS did not change. A liberty-port pointer cached on
the master terminal raised `libertyPort(pin)` from 173 million to 220
million and was removed. Delay and slew tables own separate axis
objects, so a shared bracket was not added on that path.

`findPiElmore` locked the parasitic map on every load of a net. The map
edit now bumps an epoch, and each thread keeps the four rise/fall and
min/max results for the current driver. A hit does not take the mutex.
The same run is 12,428,415,795 instructions. Slack and TNS did not change.

`annotateLoadDelays` used to hash each load pin to recover an index that
is the wire-edge ordinal. It now counts wire edges in that same order.
The same run is 12,407,614,809 instructions. Slack and TNS did not change.

`visitEdge` loads the from-path once and then applies the edge's two timing
arcs in that order. The same run is 12,350,962,570 instructions. Slack and
TNS did not change.

Identical liberty index vectors share one `TableAxis`. When a gate's
delay and slew tables use both of those axes, one bracket is interpolated
on each table with that table's own scale factor. The same run is
12,156,847,523 instructions. Slack and TNS did not change.

Skipping the scale multiply when the library has no scale-factor tables
raised that run to 12,170,374,466. Slack and TNS did not change. The
extra pointer checks cost more than the multiply by 1. That edit was
removed.

The shared bracket still evaluated the bilinear weights twice, once per
table. Those weights are now computed once and applied to each table's
corners. Each table still multiplies by its own scale factor. The same
run is 12,134,285,106 instructions. `evalOrder2` self cost fell from
105,060,816 to 3,661,840. Slack and TNS did not change.

Every timing arc of a driver walked the net to recompute a load that
depends only on the output transition, the scene, and min/max. Later
arcs of that driver reuse the load. Arc order is unchanged. The same
run is 11,562,348,144 instructions. `connectedCap` calls fell from
462,296 to 118,908. Slack and TNS did not change.

Both arcs of an edge re-read the from-pin logic constant and the edge
unateness, and each read looked the vertex up from the pin. Those reads
now happen once, on the vertex already in hand, and each arc only
applies its output transition. The same run is 11,384,804,867
instructions. `pinLoadVertex` calls from `simValue` fell from 2,929,986
to 29,874. Slack and TNS did not change.

Pin iteration and pin-vertex lookup asked liberty whether each pin was
bidirect. That driver vertex is already stored when the pin vertex is
made, so those walks read the map. The same run is 11,317,007,941
instructions. `libertyPort` self cost fell from 136,020,678 to
102,450,276. Slack and TNS did not change.

Remembering the input-slew bracket across the effective-capacitance
samples of one gate raised that run to 11,324,038,537. Slack and TNS
did not change. The bracket checks cost more than the searches they
saved. That edit was removed.

Both arcs ask for `-thru` exceptions on every edge. When none are
recorded, that lookup returns before hashing the pin. The same run is
11,260,172,596 instructions. Slack and TNS did not change.

## Still open, in this repo

- `dbNetwork::id` remains for ordered `PinSet` comparisons. Inlining it
  does not remove the virtual call through `Network*`. The hierarchy
  flag is still read. The load lives in `dbNetwork.cc` so that file
  inlines it.
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
