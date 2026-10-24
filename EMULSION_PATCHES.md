# Local changes to LibRaw 0.22.2

Keep this list current; re-apply on an upgrade. Every file changed here carries a line "Modified by
Emulsion Cam, 2026: <what> (EMULSION_PATCHES.md, item N)" under LibRaw's copyright — CDDL 3.3 asks
each Modification to name its contributor (`LibRawPatchNoticesTest` checks it). These files are CDDL "Modifications": their
source must stay available where the in-app open-source licences page says it is
(`emulsion.librawSourceUrl` in `gradle.properties`): publish the new source with any release that
changes this list.

1. `src/demosaic/xtrans_demosaic.cpp` — `xtrans_interpolate`:
   - **Cancellable.** Checks the cancel flag (`setCancelFlag`) at the top of each tile and skips
     the rest, then calls `checkCancel()` after the parallel loop. The X-Trans demosaic is the
     longest stage of a full-size develop and had no cancellation point at all, so leaving a
     photo mid-develop made the next photo's develop wait for it (`raw_develop.cpp`,
     `nativeCancel`). Measured: a cancelled develop returns in ~0.16 s instead of ~2 s.
   - **2D tiles in a 4-phase checkerboard** instead of row strips (`xt_mode = 2`). Tiles sharing
     an edge never run at once, so the output no longer depends on thread count or timing (the
     strips raced on their 16 px overlap: two 8-thread runs differed in 39k values), and 117
     tiles keep 8 cores busy where 9 strips could not. Pixels differ from single-threaded LibRaw
     only at tile seams (~1.2% of values, max 0.02 encoded) — the class of difference LibRaw's
     own runs already showed.
   - **The two hot loops rewritten, output bit-identical**: the homogeneity map counts its 3x3
     hits branch-free into a register; the final 5x5 average uses separable running sums held in
     bytes (max 225) and branch-free direction choice. Verified with `cmp` on three RAFs (26 MP
     compressed, 26 MP uncompressed portrait, 40 MP). Single-thread demosaic 20.0 s -> 2.42 s;
     26 MP develop at 8 threads 2.4–4.5 s -> 0.66 s on the Mac; emulator 2.3–2.7 s -> 0.47–0.73 s.

2. `src/postprocessing/postprocessing_utils.cpp` — `convert_to_rgb_loop` (3 colours): rows in
   parallel, histogram per thread then merged. Same float maths in the same order: bit-identical.

3. `src/postprocessing/postprocessing_aux.cpp` — `blend_highlights`: rows in parallel (every
   per-pixel temporary private). Bit-identical.

4. `src/preprocessing/raw2image.cpp` — `copy_bayer`: with `half_size` (`IO.shrink`) input rows
   2k and 2k+1 write the same output pixel, and the `omp parallel for` over input rows let two
   threads race on it (a data race; on X-Trans the editor-preview develop differed between runs
   in ~5% of rows, by up to 0.15 encoded, and so did its measured median and white). The loop now
   runs over output rows, each copying its input rows in order — the single-threaded result.
   Without shrink, the step is 1 and the loop is the original. Verified: DSCF7122, DSCF2851 and
   DSCF4253 at 1600 give one hash at 1 and 8 threads (three 8-thread runs gave three before).

5. `src/metadata/identify.cpp` — the Hasselblad X2D crop (`left_margin` 124, `width` 11664,
   `top_margin` 92) also applies to an 11904 x 8842 frame whatever its sensor code. The X2D II
   100C writes `SensorCode` 22, so it missed the X2D's `== 20` rule and developed with the
   sensor's masked border as black bars (~120 px on each long edge, 92 px on one short edge).
   Its colour already came from the same frame size (`100-20-Coated6`, the X2D's matrix).
   Verified on an X2D II 3FR: 11904 x 8842 -> 11664 x 8750, Adobe's crop for this frame.
