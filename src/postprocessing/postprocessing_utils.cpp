/* -*- C++ -*-
 * Copyright 2019-2025 LibRaw LLC (info@libraw.org)
 * Modified by Emulsion Cam, 2026: convert_to_rgb_loop runs its rows in parallel (EMULSION_PATCHES.md, item 2).
 *
 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include "../../internal/libraw_cxx_defs.h"

#define TBLN 65535

void LibRaw::exp_bef(float shift, float smooth)
{
  // params limits
  if (shift > 8)
    shift = 8;
  if (shift < 0.25)
    shift = 0.25;
  if (smooth < 0.0)
    smooth = 0.0;
  if (smooth > 1.0)
    smooth = 1.0;

  unsigned short *lut = (ushort *)malloc((TBLN + 1) * sizeof(unsigned short));

  if (shift <= 1.0)
  {
    for (int i = 0; i <= TBLN; i++)
      lut[i] = (unsigned short)((float)i * shift);
  }
  else
  {
    float x1, x2, y1, y2;

    float cstops = log(shift) / log(2.0f);
    float room = cstops * 2;
    float roomlin = powf(2.0f, room);
    x2 = (float)TBLN;
    x1 = (x2 + 1) / roomlin - 1;
    y1 = x1 * shift;
    y2 = x2 * (1 + (1 - smooth) * (shift - 1));
    float sq3x = powf(x1 * x1 * x2, 1.0f / 3.0f);
    float B = (y2 - y1 + shift * (3 * x1 - 3.0f * sq3x)) /
              (x2 + 2.0f * x1 - 3.0f * sq3x);
    float A = (shift - B) * 3.0f * powf(x1 * x1, 1.0f / 3.0f);
    float CC = y2 - A * powf(x2, 1.0f / 3.0f) - B * x2;
    for (int i = 0; i <= TBLN; i++)
    {
      float X = (float)i;
      float Y = A * powf(X, 1.0f / 3.0f) + B * X + CC;
      if (i < x1)
        lut[i] = (unsigned short)((float)i * shift);
      else
        lut[i] = Y < 0 ? 0 : (Y > TBLN ? TBLN : (unsigned short)(Y));
    }
  }
  for (int i = 0; i < S.height * S.width; i++)
  {
    imgdata.image[i][0] = lut[imgdata.image[i][0]];
    imgdata.image[i][1] = lut[imgdata.image[i][1]];
    imgdata.image[i][2] = lut[imgdata.image[i][2]];
    imgdata.image[i][3] = lut[imgdata.image[i][3]];
  }

  if (C.data_maximum <= TBLN)
    C.data_maximum = lut[C.data_maximum];
  if (C.maximum <= TBLN)
    C.maximum = lut[C.maximum];
  free(lut);
}

void LibRaw::convert_to_rgb_loop(float out_cam[3][4])
{
  int row, col, c;
  float out[3];
  ushort *img;
  memset(libraw_internal_data.output_data.histogram, 0,
         sizeof(int) * LIBRAW_HISTOGRAM_SIZE * 4);
  if (libraw_internal_data.internal_output_params.raw_color)
  {
    for (img = imgdata.image[0], row = 0; row < S.height; row++)
    {
      for (col = 0; col < S.width; col++, img += 4)
      {
        for (c = 0; c < imgdata.idata.colors; c++)
        {
          libraw_internal_data.output_data.histogram[c][img[c] >> 3]++;
        }
      }
    }
  }
  else if (imgdata.idata.colors == 3)
  {
    // Rows in parallel; the histogram (only read by auto-bright / gamma output) per thread, merged.
    const int H = S.height, Wd = S.width;
    int (*hist)[LIBRAW_HISTOGRAM_SIZE] = libraw_internal_data.output_data.histogram;
#if defined(LIBRAW_USE_OPENMP)
#pragma omp parallel
#endif
    {
      std::vector<int> local(3 * LIBRAW_HISTOGRAM_SIZE, 0);
#if defined(LIBRAW_USE_OPENMP)
#pragma omp for schedule(static)
#endif
      for (int r = 0; r < H; r++)
      {
        ushort *p = imgdata.image[0] + (size_t)r * Wd * 4;
        for (int cc = 0; cc < Wd; cc++, p += 4)
        {
          float o0 = out_cam[0][0] * p[0] + out_cam[0][1] * p[1] + out_cam[0][2] * p[2];
          float o1 = out_cam[1][0] * p[0] + out_cam[1][1] * p[1] + out_cam[1][2] * p[2];
          float o2 = out_cam[2][0] * p[0] + out_cam[2][1] * p[1] + out_cam[2][2] * p[2];
          p[0] = CLIP((int)o0);
          p[1] = CLIP((int)o1);
          p[2] = CLIP((int)o2);
          local[p[0] >> 3]++;
          local[LIBRAW_HISTOGRAM_SIZE + (p[1] >> 3)]++;
          local[2 * LIBRAW_HISTOGRAM_SIZE + (p[2] >> 3)]++;
        }
      }
#if defined(LIBRAW_USE_OPENMP)
#pragma omp critical
#endif
      for (int k = 0; k < LIBRAW_HISTOGRAM_SIZE; k++)
      {
        hist[0][k] += local[k];
        hist[1][k] += local[LIBRAW_HISTOGRAM_SIZE + k];
        hist[2][k] += local[2 * LIBRAW_HISTOGRAM_SIZE + k];
      }
    }
  }
  else if (imgdata.idata.colors == 4)
  {
    for (img = imgdata.image[0], row = 0; row < S.height; row++)
    {
      for (col = 0; col < S.width; col++, img += 4)
      {
        out[0] = out_cam[0][0] * img[0] + out_cam[0][1] * img[1] +
                 out_cam[0][2] * img[2] + out_cam[0][3] * img[3];
        out[1] = out_cam[1][0] * img[0] + out_cam[1][1] * img[1] +
                 out_cam[1][2] * img[2] + out_cam[1][3] * img[3];
        out[2] = out_cam[2][0] * img[0] + out_cam[2][1] * img[1] +
                 out_cam[2][2] * img[2] + out_cam[2][3] * img[3];
        img[0] = CLIP((int)out[0]);
        img[1] = CLIP((int)out[1]);
        img[2] = CLIP((int)out[2]);
        libraw_internal_data.output_data.histogram[0][img[0] >> 3]++;
        libraw_internal_data.output_data.histogram[1][img[1] >> 3]++;
        libraw_internal_data.output_data.histogram[2][img[2] >> 3]++;
        libraw_internal_data.output_data.histogram[3][img[3] >> 3]++;
      }
    }
  }
}

void LibRaw::scale_colors_loop(float scale_mul[4])
{
  unsigned size = S.iheight * S.iwidth;

  if (C.cblack[4] && C.cblack[5])
  {
    int val;
    for (unsigned i = 0; i < size; i++)
    {
      for (unsigned c = 0; c < 4; c++)
      {
        if (!(val = imgdata.image[i][c])) continue;
        val -= C.cblack[6 + i / S.iwidth % C.cblack[4] * C.cblack[5] +
                        i % S.iwidth % C.cblack[5]];
        val -= C.cblack[c];
        val = int(val * scale_mul[c]);
        imgdata.image[i][c] = CLIP(val);
      }
    }
  }
  else if (C.cblack[0] || C.cblack[1] || C.cblack[2] || C.cblack[3])
  {
    for (unsigned i = 0; i < size; i++)
    {
      for (unsigned c = 0; c < 4; c++)
      {
        int val = imgdata.image[i][c];
        if (!val) continue;
        val -= C.cblack[c];
        val = int(val * scale_mul[c]);
        imgdata.image[i][c] = CLIP(val);
      }
    }
  }
  else // BL is zero
  {
    for (unsigned i = 0; i < size; i++)
    {
      for (unsigned c = 0; c < 4; c++)
      {
        int val = imgdata.image[i][c];
        val = int(val * scale_mul[c]);
        imgdata.image[i][c] = CLIP(val);
      }
    }
  }
}
