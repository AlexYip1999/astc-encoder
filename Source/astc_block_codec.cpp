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

// Function to get weight count for a specific block size and mode
int get_weight_count_for_block_mode(int block_width, int block_height, int block_mode)
{
    block_size_descriptor* bsd = static_cast<block_size_descriptor*>(get_block_size_descriptor(block_width, block_height, 1));
    if (!bsd) return 0;
    
    try
    {
        const auto& bm = bsd->get_block_mode(block_mode);
        const auto& di = bsd->get_decimation_info(bm.decimation_mode);
        return di.weight_count;
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
        try
        {
            const auto& bm = bsd->get_block_mode(mode);
            const auto& di = bsd->get_decimation_info(bm.decimation_mode);
            
            std::cout << std::setw(4) << mode << " | ";
            std::cout << std::setw(10) << (int)bm.decimation_mode << " | ";
            std::cout << std::setw(12) << (int)bm.quant_mode << " | ";
            std::cout << std::setw(10) << (bm.is_dual_plane ? "Yes" : "No") << " | ";
            std::cout << std::setw(12) << di.weight_count << " | ";
            
            // Description based on quantization
            std::string desc;
            switch (bm.quant_mode)
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
    std::cout << "- Weight Count: Total number of weights stored for this mode" << std::endl;
    std::cout << std::endl;
    std::cout << "Trade-offs:" << std::endl;
    std::cout << "- Lower quantization = smaller file size but lower quality" << std::endl;
    std::cout << "- Higher quantization = better quality but larger file size" << std::endl;
    std::cout << "- Dual plane = better color separation but uses more bits" << std::endl;
}

// Calculate block_mode from individual parameters
int calculate_block_mode(int block_width, int block_height, int decimation_mode, int quant_mode, bool is_dual_plane)
{
    block_size_descriptor* bsd = static_cast<block_size_descriptor*>(get_block_size_descriptor(block_width, block_height, 1));
    if (!bsd) return -1;

    // Search through all available block modes to find a match
    for (unsigned int mode = 0; mode < WEIGHTS_MAX_BLOCK_MODES; ++mode)
    {
        try
        {
            const auto& bm = bsd->get_block_mode(mode);
            
            // Check if this mode matches our parameters
            if (bm.decimation_mode == decimation_mode && 
                bm.quant_mode == quant_mode && 
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
    
    return -1; // No matching mode found
}

// Get block_mode parameters from a given block_mode
bool get_block_mode_parameters(int block_width, int block_height, int block_mode, 
                              int& decimation_mode, int& quant_mode, bool& is_dual_plane)
{
    block_size_descriptor* bsd = static_cast<block_size_descriptor*>(get_block_size_descriptor(block_width, block_height, 1));
    if (!bsd) return false;

    try
    {
        const auto& bm = bsd->get_block_mode(block_mode);
        decimation_mode = bm.decimation_mode;
        quant_mode = bm.quant_mode;
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
        try
        {
            const auto& bm = bsd->get_block_mode(mode);
            const auto& di = bsd->get_decimation_info(bm.decimation_mode);
            
            // Only show each decimation mode once
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

// Manual ASTC block construction, allowing users to directly set block mode, partition, endpoints, weights, etc.
// This is a minimal example: configurable LDR block size, single partition, direct endpoints/weights
bool manual_construct_astc_block(uint8_t* compressed,
                                 int block_width, int block_height,
                                 int block_mode, int partition,
                                 const uint8_t* endpoints, int endpoint_count,
                                 const uint8_t* weights, int weight_count,
                                 bool has_alpha)
{
    // 1. Get block_size_descriptor for the specified block size
    block_size_descriptor* bsd = static_cast<block_size_descriptor*>(get_block_size_descriptor(block_width, block_height, 1));
    if (!bsd) return false;

    // 2. Get the decimation info for the specified block mode
    const auto& bm = bsd->get_block_mode(block_mode);
    const auto& di = bsd->get_decimation_info(bm.decimation_mode);
    int actual_weight_count = di.weight_count;
    
    // Validate weight count
    if (weight_count < actual_weight_count)
    {
        // If provided weights are insufficient, fill with default values
        std::cout << "Warning: Provided " << weight_count << " weights, but " 
                  << actual_weight_count << " are needed for " << block_width << "x" 
                  << block_height << " block with mode " << block_mode << std::endl;
    }

    // 3. Construct symbolic_compressed_block
    symbolic_compressed_block scb;
    memset(&scb, 0, sizeof(scb));
    
    // Set basic parameters
    scb.block_type = SYM_BTYPE_NONCONST;  // Non-constant block
    scb.block_mode = block_mode;
    scb.partition_count = 1;
    scb.partition_index = partition;
    scb.plane2_component = -1;  // Single plane
    scb.color_formats_matched = 0;
    
    // Set color format
    scb.color_formats[0] = has_alpha ? FMT_RGBA : FMT_RGB;
    scb.color_formats[1] = 0;
    scb.color_formats[2] = 0;
    scb.color_formats[3] = 0;
    
    // Set endpoint color values
    scb.color_values[0][0] = endpoints[0];
    scb.color_values[0][1] = endpoints[1];
    scb.color_values[0][2] = endpoints[2];
    scb.color_values[0][3] = endpoints[3];
    scb.color_values[1][0] = endpoints[4];
    scb.color_values[1][1] = endpoints[5];
    scb.color_values[1][2] = endpoints[6];
    scb.color_values[1][3] = endpoints[7];
    
    // Set weights - use actual weight count from decimation info
    for (int i = 0; i < actual_weight_count; i++)
    {
        if (i < weight_count)
    {
        scb.weights[i] = weights[i];
        }
        else
        {
            // Fill remaining weights with default value (128)
            scb.weights[i] = 128;
        }
    }
    
    scb.quant_mode = bm.get_weight_quant_mode(); // Use the quant mode from block mode
    scb.errorval = 0.0f;

    // 4. Convert to physical block
    symbolic_to_physical(*bsd, scb, compressed);
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
            std::cout << "Default configuration loaded successfully." << std::endl;
            return true;
        }
        else
        {
            std::cerr << "Failed to load default configuration. Showing help..." << std::endl;
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
            if (!parse_int(argv[++i], params.calc_decimation_mode))
            {
                std::cerr << "Error: Invalid decimation mode" << std::endl;
                return false;
            }
            if (!parse_int(argv[++i], params.calc_quant_mode))
            {
                std::cerr << "Error: Invalid quantization mode" << std::endl;
                return false;
            }
            if (!parse_bool(argv[++i], params.calc_is_dual_plane))
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
            if (!parse_int(argv[++i], params.mode))
            {
                std::cerr << "Error: Invalid block mode" << std::endl;
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
    std::cout << "  -m <mode>            Block mode (default: 0)" << std::endl;
    std::cout << "  -p <partition>       Partition index (default: 0)" << std::endl;
    std::cout << "  -e <endpoints>       Endpoint values (8 values, default: 255,255,255,255,0,0,0,0)" << std::endl;
    std::cout << "  -w <weights>         Weight values (comma-separated, default: all 128)" << std::endl;
    std::cout << "  -h                   Show this help message" << std::endl;
    std::cout << "  -l                   List supported block sizes" << std::endl;
    std::cout << "  -t                   Test public functions" << std::endl;
    std::cout << "  -x <width>x<height>  Explain block modes for specific block size" << std::endl;
    std::cout << "  -d <width>x<height>  List decimation modes for specific block size" << std::endl;
    std::cout << "  -q                   List quantization levels" << std::endl;
    std::cout << "  -c <WxH> <dec> <quant> <dual>  Calculate block_mode from parameters" << std::endl;
    std::cout << "  -a <WxH> <mode>      Analyze block_mode parameters" << std::endl;
    std::cout << "  -v <preset>          Validate HLSL preset configuration and execute compression test" << std::endl;
    std::cout << "  --test-all-hlsl     Run all HLSL preset tests" << std::endl;
    std::cout << "  --list-hlsl-presets  List available HLSL configuration presets" << std::endl;
    std::cout << "  -f <config_file>     Load parameters from a configuration file" << std::endl;
    std::cout << "  --create-config <file>  Create an example configuration file" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_name << "                    # Load default configuration" << std::endl;
    std::cout << "  " << program_name << " -b 6x6" << std::endl;
    std::cout << "  " << program_name << " -b 8x8 -m 1 -p 2" << std::endl;
    std::cout << "  " << program_name << " -b 4x4 -e 255,255,255,255,128,128,128,128" << std::endl;
    std::cout << "  " << program_name << " -x 6x6" << std::endl;
    std::cout << "  " << program_name << " -d 6x6" << std::endl;
    std::cout << "  " << program_name << " -q" << std::endl;
    std::cout << "  " << program_name << " -c 6x6 0 4 0" << std::endl;
    std::cout << "  " << program_name << " -a 6x6 5" << std::endl;
    std::cout << "  " << program_name << " -v 4x4_rgba_standard" << std::endl;
    std::cout << "  " << program_name << " --test-all-hlsl" << std::endl;
    std::cout << "  " << program_name << " --list-hlsl-presets" << std::endl;
    std::cout << "  " << program_name << " -f my_config.ini" << std::endl;
    std::cout << "  " << program_name << " --create-config example.ini" << std::endl;
}

void ASTCParameterParser::print_parameters(const Parameters& params)
{
    std::cout << "=== Parsed Parameters ===" << std::endl;
    std::cout << "Block size: " << params.block_width << "x" << params.block_height << std::endl;
    std::cout << "Block mode: " << params.mode << std::endl;
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
            std::cout << (int)params.weights[i];
        }
    }
    std::cout << std::endl;
    std::cout << "Show help: " << (params.show_help ? "yes" : "no") << std::endl;
    std::cout << "List block sizes: " << (params.list_block_sizes ? "yes" : "no") << std::endl;
    std::cout << "Test public functions: " << (params.test_public_functions ? "yes" : "no") << std::endl;
    std::cout << "Explain block mode: " << (params.explain_block_mode ? "yes" : "no") << std::endl;
    std::cout << "List decimation modes: " << (params.list_decimation_modes ? "yes" : "no") << std::endl;
    std::cout << "List quantization levels: " << (params.list_quantization_levels ? "yes" : "no") << std::endl;
    std::cout << "Calculate block mode: " << (params.calculate_block_mode ? "yes" : "no") << std::endl;
    if (params.calculate_block_mode)
    {
        std::cout << "  Calc decimation: " << params.calc_decimation_mode << std::endl;
        std::cout << "  Calc quant: " << params.calc_quant_mode << std::endl;
        std::cout << "  Calc dual plane: " << (params.calc_is_dual_plane ? "yes" : "no") << std::endl;
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

bool ASTCParameterParser::parse_endpoints(const std::string& str, uint8_t endpoints[8])
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
    
    for (int i = 0; i < 8; i++)
    {
        endpoints[i] = static_cast<uint8_t>(values[i]);
    }
    
    return true;
}

bool ASTCParameterParser::parse_weights(const std::string& str, std::vector<uint8_t>& weights)
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
            int value = std::stoi(s.substr(pos, comma_pos - pos));
            weights.push_back(static_cast<uint8_t>(value));
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
    if (str == "QUANT_2") return QUANT_2;
    if (str == "QUANT_3") return QUANT_3;
    if (str == "QUANT_4") return QUANT_4;
    if (str == "QUANT_5") return QUANT_5;
    if (str == "QUANT_6") return QUANT_6;
    if (str == "QUANT_8") return QUANT_8;
    if (str == "QUANT_10") return QUANT_10;
    if (str == "QUANT_12") return QUANT_12;
    if (str == "QUANT_16") return QUANT_16;
    if (str == "QUANT_20") return QUANT_20;
    if (str == "QUANT_24") return QUANT_24;
    if (str == "QUANT_32") return QUANT_32;
    if (str == "QUANT_40") return QUANT_40;
    if (str == "QUANT_48") return QUANT_48;
    if (str == "QUANT_64") return QUANT_64;
    if (str == "QUANT_80") return QUANT_80;
    if (str == "QUANT_96") return QUANT_96;
    if (str == "QUANT_128") return QUANT_128;
    if (str == "QUANT_160") return QUANT_160;
    if (str == "QUANT_192") return QUANT_192;
    if (str == "QUANT_256") return QUANT_256;
    // 默认
    return QUANT_4;
}

// 在block mode计算/使用处，将params.weight_quant_str转为quant_mode数值
// 例如：
// quant_method quant = parse_weight_quant_str(params.weight_quant_str);
// int quant_mode = static_cast<int>(quant);

// Main application functions implementation
bool handle_special_commands(const ASTCParameterParser::Parameters& params, const char* program_name)
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

    if (params.list_hlsl_presets)
    {
        print_hlsl_config_presets();
        return true;
    }

    if (params.calculate_block_mode)
    {
        int calculated_mode = calculate_block_mode(params.block_width, params.block_height, 
                                                  params.calc_decimation_mode, params.calc_quant_mode, 
                                                  params.calc_is_dual_plane);
        
        if (calculated_mode >= 0)
        {
            std::cout << "Calculated block_mode: " << calculated_mode << std::endl;
            std::cout << "Parameters: " << params.block_width << "x" << params.block_height 
                      << ", decimation=" << params.calc_decimation_mode 
                      << ", quant=" << params.calc_quant_mode 
                      << ", dual_plane=" << (params.calc_is_dual_plane ? "yes" : "no") << std::endl;
        }
        else
        {
            std::cout << "No valid block_mode found for the given parameters." << std::endl;
            std::cout << "Try listing available modes with: -x " << params.block_width << "x" << params.block_height << std::endl;
        }
        return true;
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

bool execute_compression_test(const ASTCParameterParser::Parameters& params)
{
    // Validate parameters
    if (!validate_compression_parameters(params))
    {
        return false;
    }

    // Get required weight count
    int required_weight_count = get_weight_count_for_block_mode(params.block_width, params.block_height, params.mode);
    if (required_weight_count == 0)
    {
        std::cerr << "Error: Invalid block mode " << params.mode << " for " 
                  << params.block_width << "x" << params.block_height << " blocks" << std::endl;
        return false;
    }

    // Initialize weights if needed
    ASTCParameterParser::Parameters working_params = params;
    int expected_weight_count = working_params.block_width * working_params.block_height * (working_params.calc_is_dual_plane ? 2 : 1);
    if (working_params.weights.empty() || working_params.weights.size() < expected_weight_count)
    {
        working_params.weights.resize(expected_weight_count, 128); // Fill with default value 128
    }

    // Print parameters
    print_compression_parameters(working_params, required_weight_count);

    // Execute compression
    return execute_compression(working_params, required_weight_count);
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

void print_compression_parameters(const ASTCParameterParser::Parameters& params, int required_weight_count)
{
    std::cout << "Block size: " << params.block_width << "x" << params.block_height << std::endl;
    std::cout << "Block mode: " << params.mode << std::endl;
    std::cout << "Partition: " << params.partition << std::endl;
    std::cout << "Required weights: " << required_weight_count << std::endl;
    
    std::cout << "Endpoints: ";
    for (int i = 0; i < 8; i++)
    {
        if (i > 0) std::cout << ",";
        std::cout << (int)params.endpoints[i];
    }
    std::cout << std::endl;
    
    std::cout << "Weights: ";
    for (size_t i = 0; i < params.weights.size(); i++)
    {
        if (i > 0) std::cout << ",";
        std::cout << (int)params.weights[i];
    }
    std::cout << std::endl;
}

bool execute_compression(const ASTCParameterParser::Parameters& params, int required_weight_count)
{
    uint8_t manual_compressed[16] = {0};
    int endpoint_count = params.has_alpha ? 8 : 6;
    bool ok = manual_construct_astc_block(manual_compressed, params.block_width, params.block_height, 
                                         params.mode, params.partition, 
                                         params.endpoints, endpoint_count, 
                                         params.weights.data(), params.weights.size(),
                                         params.has_alpha);

    if (ok)
    {
        std::cout << "Compression successful!" << std::endl;
        std::cout << "Compressed data: ";
        for (int i = 0; i < 16; i++)
        {
            if (i > 0) std::cout << " ";
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)manual_compressed[i];
        }
        std::cout << std::dec << std::endl;
        
        // Calculate compression ratio
        int original_size = params.block_width * params.block_height * 4; // 4 bytes per pixel (RGBA)
        int compressed_size = 16; // ASTC blocks are always 16 bytes
        float ratio = (float)original_size / compressed_size;
        std::cout << "Compression ratio: " << std::fixed << std::setprecision(2) << ratio << ":1" << std::endl;
        return true;
    }
    else
    {
        std::cout << "Compression failed!" << std::endl;
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
    std::cout << "  4x4 mode 0 weights: " << get_weight_count_for_block_mode(4, 4, 0) << std::endl;
    std::cout << "  6x6 mode 0 weights: " << get_weight_count_for_block_mode(6, 6, 0) << std::endl;
    std::cout << "  8x8 mode 0 weights: " << get_weight_count_for_block_mode(8, 8, 0) << std::endl;

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
        0, 4,           // decimation_mode, quant_mode
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
        1, 4,           // decimation_mode, quant_mode
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
        0, 4,           // decimation_mode, quant_mode
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
        1, 4,           // decimation_mode, quant_mode
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
    params.calc_is_dual_plane = preset.is_dual_plane;
    
    // Set decimation and quantization modes
    params.calc_decimation_mode = preset.decimation_mode;
    params.calc_quant_mode = preset.quant_mode;
    
    // Set default partition
    params.partition = 0;
    
    // Set endpoints based on alpha and normal map settings
    if (preset.has_alpha)
    {
        if (preset.is_normalmap)
        {
            // Normal map with alpha (RGBA)
            params.endpoints[0] = 128; // R1 (normal X)
            params.endpoints[1] = 128; // G1 (normal Y)
            params.endpoints[2] = 255; // B1 (normal Z)
            params.endpoints[3] = 255; // A1 (alpha)
            params.endpoints[4] = 128; // R2
            params.endpoints[5] = 128; // G2
            params.endpoints[6] = 0;   // B2
            params.endpoints[7] = 0;   // A2
        }
        else
        {
            // Standard RGBA
            params.endpoints[0] = 255; // R1
            params.endpoints[1] = 255; // G1
            params.endpoints[2] = 255; // B1
            params.endpoints[3] = 255; // A1
            params.endpoints[4] = 0;   // R2
            params.endpoints[5] = 0;   // G2
            params.endpoints[6] = 0;   // B2
            params.endpoints[7] = 0;   // A2
        }
    }
    else
    {
        if (preset.is_normalmap)
        {
            // Normal map without alpha (RGB)
            params.endpoints[0] = 128; // R1 (normal X)
            params.endpoints[1] = 128; // G1 (normal Y)
            params.endpoints[2] = 255; // B1 (normal Z)
            params.endpoints[3] = 255; // A1 (unused)
            params.endpoints[4] = 128; // R2
            params.endpoints[5] = 128; // G2
            params.endpoints[6] = 0;   // B2
            params.endpoints[7] = 0;   // A2 (unused)
        }
        else
        {
            // Standard RGB
            params.endpoints[0] = 255; // R1
            params.endpoints[1] = 255; // G1
            params.endpoints[2] = 255; // B1
            params.endpoints[3] = 255; // A1 (unused)
            params.endpoints[4] = 0;   // R2
            params.endpoints[5] = 0;   // G2
            params.endpoints[6] = 0;   // B2
            params.endpoints[7] = 0;   // A2 (unused)
        }
    }
    
    // Calculate weights based on grid size and dual plane
    int weight_count = preset.x_grids * preset.y_grids;
    if (preset.is_dual_plane)
        weight_count *= 2;
    
    params.weights.resize(weight_count, 128); // Default weight value
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
        std::cout << "  Decimation mode: " << preset.decimation_mode << std::endl;
        std::cout << "  Quantization mode: " << preset.quant_mode << std::endl;
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
    std::cout << "  Dual plane: " << (cpp_params.calc_is_dual_plane ? "yes" : "no") << std::endl;
    std::cout << "  Weight count: " << cpp_params.weights.size() << std::endl;
    std::cout << "  Decimation mode: " << cpp_params.calc_decimation_mode << std::endl;
    std::cout << "  Quantization mode: " << cpp_params.calc_quant_mode << std::endl;
    
    // Calculate block mode
    int calculated_block_mode = calculate_block_mode(cpp_params.block_width, cpp_params.block_height,
                                                    cpp_params.calc_decimation_mode, cpp_params.calc_quant_mode,
                                                    cpp_params.calc_is_dual_plane);
    
    if (calculated_block_mode >= 0)
    {
        std::cout << "Calculated block mode: " << calculated_block_mode << std::endl;
        
        // Execute compression test
        cpp_params.mode = calculated_block_mode;
        
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
        std::cerr << "Could not calculate valid block mode for preset parameters" << std::endl;
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
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;

        // Remove leading/trailing whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);

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
                else if (key == "mode")
                    params.mode = std::stoi(value);
                else if (key == "partition")
                    params.partition = std::stoi(value);
                else if (key == "has_alpha")
                    params.has_alpha = (value == "true" || value == "1" || value == "yes");
                else if (key == "is_dual_plane")
                    params.calc_is_dual_plane = (value == "true" || value == "1" || value == "yes");
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
                    
                    for (int i = 0; i < 8 && i < (int)parts.size(); i++)
                    {
                        params.endpoints[i] = std::stoi(parts[i]);
                    }
                }
            }
            else if (current_section == "weights")
            {
                if (key == "values")
                {
                    // Parse comma-separated weight values
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
                        params.weights.push_back(std::stoi(part));
                    }
                }
                else if (key == "count")
                {
                    int count = std::stoi(value);
                    params.weights.resize(count, 128); // Default weight value
                }
            }
            else if (current_section == "calculation")
            {
                if (key == "decimation_mode")
                {
                    params.calc_decimation_mode = std::stoi(value);
                }
                else if (key == "quant_mode")
                {
                    params.calc_quant_mode = std::stoi(value);
                }
                else if (key == "is_dual_plane")
                {
                    params.calc_is_dual_plane = (value == "true" || value == "1" || value == "yes");
                }
            }
            else if (current_section == "weight_quant")
            {
                params.weight_quant_str = value;
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
    std::cout << "  Block mode: " << params.mode << std::endl;
    std::cout << "  Partition: " << params.partition << std::endl;
    std::cout << "  Weight count: " << params.weights.size() << std::endl;
    
    if (params.calc_decimation_mode >= 0 || params.calc_quant_mode >= 0)
    {
        std::cout << "  Decimation mode: " << params.calc_decimation_mode << std::endl;
        std::cout << "  Quantization mode: " << params.calc_quant_mode << std::endl;
        std::cout << "  Dual plane: " << (params.calc_is_dual_plane ? "yes" : "no") << std::endl;
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
    file << "# Block mode (0 for auto-calculation, or specific mode number)" << std::endl;
    file << "mode=0" << std::endl;
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
    file << "# Parameters for block mode calculation (when mode=0)" << std::endl;
    file << "decimation_mode=1" << std::endl;
    file << "quant_mode=4" << std::endl;
    file << "is_dual_plane=false" << std::endl;
    file << std::endl;
    
    file << "[analysis]" << std::endl;
    file << "# Parameters for block mode analysis" << std::endl;
    file << "block_mode_value=5" << std::endl;
    file << std::endl;
    
    file << "[weight_quant]" << std::endl;
    file << "# Weight quantization mode (e.g., 4, 5, 6)" << std::endl;
    file << "mode=4" << std::endl;
    file << std::endl;
    
    file << "# Additional notes:" << std::endl;
    file << "# - Block mode 0 means auto-calculate from decimation/quant/dual_plane" << std::endl;
    file << "# - Weight count should match the expected count for the block size and mode" << std::endl;
    file << "# - Endpoint values are 8-bit (0-255)" << std::endl;
    file << "# - Weight values are 8-bit (0-255)" << std::endl;

    file.close();
    std::cout << "Example configuration file created: " << file_path << std::endl;
    return true;
}

