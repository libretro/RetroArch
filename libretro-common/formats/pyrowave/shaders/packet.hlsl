/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: MIT
 * Independently authored from the formal PyroWave v1 packet definition.
 */

cbuffer Parameters : register(b0)
{
   uint image_width;
   uint image_height;
   uint block_count;
   uint operation;
   uint maximum_bytes;
   uint sequence;
   uint width;
   uint height;
   uint usability;
   uint fixed_quant_code;
   uint fixed_quantizer;
   uint reserved;
};

struct Block
{
   uint origin;
   uint width;
   uint height;
   uint index;
};

StructuredBuffer<float> coefficients : register(t0);
StructuredBuffer<Block> blocks : register(t1);
RWByteAddressBuffer workspace : register(u0);
RWStructuredBuffer<float> reconstructed : register(u1);

groupshared uint depths[128];
groupshared uint nonzeros[128];
groupshared uint group_codes[16];
groupshared uint group_bases[16];
groupshared uint group_active[16];
groupshared uint invalid_values[128];
groupshared uint ballot;
groupshared uint active_groups;
groupshared uint magnitude_bytes;
groupshared uint coefficient_count;
groupshared uint invalid_block;
groupshared uint decode_size;
groupshared uint decode_packet;
groupshared uint decode_header;
groupshared uint decode_info;
groupshared uint decode_start;
groupshared uint decode_groups;

uint payload_base()
{
   return (64 + block_count * 16 + 255) & ~255u;
}

uint quant_from_rank(uint rank)
{
   return (rank & ~7u) | (7 - (rank & 7));
}

float quant_scale(uint code)
{
   return float(8 + (code & 7)) * asfloat((128 - (code >> 3)) << 23);
}

uint2 coefficient_position(uint lane, uint pixel)
{
   uint group = lane / 8;
   uint sub = lane & 7;
   return uint2((group & 3) * 8 + (sub / 4) * 4 + pixel / 2,
      (group / 4) * 8 + (sub & 3) * 2 + (pixel & 1));
}

uint read_byte(uint address)
{
   return (workspace.Load(address & ~3u) >> ((address & 3) * 8)) & 255;
}

void write_byte(uint address, uint value)
{
   if (value != 0)
   {
      uint previous;
      workspace.InterlockedOr(address & ~3u, value << ((address & 3) * 8), previous);
   }
}

void report_error()
{
   uint previous;
   workspace.InterlockedOr(12u, 1u, previous);
}

void initialize(uint index)
{
   uint words = (payload_base() + maximum_bytes + 3) / 4;
   if (index < words)
   {
      uint value = 0;
      if (index == 0)
         value = fixed_quantizer != 0 ? fixed_quant_code : quant_from_rank(95);
      if (index == 5) value = 191;
      if (index == 7) value = 95;
      workspace.Store(index * 4, value);
   }
}

void search_quantizer()
{
   uint index;
   if (workspace.Load(32) != 0)
      return;
   uint total = 8;
   uint fits = 1;
   for (index = 0; index < block_count; index++)
   {
      uint bytes = workspace.Load(64 + index * 16);
      if (bytes > maximum_bytes - total)
         fits = 0;
      else if (fits != 0)
         total += bytes;
   }
   uint lo = workspace.Load(16);
   uint hi = workspace.Load(20);
   uint selected = workspace.Load(24);
   uint current = workspace.Load(28);
   if (fits != 0)
   {
      selected = current;
      lo = current + 1;
   }
   else if (current != 0)
      hi = current - 1;
   else
      lo = 1;
   uint done = lo > hi ? 1 : 0;
   uint next = done != 0 ? selected : lo + (hi - lo) / 2;
   workspace.Store(0, quant_from_rank(next));
   workspace.Store(16, lo);
   workspace.Store(20, hi);
   workspace.Store(24, selected);
   workspace.Store(28, next);
   workspace.Store(32, done);
}

void prefix_packets()
{
   uint index;
   uint total = 8;
   uint active = 0;
   uint fits = 1;
   for (index = 0; index < block_count; index++)
   {
      uint bytes = workspace.Load(64 + index * 16);
      workspace.Store(68 + index * 16, total);
      if (bytes > maximum_bytes - total)
         fits = 0;
      else if (fits != 0)
      {
         total += bytes;
         active += bytes != 0 ? 1 : 0;
      }
   }
   if (fits == 0)
   {
      for (index = 0; index < block_count; index++)
         workspace.Store(64 + index * 16, 0);
      total = 8;
      active = 0;
   }
   workspace.Store(4, total);
   workspace.Store(8, active);
   workspace.Store(payload_base(), (width - 1) | ((height - 1) << 14)
      | (sequence << 28) | 0x80000000u);
   workspace.Store(payload_base() + 4, active | usability);
}

void encode_coefficients(uint index, uint lane)
{
   uint pixel;
   uint i;
   uint plane;
   Block block = blocks[index];
   uint code = workspace.Load(0);
   float scale = quant_scale(code) * 0.25;
   uint values[8];
   uint negatives = 0;
   uint combined = 0;
   uint count = 0;
   uint bad = 0;
   uint group = lane / 8;
   uint sub = lane & 7;
   [unroll] for (pixel = 0; pixel < 8; pixel++)
   {
      uint2 position = coefficient_position(lane, pixel);
      float value = 0.0;
      /* FXC can flatten this conditional into a speculative SRV load.
       * Keep the address valid even for lanes beyond a partial edge block. */
      uint2 safe_position = min(position, uint2(block.width - 1, block.height - 1));
      if (position.x < block.width && position.y < block.height)
         value = coefficients[block.origin + safe_position.y * image_width + safe_position.x];
      float magnitude = floor(abs(value) / scale);
      uint integer_value = 0;
      if (!(magnitude >= 0.0 && magnitude <= 262143.0))
         bad = 1;
      else
         integer_value = uint(magnitude);
      if (!isfinite(value))
         report_error();
      values[pixel] = integer_value;
      combined |= integer_value;
      count += integer_value != 0 ? 1 : 0;
      negatives |= value < 0.0 ? 1u << pixel : 0;
   }
   depths[lane] = combined != 0 ? uint(firstbithigh(combined)) + 1 : 0;
   nonzeros[lane] = count;
   invalid_values[lane] = bad;
   GroupMemoryBarrierWithGroupSync();
   if (sub == 0)
   {
      uint maximum = 0;
      for (i = 0; i < 8; i++)
         maximum = max(maximum, depths[group * 8 + i]);
      uint base = maximum > 3 ? maximum - 3 : 0;
      uint codes = 0;
      group_active[group] = maximum != 0 ? 1 : 0;
      group_bases[group] = base;
      for (i = 0; i < 8; i++)
      {
         uint bits = max(base, depths[group * 8 + i]);
         depths[group * 8 + i] = bits;
         codes |= (bits - base) << (i * 2);
      }
      group_codes[group] = codes;
   }
   GroupMemoryBarrierWithGroupSync();
   if (lane == 0)
   {
      ballot = 0;
      active_groups = 0;
      magnitude_bytes = 0;
      coefficient_count = 0;
      invalid_block = 0;
      for (i = 0; i < 16; i++)
      {
         ballot |= group_active[i] << i;
         active_groups += group_active[i];
      }
      for (i = 0; i < 128; i++)
      {
         magnitude_bytes += depths[i];
         coefficient_count += nonzeros[i];
         invalid_block |= invalid_values[i];
      }
      if (operation == 1)
      {
         uint bytes = ballot != 0
            ? (8 + active_groups * 3 + magnitude_bytes + (coefficient_count + 7) / 8 + 3) & ~3u
            : 0;
         workspace.Store(64 + index * 16, invalid_block != 0 ? 0xffffffffu : bytes);
      }
   }
   GroupMemoryBarrierWithGroupSync();
   if (operation != 4 || invalid_block != 0 || ballot == 0
         || workspace.Load(64 + index * 16) == 0)
      return;
   uint packet = payload_base() + workspace.Load(68 + index * 16);
   if (lane == 0)
   {
      uint bytes = workspace.Load(64 + index * 16);
      workspace.Store(packet, ballot | ((bytes / 4) << 16) | (sequence << 28));
      workspace.Store(packet + 4, code | (block.index << 8));
   }
   if (group_active[group] == 0)
      return;
   uint compact = countbits(ballot & ((1u << group) - 1));
   if (sub == 0)
   {
      write_byte(packet + 8 + compact * 2, group_codes[group] & 255);
      write_byte(packet + 9 + compact * 2, group_codes[group] >> 8);
      write_byte(packet + 8 + active_groups * 2 + compact, group_bases[group]);
   }
   uint plane_offset = packet + 8 + active_groups * 3;
   uint sign_index = 0;
   for (i = 0; i < lane; i++)
   {
      plane_offset += depths[i];
      sign_index += nonzeros[i];
   }
   uint bits = depths[lane];
   for (plane = 0; plane < bits; plane++)
   {
      uint encoded = 0;
      [unroll] for (pixel = 0; pixel < 8; pixel++)
         encoded |= ((values[pixel] >> (bits - plane - 1)) & 1) << pixel;
      write_byte(plane_offset + plane, encoded);
   }
   uint sign_start = packet + 8 + active_groups * 3 + magnitude_bytes;
   [unroll] for (pixel = 0; pixel < 8; pixel++)
      if (values[pixel] != 0)
      {
         if ((negatives & (1u << pixel)) != 0)
            write_byte(sign_start + sign_index / 8, 1u << (sign_index & 7));
         sign_index++;
      }
}

void decode_coefficients(uint index, uint lane)
{
   uint pixel;
   uint i;
   uint plane;
   Block block = blocks[index];
   [unroll] for (pixel = 0; pixel < 8; pixel++)
   {
      uint2 position = coefficient_position(lane, pixel);
      if (position.x < block.width && position.y < block.height)
         reconstructed[block.origin + position.y * image_width + position.x] = 0.0;
   }
   if (lane == 0)
   {
      uint size = workspace.Load(64 + index * 16);
      uint offset = workspace.Load(68 + index * 16);
      uint frame_bytes = min(maximum_bytes, workspace.Load(4));
      invalid_block = frame_bytes < 8 ? 1 : 0;
      decode_size = size;
      decode_packet = payload_base() + offset;
      decode_header = 0;
      decode_info = 0;
      decode_start = 0;
      decode_groups = 0;
      if (size != 0 && invalid_block == 0)
      {
         if (offset > frame_bytes || size > frame_bytes - offset || size < 8)
            invalid_block = 1;
         else
         {
            decode_header = workspace.Load(decode_packet);
            decode_info = workspace.Load(decode_packet + 4);
            decode_groups = countbits(decode_header & 65535);
            decode_start = 8 + decode_groups * 3;
            if (((decode_header >> 16) & 4095) * 4 != size || (decode_header >> 31) != 0
                  || ((decode_header >> 28) & 7) != sequence || (decode_info >> 8) != block.index
                  || decode_start > size)
               invalid_block = 1;
         }
      }
   }
   GroupMemoryBarrierWithGroupSync();
   uint packet = decode_packet;
   uint mask = decode_header & 65535;
   uint groups = decode_groups;
   uint start = decode_start;
   uint group = lane / 8;
   uint compact = countbits(mask & ((1u << group) - 1));
   uint scale_code = 0;
   uint bits = 0;
   if (invalid_block == 0 && (mask & (1u << group)) != 0)
   {
      uint codes = read_byte(packet + 8 + compact * 2)
         | (read_byte(packet + 9 + compact * 2) << 8);
      scale_code = read_byte(packet + 8 + groups * 2 + compact);
      bits = (scale_code & 15) + ((codes >> ((lane & 7) * 2)) & 3);
   }
   depths[lane] = bits;
   GroupMemoryBarrierWithGroupSync();
   if (lane == 0)
   {
      magnitude_bytes = 0;
      for (i = 0; i < 128; i++) magnitude_bytes += depths[i];
      if (invalid_block == 0 && decode_size != 0 && magnitude_bytes > decode_size - start)
         invalid_block = 1;
   }
   GroupMemoryBarrierWithGroupSync();
   uint plane_offset = packet + start;
   for (i = 0; i < lane; i++) plane_offset += depths[i];
   uint values[8];
   uint count = 0;
   [unroll] for (pixel = 0; pixel < 8; pixel++) values[pixel] = 0;
   for (plane = 0; plane < (invalid_block == 0 ? bits : 0); plane++)
   {
      uint encoded = read_byte(plane_offset + plane);
      [unroll] for (pixel = 0; pixel < 8; pixel++)
         values[pixel] = (values[pixel] << 1) | ((encoded >> pixel) & 1);
   }
   [unroll] for (pixel = 0; pixel < 8; pixel++) count += values[pixel] != 0 ? 1 : 0;
   nonzeros[lane] = count;
   GroupMemoryBarrierWithGroupSync();
   if (lane == 0)
   {
      coefficient_count = 0;
      for (i = 0; i < 128; i++) coefficient_count += nonzeros[i];
      if (invalid_block == 0 && decode_size != 0
            && (coefficient_count + 7) / 8 > decode_size - start - magnitude_bytes)
         invalid_block = 1;
   }
   GroupMemoryBarrierWithGroupSync();
   if (invalid_block != 0 || decode_size == 0)
   {
      if (lane == 0 && invalid_block != 0) report_error();
      return;
   }
   uint sign_index = 0;
   for (i = 0; i < lane; i++) sign_index += nonzeros[i];
   uint sign_start = packet + start + magnitude_bytes;
   float scale = quant_scale(decode_info & 255) * (0.25 + (scale_code >> 4) * 0.125);
   [unroll] for (pixel = 0; pixel < 8; pixel++)
   {
      float value = 0.0;
      if (values[pixel] != 0)
      {
         value = (float(values[pixel]) + 0.5) * scale;
         if (((read_byte(sign_start + sign_index / 8) >> (sign_index & 7)) & 1) != 0)
            value = -value;
         sign_index++;
      }
      uint2 position = coefficient_position(lane, pixel);
      if (position.x < block.width && position.y < block.height)
         reconstructed[block.origin + position.y * image_width + position.x] = value;
   }
}

[numthreads(128, 1, 1)]
void main(uint3 group : SV_GroupID, uint lane : SV_GroupIndex)
{
   uint index = group.x + group.y * 65535;
   if (operation == 0)
      initialize(index * 128 + lane);
   else if (operation == 2)
   {
      if (lane == 0 && index == 0) search_quantizer();
   }
   else if (operation == 3)
   {
      if (lane == 0 && index == 0) prefix_packets();
   }
   else if (index < block_count)
   {
      if (operation == 5)
         decode_coefficients(index, lane);
      else if (operation == 1 || operation == 4)
         encode_coefficients(index, lane);
   }
}
