#ifndef SCALER_INT_H__
#define SCALER_INT_H__

#include "scaler.h"

void scaler_argb8888_vert(const struct scaler_ctx *ctx, void *output, int stride);
void scaler_argb8888_horiz(const struct scaler_ctx *ctx, const void *input, int stride);

#endif

