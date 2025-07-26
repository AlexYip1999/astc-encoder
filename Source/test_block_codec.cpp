#include "astc_block_codec.h"
#include <iostream>
#include <vector>

int main()
{
    uint8_t block[4 * 4 * 4];
    std::cout << "Input 16 pixels, each pixel 4 bytes (R G B A), total 64 integers (0-255):\n";

    for (int i = 0; i < 64; ++i) 
    {
        int v;
        std::cin >> v;
        block[i] = static_cast<uint8_t>(v);
    }

    uint8_t compressed[16] = {0};
    if (compress_astc_block(block, compressed, 4, 4, 4)) 
    {
        std::cout << "Compressed result (16 bytes): ";
        for (int i = 0; i < 16; ++i) std::cout << (int)compressed[i] << " ";
        std::cout << std::endl;
    }
    else 
    {
        std::cout << "Compression failed!" << std::endl;
        return 1;
    }

    // Decompress
    uint8_t outblock[4 * 4 * 4] = {0};
    if (decompress_astc_block(compressed, outblock, 4, 4, 4)) 
    {
        std::cout << "Decompressed result (16 pixels, 4 bytes per pixel):\n";
        for (int i = 0; i < 16; ++i) 
        {
            std::cout << "Pixel " << i << ": ";
            for (int j = 0; j < 4; ++j) std::cout << (int)outblock[i*4+j] << " ";
            std::cout << std::endl;
        }
    }
    else 
    {
        std::cout << "Decompression failed!" << std::endl;
        return 2;
    }
    return 0;
}
