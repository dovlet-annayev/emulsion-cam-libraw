# LibRaw for Emulsion Cam

This is [LibRaw](https://www.libraw.org) 0.22.2 with the changes that the Emulsion Cam Android
app ships. Emulsion Cam uses LibRaw under the Common Development and Distribution License 1.0
(`LICENSE.CDDL`). LibRaw's copyright and the notices of the code it includes are in `COPYRIGHT`.

- The first commit is the official release, unchanged: `LibRaw-0.22.2.tar.gz` from libraw.org
  (sha256 `de86b035655accff8d4010f1a221fdf50d353cb7b1422ba26f14a0db92612cfa`).
- Every later commit is a change made for Emulsion Cam. `EMULSION_PATCHES.md` describes each
  one, and every changed file says so in its header ("Modified by Emulsion Cam").

LibRaw's own README is now `README.LibRaw.md`.

## What the app builds

Emulsion Cam compiles the `.cpp` files under `src/` with the headers in `libraw/` and
`internal/`, using the Android NDK (`-O2 -fopenmp`), linked statically into its own native
library. It leaves out the three `*_ph.cpp` placeholder files (stubs for builds without
post-processing), and it does not define `USE_X3FTOOLS`, so the X3F (Sigma) code is not built.
Nothing else in this repository goes into the app.

## Licence

LibRaw's files stay under LibRaw's licences: LGPL 2.1 or CDDL 1.0, as `COPYRIGHT` explains.
Emulsion Cam's changes to them are offered under CDDL 1.0.
