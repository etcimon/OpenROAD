# Data and cost

## OpenDB (`src/odb`)

`dbBlock` owns instances, nets, iterms, and bterms. `dbSet` iteration is a
linked walk of the block, O(objects). `dbITerm::getNet()` is O(1). Signal
and direction (`getSigType`, `getIoType`) are O(1) and are enough to decide
`Network::isDriver` for a leaf pin or a top port without building an STA pin.

## STA pin identity (`dbNetwork`)

An STA `Pin*` is an 8-byte-aligned `dbITerm`, `dbBTerm`, or `dbModITerm`
plus a 3-bit tag (`kPinPointerTagMask`, `PinPointerTags` in
`dbNetwork.hh`). `staToDb` masks the tag. That is O(1). It used to be an
out-of-line call. Callgrind on TinyRocket spent 1.07 billion instructions
in the call itself. The body is now inline in the header. The tag values
are unchanged.

`dbNetwork::id` still strips the tag when the design is hierarchical, then
reads `dbObject` id. After the inline, that function is the largest named
cost in the same profile (about 4.6%).

## Levelized drivers

`dbSta::levelizedDrvrVertices()` is a vector of STA driver vertices sorted
by level, then name. `DbStaLevelizeObserver::levelChangedBefore` clears the
valid bit on any level change. The next query rebuilds the vector, which
walks vertices and sorts. A repair loop that inserts buffers must copy the
vector before the insert. A read-only scan, such as the driver filter in
`fullyRebuffer`, can hold a const reference.

## Parasitics

`EstimateParasitics::parasitics_invalid_` is the set of nets edited since
the last update. `updateParasitics` returns immediately when that set is
empty. It does not clear the STA driver-pin cache on that path. A non-empty
set still clears the cache and re-estimates each dirty net.

## Steiner tables

`Flute::initLUT` expands base64 text (`powv9`, `post9`) into group tables.
The decoder is a 256-entry map and reserves the output string. The decoded
text is cached, so raising the LUT degree does not decode again. The parse
of that text still runs again for the higher degree.

## Slack

`dbSta::vertexSlack` reads path slack already stored on the vertex. It does
not search. Callers that rank many vertices call `Sta::findRequireds()` once
first. `Sta::slack(Vertex*)` still searches and reseeds. Do not put it back
inside a per-gate loop.

## What is not a hash

Repair violator order is a sorted set. Replacing it with an unordered set
changes which gate is repaired first and changes the slack result. Fanin
traversal in `RepairTargetCollector` keeps that order on purpose.
