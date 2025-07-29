#include "astc_block_codec.h"
#include "astcenc_internal.h"
#include "astcenc.h"
#include <iostream>
#include <iomanip> // Added for std::setw and std::setfill
#include <set> // Added for std::set
#include <string>
#include <fstream> // Added for file parsing
#include <cctype> // Added for std::isspace
#include <sstream> // Added for std::stringstream
#include <cmath> // Added for sqrt function
#include <array>

#define QUANT_CASE(level) if (str == "QUANT_" #level) return QUANT_##level;

// Implementation of get_block_size_descriptor function
void* get_block_size_descriptor(int x, int y, int z)
{
    static block_size_descriptor bsd;
    init_block_size_descriptor(x, y, z, false, 4, 1.0f, bsd);
    return &bsd;
}

// Function to validate block size
bool is_valid_block_size(int x, int y)
{
    return is_legal_2d_block_size(x, y);
}

// Function to get weight count for a specific block size and block_mode
int get_weight_count_for_block_mode(int block_width, int block_height, int block_mode)
{
    block_size_descriptor* bsd = static_cast<block_size_descriptor*>(get_block_size_descriptor(block_width, block_height, 1));
    if (!bsd) return 0;

    // Check if the packed index is valid
    unsigned int packed_index = bsd->block_mode_packed_index[block_mode];
    if (packed_index == BLOCK_BAD_BLOCK_MODE || packed_index >= bsd->block_mode_count_all)
    {
        return 0;
    }
    
    try
    {
        const auto& bm = bsd->block_modes[packed_index];
        const auto& di = bsd->get_decimation_info(bm.decimation_mode);
        int weight_count = di.weight_count;
        
        // For dual plane modes, we need twice the weight count
        if (bm.is_dual_plane)
        {
            weight_count *= 2;
        }
        
        return weight_count;
    }
    catch (...)
    {
        return 0;
    }
}

// Function to print supported block sizes
void print_supported_block_sizes()
{
    std::cout << "Supported ASTC block sizes:" << std::endl;
    std::cout << "  4x4, 5x4, 5x5, 6x5, 6x6, 8x5, 8x6, 8x8," << std::endl;
    std::cout << "  10x5, 10x6, 10x8, 10x10, 12x10, 12x12" << std::endl;
}

// Function to explain block_mode and show available modes for a block size
void explain_block_mode(int block_width, int block_height)
{
    std::cout << "=== Block Mode Explanation for " << block_width << "x" << block_height << " blocks ===" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Block Mode determines:" << std::endl;
    std::cout << "1. Decimation Mode: How weights are distributed across the block" << std::endl;
    std::cout << "2. Weight Quantization: How many bits are used to encode each weight" << std::endl;
    std::cout << "3. Dual Plane: Whether to use separate weight planes for different color components" << std::endl;
    std::cout << std::endl;
    
    block_size_descriptor* bsd = static_cast<block_size_descriptor*>(get_block_size_descriptor(block_width, block_height, 1));
    if (!bsd)
    {
        std::cout << "Error: Could not get block size descriptor" << std::endl;
        return;
    }
    
    std::cout << "Available block modes for " << block_width << "x" << block_height << ":" << std::endl;
    std::cout << "Mode | Decimation | Weight Quant | Dual Plane | Weight Count | Description" << std::endl;
    std::cout << "-----|------------|--------------|------------|--------------|-------------" << std::endl;
    
    // Show first few block modes
    int max_modes_to_show = 10;
    int modes_shown = 0;
    
    for (unsigned int mode = 0; mode < WEIGHTS_MAX_BLOCK_MODES && modes_shown < max_modes_to_show; ++mode)
    {
        // Check if this block block_mode is valid before accessing it
        if (mode >= bsd->block_mode_count_all)
        {
            continue; // Skip invalid modes
        }
        
        // Check if the packed index is valid
        unsigned int packed_index = bsd->block_mode_packed_index[mode];
        if (packed_index == BLOCK_BAD_BLOCK_MODE || packed_index >= bsd->block_mode_count_all)
        {
            continue; // Skip invalid modes
        }
        
        try
        {
            const auto& bm = bsd->block_modes[packed_index];
            const auto& di = bsd->get_decimation_info(bm.decimation_mode);
            
            std::cout << std::setw(4) << mode << " | ";
            std::cout << std::setw(10) << (int)bm.decimation_mode << " | ";
            std::cout << std::setw(12) << (int)bm.quant_mode << " | ";
            std::cout << std::setw(10) << (bm.is_dual_plane ? "Yes" : "No") << " | ";
            std::cout << std::setw(12) << di.weight_count << " | ";
            
            // Description based on quantization
            std::string desc;
            switch (static_cast<int>(bm.quant_mode))
            {
                case 0: desc = "2 levels"; break;
                case 1: desc = "3 levels"; break;
                case 2: desc = "4 levels"; break;
                case 3: desc = "5 levels"; break;
                case 4: desc = "6 levels"; break;
                case 5: desc = "8 levels"; break;
                case 6: desc = "10 levels"; break;
                case 7: desc = "12 levels"; break;
                case 8: desc = "16 levels"; break;
                case 9: desc = "20 levels"; break;
                case 10: desc = "24 levels"; break;
                case 11: desc = "32 levels"; break;
                case 12: desc = "40 levels"; break;
                case 13: desc = "48 levels"; break;
                case 14: desc = "64 levels"; break;
                case 15: desc = "80 levels"; break;
                case 16: desc = "96 levels"; break;
                case 17: desc = "128 levels"; break;
                case 18: desc = "160 levels"; break;
                case 19: desc = "192 levels"; break;
                case 20: desc = "256 levels"; break;
                default: desc = "Unknown"; break;
            }
            
            if (bm.is_dual_plane)
            {
                desc += " (dual)";
            }
            
            std::cout << desc << std::endl;
            modes_shown++;
        }
        catch (...)
        {
            // Skip invalid modes
            continue;
        }
    }
    
    if (modes_shown == 0)
    {
        std::cout << "No valid block modes found for this block size." << std::endl;
    }
    else if (modes_shown >= max_modes_to_show)
    {
        std::cout << "... (showing first " << max_modes_to_show << " modes)" << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "Key concepts:" << std::endl;
    std::cout << "- Decimation Mode: Determines the weight grid pattern (e.g., 4x4, 6x6, 8x8)" << std::endl;
    std::cout << "- Weight Quantization: Higher values = more precision but more bits" << std::endl;
    std::cout << "- Dual plane: Separate weight planes for different color components" << std::endl;
    std::cout << "- Weight Count: Total number of weights stored for this block_mode" << std::endl;
    std::cout << std::endl;
    std::cout << "Trade-offs:" << std::endl;
    std::cout << "- Lower quantization = smaller file size but lower quality" << std::endl;
    std::cout << "- Higher quantization = better quality but larger file size" << std::endl;
    std::cout << "- Dual plane = better color separation but uses more bits" << std::endl;
}

// Calculate block_mode from individual parameters
int calculate_block_mode(int block_width, int block_height, int weight_grid_size, int quant_mode, bool is_dual_plane)
{
    block_size_descriptor* bsd = static_cast<block_size_descriptor*>(get_block_size_descriptor(block_width, block_height, 1));
    if (!bsd) return -1;

    // Search through all available block modes to find a match
    for (unsigned int mode = 0; mode < WEIGHTS_MAX_BLOCK_MODES; ++mode)
    {
        // Check if the packed index is valid
        unsigned int packed_index = bsd->block_mode_packed_index[mode];
        if (packed_index == BLOCK_BAD_BLOCK_MODE || packed_index >= bsd->block_mode_count_all)
        {
            continue;
        }
        
        try
        {
            const auto& bm = bsd->block_modes[packed_index];
            const auto& di = bsd->get_decimation_info(bm.decimation_mode);

            // Check if this block_mode matches our parameters
            // weight_grid_size should be the total number of weights (e.g., 16 for 4x4, 36 for 6x6)
            // quant_mode should match the quant_method enum value, not the quantization level
            if (di.weight_count == weight_grid_size && 
                bm.get_weight_quant_mode() == quant_mode && 
                bm.is_dual_plane == is_dual_plane)
            {
                return mode;
            }
        }
        catch (...)
        {
            // Skip invalid modes
            continue;
        }
    }
    
    // No matching block_mode found
    if (is_dual_plane)
    {
        std::cout << "Warning: No dual plane block_mode found for " << block_width << "x" << block_height 
                  << " with weight_grid_size=" << weight_grid_size 
                  << ", quant_mode=" << quant_mode << std::endl;
        std::cout << "This is normal for small block sizes due to bit budget constraints." << std::endl;
        std::cout << "Try using:" << std::endl;
        std::cout << "- Larger block sizes (6x6, 8x8, etc.)" << std::endl;
        std::cout << "- Lower quantization levels (QUANT_2, QUANT_3, QUANT_4)" << std::endl;
        std::cout << "- Single plane block_mode (is_dual_plane=false)" << std::endl;
        std::cout << "Use -ldp " << block_width << "x" << block_height << " to see available dual plane modes." << std::endl;
    }
    
    return -1; // No matching block_mode found
}

// Get block_mode parameters from a given block_mode
bool get_block_mode_parameters(int block_width, int block_height, int block_mode, 
                              int& decimation_mode, int& quant_mode, bool& is_dual_plane)
{
    block_size_descriptor* bsd = static_cast<block_size_descriptor*>(get_block_size_descriptor(block_width, block_height, 1));
    if (!bsd) return false;

    // Check if block_mode is valid
    if (block_mode < 0 || (unsigned int)block_mode >= bsd->block_mode_count_all)
    {
        return false;
    }
    
    // Check if the packed index is valid
    unsigned int packed_index = bsd->block_mode_packed_index[block_mode];
    if (packed_index == BLOCK_BAD_BLOCK_MODE || packed_index >= bsd->block_mode_count_all)
    {
        return false;
    }

    try
    {
        const auto& bm = bsd->block_modes[packed_index];
        decimation_mode = static_cast<int>(bm.decimation_mode);
        quant_mode = static_cast<int>(bm.quant_mode);  // This is the enum value, not quantization level
        is_dual_plane = bm.is_dual_plane;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

// List available decimation modes for a block size
void list_decimation_modes(int block_width, int block_height)
{
    std::cout << "=== Available Decimation Modes for " << block_width << "x" << block_height << " blocks ===" << std::endl;
    std::cout << std::endl;
    
    block_size_descriptor* bsd = static_cast<block_size_descriptor*>(get_block_size_descriptor(block_width, block_height, 1));
    if (!bsd)
    {
        std::cout << "Error: Could not get block size descriptor" << std::endl;
        return;
    }
    
    std::cout << "Decimation Mode | Weight Grid | Weight Count | Description" << std::endl;
    std::cout << "----------------|-------------|--------------|-------------" << std::endl;
    
    // Track unique decimation modes
    std::set<int> seen_modes;
    
    for (unsigned int mode = 0; mode < WEIGHTS_MAX_BLOCK_MODES; ++mode)
    {
        // Check if this block block_mode is valid before accessing it
        if (mode >= bsd->block_mode_count_all)
        {
            continue; // Skip invalid modes
        }
        
        // Check if the packed index is valid
        unsigned int packed_index = bsd->block_mode_packed_index[mode];
        if (packed_index == BLOCK_BAD_BLOCK_MODE || packed_index >= bsd->block_mode_count_all)
        {
            continue; // Skip invalid modes
        }
        
        try
        {
            const auto& bm = bsd->block_modes[packed_index];
            const auto& di = bsd->get_decimation_info(bm.decimation_mode);
            
            // Only show each decimation block_mode once
            if (seen_modes.find(bm.decimation_mode) != seen_modes.end())
                continue;
                
            seen_modes.insert(bm.decimation_mode);
            
            std::cout << std::setw(14) << (int)bm.decimation_mode << " | ";
            std::cout << std::setw(10) << (int)di.weight_x << "x" << (int)di.weight_y << " | ";
            std::cout << std::setw(12) << di.weight_count << " | ";
            
            // Description based on weight grid
            std::string desc = std::to_string(di.weight_x) + "x" + std::to_string(di.weight_y) + " grid";
            
            if (di.weight_count == block_width * block_height)
            {
                desc += " (full resolution)";
            }
            else
            {
                desc += " (decimated)";
            }
            
            std::cout << desc << std::endl;
        }
        catch (...)
        {
            // Skip invalid modes
            continue;
        }
    }
    
    std::cout << std::endl;
    std::cout << "Note: Decimation modes determine how weights are distributed across the block." << std::endl;
    std::cout << "      Full resolution means one weight per texel, decimated means fewer weights." << std::endl;
}

// List available quantization levels
void list_quantization_levels()
{
    std::cout << "=== Available Quantization Levels ===" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Level | Quantization | Bits | Description" << std::endl;
    std::cout << "------|--------------|------|-------------" << std::endl;
    
    const char* level_names[] = {
        "QUANT_2", "QUANT_3", "QUANT_4", "QUANT_5", "QUANT_6",
        "QUANT_8", "QUANT_10", "QUANT_12", "QUANT_16", "QUANT_20",
        "QUANT_24", "QUANT_32", "QUANT_40", "QUANT_48", "QUANT_64",
        "QUANT_80", "QUANT_96", "QUANT_128", "QUANT_160", "QUANT_192", "QUANT_256"
    };
    
    const int quant_levels[] = {2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 32, 40, 48, 64, 80, 96, 128, 160, 192, 256};
    
    for (int i = 0; i <= 20; ++i)
    {
        int bits = 0;
        int temp = quant_levels[i];
        while (temp > 1)
        {
            bits++;
            temp >>= 1;
        }
        
        std::cout << std::setw(5) << i << " | ";
        std::cout << std::setw(12) << level_names[i] << " | ";
        std::cout << std::setw(4) << bits << " | ";
        
        std::string desc;
        if (i <= 4)
            desc = "Very low quality, small size";
        else if (i <= 8)
            desc = "Low quality, small size";
        else if (i <= 12)
            desc = "Medium quality, balanced";
        else if (i <= 16)
            desc = "High quality, larger size";
        else
            desc = "Very high quality, large size";
            
        std::cout << desc << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "Note: Higher quantization levels provide better quality but use more bits." << std::endl;
    std::cout << "      Choose based on your quality vs. size requirements." << std::endl;
}

// List available dual plane modes for a block size
void list_dual_plane_modes(int block_width, int block_height)
{
    block_size_descriptor* bsd = static_cast<block_size_descriptor*>(get_block_size_descriptor(block_width, block_height, 1));
    if (!bsd)
    {
        std::cerr << "Error: Invalid block size " << block_width << "x" << block_height << std::endl;
        return;
    }

    std::cout << "Available dual plane modes for " << block_width << "x" << block_height << " blocks:" << std::endl;
    std::cout << "Mode\tPacked\tDecimation\tQuant\tWeight_Bits\tDual_Plane" << std::endl;
    std::cout << "----\t-----\t----------\t-----\t-----------\t----------" << std::endl;

    int dual_plane_count = 0;
    
    for (unsigned int mode = 0; mode < WEIGHTS_MAX_BLOCK_MODES; ++mode)
    {
        // Check if this block block_mode is valid before accessing it
        if (mode >= bsd->block_mode_count_all)
        {
            continue;
        }
        
        // Check if the packed index is valid
        unsigned int packed_index = bsd->block_mode_packed_index[mode];
        if (packed_index == BLOCK_BAD_BLOCK_MODE || packed_index >= bsd->block_mode_count_all)
        {
            continue;
        }
        
        try
        {
            const auto& bm = bsd->block_modes[packed_index];
            const auto& di = bsd->get_decimation_info(bm.decimation_mode);
            
            if (bm.is_dual_plane)
            {
                std::cout << mode << "\t" << packed_index << "\t" 
                          << (int)bm.decimation_mode << "\t\t" 
                          << (int)bm.quant_mode << "\t" 
                          << (int)bm.weight_bits << "\t\t" 
                          << (bm.is_dual_plane ? "Yes" : "No") << std::endl;
                dual_plane_count++;
            }
        }
        catch (...)
        {
            continue;
        }
    }
    
    if (dual_plane_count == 0)
    {
        std::cout << "No dual plane modes available for " << block_width << "x" << block_height << " blocks." << std::endl;
        std::cout << "This is normal for small block sizes due to bit budget constraints." << std::endl;
        std::cout << "Dual plane modes require more bits and are typically only available for:" << std::endl;
        std::cout << "- Larger block sizes (6x6, 8x8, etc.)" << std::endl;
        std::cout << "- Lower quantization levels (QUANT_2, QUANT_3, QUANT_4)" << std::endl;
        std::cout << "- Single partition blocks" << std::endl;
    }
    else
    {
        std::cout << "Total dual plane modes: " << dual_plane_count << std::endl;
    }
}

// Manual ASTC block construction, allowing users to directly set block block_mode, partition, endpoints, weights, etc.
// This is a minimal example: configurable LDR block size, single partition, direct endpoints/weights
bool manual_construct_astc_block(
    uint8_t* compressed,
    int block_width, int block_height,
    int block_mode, int partition_index, int partition_count,
    const uint8_t* endpoints, int endpoint_count,
    const float* weights, int weight_count,
    bool has_alpha,
    int plane2_component,
    uint8_t* result)
{
    // 1. Get block_size_descriptor for the specified block size
    block_size_descriptor* bsd = static_cast<block_size_descriptor*>(get_block_size_descriptor(block_width, block_height, 1));
    if (!bsd)
    {
        return false;
    }

    // 2. Get the decimation info for the specified block block_mode
    if (block_mode <= 0)
    {
        return false;
    }

    unsigned int packed_index = bsd->block_mode_packed_index[block_mode];
    if (packed_index == BLOCK_BAD_BLOCK_MODE || packed_index >= bsd->block_mode_count_all)
    {
        return false;
    }

    const auto& bm = bsd->block_modes[packed_index];
    const auto& di = bsd->get_decimation_info(bm.decimation_mode);
    const int actual_weight_count = di.weight_count;
    const bool is_dual_plane = bm.is_dual_plane;
    const int required_weight_count = is_dual_plane ? actual_weight_count * 2 : actual_weight_count;

    if (weight_count < required_weight_count)
    {
        std::cout << "Warning: Provided " << weight_count << " weights, but " 
                  << required_weight_count << " are needed for " << block_width << "x" 
                  << block_height << " block with block_mode " << block_mode 
                  << (is_dual_plane ? " (dual plane)" : " (single plane)") << std::endl;
    }

    symbolic_compressed_block scb {};
    scb.block_type = SYM_BTYPE_NONCONST;
    scb.block_mode = block_mode;
    scb.partition_count = partition_count;
    scb.partition_index = partition_index;
    scb.plane2_component = is_dual_plane ? plane2_component : -1;
    scb.color_formats_matched = 0;
    scb.color_formats[0] = has_alpha ? FMT_RGBA : FMT_RGB;
    scb.color_formats[1] = 0;
    scb.color_formats[2] = 0;
    scb.color_formats[3] = 0;
    scb.color_values[partition_index][0] = endpoints[0];
    scb.color_values[partition_index][2] = endpoints[1];
    scb.color_values[partition_index][4] = endpoints[2];
    scb.color_values[partition_index][6] = endpoints[3];
    scb.color_values[partition_index][1] = endpoints[4];
    scb.color_values[partition_index][3] = endpoints[5];
    scb.color_values[partition_index][5] = endpoints[6];
    scb.color_values[partition_index][7] = endpoints[7];

    // Get weight quantization levels for proper scaling
    quant_method weight_quant_method = bm.get_weight_quant_mode();
    int weight_range = get_quant_level(weight_quant_method) - 1;
    
    // Set weights for both planes (plane 1 and plane 2 if dual plane)
    for (int i = 0; i < actual_weight_count; i++)
    {
        // Set plane 1 weights: convert from 0-1 range to 0-64 range
        // weights[i] is in 0-1 range, multiply by 64 to get 0-64 range
        scb.weights[i] = i < weight_count ? weights[i] * 64.0f : 64.0f; // Default to middle value

        if (is_dual_plane)
        {
            scb.weights[i + WEIGHTS_PLANE2_OFFSET] = i + actual_weight_count < weight_count ? 
                weights[i + actual_weight_count] * 64.0f : 64.0f; // Default to middle value
        }
    }

    // 1. 计算weight bits
    int weight_count_total = di.weight_count * (bm.is_dual_plane ? 2 : 1);
    int weight_bits = get_ise_sequence_bitcount(weight_count_total, static_cast<quant_method>(bm.get_weight_quant_mode()));

    // 2. 计算剩余bit budget
    static const int8_t free_bits_for_partition_count[4] = 
    {
        115 - 4,
        111 - 4 - PARTITION_INDEX_BITS,
        108 - 4 - PARTITION_INDEX_BITS,
        105 - 4 - PARTITION_INDEX_BITS
    };

    int color_budget = free_bits_for_partition_count[partition_count - 1] - weight_bits;
    if (bm.is_dual_plane)
    {
        color_budget -= 2;
    }

    // 3. 统计endpoint整数数目
    int endpoint_ints = (has_alpha ? 8 : 6); // 这里只考虑单分区RGBA/RGB
    // 4. 查表 quant_mode_table，选最大可用 color quant

    extern const int8_t quant_mode_table[10][128];
    int color_quant_level = QUANT_4;
    if (color_budget > 0 && endpoint_ints/2 < 10 && color_budget < 128)
    {
        int q = quant_mode_table[endpoint_ints/2][color_budget];
        if (q >= QUANT_2 && q <= QUANT_256)
        {
            color_quant_level = q;
        }
    }

    scb.quant_mode = static_cast<quant_method>(color_quant_level);
    scb.errorval = 0.0f;

    std::cout << "Color quant: QUANT_" << std::to_string(get_quant_level(scb.quant_mode)) << " (" << scb.quant_mode << ") \n";

    symbolic_to_physical(*bsd, scb, compressed);
    // Save to params
    if (result) 
    {
        memcpy(result, compressed, 16);
    }

    std::cout << "\n";

    return true;
}

bool compress_astc_block2d(const uint8_t* block, uint8_t* compressed, int width, int height, int channels, astcenc_type type)
{
    astcenc_config config;
    astcenc_error err = astcenc_config_init(
        ASTCENC_PRF_LDR, width, height, 1,  // block size 4x4x1
        1.0f, 0, &config);
    if (err != ASTCENC_SUCCESS) return false;

    astcenc_context* ctx = nullptr;
    err = astcenc_context_alloc(&config, 1, &ctx);
    if (err != ASTCENC_SUCCESS) return false;

    astcenc_image image;
    image.dim_x = width;
    image.dim_y = height;
    image.dim_z = 1;
    image.data_type = type;
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

// Parameter parser implementation
bool ASTCParameterParser::parse(int argc, char* argv[], Parameters& params)
{
    // If no arguments provided, load default configuration
    if (argc <= 1)
    {
        std::cout << "No arguments provided. Loading default configuration from: " << DEFAULT_CONFIG_FILE_PATH << std::endl;
        if (parse_config_file(DEFAULT_CONFIG_FILE_PATH, params))
        {
            std::cout << "Default configuration loaded successfully. \n\n";
            return true;
        }
        else
        {
            std::cerr << "Failed to load default configuration. Showing help... \n\n";
            print_usage(argv[0]);
            return false;
        }
    }

    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help")
        {
            params.show_help = true;
        }
        else if (arg == "-l" || arg == "--list")
        {
            params.list_block_sizes = true;
        }
        else if (arg == "-t" || arg == "--test")
        {
            params.test_public_functions = true;
        }
        else if (arg == "-q" || arg == "--quantization")
        {
            params.list_quantization_levels = true;
        }
        else if (arg == "-ldp" && i + 1 < argc)
        {
            params.list_dual_plane_modes = true;
            if (!parse_block_size(argv[++i], params.block_width, params.block_height))
            {
                std::cerr << "Error: Invalid block size format. Use WxH (e.g., 6x6)" << std::endl;
                return false;
            }
        }
        else if (arg == "-x" && i + 1 < argc)
        {
            params.explain_block_mode = true;
            if (!parse_block_size(argv[++i], params.block_width, params.block_height))
            {
                std::cerr << "Error: Invalid block size format for explanation. Use WxH (e.g., 6x6)" << std::endl;
                return false;
            }
        }
        else if (arg == "-d" && i + 1 < argc)
        {
            params.list_decimation_modes = true;
            if (!parse_block_size(argv[++i], params.block_width, params.block_height))
            {
                std::cerr << "Error: Invalid block size format for decimation modes. Use WxH (e.g., 6x6)" << std::endl;
                return false;
            }
        }
        else if (arg == "-c" && i + 4 < argc)
        {
            params.calculate_block_mode = true;
            if (!parse_block_size(argv[++i], params.block_width, params.block_height))
            {
                std::cerr << "Error: Invalid block size format. Use WxH (e.g., 6x6)" << std::endl;
                return false;
            }
            if (!parse_int(argv[++i], params.weight_grid_size))
            {
                std::cerr << "Error: Invalid decimation block_mode" << std::endl;
                return false;
            }
            params.weight_quant_str = argv[++i];
            // Validate the quantization string
            if (parse_weight_quant_str(params.weight_quant_str) == QUANT_4 && params.weight_quant_str != "QUANT_4")
            {
                std::cerr << "Error: Invalid quantization block_mode. Use format like QUANT_4, QUANT_8, etc." << std::endl;
                return false;
            }
            if (!parse_bool(argv[++i], params.is_dual_plane_mode))
            {
                std::cerr << "Error: Invalid dual plane flag (use 0 or 1)" << std::endl;
                return false;
            }
        }
        else if (arg == "-a" && i + 1 < argc)
        {
            if (!parse_block_size(argv[++i], params.block_width, params.block_height))
            {
                std::cerr << "Error: Invalid block size format. Use WxH (e.g., 6x6)" << std::endl;
                return false;
            }
        }
        else if (arg == "-v" && i + 1 < argc)
        {
            params.validate_hlsl = true;
            params.hlsl_preset_name = argv[++i];
        }
        else if (arg == "--test-all-hlsl")
        {
            params.test_all_hlsl_presets = true;
        }
        else if (arg == "--list-hlsl-presets")
        {
            params.list_hlsl_presets = true;
        }
        else if (arg == "-f" && i + 1 < argc)
        {
            params.load_from_config = true;
            params.config_file_path = argv[++i];
        }
        else if (arg == "--create-config" && i + 1 < argc)
        {
            return create_example_config_file(argv[++i]);
        }
        else if (arg == "-b" && i + 1 < argc)
        {
            if (!parse_block_size(argv[++i], params.block_width, params.block_height))
            {
                std::cerr << "Error: Invalid block size format. Use WxH (e.g., 6x6)" << std::endl;
                return false;
            }
        }
        else if (arg == "-m" && i + 1 < argc)
        {
            if (!parse_int(argv[++i], params.block_mode))
            {
                std::cerr << "Error: Invalid block block_mode" << std::endl;
                return false;
            }
        }
        else if (arg == "-p" && i + 1 < argc)
        {
            if (!parse_int(argv[++i], params.partition))
            {
                std::cerr << "Error: Invalid partition index" << std::endl;
                return false;
            }
        }
        else if (arg == "-e" && i + 1 < argc)
        {
            if (!parse_endpoints(argv[++i], params.endpoints))
            {
                std::cerr << "Error: Invalid endpoint format. Use 8 comma-separated values (e.g., 255,255,255,255,0,0,0,0)" << std::endl;
                return false;
            }
        }
        else if (arg == "-w" && i + 1 < argc)
        {
            if (!parse_weights(argv[++i], params.weights))
            {
                std::cerr << "Error: Invalid weight format. Use comma-separated values (e.g., 128,128,128,128)" << std::endl;
                return false;
            }
        }
        else if (arg == "--plane2-component" && i + 1 < argc)
        {
            params.plane2_component_str = argv[++i];
        }
        else
        {
            std::cerr << "Error: Unknown option '" << arg << "'" << std::endl;
            std::cerr << "Use -h for help" << std::endl;
            return false;
        }
    }
    
    return true;
}

void ASTCParameterParser::print_usage(const char* program_name)
{
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -b <width>x<height>  Block size (default: 4x4)" << std::endl;
    std::cout << "  -m <block_mode>            Block block_mode (default: 0)" << std::endl;
    std::cout << "  -p <partition>       Partition index (default: 0)" << std::endl;
    std::cout << "  -e <endpoints>       Endpoint values (8 values, default: 255,255,255,255,0,0,0,0)" << std::endl;
    std::cout << "  -w <weights>         Weight values (comma-separated, default: all 128)" << std::endl;
    std::cout << "  -h                   Show this help message" << std::endl;
    std::cout << "  -l                   List supported block sizes" << std::endl;
    std::cout << "  -t                   Test public functions" << std::endl;
    std::cout << "  -x <width>x<height>  Explain block modes for specific block size" << std::endl;
    std::cout << "  -d <width>x<height>  List decimation modes for specific block size" << std::endl;
    std::cout << "  -q                   List quantization levels" << std::endl;
    std::cout << "  -ldp <width>x<height> List dual plane modes for specific block size" << std::endl;
    std::cout << "  -c <WxH> <weight_grid> <quant> <dual>  Calculate block_mode from parameters" << std::endl;
    std::cout << "  -a <WxH> <block_mode>      Analyze block_mode parameters" << std::endl;
    std::cout << "  -v <preset>          Validate HLSL preset configuration and execute compression test" << std::endl;
    std::cout << "  --test-all-hlsl     Run all HLSL preset tests" << std::endl;
    std::cout << "  --list-hlsl-presets  List available HLSL configuration presets" << std::endl;
    std::cout << "  -f <config_file>     Load parameters from a configuration file" << std::endl;
    std::cout << "  --create-config <file>  Create an example configuration file" << std::endl;
    std::cout << "  --plane2-component <R|G|B|A|0|1|2|3>  Set dual plane component (default: A)" << std::endl;
}

void ASTCParameterParser::print_parameters(const Parameters& params)
{
    std::cout << "=== Parsed Parameters ===" << std::endl;
    std::cout << "Block size: " << params.block_width << "x" << params.block_height << std::endl;
    std::cout << "Block block_mode: " << params.block_mode << std::endl;
    std::cout << "Partition: " << params.partition << std::endl;
    std::cout << "Endpoints: ";
    for (int i = 0; i < 8; i++)
    {
        if (i > 0) std::cout << ",";
        std::cout << (int)params.endpoints[i];
    }
    std::cout << std::endl;
    std::cout << "Weights: ";
    if (params.weights.empty())
    {
        std::cout << "default (all 128)";
    }
    else
    {
        for (size_t i = 0; i < params.weights.size(); i++)
        {
            if (i > 0) std::cout << ",";
            std::cout << params.weights[i];
        }
    }
    std::cout << std::endl;
    std::cout << "Show help: " << (params.show_help ? "yes" : "no") << std::endl;
    std::cout << "List block sizes: " << (params.list_block_sizes ? "yes" : "no") << std::endl;
    std::cout << "Test public functions: " << (params.test_public_functions ? "yes" : "no") << std::endl;
    std::cout << "Explain block block_mode: " << (params.explain_block_mode ? "yes" : "no") << std::endl;
    std::cout << "List decimation modes: " << (params.list_decimation_modes ? "yes" : "no") << std::endl;
    std::cout << "List quantization levels: " << (params.list_quantization_levels ? "yes" : "no") << std::endl;
    std::cout << "Calculate block block_mode: " << (params.calculate_block_mode ? "yes" : "no") << std::endl;
    if (params.calculate_block_mode)
    {
        std::cout << "  Weight grid size: " << params.weight_grid_size << std::endl;
        std::cout << "  Weight quant block_mode: " << params.weight_quant_str << " (" << params.get_weight_quant_mode() << ")" << std::endl;
        std::cout << "  Dual plane block_mode: " << (params.is_dual_plane_mode ? "yes" : "no") << std::endl;
    }
    if (params.validate_hlsl)
    {
        std::cout << "  Validate HLSL: " << params.hlsl_preset_name << std::endl;
    }
    if (params.test_all_hlsl_presets)
    {
        std::cout << "  Test all HLSL presets: yes" << std::endl;
    }
    if (params.list_hlsl_presets)
    {
        std::cout << "  List HLSL presets: yes" << std::endl;
    }
    if (params.load_from_config)
    {
        std::cout << "  Load from config file: " << params.config_file_path << std::endl;
    }
    std::cout << "========================" << std::endl;
}

bool ASTCParameterParser::parse_block_size(const std::string& str, int& width, int& height)
{
    size_t x_pos = str.find('x');
    if (x_pos == std::string::npos)
        return false;
    
    try
    {
        width = std::stoi(str.substr(0, x_pos));
        height = std::stoi(str.substr(x_pos + 1));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool ASTCParameterParser::parse_endpoints(const std::string& str, std::vector<uint8_t>& endpoints)
{
    std::vector<int> values;
    
    size_t pos = 0;
    while (pos < str.length())
    {
        size_t comma_pos = str.find(',', pos);
        if (comma_pos == std::string::npos)
            comma_pos = str.length();
        
        try
        {
            int value = std::stoi(str.substr(pos, comma_pos - pos));
            values.push_back(value);
        }
        catch (...)
        {
            return false;
        }
        
        pos = comma_pos + 1;
    }
    
    if (values.size() != 8)
        return false;
    
    endpoints.clear();
    for (int i = 0; i < 8; i++)
    {
        endpoints.push_back(static_cast<uint8_t>(values[i]));
    }
    
    return true;
}

bool ASTCParameterParser::parse_weights(const std::string& str, std::vector<float>& weights)
{
    std::string s = str;
    weights.clear();
    
    size_t pos = 0;
    while (pos < s.length())
    {
        size_t comma_pos = s.find(',', pos);
        if (comma_pos == std::string::npos)
        {
            comma_pos = s.length();
        }
        
        try
        {
            float value = std::stof(s.substr(pos, comma_pos - pos));
            weights.push_back(value);
        }
        catch (...)
        {
            return false;
        }
        
        pos = comma_pos + 1;
    }
    
    return true;
}

bool ASTCParameterParser::parse_int(const std::string& str, int& value)
{
    try
    {
        value = std::stoi(str);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool ASTCParameterParser::parse_bool(const std::string& str, bool& value)
{
    if (str == "0" || str == "false" || str == "no")
    {
        value = false;
        return true;
    }
    else if (str == "1" || str == "true" || str == "yes")
    {
        value = true;
        return true;
    }
    return false;
}

// 辅助函数：字符串转quant_method枚举
quant_method parse_weight_quant_str(const std::string& str)
{
    QUANT_CASE(2)
    QUANT_CASE(3)
    QUANT_CASE(4)
    QUANT_CASE(5)
    QUANT_CASE(6)
    QUANT_CASE(8)
    QUANT_CASE(10)
    QUANT_CASE(12)
    QUANT_CASE(16)
    QUANT_CASE(20)
    QUANT_CASE(24)
    QUANT_CASE(32)
    QUANT_CASE(40)
    QUANT_CASE(48)
    QUANT_CASE(64)
    QUANT_CASE(80)
    QUANT_CASE(96)
    QUANT_CASE(128)
    QUANT_CASE(160)
    QUANT_CASE(192)
    QUANT_CASE(256)

    // 默认
    return QUANT_4;
}

// 在block mode计算/使用处，将params.weight_quant_str转为quant_mode数值
// 例如：
// quant_method quant = parse_weight_quant_str(params.weight_quant_str);
// int quant_mode = static_cast<int>(quant);

// 通用函数：计算block mode并处理结果
bool calculate_and_handle_block_mode(ASTCParameterParser::Parameters& params, bool is_compression_mode)
{
    const int calculated_mode = calculate_block_mode(params.block_width, params.block_height,
                                              params.weight_grid_size, params.get_weight_quant_mode(),
                                              params.is_dual_plane_mode);
    
    if (calculated_mode >= 0)
    {
        if (is_compression_mode)
        {
            // 压缩模式：更新参数并继续
            params.block_mode = calculated_mode;
            std::cout << "Calculated block block_mode: " << calculated_mode << std::endl;
            return true;
        }
        else
        {
            // 显示模式：只输出结果
            std::cout << "Calculated block_mode: " << calculated_mode << std::endl;
            std::cout << "Parameters: " << params.block_width << "x" << params.block_height 
                      << ", weight_grid_size=" << params.weight_grid_size 
                      << ", quant=" << params.get_weight_quant_mode() 
                      << ", dual_plane=" << (params.is_dual_plane_mode ? "yes" : "no") << std::endl;
            return true;
        }
    }
    else
    {
        if (is_compression_mode)
        {
            // 压缩模式：输出错误并返回false
            std::cerr << "Error: Could not calculate valid block block_mode for the given parameters." << std::endl;
            std::cerr << "Parameters: " << params.block_width << "x" << params.block_height 
                      << ", weight_grid_size=" << params.weight_grid_size 
                      << ", quant_mode=" << params.get_weight_quant_mode() 
                      << ", dual_plane=" << (params.is_dual_plane_mode ? "yes" : "no") << std::endl;
            return false;
        }
        else
        {
            // 显示模式：输出提示信息
            std::cout << "No valid block_mode found for the given parameters." << std::endl;
            std::cout << "Try listing available modes with: -x " << params.block_width << "x" << params.block_height << std::endl;
            return true;
        }
    }
}

// Main application functions implementation
bool handle_special_commands(ASTCParameterParser::Parameters& params, const char* program_name)
{
    if (params.show_help)
    {
        ASTCParameterParser::print_usage(program_name);
        return true;
    }

    if (params.list_block_sizes)
    {
        print_supported_block_sizes();
        return true;
    }

    if (params.test_public_functions)
    {
        test_public_utilities();
        return true;
    }

    if (params.explain_block_mode)
    {
        explain_block_mode(params.block_width, params.block_height);
        return true;
    }

    if (params.list_decimation_modes)
    {
        list_decimation_modes(params.block_width, params.block_height);
        return true;
    }

    if (params.list_quantization_levels)
    {
        list_quantization_levels();
        return true;
    }

    if (params.list_dual_plane_modes)
    {
        list_dual_plane_modes(params.block_width, params.block_height);
        return true;
    }

    if (params.list_hlsl_presets)
    {
        print_hlsl_config_presets();
        return true;
    }

    if (params.calculate_block_mode)
    {
        return calculate_and_handle_block_mode(params, true);
    }

    if (params.validate_hlsl)
    {
        if (params.hlsl_preset_name.empty())
        {
            std::cerr << "Error: HLSL preset name not specified for validation." << std::endl;
            return false;
        }
        return execute_hlsl_preset_validation(params.hlsl_preset_name);
    }

    if (params.test_all_hlsl_presets)
    {
        return execute_all_hlsl_preset_tests();
    }

    if (params.load_from_config)
    {
        return execute_config_from_file(params.config_file_path);
    }

    return false; // No special command handled
}

bool execute_compression_test(ASTCParameterParser::Parameters& params)
{
    std::cout << "\n=========== execute_compression_test ===========\n\n";

    // Validate parameters
    if (!validate_compression_parameters(params))
    {
        return false;
    }

    // If block_mode is 0, calculate the block block_mode from parameters
    if (params.block_mode == 0)
    {
        if (!calculate_and_handle_block_mode(params, true))
        {
            return false;
        }
    }

    // Get required weight count
    const int required_weight_count = get_weight_count_for_block_mode(params.block_width, params.block_height, params.block_mode);
    if (required_weight_count != params.weights.size())
    {
        std::cerr << "Error: Invalid block block_mode " << params.block_mode << " for " 
                  << params.block_width << "x" << params.block_height << " blocks" << std::endl;
        return false;
    }

    // Initialize weights if needed
    const int expected_weight_count = params.block_width * params.block_height * (params.is_dual_plane_mode ? 2 : 1);
    if (params.weights.empty() || params.weights.size() < expected_weight_count)
    {
        params.weights.resize(expected_weight_count, 0.5f); // Fill with default value 0.5
    }

    // Initialize endpoints if needed
    if (params.endpoints.empty())
    {
        params.endpoints = {255, 255, 255, 255, 0, 0, 0, 0}; // Default endpoints
    }

    // Print parameters
    print_compression_parameters(params);

    // Execute compression
    return execute_compression(params);
}

bool validate_compression_parameters(const ASTCParameterParser::Parameters& params)
{
    if (!is_valid_block_size(params.block_width, params.block_height))
    {
        std::cerr << "Error: Invalid block size " << params.block_width << "x" << params.block_height << std::endl;
        print_supported_block_sizes();
        return false;
    }
    return true;
}

void print_compression_parameters(const ASTCParameterParser::Parameters& params)
{
    std::cout << "Block block_mode: " << params.block_mode << "\n";
    std::cout << "Block size: " << params.block_width << "x" << params.block_height << "\n";
    std::cout << "Weight quant: " << params.weight_quant_str << " (" << params.get_weight_quant_mode() << ")\n";
    std::cout << "Partition: " << params.partition << "\n";
    std::cout << "Dual plane: " << (params.is_dual_plane_mode ? "yes" : "no") << "\n\n";

    // Print endpoints in a more readable format
    std::cout << "Endpoints: ";
    for (int i = 0; i < 8; i++)
    {
        if (i > 0) 
        {
            std::cout << " ";
        }
        std::cout << std::setw(3) << (int)(i < params.endpoints.size() ? params.endpoints[i] : 0);
    }
    std::cout << std::endl;

    // 新增：输出归一化到0.0~1.0的浮点数
    std::cout << "Endpoints (normalized):" << std::endl;
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "  Endpoint 0: ";
    for (int i = 0; i < 4; i++)
    {
        if (i > 0)
        {
            std::cout << " ";
        }
        std::cout << std::setw(6) << (float(i < params.endpoints.size() ? params.endpoints[i] : 0) / 255.0f);
    }
    std::cout << std::endl;

    std::cout << "  Endpoint 1: ";
    for (int i = 4; i < 8; i++)
    {
        if (i > 4)
        {
            std::cout << " ";
        }
        std::cout << std::setw(6) << (float(i < params.endpoints.size() ? params.endpoints[i] : 0) / 255.0f);
    }
    std::cout.unsetf(std::ios::fixed);
    std::cout << "\n\n";
    
    // Print weights in block layout format
    std::cout << "Weights (" << params.weights.size() << "): ";
    if (params.weights.empty())
    {
        std::cout << "(empty)";
    }
    else
    {
        std::cout << std::endl;
        
        // Calculate weight grid dimensions based on block size
        int weight_x = 0, weight_y = 0, plane_size = 0;
        if (params.block_mode > 0)
        {
            // Try to get weight grid dimensions from block block_mode
            block_size_descriptor* bsd = static_cast<block_size_descriptor*>(get_block_size_descriptor(params.block_width, params.block_height, 1));
            if (bsd)
            {
                unsigned int packed_index = bsd->block_mode_packed_index[params.block_mode];
                if (packed_index != BLOCK_BAD_BLOCK_MODE && packed_index < bsd->block_mode_count_all)
                {
                    const auto& bm = bsd->block_modes[packed_index];
                    const auto& di = bsd->get_decimation_info(bm.decimation_mode);
                    weight_x = di.weight_x;
                    weight_y = di.weight_y;
                    plane_size = di.weight_count;
                }
            }
        }
        // If we couldn't determine from block block_mode, use weight_grid_size to estimate
        if (weight_x == 0 || weight_y == 0 || plane_size == 0)
        {
            if (params.weight_grid_size > 0)
            {
                int total_weights = params.weight_grid_size;
                weight_x = static_cast<int>(sqrt(total_weights));
                weight_y = total_weights / weight_x;
                plane_size = total_weights;
                if (weight_x * weight_y != total_weights)
                {
                    for (int w = 1; w <= total_weights; w++)
                    {
                        if (total_weights % w == 0)
                        {
                            int h = total_weights / w;
                            if (w <= h)
                            {
                                weight_x = w;
                                weight_y = h;
                                plane_size = w * h;
                            }
                        }
                    }
                }
            }
            else
            {
                weight_x = params.block_width;
                weight_y = params.block_height;
                plane_size = weight_x * weight_y;
            }
        }
        // 判断是否为双平面
        bool is_dual_plane = params.is_dual_plane_mode || (params.weights.size() == (size_t)(plane_size * 2));
        size_t total_weights = params.weights.size();
        if (is_dual_plane && total_weights >= (size_t)(plane_size * 2))
        {
            // Plane 1
            std::cout << "Plane 1:" << std::endl;
            for (int y = 0; y < weight_y; y++)
            {
                std::cout << "  ";
                for (int x = 0; x < weight_x; x++)
                {
                    int index = y * weight_x + x;
                    if (index < plane_size)
                        std::cout << std::setw(6) << std::fixed << std::setprecision(2) << params.weights[index];
                    else
                        std::cout << "    -";
                    if (x < weight_x - 1) std::cout << " ";
                }
                std::cout << std::endl;
            }
            // Plane 2
            std::cout << "Plane 2:" << std::endl;
            for (int y = 0; y < weight_y; y++)
            {
                std::cout << "  ";
                for (int x = 0; x < weight_x; x++)
                {
                    int index = y * weight_x + x;
                    if (index < plane_size)
                        std::cout << std::setw(6) << std::fixed << std::setprecision(2) << params.weights[plane_size + index];
                    else
                        std::cout << "    -";
                    if (x < weight_x - 1) std::cout << " ";
                }
                std::cout << std::endl;
            }
            // 多余权重
            if (total_weights > (size_t)(plane_size * 2))
            {
                std::cout << "  Remaining weights: ";
                for (size_t i = plane_size * 2; i < total_weights; i++)
                {
                    if (i > plane_size * 2) std::cout << " ";
                    std::cout << std::setw(6) << std::fixed << std::setprecision(2) << params.weights[i];
                }
                std::cout << std::endl;
            }
        }
        else
        {
            // 单平面
            for (int y = 0; y < weight_y; y++)
            {
                std::cout << "  ";
                for (int x = 0; x < weight_x; x++)
                {
                    int index = y * weight_x + x;
                    if (index < static_cast<int>(params.weights.size()))
                        std::cout << std::setw(6) << std::fixed << std::setprecision(2) << params.weights[index];
                    else
                        std::cout << "    -";
                    if (x < weight_x - 1) std::cout << " ";
                }
                std::cout << std::endl;
            }
            if (static_cast<int>(params.weights.size()) > plane_size)
            {
                std::cout << "  Remaining weights: ";
                for (size_t i = plane_size; i < params.weights.size(); i++)
                {
                    if (i > plane_size) std::cout << " ";
                    std::cout << std::setw(6) << std::fixed << std::setprecision(2) << params.weights[i];
                }
                std::cout << std::endl;
            }
        }
    }
}

bool execute_compression(ASTCParameterParser::Parameters& params)
{
    std::cout << "\n=========== execute_compression ===========\n\n";

    if (params.partition > 1)
    {
        std::cout << "Warning: Multiple partitions (" << params.partition 
                  << ") specified, using partition index 0" << std::endl;
    }

    block_size_descriptor* bsd = static_cast<block_size_descriptor*>(get_block_size_descriptor(params.block_width, params.block_height, 1));
    const block_mode* bm_ptr = nullptr;
    if (bsd && params.block_mode >= 0)
    {
        unsigned int packed_index = bsd->block_mode_packed_index[params.block_mode];
        if (packed_index != BLOCK_BAD_BLOCK_MODE && packed_index < bsd->block_mode_count_all)
        {
            bm_ptr = &bsd->block_modes[packed_index];
        }
    }

    const int plane2_component = params.get_plane2_component();
    const int endpoint_count = params.has_alpha ? 8 : 6;

    uint8_t manual_compressed[16] = { 0 };

    std::string weight_quant_str = params.weight_quant_str;
    int weight_quant = bm_ptr ? static_cast<int>(bm_ptr->get_weight_quant_mode()) : params.get_weight_quant_mode();
    // 获取color quant

    std::cout << "Weight quant: " << weight_quant_str << " (" << weight_quant << ") " << std::endl;

    bool ok = manual_construct_astc_block(manual_compressed, params.block_width, params.block_height, 
                                         params.block_mode, 0, params.partition,
                                         params.endpoints.data(), endpoint_count, 
                                         params.weights.data(), params.weights.size(),
                                         params.has_alpha, plane2_component,
                                         nullptr); // 不再传递 last_compressed_block
    if (ok)
    {
        // 将压缩结果写入 params.compressed_block
        params.compressed_block.assign(manual_compressed, manual_compressed + 16);
        // 获取weight quant

        std::cout << "Compression successful!" << std::endl;
        std::cout << "Compressed data: ";
        for (int i = 0; i < 16; i++)
        {
            if (i > 0) std::cout << " ";
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)manual_compressed[i];
        }
        std::cout << std::dec << std::endl;
        
        // 新增：以4个32位无符号整数（十进制）输出
        const uint32_t* compressed_u32 = reinterpret_cast<const uint32_t*>(manual_compressed);
        std::cout << "Compressed data (decimal): ";
        for (int i = 0; i < 4; i++) 
        {
            if (i > 0) std::cout << " ";
            std::cout << std::setw(10) << compressed_u32[i];
        }
        std::cout << std::endl;
        
        // 新增：以4个32位无符号整数（十六进制）输出
        std::cout << "Compressed data (hex): ";
        for (int i = 0; i < 4; i++) 
        {
            if (i > 0) std::cout << " ";
            std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') << compressed_u32[i];
        }
        std::cout << std::dec << std::endl;
        // Calculate compression ratio

        int original_size = params.block_width * params.block_height * 4; // 4 bytes per pixel (RGBA)
        int compressed_size = 16; // ASTC blocks are always 16 bytes
        float ratio = (float)original_size / compressed_size;
        std::cout << "Compression ratio: " << std::fixed << std::setprecision(2) << ratio << ":1\n\n";
        return true;
    }
    else
    {
        std::cout << "Compression failed! \n\n" << std::endl;
        return false;
    }
}

// Internal test function for public utilities
void test_public_utilities()
{
    // Test block size validation
    std::cout << "Testing block size validation:" << std::endl;
    std::cout << "  4x4 valid: " << (is_valid_block_size(4, 4) ? "yes" : "no") << std::endl;
    std::cout << "  6x6 valid: " << (is_valid_block_size(6, 6) ? "yes" : "no") << std::endl;
    std::cout << "  8x8 valid: " << (is_valid_block_size(8, 8) ? "yes" : "no") << std::endl;
    std::cout << "  3x3 valid: " << (is_valid_block_size(3, 3) ? "yes" : "no") << std::endl;

    // Test weight count calculation
    std::cout << "Testing weight count calculation:" << std::endl;
    std::cout << "  4x4 block_mode 0 weights: " << get_weight_count_for_block_mode(4, 4, 0) << std::endl;
    std::cout << "  6x6 block_mode 0 weights: " << get_weight_count_for_block_mode(6, 6, 0) << std::endl;
    std::cout << "  8x8 block_mode 0 weights: " << get_weight_count_for_block_mode(8, 8, 0) << std::endl;

    // Test block size descriptor
    std::cout << "Testing block size descriptor:" << std::endl;
    void* bsd_4x4 = get_block_size_descriptor(4, 4, 1);
    void* bsd_6x6 = get_block_size_descriptor(6, 6, 1);
    std::cout << "  4x4 descriptor: " << (bsd_4x4 ? "valid" : "invalid") << std::endl;
    std::cout << "  6x6 descriptor: " << (bsd_6x6 ? "valid" : "invalid") << std::endl;
}

// HLSL configuration presets implementation
std::vector<HLSLConfigPreset> get_hlsl_config_presets()
{
    std::vector<HLSLConfigPreset> presets;
    
    // Preset 1: Standard 4x4 RGBA (most common)
    presets.push_back({
        "4x4_rgba_standard",
        false,          // block_6x6
        true,           // has_alpha
        false,          // is_dual_plane
        false,          // is_normalmap
        4, 4,           // x_grids, y_grids
        16, 4,          // weight_grid_size, quant_mode
        "Standard 4x4 RGBA compression (most common)"
    });
    
    // Preset 2: 6x6 RGBA
    presets.push_back({
        "6x6_rgba_standard",
        true,           // block_6x6
        true,           // has_alpha
        false,          // is_dual_plane
        false,          // is_normalmap
        6, 6,           // x_grids, y_grids
        36, 4,          // weight_grid_size, quant_mode
        "Standard 6x6 RGBA compression"
    });
    
    // Preset 3: 4x4 RGB (no alpha)
    presets.push_back({
        "4x4_rgb_noalpha",
        false,          // block_6x6
        false,          // has_alpha
        false,          // is_dual_plane
        false,          // is_normalmap
        4, 4,           // x_grids, y_grids
        16, 4,          // weight_grid_size, quant_mode
        "4x4 RGB compression (no alpha channel)"
    });
    
    // Preset 4: 6x6 RGB (no alpha)
    presets.push_back({
        "6x6_rgb_noalpha",
        true,           // block_6x6
        false,          // has_alpha
        false,          // is_dual_plane
        false,          // is_normalmap
        6, 6,           // x_grids, y_grids
        36, 4,          // weight_grid_size, quant_mode
        "6x6 RGB compression (no alpha channel)"
    });
    
    // Preset 5: 4x4 Dual Plane RGBA
    presets.push_back({
        "4x4_rgba_dualplane",
        false,          // block_6x6
        true,           // has_alpha
        true,           // is_dual_plane
        false,          // is_normalmap
        4, 4,           // x_grids, y_grids
        0, 4,           // decimation_mode, quant_mode
        "4x4 RGBA dual plane compression"
    });
    
    // Preset 6: 6x6 Dual Plane RGBA
    presets.push_back({
        "6x6_rgba_dualplane",
        true,           // block_6x6
        true,           // has_alpha
        true,           // is_dual_plane
        false,          // is_normalmap
        6, 6,           // x_grids, y_grids
        1, 4,           // decimation_mode, quant_mode
        "6x6 RGBA dual plane compression"
    });
    
    // Preset 7: 4x4 Normal Map
    presets.push_back({
        "4x4_normalmap",
        false,          // block_6x6
        false,          // has_alpha
        false,          // is_dual_plane
        true,           // is_normalmap
        4, 4,           // x_grids, y_grids
        0, 4,           // decimation_mode, quant_mode
        "4x4 normal map compression"
    });
    
    // Preset 8: 6x6 Normal Map
    presets.push_back({
        "6x6_normalmap",
        true,           // block_6x6
        false,          // has_alpha
        false,          // is_dual_plane
        true,           // is_normalmap
        6, 6,           // x_grids, y_grids
        1, 4,           // decimation_mode, quant_mode
        "6x6 normal map compression"
    });
    
    // Preset 9: High Quality 4x4
    presets.push_back({
        "4x4_high_quality",
        false,          // block_6x6
        true,           // has_alpha
        false,          // is_dual_plane
        false,          // is_normalmap
        4, 4,           // x_grids, y_grids
        0, 6,           // decimation_mode, quant_mode (higher quality)
        "4x4 high quality compression (QUANT_8)"
    });
    
    // Preset 10: High Quality 6x6
    presets.push_back({
        "6x6_high_quality",
        true,           // block_6x6
        true,           // has_alpha
        false,          // is_dual_plane
        false,          // is_normalmap
        6, 6,           // x_grids, y_grids
        1, 6,           // decimation_mode, quant_mode (higher quality)
        "6x6 high quality compression (QUANT_8)"
    });
    
    return presets;
}

void convert_hlsl_preset_to_cpp_params(const HLSLConfigPreset& preset, ASTCParameterParser::Parameters& params)
{
    // Convert block size
    params.block_width = preset.block_6x6 ? 6 : 4;
    params.block_height = preset.block_6x6 ? 6 : 4;
    
    // Convert dual plane setting
    params.is_dual_plane_mode = preset.is_dual_plane;
    
    // Set weight grid size and quantization modes
    params.weight_grid_size = preset.weight_grid_size;
            // Convert quant_mode enum to string format
        switch (preset.quant_mode)
        {
            case 0: params.weight_quant_str = "QUANT_2"; break;
            case 1: params.weight_quant_str = "QUANT_3"; break;
            case 2: params.weight_quant_str = "QUANT_4"; break;
            case 3: params.weight_quant_str = "QUANT_5"; break;
            case 4: params.weight_quant_str = "QUANT_6"; break;
            case 5: params.weight_quant_str = "QUANT_8"; break;
            case 6: params.weight_quant_str = "QUANT_10"; break;
            case 7: params.weight_quant_str = "QUANT_12"; break;
            case 8: params.weight_quant_str = "QUANT_16"; break;
            case 9: params.weight_quant_str = "QUANT_20"; break;
            case 10: params.weight_quant_str = "QUANT_24"; break;
            case 11: params.weight_quant_str = "QUANT_32"; break;
            case 12: params.weight_quant_str = "QUANT_40"; break;
            case 13: params.weight_quant_str = "QUANT_48"; break;
            case 14: params.weight_quant_str = "QUANT_64"; break;
            case 15: params.weight_quant_str = "QUANT_80"; break;
            case 16: params.weight_quant_str = "QUANT_96"; break;
            case 17: params.weight_quant_str = "QUANT_128"; break;
            case 18: params.weight_quant_str = "QUANT_160"; break;
            case 19: params.weight_quant_str = "QUANT_192"; break;
            case 20: params.weight_quant_str = "QUANT_256"; break;
            default: params.weight_quant_str = "QUANT_4"; break;
        }
    
    // Set default partition
    params.partition = 0;
    
    // Set endpoints based on alpha and normal map settings
    if (preset.has_alpha)
    {
        if (preset.is_normalmap)
        {
            // Normal map with alpha (RGBA)
            params.endpoints = {128, 128, 255, 255, 128, 128, 0, 0}; // R1,G1,B1,A1,R2,G2,B2,A2
        }
        else
        {
            // Standard RGBA
            params.endpoints = {255, 255, 255, 255, 0, 0, 0, 0}; // R1,G1,B1,A1,R2,G2,B2,A2
        }
    }
    else
    {
        if (preset.is_normalmap)
        {
            // Normal map without alpha (RGB)
            params.endpoints = {128, 128, 255, 255, 128, 128, 0, 0}; // R1,G1,B1,A1,R2,G2,B2,A2
        }
        else
        {
            // Standard RGB
            params.endpoints = {255, 255, 255, 255, 0, 0, 0, 0}; // R1,G1,B1,A1,R2,G2,B2,A2
        }
    }
    
    // Calculate weights based on grid size and dual plane
    int weight_count = preset.x_grids * preset.y_grids;
    if (preset.is_dual_plane)
        weight_count *= 2;
    
            params.weights.resize(weight_count, 0.5f); // Default weight value
}

void print_hlsl_config_presets()
{
    std::vector<HLSLConfigPreset> presets = get_hlsl_config_presets();
    
    std::cout << "=== Available HLSL Configuration Presets ===" << std::endl;
    for (const auto& preset : presets)
    {
        std::cout << "Name: " << preset.name << std::endl;
        std::cout << "  Description: " << preset.description << std::endl;
        std::cout << "  Block size: " << (preset.block_6x6 ? "6x6" : "4x4") << std::endl;
        std::cout << "  Alpha: " << (preset.has_alpha ? "yes" : "no") << std::endl;
        std::cout << "  Dual plane: " << (preset.is_dual_plane ? "yes" : "no") << std::endl;
        std::cout << "  Normal map: " << (preset.is_normalmap ? "yes" : "no") << std::endl;
        std::cout << "  Grid size: " << preset.x_grids << "x" << preset.y_grids << std::endl;
        std::cout << "  Weight grid size: " << preset.weight_grid_size << std::endl;
        std::cout << "  Quantization block_mode: " << preset.quant_mode << std::endl;
        std::cout << std::endl;
    }
    std::cout << "===========================================" << std::endl;
}

bool execute_hlsl_preset_validation(const std::string& preset_name)
{
    std::vector<HLSLConfigPreset> presets = get_hlsl_config_presets();
    
    // Find the preset by name
    const HLSLConfigPreset* selected_preset = nullptr;
    for (const auto& preset : presets)
    {
        if (preset.name == preset_name)
        {
            selected_preset = &preset;
            break;
        }
    }
    
    if (!selected_preset)
    {
        std::cerr << "Error: Unknown HLSL preset '" << preset_name << "'" << std::endl;
        std::cout << "Available presets:" << std::endl;
        for (const auto& preset : presets)
        {
            std::cout << "  " << preset.name << std::endl;
        }
        return false;
    }
    
    std::cout << "=== HLSL Preset Validation Test ===" << std::endl;
    std::cout << "Testing preset: " << selected_preset->name << std::endl;
    std::cout << "Description: " << selected_preset->description << std::endl;
    
    // Convert to C++ parameters
    ASTCParameterParser::Parameters cpp_params;
    convert_hlsl_preset_to_cpp_params(*selected_preset, cpp_params);
    
    std::cout << "Converted parameters:" << std::endl;
    std::cout << "  Block size: " << cpp_params.block_width << "x" << cpp_params.block_height << std::endl;
    std::cout << "  Dual plane: " << (cpp_params.is_dual_plane_mode ? "yes" : "no") << std::endl;
    std::cout << "  Weight count: " << cpp_params.weights.size() << std::endl;
    std::cout << "  Weight grid size: " << cpp_params.weight_grid_size << std::endl;
            std::cout << "  Quantization block_mode: " << cpp_params.weight_quant_str << " (" << cpp_params.get_weight_quant_mode() << ")" << std::endl;
    
    // Calculate block block_mode
    int calculated_block_mode = calculate_block_mode(cpp_params.block_width, cpp_params.block_height,
                                                    cpp_params.weight_grid_size, cpp_params.get_weight_quant_mode(),
                                                    cpp_params.is_dual_plane_mode);
    
    if (calculated_block_mode >= 0)
    {
        std::cout << "Calculated block block_mode: " << calculated_block_mode << std::endl;
        
        // Execute compression test
        cpp_params.block_mode = calculated_block_mode;
        
        std::cout << "Executing compression test..." << std::endl;
        if (execute_compression_test(cpp_params))
        {
            std::cout << "HLSL preset validation completed successfully!" << std::endl;
            return true;
        }
        else
        {
            std::cerr << "Compression test failed" << std::endl;
            return false;
        }
    }
    else
    {
        std::cerr << "Could not calculate valid block block_mode for preset parameters" << std::endl;
        return false;
    }
}

bool execute_all_hlsl_preset_tests()
{
    std::vector<HLSLConfigPreset> presets = get_hlsl_config_presets();
    
    std::cout << "=== Running All HLSL Preset Tests ===" << std::endl;
    std::cout << "Total presets to test: " << presets.size() << std::endl;
    std::cout << std::endl;
    
    int success_count = 0;
    int total_count = presets.size();
    
    for (const auto& preset : presets)
    {
        std::cout << "Testing preset: " << preset.name << std::endl;
        std::cout << "Description: " << preset.description << std::endl;
        
        if (execute_hlsl_preset_validation(preset.name))
        {
            success_count++;
            std::cout << "? PASSED" << std::endl;
        }
        else
        {
            std::cout << "? FAILED" << std::endl;
        }
        
        std::cout << std::endl;
    }
    
    std::cout << "=== Test Summary ===" << std::endl;
    std::cout << "Total tests: " << total_count << std::endl;
    std::cout << "Passed: " << success_count << std::endl;
    std::cout << "Failed: " << (total_count - success_count) << std::endl;
    std::cout << "Success rate: " << (success_count * 100 / total_count) << "%" << std::endl;
    
    return (success_count == total_count);
}

// Configuration file parser implementation
bool parse_config_file(const std::string& file_path, ASTCParameterParser::Parameters& params)
{
    std::ifstream file(file_path);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not open configuration file: " << file_path << std::endl;
        return false;
    }

    std::string line;
    bool in_section = false;
    std::string current_section;

    while (std::getline(file, line))
    {
        // Remove leading/trailing whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        // Skip empty lines and comments (lines starting with # or ;)
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;
            
        // Remove inline comments (everything after # or ;)
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos)
            line = line.substr(0, comment_pos);
            
        comment_pos = line.find(';');
        if (comment_pos != std::string::npos)
            line = line.substr(0, comment_pos);
            
        // Remove trailing whitespace after removing comments
        line.erase(line.find_last_not_of(" \t") + 1);
        
        // Skip empty lines after comment removal
        if (line.empty())
            continue;

        // Check for section headers [section]
        if (line[0] == '[' && line[line.length() - 1] == ']')
        {
            current_section = line.substr(1, line.length() - 2);
            in_section = true;
            continue;
        }

        // Parse key=value pairs
        size_t equal_pos = line.find('=');
        if (equal_pos != std::string::npos)
        {
            std::string key = line.substr(0, equal_pos);
            std::string value = line.substr(equal_pos + 1);

            // Remove whitespace from key and value
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            // Parse based on section and key
            if (current_section == "block" || current_section.empty())
            {
                if (key == "width")
                    params.block_width = std::stoi(value);
                else if (key == "height")
                    params.block_height = std::stoi(value);
                else if (key == "block_mode")
                {
                    params.block_mode = std::stoi(value);
                    params.calculate_block_mode = !params.block_mode;
                }
                else if (key == "partition")
                {
                    params.partition = std::stoi(value);
                }
                else if (key == "has_alpha")
                {
                    params.has_alpha = (value == "true" || value == "1" || value == "yes");
                }
                else if (key == "is_dual_plane")
                {
                    params.is_dual_plane = (value == "true" || value == "1" || value == "yes");
                }
            }
            else if (current_section == "endpoints")
            {
                // Parse comma-separated endpoint values
                if (key == "values")
                {
                    std::vector<std::string> parts;
                    std::istringstream ss(value);
                    std::string part;
                    while (std::getline(ss, part, ','))
                    {
                        part.erase(0, part.find_first_not_of(" \t"));
                        part.erase(part.find_last_not_of(" \t") + 1);
                        parts.push_back(part);
                    }
                    
                    params.endpoints.clear();
                    for (int i = 0; i < 8 && i < (int)parts.size(); i++)
                    {
                        params.endpoints.push_back(static_cast<uint8_t>(std::stoi(parts[i])));
                    }
                }
            }
            else if (current_section == "weights")
            {
                // Parse comma-separated weight values
                if (key == "values")
                {
                    std::vector<std::string> parts;
                    std::istringstream ss(value);
                    std::string part;
                    while (std::getline(ss, part, ','))
                    {
                        part.erase(0, part.find_first_not_of(" \t"));
                        part.erase(part.find_last_not_of(" \t") + 1);
                        parts.push_back(part);
                    }

                    params.weights.clear();
                    for (const auto& part : parts)
                    {
                        float weight_val = std::stof(part);
                        // Check if weight is in valid range 0.0-1.0
                        if (weight_val < 0.0f || weight_val > 1.0f)
                        {
                            std::cerr << "Error: Weight value must be between 0.0 and 1.0, got: " << weight_val << std::endl;
                            return false;
                        }
                        params.weights.push_back(weight_val);
                    }
                }
                else if (key == "count")
                {
                    // Note: weight count is determined by weights.size(), not stored separately
                    // This is just for validation purposes
                    int expected_count = std::stoi(value);
                    if (!params.weights.empty() && params.weights.size() != expected_count)
                    {
                        std::cerr << "Warning: Expected " << expected_count << " weights, but got " << params.weights.size() << std::endl;
                    }
                }
            }
            else if (current_section == "calculation")
            {
                if (key == "weight_grid_size")
                {
                    params.weight_grid_size = std::stoi(value);
                }
                else if (key == "is_dual_plane")
                {
                    params.is_dual_plane_mode = (value == "true" || value == "1" || value == "yes");
                }
                else if (key == "weight_quant")
                {
                    params.weight_quant_str = value;
                }
            }
            else if (current_section == "plane2_component")
            {
                params.plane2_component_str = value;
            }
            else if (current_section == "decompress")
            {
                if (key == "block")
                {
                    std::vector<std::string> parts;
                    std::istringstream ss(value);
                    std::string part;
                    int idx = 0;
                    while (std::getline(ss, part, ',') && idx < 4)
                    {
                        part.erase(0, part.find_first_not_of(" \t"));
                        part.erase(part.find_last_not_of(" \t") + 1);
                        // Support 0x prefix for hexadecimal
                        uint32_t value = std::stoul(part, nullptr, 0);
                        // Push 32-bit value as 4 bytes in little-endian order
                        params.compressed_block.push_back(static_cast<uint8_t>(value & 0xFF));
                        params.compressed_block.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
                        params.compressed_block.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
                        params.compressed_block.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
                        idx++;
                    }
                }
            }
        }
    }

    return true; 
}

bool execute_config_from_file(const std::string& config_file)
{
    std::cout << "=== Configuration File Test ===" << std::endl;
    std::cout << "Loading configuration from: " << config_file << std::endl;

    // Parse configuration file
    ASTCParameterParser::Parameters params;
    if (!parse_config_file(config_file, params))
    {
        std::cerr << "Failed to parse configuration file" << std::endl;
        return false;
    }

    // Print loaded configuration
    std::cout << "Loaded configuration:" << std::endl;
    std::cout << "  Block size: " << params.block_width << "x" << params.block_height << std::endl;
    std::cout << "  Block block_mode: " << params.block_mode << std::endl;
    std::cout << "  Partition: " << params.partition << std::endl;
    std::cout << "  Weight count: " << params.weights.size() << std::endl;
    
    if (params.weight_grid_size > 0 || !params.weight_quant_str.empty())
    {
        std::cout << "  Weight grid size: " << params.weight_grid_size << std::endl;
        std::cout << "  Quantization block_mode: " << params.weight_quant_str << " (" << params.get_weight_quant_mode() << ")" << std::endl;
        std::cout << "  Dual plane: " << (params.is_dual_plane_mode ? "yes" : "no") << std::endl;
    }

    // Execute compression test
    std::cout << "Executing compression test..." << std::endl;
    if (execute_compression_test(params))
    {
        std::cout << "Configuration file test completed successfully!" << std::endl;
        return true;
    }
    else
    {
        std::cerr << "Configuration file test failed" << std::endl;
        return false;
    }
}

bool create_example_config_file(const std::string& file_path)
{
    std::ofstream file(file_path);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not create example configuration file: " << file_path << std::endl;
        return false;
    }

    file << "# ASTC Configuration File Example" << std::endl;
    file << "# This file contains configuration parameters for ASTC compression testing" << std::endl;
    file << std::endl;
    
    file << "[block]" << std::endl;
    file << "# Block dimensions" << std::endl;
    file << "width=6" << std::endl;
    file << "height=6" << std::endl;
    file << "# Block block_mode (0 for auto-calculation, or specific block_mode number)" << std::endl;
    file << "block_mode=0" << std::endl;
    file << "# Partition index" << std::endl;
    file << "partition=0" << std::endl;
    file << std::endl;
    
    file << "[endpoints]" << std::endl;
    file << "# RGBA endpoint values (8 values: R1,G1,B1,A1,R2,G2,B2,A2)" << std::endl;
    file << "values=255,255,255,255,0,0,0,0" << std::endl;
    file << std::endl;
    
    file << "[weights]" << std::endl;
    file << "# Weight values (comma-separated) or count for auto-generation" << std::endl;
    file << "# values=128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128" << std::endl;
    file << "count=16" << std::endl;
    file << std::endl;
    
    file << "[calculation]" << std::endl;
    file << "# Parameters for block block_mode calculation (when block_mode=0)" << std::endl;
    file << "weight_grid_size=36" << std::endl;
    file << "# quant_mode can be:" << std::endl;
    file << "# - String format: quant_mode=QUANT_4 (recommended)" << std::endl;
    file << "# - Enum value: quant_mode=2 (QUANT_4 = enum value 2)" << std::endl;
    file << "# - Quantization level: quant_mode=4 (4 levels, maps to QUANT_4)" << std::endl;
    file << "quant_mode=QUANT_4" << std::endl;
    file << "is_dual_plane=false" << std::endl;
    file << std::endl;
    
    file << "[analysis]" << std::endl;
    file << "# Parameters for block block_mode analysis" << std::endl;
    file << "block_mode_value=5" << std::endl;
    file << std::endl;
    
    file << "# Note: quant_mode in [calculation] section supports three formats:" << std::endl;
    file << "# - String format: quant_mode=QUANT_4 (recommended)" << std::endl;
    file << "# - Enum value: quant_mode=2 (QUANT_4 = enum value 2)" << std::endl;
    file << "# - Quantization level: quant_mode=4 (4 levels, maps to QUANT_4)" << std::endl;
    file << std::endl;
    
    file << "# Additional notes:" << std::endl;
    file << "# - Block block_mode 0 means auto-calculate from weight_grid_size/quant_mode/dual_plane" << std::endl;
    file << "# - Weight count should match the expected count for the block size and block_mode" << std::endl;
    file << "# - Endpoint values are 8-bit (0-255)" << std::endl;
    file << "# - Weight values are 8-bit (0-255)" << std::endl;

    file.close();
    std::cout << "Example configuration file created: " << file_path << std::endl;
    return true;
}

// Parse plane2 component string to int (0=R, 1=G, 2=B, 3=A)
int parse_plane2_component_str(const std::string& str)
{
    if (str == "R" || str == "0") return 0;
    if (str == "G" || str == "1") return 1;
    if (str == "B" || str == "2") return 2;
    if (str == "A" || str == "3") return 3;
    return 3;
}

int ASTCParameterParser::Parameters::get_plane2_component() const
{
    return parse_plane2_component_str(plane2_component_str);
}

// Helper function to get profile name
std::string get_profile_name(astcenc_profile profile)
{
    switch (profile)
    {
        case ASTCENC_PRF_LDR_SRGB: return "LDR_SRGB";
        case ASTCENC_PRF_LDR: return "LDR";
        case ASTCENC_PRF_HDR_RGB_LDR_A: return "HDR_RGB_LDR_A";
        case ASTCENC_PRF_HDR: return "HDR";
        default: return "UNKNOWN(" + std::to_string(static_cast<int>(profile)) + ")";
    }
}

// Function to print decompressed color results in block layout
void print_decompressed_colors(const uint8_t* color_data, int width, int height, int channels)
{
    std::cout << "Decompressed colors (RGBA):" << std::endl;

    for (int y = 0; y < height; ++y)
    {
        std::cout << "  ";
        for (int x = 0; x < width; ++x)
        {
            int idx = (y * width + x) * channels;
            std::cout << "(";
            for (int c = 0; c < channels; ++c)
            {
                if (c > 0) std::cout << ", ";
                std::cout << (int)color_data[idx + c];
            }
            std::cout << ") ";
        }
        std::cout << std::endl;
    }

    // Print normalized colors (0.0-1.0 range)
    std::cout << "\nDecompressed colors (normalized 0.0-1.0):" << std::endl;
    std::cout << std::fixed << std::setprecision(2);

    for (int y = 0; y < height; ++y)
    {
        std::cout << "  ";
        for (int x = 0; x < width; ++x)
        {
            int idx = (y * width + x) * channels;
            std::cout << "(";
            for (int c = 0; c < channels; ++c)
            {
                if (c > 0) std::cout << ", ";
                float normalized = color_data[idx + c] / 255.0f;
                std::cout << normalized;
            }
            std::cout << ") ";
        }
        std::cout << std::endl;
    }
}

// Decompress from 4 uint32_t to 4x4 RGBA pixel block
void decompress_astc_block_from_u32(const uint32_t compressed_u32[4], uint8_t* out_pixels, int width, int height)
{
    // 1. Assemble 16-byte compressed block
    uint8_t compressed[16];
    for (int i = 0; i < 4; ++i) 
    {
        compressed[i * 4 + 0] = (compressed_u32[i] >> 0) & 0xFF;
        compressed[i * 4 + 1] = (compressed_u32[i] >> 8) & 0xFF;
        compressed[i * 4 + 2] = (compressed_u32[i] >> 16) & 0xFF;
        compressed[i * 4 + 3] = (compressed_u32[i] >> 24) & 0xFF;
    }

    // 2. Call existing decompression function
    // channels=4 means RGBA
    bool ok = decompress_astc_block(compressed, out_pixels, width, height, 4);

    if (!ok) {
        std::cout << "Decompression failed!" << std::endl;
        return;
    }

    // 3. Output pixel data in block layout
    print_decompressed_colors(out_pixels, width, height, 4);

    // 4. Parse compressed block parameters and output
    std::cout << "\n=== Decompressed Block Parameters ===" << std::endl;
    
    // Get block size descriptor
    block_size_descriptor* bsd = static_cast<block_size_descriptor*>(get_block_size_descriptor(width, height, 1));
    if (!bsd) {
        std::cout << "Failed to get block size descriptor!" << std::endl;
        return;
    }

    // Parse physical block to symbolic block
    symbolic_compressed_block scb;
    physical_to_symbolic(*bsd, compressed, scb);

    // Output basic parameters
    std::cout << "Block type: ";
    switch (scb.block_type) {
        case SYM_BTYPE_ERROR: std::cout << "ERROR"; break;
        case SYM_BTYPE_CONST_U16: std::cout << "CONST_U16"; break;
        case SYM_BTYPE_CONST_F16: std::cout << "CONST_F16"; break;
        case SYM_BTYPE_NONCONST: std::cout << "NONCONST"; break;
        default: std::cout << "UNKNOWN(" << (int)scb.block_type << ")"; break;
    }
    std::cout << std::endl;

    if (scb.block_type == SYM_BTYPE_NONCONST) {
        std::cout << "Block block_mode: " << scb.block_mode << std::endl;
        std::cout << "Partition count: " << (int)scb.partition_count << std::endl;
        std::cout << "Partition index: " << scb.partition_index << std::endl;
        std::cout << "Color quant block_mode: QUANT_" << get_quant_level(scb.quant_mode) << " (" << (int)scb.quant_mode << ")" << std::endl;
        std::cout << "Plane2 component: " << (int)scb.plane2_component << std::endl;

        // Get detailed block block_mode information
        const auto& bm = bsd->get_block_mode(scb.block_mode);
        const auto& di = bsd->get_decimation_info(bm.decimation_mode);
        
        std::cout << "Decimation block_mode: " << std::to_string(bm.decimation_mode) << std::endl;
        std::cout << "Weight grid: "
                  << std::to_string(di.weight_x) << "x"
                  << std::to_string(di.weight_y) << "x"
                  << std::to_string(di.weight_z) << std::endl;
        std::cout << "Weight count: " << std::to_string(di.weight_count) << std::endl;
        std::cout << "Is dual plane: " << (bm.is_dual_plane ? "true" : "false") << std::endl;
        std::cout << "Weight quant: QUANT_"
                  << std::to_string(get_quant_level(bm.get_weight_quant_mode()))
                  << " (" << std::to_string(static_cast<int>(bm.get_weight_quant_mode())) << ")" << std::endl;

        // Output color endpoints as matrix (beautified, with space after each element)
        std::cout << "Color endpoints (by partition):" << std::endl;
        for (int p = 0; p < scb.partition_count; ++p) {
            std::cout << "Partition " << std::to_string(p) << ": ";
            int vals = 2 * (scb.color_formats[p] >> 2) + 2;
            for (int j = 0; j < vals; ++j) {
                std::cout << std::setw(4) << std::to_string(static_cast<int>(scb.color_values[p][j])) << " ";
                if ((j + 1) % 4 == 0 && j + 1 < vals) std::cout << "  "; // extra space between RGBA groups
            }
            std::cout << "(format: " << std::to_string(static_cast<int>(scb.color_formats[p])) << ")" << std::endl;
        }

        // Output weights as 0.0/1.0 floats, aligned and beautified, with space after each element
        std::cout << "Weights (by pixel layout, float 0.0/1.0):" << std::endl;
        std::cout << std::fixed << std::setprecision(1);
        int plane_size = di.weight_count;
        if (bm.is_dual_plane) {
            std::cout << "Plane 0:" << std::endl;
            for (int y = 0; y < di.weight_y; ++y) {
                for (int x = 0; x < di.weight_x; ++x) {
                    int idx = y * di.weight_x + x;
                    float w = static_cast<int>(scb.weights[idx]) == 0 ? 0.0f : 1.0f;
                    std::cout << std::setw(5) << w << " ";
                }
                std::cout << std::endl;
            }
            std::cout << "Plane 1:" << std::endl;
            for (int y = 0; y < di.weight_y; ++y) {
                for (int x = 0; x < di.weight_x; ++x) {
                    int idx = plane_size + y * di.weight_x + x;
                    float w = static_cast<int>(scb.weights[idx]) == 0 ? 0.0f : 1.0f;
                    std::cout << std::setw(5) << w << " ";
                }
                std::cout << std::endl;
            }
        } else {
            for (int y = 0; y < di.weight_y; ++y) {
                for (int x = 0; x < di.weight_x; ++x) {
                    int idx = y * di.weight_x + x;
                    float w = static_cast<int>(scb.weights[idx]) == 0 ? 0.0f : 1.0f;
                    std::cout << std::setw(5) << w << " ";
                }
                std::cout << std::endl;
            }
        }
        std::cout.unsetf(std::ios::fixed);

        // Output endpoints
        for (int p = 0; p < scb.partition_count; ++p) {
            std::cout << "Partition " << std::to_string(p) << " endpoints: ";
            int vals = 2 * (scb.color_formats[p] >> 2) + 2;
            for (int j = 0; j < vals; ++j) {
                if (j > 0) std::cout << ", ";
                std::cout << std::to_string(static_cast<int>(scb.color_values[p][j]));
            }
            std::cout << " (format: " << std::to_string(static_cast<int>(scb.color_formats[p])) << ")" << std::endl;
        }
    }
    else if
    (scb.block_type == SYM_BTYPE_CONST_U16 || scb.block_type == SYM_BTYPE_CONST_F16) 
    {
        std::cout << "Constant color: ";
        for (int i = 0; i < 4; ++i) 
        {
            if (i > 0) std::cout << ", ";
            std::cout << scb.constant_color[i];
        }
        std::cout << std::endl;
    }

    std::cout << "=== End Block Parameters ===" << std::endl;
}

// 支持指定块大小的解压函数（带默认参数）
void decompress_astc_block_from_vec(const std::vector<uint8_t>& compressed)
{
    if (compressed.size() != 16) 
    {
        std::cout << "Error: compressed_block size != 16, cannot decompress." << std::endl;
        return;
    }
    
    // 读取块信息
    astcenc_block_info block_info;
    if (!read_astc_block_info(compressed, block_info))
    {
        std::cout << "Failed to read block info!" << std::endl;
        return;
    }
    
    // 分配足够大的输出缓冲区
    uint8_t out_pixels[16 * 4] = { 0 }; // 最大支持 4x4 块
    
    // 直接调用原有解压流程
    bool ok = decompress_astc_block(compressed.data(), out_pixels, block_info.block_x, block_info.block_y, 4);
    if (!ok) 
    {
        std::cout << "Decompression failed!" << std::endl;
        return;
    }

    // 输出块信息
    print_astc_block_info(block_info);

    // 输出解压的像素数据（按块布局）
    print_decompressed_colors(out_pixels, block_info.block_x, block_info.block_y, 4);

}

// Function to print ASTC block information in a readable format
// Helper function to get dual plane component name
std::string get_dual_plane_component_name(unsigned int component)
{
    switch (component)
    {
        case 0: return "Red";
        case 1: return "Green";
        case 2: return "Blue";
        case 3: return "Alpha";
        default: return "Unknown(" + std::to_string(component) + ")";
    }
}

// Helper function to get color endpoint mode name
std::string get_color_endpoint_mode_name(unsigned int mode)
{
    switch (mode)
    {
        case 0: return "FMT_LUMINANCE";
        case 1: return "FMT_LUMINANCE_DELTA";
        case 2: return "FMT_HDR_LUMINANCE_LARGE_RANGE";
        case 3: return "FMT_HDR_LUMINANCE_SMALL_RANGE";
        case 4: return "FMT_LUMINANCE_ALPHA";
        case 5: return "FMT_LUMINANCE_ALPHA_DELTA";
        case 6: return "FMT_RGB_SCALE";
        case 7: return "FMT_HDR_RGB_SCALE";
        case 8: return "FMT_RGB";
        case 9: return "FMT_RGB_DELTA";
        case 10: return "FMT_RGB_SCALE_ALPHA";
        case 11: return "FMT_HDR_RGB";
        case 12: return "FMT_RGBA";
        case 13: return "FMT_RGBA_DELTA";
        case 14: return "FMT_HDR_RGB_LDR_ALPHA";
        case 15: return "FMT_HDR_RGBA";
        default: return "UNKNOWN(" + std::to_string(mode) + ")";
    }
}

void print_astc_block_info(const astcenc_block_info& info)
{
    std::cout << "=== ASTC Block Information ===" << std::endl;
    std::cout << "Block dimensions: " << info.block_x << "x" << info.block_y << "x" << info.block_z << std::endl;
    std::cout << "Texel count: " << info.texel_count << std::endl;
    std::cout << "Profile: " << get_profile_name(info.profile) << std::endl;
    
    if (info.is_error_block)
    {
        std::cout << "Block Type: ERROR BLOCK" << std::endl;
        return;
    }
    
    if (info.is_constant_block)
    {
        std::cout << "Block Type: CONSTANT COLOR" << std::endl;
        return;
    }
    
    std::cout << "Block Type: NORMAL BLOCK" << std::endl;
    if (info.is_hdr_block)
    {
        std::cout << "HDR block: Yes" << std::endl;
    }
    
    std::cout << "Partition count: " << info.partition_count << std::endl;
    std::cout << "Partition index: " << info.partition_index << std::endl;
    std::cout << "Dual plane: " << (info.is_dual_plane_block ? "Yes" : "No") << std::endl;
    
    if (info.is_dual_plane_block)
    {
        std::cout << "Dual plane component: " << get_dual_plane_component_name(info.dual_plane_component) << std::endl;
    }
    
    std::cout << "Weight grid: " << info.weight_x << "x" << info.weight_y << "x" << info.weight_z << std::endl;
    std::cout << "Color quantization: " << info.color_level_count << " levels" << std::endl;
    std::cout << "Weight quantization: " << info.weight_level_count << " levels" << std::endl;
    
    std::cout << "Color endpoint modes: ";
    for (int p = 0; p < info.partition_count; ++p)
    {
        if (p > 0)
        {
            std::cout << ", ";
        }
        std::cout << get_color_endpoint_mode_name(info.color_endpoint_modes[p]) << "\n";
    }
    std::cout << "\n";
    
    std::cout << "Color endpoints:" << std::endl;
    for (int p = 0; p < info.partition_count; ++p)
    {
        std::cout << "  Partition " << p << ":" << std::endl;
        for (int e = 0; e < 2; ++e)
        {
            std::cout << "    Endpoint " << e << ": ";
            for (int c = 0; c < 4; ++c)
            {
                if (c > 0) std::cout << ", ";
                std::cout << std::fixed << std::setprecision(2) << info.color_endpoints[p][e][c];
            }
            std::cout << std::endl;
        }
    }

    std::cout << "\n";

    // Print weights in block layout (normalized 0.0-1.0)
    std::cout << "Weight values (plane 1, normalized 0.0-1.0):" << std::endl;
    std::cout << std::fixed << std::setprecision(2) << std::left;
    for (int y = 0; y < info.block_y; ++y)
    {
        std::cout << "  ";
        for (int x = 0; x < info.block_x; ++x)
        {
            int idx = y * info.block_x + x;
            std::cout << std::setw(4) << info.weight_values_plane1[idx] / 4.0f << " ";
        }
        std::cout << std::endl;
    }

    if (info.is_dual_plane_block)
    {
        std::cout << "\nWeight values (plane 2, normalized 0.0-1.0):" << std::endl;
        std::cout << std::fixed << std::setprecision(2) << std::left;
        for (int y = 0; y < info.block_y; ++y)
        {
            std::cout << "  ";
            for (int x = 0; x < info.block_x; ++x)
            {
                int idx = y * info.block_x + x;
                std::cout << std::setw(4) << info.weight_values_plane2[idx] / 4.0f << " ";
            }
            std::cout << std::endl;
        }
    }

    std::cout << "\n";
}

// Convenience function to read block info with automatic block size detection
bool read_astc_block_info(const std::vector<uint8_t>& compressed, astcenc_block_info& info)
{
    if (compressed.size() != 16)
    {
        std::cout << "Error: compressed_block size != 16, cannot read info." << std::endl;
        return false;
    }

    // Try to detect block size by attempting to parse with different block sizes
    // Common ASTC block sizes: 4x4, 5x5, 6x6, 8x8, 10x10, 12x12
    const int block_sizes[][2] = {
        {4, 4}, {5, 5}, {6, 6}, {8, 8}, {10, 10}, {12, 12},
        {4, 5}, {5, 4}, {6, 5}, {5, 6}, {8, 6}, {6, 8},
        {10, 6}, {6, 10}, {8, 5}, {5, 8}, {10, 5}, {5, 10}
    };
    
    const int num_block_sizes = sizeof(block_sizes) / sizeof(block_sizes[0]);
    
    for (int i = 0; i < num_block_sizes; ++i)
    {
        int block_width = block_sizes[i][0];
        int block_height = block_sizes[i][1];
        
        // Create ASTC context for this block size
        astcenc_config config;
        astcenc_error status = astcenc_config_init(ASTCENC_PRF_LDR, block_width, block_height, 1, 
                                                  ASTCENC_PRE_MEDIUM, 0, &config);
        if (status != ASTCENC_SUCCESS)
        {
            continue;
        }
        
        astcenc_context* context = nullptr;
        status = astcenc_context_alloc(&config, 1, &context);
        if (status != ASTCENC_SUCCESS || !context)
        {
            continue;
        }
        
        // Try to get block info using astcenc_get_block_info
        status = astcenc_get_block_info(context, compressed.data(), &info);
        
        // Free the context
        astcenc_context_free(context);
        
        if (status == ASTCENC_SUCCESS)
        {
            // Successfully parsed! The info structure is now populated
            return true;
        }
    }
    
    // If we get here, we couldn't parse the block with any known block size
    std::cout << "Error: Could not parse compressed block with any known block size!" << std::endl;
    std::cout << "Tried block sizes: ";
    for (int i = 0; i < num_block_sizes; ++i)
    {
        if (i > 0) std::cout << ", ";
        std::cout << block_sizes[i][0] << "x" << block_sizes[i][1];
    }
    std::cout << std::endl;
    
    return false;
}


