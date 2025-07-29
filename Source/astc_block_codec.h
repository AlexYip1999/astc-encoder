#pragma once
#include <astcenc.h>
#include <astcenc_internal.h>  // 添加这个包含以获取quant_method类型定义
#include <cstdint>
#include <vector> // Added for std::vector
#include <string> // Added for std::string

// Default configuration file path
// This can be overridden by CMake
#ifndef DEFAULT_CONFIG_FILE_PATH
#define DEFAULT_CONFIG_FILE_PATH "default_config.ini"
#endif

// Parse weight quantization string to enum value
// str: input string (e.g., "QUANT_4", "QUANT_8")
// Returns quant_method enum value
quant_method parse_weight_quant_str(const std::string& str);

// Parameter parser class for command line arguments
class ASTCParameterParser
{
public:
    // Structure to hold parsed parameters
    struct Parameters
    {
        int block_width = 4;
        int block_height = 4;
        int block_mode = 0; // 0=auto, >0=force specified block block_mode
        int partition = 0;
        std::vector<uint8_t> endpoints = {255, 255, 255, 255, 0, 0, 0, 0};
        std::vector<float> weights;
        bool show_help = false;
        bool list_block_sizes = false;
        bool test_public_functions = false;
        bool explain_block_mode = false;
        bool list_decimation_modes = false;
        bool list_quantization_levels = false;
        bool list_dual_plane_modes = false;
        bool calculate_block_mode = true;
        
        // For block block_mode calculation
        int weight_grid_size = 16;  // Default to 4x4 (16 weights)
        bool is_dual_plane_mode = false;
        
        // For HLSL validation
        bool validate_hlsl = false;
        std::string hlsl_preset_name;
        bool test_all_hlsl_presets = false;
        bool list_hlsl_presets = false;
        
        // For configuration file
        bool load_from_config = false;
        std::string config_file_path;
        // Whether the block has alpha channel, affects endpoint and weight count
        bool has_alpha = true;
        // Whether dual plane, affects weight count and blockmode
        bool is_dual_plane = false;
        // Weight quantization method (string), e.g. QUANT_4, QUANT_48
        std::string weight_quant_str = "QUANT_4";
        // Dual plane component (string), e.g. "A", "R", "G", "B", "0", "1", "2", "3"
        std::string plane2_component_str = "A";
        // For decompress block from config
        // bool enable_decompress_block = false;
        // uint32_t decompress_block_u32[4] = {0, 0, 0, 0};
        // Save the result of the most recent compression
        // uint8_t last_compressed_block[16] = {0};
        // bool last_compressed_block_valid = false;
        std::vector<uint8_t> compressed_block; // 统一存储压缩块
        // Helper method to get quant_mode as enum value
        int get_weight_quant_mode() const
        {
            return static_cast<int>(parse_weight_quant_str(weight_quant_str));
        }
        // Helper method to get plane2_component as int
        int get_plane2_component() const;

    };

    // Parse command line arguments
    static bool parse(int argc, char* argv[], Parameters& params);

    // Print usage information
    static void print_usage(const char* program_name);

    // Print parsed parameters (for debugging)
    static void print_parameters(const Parameters& params);

private:
    // Helper functions for parsing specific parameter types
    static bool parse_block_size(const std::string& str, int& width, int& height);
    static bool parse_endpoints(const std::string& str, std::vector<uint8_t>& endpoints);
    static bool parse_weights(const std::string& str, std::vector<float>& weights);
    static bool parse_int(const std::string& str, int& value);
    static bool parse_bool(const std::string& str, bool& value);
};

// Compress a single 4x4 RGBA pixel block
// block: input pixel data, length 16*4=64 bytes
// compressed: output compressed data, length 16 bytes
// Returns true on success
bool compress_astc_block2d(const uint8_t* block, uint8_t* compressed, int width, int height, int channels, astcenc_type type);

// Decompress a single 4x4 ASTC compressed block
// compressed: input compressed data, length 16 bytes
// block: output pixel data, length 16*4=64 bytes
// Returns true on success
bool decompress_astc_block(const uint8_t* compressed, uint8_t* block, int& width, int& height, int& channels);

// Declaration for decompress_astc_block_from_u32, available for use in other files.
void decompress_astc_block_from_u32(const uint32_t compressed_u32[4], uint8_t* out_pixels);

// Declaration for decompress_astc_block_from_vec, available for use in other files.
void decompress_astc_block_from_vec(const std::vector<uint8_t>& compressed);

// Function to read block info with automatic block size detection
bool read_astc_block_info(const std::vector<uint8_t>& compressed, astcenc_block_info& info);

// Function to print ASTC block information in a readable format
void print_astc_block_info(const astcenc_block_info& info);

// Manual ASTC block construction with configurable block size
// compressed: output compressed data, length 16 bytes
// block_width, block_height: block dimensions
// block_mode: block block_mode index
// partition_index: partition index (0-based)
// partition_count: number of partitions (1-4)
// endpoints: endpoint color values, length endpoint_count
// endpoint_count: number of endpoint values (should be 8 for RGBA, 6 for RGB)
// weights: weight values, length weight_count
// weight_count: number of weight values (should match the weight count for the given block size and block_mode)
// has_alpha: whether the block has alpha channel, affects color_formats and endpoint count
// Returns true on success
// Note: The actual number of weights needed depends on the block size and block block_mode.
//       Use get_weight_count_for_block_mode() to determine the required weight count.
bool manual_construct_astc_block(uint8_t* compressed, 
                                int block_width, int block_height,
                                int block_mode, int partition_index, int partition_count,
                                const uint8_t* endpoints, int endpoint_count, 
                                const float* weights, int weight_count,
                                bool has_alpha, uint8_t* result);

// Get the required weight count for a specific block size and block_mode
// block_width, block_height: block dimensions
// block_mode: block block_mode index
// Returns the number of weights required, or 0 if the block block_mode is invalid
int get_weight_count_for_block_mode(int block_width, int block_height, int block_mode);

// Validate if a block size is supported by ASTC
// x, y: block dimensions
// Returns true if the block size is valid
bool is_valid_block_size(int x, int y);

// Get block size descriptor for a specific block size
// x, y, z: block dimensions
// Returns pointer to block_size_descriptor, or nullptr if failed
void* get_block_size_descriptor(int x, int y, int z);

// Print supported ASTC block sizes to console
void print_supported_block_sizes();

// Explain block_mode and show available modes for a specific block size
// block_width, block_height: block dimensions
// This function will display detailed information about available block modes
void explain_block_mode(int block_width, int block_height);

// Calculate block_mode from individual parameters
// block_width, block_height: block dimensions
// weight_grid_size: total number of weights (e.g., 16 for 4x4, 36 for 6x6)
// quant_mode: weight quantization level (0-20, see quant_method enum)
// is_dual_plane: whether to use dual weight planes
// Returns the calculated block_mode, or -1 if invalid combination
int calculate_block_mode(int block_width, int block_height, int weight_grid_size, int quant_mode, bool is_dual_plane);

// 通用函数：计算block mode并处理结果
// params: 输入参数，压缩模式下会更新params.block_mode
// is_compression_mode: 是否为压缩模式（影响输出格式和返回值）
// Returns true if successful, false if calculation failed (only in compression block_mode)
bool calculate_and_handle_block_mode(ASTCParameterParser::Parameters& params, bool is_compression_mode);

// Get block_mode parameters from a given block_mode
// block_width, block_height: block dimensions
// block_mode: the block block_mode to analyze
// decimation_mode: output decimation block_mode
// quant_mode: output quantization block_mode
// is_dual_plane: output dual plane flag
// Returns true if successful, false if block_mode is invalid
bool get_block_mode_parameters(int block_width, int block_height, int block_mode, 
                              int& decimation_mode, int& quant_mode, bool& is_dual_plane);

// List available decimation modes for a block size
// block_width, block_height: block dimensions
// This function will show all available decimation patterns
void list_decimation_modes(int block_width, int block_height);

// List available quantization levels
// This function will show all available quantization options
void list_quantization_levels();

// Main application functions
// Handle special commands (help, list, test, etc.)
bool handle_special_commands(ASTCParameterParser::Parameters& params, const char* program_name);

// Execute the main ASTC compression test
bool execute_compression_test(ASTCParameterParser::Parameters& params);

// Validate parameters for compression test
bool validate_compression_parameters(const ASTCParameterParser::Parameters& params);

// Print compression test parameters
void print_compression_parameters(const ASTCParameterParser::Parameters& params);

// Execute compression and display results
bool execute_compression(ASTCParameterParser::Parameters& params);

// Internal test function for public utilities
// This function tests the basic functionality of public utility functions
void test_public_utilities();

// HLSL configuration presets for validation
// Structure to hold HLSL-style configuration parameters
struct HLSLConfigPreset
{
    std::string name;
    bool block_6x6;
    bool has_alpha;
    bool is_dual_plane;
    bool is_normalmap;
    int x_grids;
    int y_grids;
    int weight_grid_size;
    int quant_mode;
    std::string description;
};

// Get predefined HLSL configuration presets
// Returns a vector of available configuration presets
std::vector<HLSLConfigPreset> get_hlsl_config_presets();

// Convert HLSL preset to C++ parameters
// preset: HLSL configuration preset
// params: output C++ parameters
void convert_hlsl_preset_to_cpp_params(const HLSLConfigPreset& preset, ASTCParameterParser::Parameters& params);

// Print available HLSL configuration presets
void print_hlsl_config_presets();

// Execute HLSL preset validation test
// preset_name: name of the preset to test
// Returns true if validation was successful
bool execute_hlsl_preset_validation(const std::string& preset_name);

// Execute all HLSL preset validation tests
// Returns true if all tests were successful
bool execute_all_hlsl_preset_tests();

// Configuration file parser
// Parse configuration from JSON or INI file
bool parse_config_file(const std::string& file_path, ASTCParameterParser::Parameters& params);

// Load and execute configuration from file
// config_file: path to configuration file
// Returns true if execution was successful
bool execute_config_from_file(const std::string& config_file);

// Create example configuration file
// file_path: path where to create the example file
// Returns true if file was created successfully
bool create_example_config_file(const std::string& file_path);

// Parse plane2 component string to int (0=R, 1=G, 2=B, 3=A)
int parse_plane2_component_str(const std::string& str);
