/* -*- C++ -*-
 * Copyright 2019-2025 LibRaw LLC (info@libraw.org)
 * Modified by Emulsion Cam, 2026: X-Trans demosaic made cancellable, tiled deterministically and faster (EMULSION_PATCHES.md, item 1).
 *
 LibRaw uses code from dcraw.c -- Dave Coffin's raw photo decoder,
 dcraw.c is copyright 1997-2018 by Dave Coffin, dcoffin a cybercom o net.
 LibRaw do not use RESTRICTED code from dcraw.c

 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include "../../internal/dcraw_defs.h"
#include <vector>

#define fcol(row, col) xtrans[(row + 6) % 6][(col + 6) % 6]
/*
   Frank Markesteijn's algorithm for Fuji X-Trans sensors
 */
void LibRaw::xtrans_interpolate(int passes)
{
  int cstat[4] = {0, 0, 0, 0};
  int ndir;
  static const short orth[12] = {1, 0, 0, 1, -1, 0, 0, -1, 1, 0, 0, 1},
                     patt[2][16] = {{0, 1, 0, -1, 2, 0, -1, 0, 1, 1, 1, -1, 0,
                                     0, 0, 0},
                                    {0, 1, 0, -2, 1, 0, -2, 0, 1, 1, -2, -2, 1,
                                     -1, -1, 1}},
                     dir[4] = {1, LIBRAW_AHD_TILE, LIBRAW_AHD_TILE + 1,
                               LIBRAW_AHD_TILE - 1};
  short allhex[3][3][2][8];
  ushort sgrow = 0, sgcol = 0;

  if (width < LIBRAW_AHD_TILE || height < LIBRAW_AHD_TILE || filters != 9)
    throw LIBRAW_EXCEPTION_IO_CORRUPT; // too small image
                                       /* Check against right pattern */
  for (int row = 0; row < 6; row++)
    for (int col = 0; col < 6; col++)
      cstat[(unsigned)fcol(row, col)]++;

  if (cstat[0] < 6 || cstat[0] > 10 || cstat[1] < 16 || cstat[1] > 24 ||
      cstat[2] < 6 || cstat[2] > 10 || cstat[3])
    throw LIBRAW_EXCEPTION_IO_CORRUPT;

  // Init allhex table to unreasonable values
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      for (int k = 0; k < 2; k++)
        for (int l = 0; l < 8; l++)
          allhex[i][j][k][l] = 32700;

  cielab(0, 0);
  ndir = 4 << int(passes > 1);

  int minv = 0, maxv = 0, minh = 0, maxh = 0;
  /* Map a green hexagon around each non-green pixel and vice versa:	*/
  for (int row = 0; row < 3; row++)
    for (int col = 0; col < 3; col++)
      for (int ng = 0, d = 0; d < 10; d += 2)
      {
        int g = fcol(row, col) == 1;
        if (fcol(row + orth[d], col + orth[d + 2]) == 1)
          ng = 0;
        else
          ng++;
        if (ng == 4)
        {
          sgrow = row;
          sgcol = col;
        }
        if (ng == g + 1)
        {
            int c;
            FORC(8)
            {
                int v = orth[d] * patt[g][c * 2] + orth[d + 1] * patt[g][c * 2 + 1];
                int h = orth[d + 2] * patt[g][c * 2] + orth[d + 3] * patt[g][c * 2 + 1];
                minv = MIN(v, minv);
                maxv = MAX(v, maxv);
                minh = MIN(h, minh);
                maxh = MAX(h, maxh);
                allhex[row][col][0][c ^ (g * 2 & d)] = h + v * width;
                allhex[row][col][1][c ^ (g * 2 & d)] = h + v * LIBRAW_AHD_TILE;
            }
        }
      }

  // Check allhex table initialization
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      for (int k = 0; k < 2; k++)
        for (int l = 0; l < 8; l++)
          if (allhex[i][j][k][l] > maxh + maxv * width + 1 ||
              allhex[i][j][k][l] < minh + minv * width - 1)
            throw LIBRAW_EXCEPTION_IO_CORRUPT;
  int retrycount = 0;

  /* Set green1 and green3 to the minimum and maximum allowed values:	*/
  for (int row = 2; row < height - 2; row++)
  {
      int col;
      ushort min, max;
      for (col = 2, max = 0u, min = 0xffffu; col < int(width) - 2; col++)
      {
          if (fcol(row, col) == 1 && (min = ~(max = 0)))
              continue;
          ushort(*pix)[4];
          pix = image + row * width + col;
          short* hex = allhex[row % 3][col % 3][0];
          if (!max)
          {
              int c;
              FORC(6)
              {
                  int val = pix[hex[c]][1];
                  if (min > val)
                      min = val;
                  if (max < val)
                      max = val;
              }
          }
          pix[0][1] = min;
          pix[0][3] = max;
          switch ((row - sgrow) % 3)
          {
          case 1:
              if (row < height - 3)
              {
                  row++;
                  col--;
              }
              break;
          case 2:
              if ((min = ~(max = 0)) && (col += 2) < width - 3 && row > 2)
              {
                  row--;
                  if (retrycount++ > width * height)
                      throw LIBRAW_EXCEPTION_IO_CORRUPT;
              }
          }
      }
  }

  for (int row = 3; row < 9 && row < height - 3; row++)
	  for (int col = 3; col < 9 && col < width - 3; col++)
	  {
		  if ((fcol(row, col)) == 1)
			  continue;
          short* hex = allhex[row % 3][col % 3][0];
          int c;
		  FORC(2)
		  {
			  int idx3 = 3 * hex[4 + c] + row * width + col;
			  int idx4 = -3 * hex[4 + c] + row * width + col;
			  int maxidx = width * height;
			  if (idx3 < 0 || idx3 >= maxidx)
				  throw LIBRAW_EXCEPTION_IO_CORRUPT;
			  if (idx4 < 0 || idx4 >= maxidx)
				  throw LIBRAW_EXCEPTION_IO_CORRUPT;
		  }
	  }

#if defined(LIBRAW_USE_OPENMP)
  int buffer_count = omp_get_max_threads();
#else
  int buffer_count = 1;
#endif

  size_t buffer_size = LIBRAW_AHD_TILE * LIBRAW_AHD_TILE * (ndir * 11 + 6);
  char** buffers = malloc_omp_buffers(buffer_count, buffer_size);

    // emulsion-cam patch (EMULSION_PATCHES.md): 2D tiles in a 4-phase checkerboard instead of
    // row strips. Tiles that share an edge never run at the same time, so the output no longer
    // depends on thread count or timing (strips raced on their 16 px overlap), the cores stay
    // busy (117 tiles instead of 9 strips on a 26 MP frame), and a cancel lands per tile.
    // Modes kept for comparison: 0 = original strips, 1 = 2D tiles, unordered.
    const int xt_mode = 2;
    std::vector<int> xt_tops, xt_lefts;
    for (int top = 3; top < height - 19; top += LIBRAW_AHD_TILE - 16) xt_tops.push_back(top);
    for (int left = 3; left < width - 19; left += LIBRAW_AHD_TILE - 16) xt_lefts.push_back(left);
    const int xt_nt = (int)xt_tops.size();
    const int xt_nl = xt_mode == 0 ? 1 : (int)xt_lefts.size();
    const int xt_phases = xt_mode == 2 ? 4 : 1;
    const int *xt_tp = xt_tops.data(), *xt_lp = xt_lefts.data();
    for (int xt_phase = 0; xt_phase < xt_phases; xt_phase++)
    {
#if defined(LIBRAW_USE_OPENMP)
# pragma omp parallel for schedule(dynamic) default(none) firstprivate(buffers, allhex, passes, sgrow, sgcol, ndir, xt_phase, xt_phases, xt_nt, xt_nl, xt_tp, xt_lp, xt_mode) shared(dir)
#endif
    for (int xt_t = 0; xt_t < xt_nt * xt_nl; xt_t++)
    {
        // emulsion-cam patch: a cancel (setCancelFlag) skips the remaining tiles; the
        // checkCancel() after the loop then aborts.
        if (__atomic_load_n(&_exitflag, __ATOMIC_RELAXED))
            continue;
        const int xt_ti = xt_t / xt_nl, xt_tj = xt_t % xt_nl;
        if (xt_phases == 4 && ((xt_ti & 1) * 2 + (xt_tj & 1)) != xt_phase) continue;
        const int top = xt_tp[xt_ti];
#if defined(LIBRAW_USE_OPENMP)
        char* buffer = buffers[omp_get_thread_num()];
#else
        char* buffer = buffers[0];
#endif

        ushort(*rgb)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE][3], (*rix)[3];
        short(*lab)[LIBRAW_AHD_TILE][3], (*lix)[3];
        float(*drv)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE];
        char(*homo)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE];

        rgb = (ushort(*)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE][3])buffer;
        lab = (short(*)[LIBRAW_AHD_TILE][3])(
            buffer + LIBRAW_AHD_TILE * LIBRAW_AHD_TILE * (ndir * 6));
        drv = (float(*)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE])(
            buffer + LIBRAW_AHD_TILE * LIBRAW_AHD_TILE * (ndir * 6 + 6));
        homo = (char(*)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE])(
            buffer + LIBRAW_AHD_TILE * LIBRAW_AHD_TILE * (ndir * 10 + 6));

        for (int xt_li = (xt_mode == 0 ? 0 : xt_tj); xt_li < (xt_mode == 0 ? (int)(width > 22 ? 1 << 30 : 0) : xt_tj + 1); xt_li++)
        {
            const int left = xt_mode == 0 ? 3 + xt_li * (LIBRAW_AHD_TILE - 16) : xt_lp[xt_li];
            if (left >= width - 19) break;
            int mrow = MIN(top + LIBRAW_AHD_TILE, height - 3);
            int mcol = MIN(left + LIBRAW_AHD_TILE, width - 3);
            for (int row = top; row < mrow; row++)
                for (int col = left; col < mcol; col++)
                    memcpy(rgb[0][row - top][col - left], image[row * width + col], 6);
            int c;
            FORC3 memcpy(rgb[c + 1], rgb[0], sizeof * rgb);

            /* Interpolate green horizontally, vertically, and along both diagonals:
             */
            int color[3][8];
            for (int row = top; row < mrow; row++)
                for (int col = left; col < mcol; col++)
                {
                    int f;
                    if ((f = fcol(row, col)) == 1)
                        continue;
                    ushort (*pix)[4] = image + row * width + col;
                    short* hex = allhex[row % 3][col % 3][0];
                    color[1][0] = 174 * (pix[hex[1]][1] + pix[hex[0]][1]) -
                        46 * (pix[2 * hex[1]][1] + pix[2 * hex[0]][1]);
                    color[1][1] = 223 * pix[hex[3]][1] + pix[hex[2]][1] * 33 +
                        92 * (pix[0][f] - pix[-hex[2]][f]);
                    FORC(2)
                        color[1][2 + c] = 164 * pix[hex[4 + c]][1] +
                        92 * pix[-2 * hex[4 + c]][1] +
                        33 * (2 * pix[0][f] - pix[3 * hex[4 + c]][f] -
                            pix[-3 * hex[4 + c]][f]);
                    FORC4 rgb[c ^ !((row - sgrow) % 3)][row - top][col - left][1] =
                        LIM(color[1][c] >> 8, pix[0][1], pix[0][3]);
                }

            for (int pass = 0; pass < passes; pass++)
            {
                if (pass == 1)
                    memcpy(rgb += 4, buffer, 4 * sizeof * rgb);

                /* Recalculate green from interpolated values of closer pixels:	*/
                if (pass)
                {
                    for (int row = top + 2; row < mrow - 2; row++)
                        for (int col = left + 2; col < mcol - 2; col++)
                        {
                            int f;
                            if ((f = fcol(row, col)) == 1)
                                continue;
                            ushort(*pix)[4] = image + row * width + col;
                            short* hex = allhex[row % 3][col % 3][1];
                            for (int d = 3; d < 6; d++)
                            {
                                rix =
                                    &rgb[(d - 2) ^ !((row - sgrow) % 3)][row - top][col - left];
                                int val = rix[-2 * hex[d]][1] + 2 * rix[hex[d]][1] -
                                    rix[-2 * hex[d]][f] - 2 * rix[hex[d]][f] + 3 * rix[0][f];
                                rix[0][1] = LIM(val / 3, pix[0][1], pix[0][3]);
                            }
                        }
                }

                /* Interpolate red and blue values for solitary green pixels:	*/
                for (int row = (top - sgrow + 4) / 3 * 3 + sgrow; row < mrow - 2; row += 3)
                    for (int col = (left - sgcol + 4) / 3 * 3 + sgcol; col < mcol - 2; col += 3)
                {
                    rix = &rgb[0][row - top][col - left];
                    int h = fcol(row, col + 1);

					if (h == 1) // Incorrect pattern
                      break;

                    float diff[6];
                    memset(diff, 0, sizeof diff);
                    for (int i = 1, d = 0; d < 6; d++, i ^= LIBRAW_AHD_TILE ^ 1, h ^= 2)
                    {
                        for (c = 0; c < 2; c++, h ^= 2)
                        {
                            int g = 2 * rix[0][1] - rix[i << c][1] - rix[-i << c][1];
                            color[h][d] = g + rix[i << c][h] + rix[-i << c][h];
                            if (d > 1)
                                diff[d] += SQR((float)rix[i << c][1] - (float)rix[-i << c][1] -
                                    (float)rix[i << c][h] + (float)rix[-i << c][h]) + SQR((float)g);
                        }
                        if (d > 1 && (d & 1))
                            if (diff[d - 1] < diff[d])
                                FORC(2) color[c * 2][d] = color[c * 2][d - 1];
                        if (d < 2 || (d & 1))
                        {
                            FORC(2) rix[0][c * 2] = CLIP(color[c * 2][d] / 2);
                            rix += LIBRAW_AHD_TILE * LIBRAW_AHD_TILE;
                        }
                    }
                }

                /* Interpolate red for blue pixels and vice versa:		*/
                for (int row = top + 3; row < mrow - 3; row++)
                    for (int col = left + 3; col < mcol - 3; col++)
                    {
                        int f;
                        if ((f = 2 - fcol(row, col)) == 1)
                            continue;
                        rix = &rgb[0][row - top][col - left];
                        c = (row - sgrow) % 3 ? LIBRAW_AHD_TILE : 1;
                        int h = 3 * (c ^ LIBRAW_AHD_TILE ^ 1);
                        for (int d = 0; d < 4; d++, rix += LIBRAW_AHD_TILE * LIBRAW_AHD_TILE)
                        {
                            int i = d > 1 || ((d ^ c) & 1) ||
                                ((ABS(rix[0][1] - rix[c][1]) +
                                    ABS(rix[0][1] - rix[-c][1])) <
                                    2 * (ABS(rix[0][1] - rix[h][1]) +
                                        ABS(rix[0][1] - rix[-h][1])))
                                ? c
                                : h;
                            rix[0][f] = CLIP((rix[i][f] + rix[-i][f] + 2 * rix[0][1] -
                                rix[i][1] - rix[-i][1]) /
                                2);
                        }
                    }

                /* Fill in red and blue for 2x2 blocks of green:		*/
                for (int row = top + 2; row < mrow - 2; row++)
                    if ((row - sgrow) % 3)
                        for (int col = left + 2; col < mcol - 2; col++)
                            if ((col - sgcol) % 3)
                            {
                                rix = &rgb[0][row - top][col - left];
                                short* hex = allhex[row % 3][col % 3][1];
                                for (int d = 0; d < 8;
                                    d += 2, rix += LIBRAW_AHD_TILE * LIBRAW_AHD_TILE)
                                    if (hex[d] + hex[d + 1])
                                    {
                                        int g = 3 * rix[0][1] - 2 * rix[hex[d]][1] - rix[hex[d + 1]][1];
                                        for (c = 0; c < 4; c += 2)
                                            rix[0][c] = CLIP(
                                                (g + 2 * rix[hex[d]][c] + rix[hex[d + 1]][c]) / 3);
                                    }
                                    else
                                    {
                                        int g = 2 * rix[0][1] - rix[hex[d]][1] - rix[hex[d + 1]][1];
                                        for (c = 0; c < 4; c += 2)
                                            rix[0][c] =
                                            CLIP((g + rix[hex[d]][c] + rix[hex[d + 1]][c]) / 2);
                                    }
                            }
            }
            rgb = (ushort(*)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE][3])buffer;
            mrow -= top;
            mcol -= left;

            /* Convert to CIELab and differentiate in all directions:	*/
            // no effect
            for (int d = 0; d < ndir; d++)
            {
                for (int row = 2; row < mrow - 2; row++)
                    for (int col = 2; col < mcol - 2; col++)
                        cielab(rgb[d][row][col], lab[row][col]);
                for (int f = dir[d & 3], row = 3; row < mrow - 3; row++)
                    for (int col = 3; col < mcol - 3; col++)
                    {
                        lix = &lab[row][col];
                        int g = 2 * lix[0][0] - lix[f][0] - lix[-f][0];
                        drv[d][row][col] =
							float(
                            SQR(g) +
                            SQR((2 * lix[0][1] - lix[f][1] - lix[-f][1] + g * 500 / 232)) +
                            SQR((2 * lix[0][2] - lix[f][2] - lix[-f][2] - g * 500 / 580))
								);
                    }
            }

            /* Build homogeneity maps from the derivatives:			*/
            memset(homo, 0, ndir * LIBRAW_AHD_TILE * LIBRAW_AHD_TILE);
            {
                float trrow[LIBRAW_AHD_TILE];
                for (int row = 4; row < mrow - 4; row++)
                {
                    for (int col = 4; col < mcol - 4; col++)
                    {
                        float tr = drv[0][row][col];
                        for (int d = 1; d < ndir; d++)
                            tr = drv[d][row][col] < tr ? drv[d][row][col] : tr;
                        trrow[col] = tr * 8;
                    }
                    for (int dd = 0; dd < ndir; dd++)
                    {
                        const float *__restrict r0 = drv[dd][row - 1];
                        const float *__restrict r1 = drv[dd][row];
                        const float *__restrict r2 = drv[dd][row + 1];
                        unsigned char *__restrict out = (unsigned char *)homo[dd][row];
                        for (int col = 4; col < mcol - 4; col++)
                        {
                            const float tr = trrow[col];
                            out[col] = (unsigned char)(
                                (r0[col - 1] <= tr) + (r0[col] <= tr) + (r0[col + 1] <= tr) +
                                (r1[col - 1] <= tr) + (r1[col] <= tr) + (r1[col + 1] <= tr) +
                                (r2[col - 1] <= tr) + (r2[col] <= tr) + (r2[col + 1] <= tr));
                        }
                    }
                }
            }

            /* Average the most homogeneous pixels for the final result:	*/
            if (height - top < LIBRAW_AHD_TILE + 4)
                mrow = height - top + 2;
            if (width - left < LIBRAW_AHD_TILE + 4)
                mcol = width - left + 2;
            {
                const int r0 = MIN(top, 8), c0 = MIN(left, 8);
                // vertical 5-sums of homo for each direction, then horizontal 5-sums: hm == the 5x5 box
                unsigned char vs[8][LIBRAW_AHD_TILE];
                unsigned char hs[8][LIBRAW_AHD_TILE];
                for (int row = r0; row < mrow - 8; row++)
                {
                    const int cl = c0 - 2, cr = mcol - 8 + 2;
                    for (int d = 0; d < ndir; d++)
                    {
                        const unsigned char *__restrict a0 = (const unsigned char *)homo[d][row - 2];
                        const unsigned char *__restrict a1 = (const unsigned char *)homo[d][row - 1];
                        const unsigned char *__restrict a2 = (const unsigned char *)homo[d][row];
                        const unsigned char *__restrict a3 = (const unsigned char *)homo[d][row + 1];
                        const unsigned char *__restrict a4 = (const unsigned char *)homo[d][row + 2];
                        unsigned char *__restrict v = vs[d];
                        for (int col = cl; col < cr; col++)
                            v[col] = (unsigned char)(a0[col] + a1[col] + a2[col] + a3[col] + a4[col]);
                        unsigned char *__restrict hrow = hs[d];
                        for (int col = c0; col < mcol - 8; col++)
                            hrow[col] = (unsigned char)(v[col - 2] + v[col - 1] + v[col] + v[col + 1] + v[col + 2]);
                    }
                    for (int col = c0; col < mcol - 8; col++)
                    {
                        // Same decisions as dcraw's loop, without data-dependent branches.
                        int hm[8];
                        for (int d = 0; d < ndir; d++) hm[d] = hs[d][col];
                        for (int d = 0; d < ndir - 4; d++)
                        {
                            const int a = hm[d], b = hm[d + 4];
                            hm[d] = a < b ? 0 : a;
                            hm[d + 4] = a > b ? 0 : b;
                        }
                        int mx = hm[0];
                        for (int d = 1; d < ndir; d++) mx = hm[d] > mx ? hm[d] : mx;
                        mx -= mx >> 3;
                        int a0 = 0, a1 = 0, a2 = 0, n = 0;
                        for (int dd = 0; dd < ndir; dd++)
                        {
                            const int m = -(hm[dd] >= mx);
                            const ushort *px = rgb[dd][row][col];
                            a0 += px[0] & m; a1 += px[1] & m; a2 += px[2] & m; n -= m;
                        }
                        ushort *dst = image[(row + top) * width + col + left];
                        dst[0] = ushort(a0 / n); dst[1] = ushort(a1 / n); dst[2] = ushort(a2 / n);
                    }
                }
            }
        }
    }
  
    } // phases
#ifdef LIBRAW_USE_OPENMP
#pragma omp barrier
#endif

    free_omp_buffers(buffers, buffer_count);
    checkCancel(); // emulsion-cam patch: see the tile loop

    border_interpolate(8);
}
#undef fcol
