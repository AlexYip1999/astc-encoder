#pragma once
#include <cstdint>
#include <cstddef>

// Compress a single 4x4 RGBA pixel block
// block: input pixel data, length 16*4=64 bytes
// compressed: output compressed data, length 16 bytes
// Returns true on success
bool compress_astc_block(const uint8_t* block, uint8_t* compressed, int width, int height, int channels);

// Decompress a single 4x4 ASTC compressed block
// compressed: input compressed data, length 16 bytes
// block: output pixel data, length 16*4=64 bytes
// Returns true on success
bool decompress_astc_block(const uint8_t* compressed, uint8_t* block, int width, int height, int channels);
