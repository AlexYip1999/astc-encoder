#include "astcenc.h"

bool compress_astc_block(const uint8_t* block, uint8_t* compressed, int width, int height, int channels)
{
    astcenc_config config;
    astcenc_error err = astcenc_config_init(
        ASTCENC_PRF_LDR_SRGB, 4, 4, 1,  // block size 4x4x1
        1.0f, 0, &config);
    if (err != ASTCENC_SUCCESS) return false;

    astcenc_context* ctx = nullptr;
    err = astcenc_context_alloc(&config, 1, &ctx);
    if (err != ASTCENC_SUCCESS) return false;

    astcenc_image image;
    image.dim_x = 4;
    image.dim_y = 4;
    image.dim_z = 1;
    image.data_type = ASTCENC_TYPE_U8;
    image.data = new void*[1];
    image.data[0] = (void*)block;

    astcenc_swizzle swz = 
    {
        static_cast<astcenc_swz>(0),
        static_cast<astcenc_swz>(1),
        static_cast<astcenc_swz>(2),
        static_cast<astcenc_swz>(3)
    }; // RGBA

    err = astcenc_compress_image(ctx, &image, &swz, compressed, 16, 0);
    astcenc_context_free(ctx);
    delete[] image.data;
    return err == ASTCENC_SUCCESS;
}

bool decompress_astc_block(const uint8_t* compressed, uint8_t* block, int width, int height, int channels)
{
    astcenc_config config;
    astcenc_error err = astcenc_config_init(
        ASTCENC_PRF_LDR_SRGB, 4, 4, 1,
        1.0f, ASTCENC_FLG_DECOMPRESS_ONLY, &config);
    if (err != ASTCENC_SUCCESS) return false;

    astcenc_context* ctx = nullptr;
    err = astcenc_context_alloc(&config, 1, &ctx);
    if (err != ASTCENC_SUCCESS) return false;

    astcenc_image image;
    image.dim_x = 4;
    image.dim_y = 4;
    image.dim_z = 1;
    image.data_type = ASTCENC_TYPE_U8;
    image.data = new void*[1];
    image.data[0] = (void*)block;

    astcenc_swizzle swz = 
    {
        static_cast<astcenc_swz>(0),
        static_cast<astcenc_swz>(1),
        static_cast<astcenc_swz>(2),
        static_cast<astcenc_swz>(3)
    };

    err = astcenc_decompress_image(ctx, compressed, 16, &image, &swz, 0);
    astcenc_context_free(ctx);
    delete[] image.data;
    return err == ASTCENC_SUCCESS;
}