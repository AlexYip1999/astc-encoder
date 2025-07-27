#pragma once
#include <astcenc.h>
#include <cstdint>
#include <vector> // Added for std::vector
#include <string> // Added for std::string

// Default configuration file path
// This can be overridden by CMake
#ifndef DEFAULT_CONFIG_FILE_PATH
#define DEFAULT_CONFIG_FILE_PATH "default_config.ini"
#endif

// Parameter parser class for command line arguments
class ASTCParameterParser
{
public:
    // Structure to hold parsed parameters
    struct Parameters
    {
        int block_width = 4;
        int block_height = 4;
        int mode = 0; // 0=auto, >0=force specified block mode
        int partition = 0;
        uint8_t endpoints[8] = {255, 255, 255, 255, 0, 0, 0, 0};
        std::vector<uint8_t> weights;
        bool show_help = false;
        bool list_block_sizes = false;
        bool test_public_functions = false;
        bool explain_block_mode = false;
        bool list_decimation_modes = false;
        bool list_quantization_levels = false;
        bool calculate_block_mode = false;
        
        // For calculate_block_mode
        int calc_decimation_mode = 0;
        int calc_quant_mode = 0;
        bool calc_is_dual_plane = false;
        
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
    static bool parse_endpoints(const std::string& str, uint8_t endpoints[8]);
    static bool parse_weights(const std::string& str, std::vector<uint8_t>& weights);
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
bool decompress_astc_block(const uint8_t* compressed, uint8_t* block, int width, int height, int channels);

// Manual ASTC block construction with configurable block size
// compressed: output compressed data, length 16 bytes
// block_width, block_height: block dimensions
// block_mode: block mode index
// partition: partition index
// endpoints: endpoint color values, length endpoint_count
// endpoint_count: number of endpoint values (should be 8 for RGBA, 6 for RGB)
// weights: weight values, length weight_count
// weight_count: number of weight values (should match the weight count for the given block size and mode)
// has_alpha: whether the block has alpha channel, affects color_formats and endpoint count
// Returns true on success
// Note: The actual number of weights needed depends on the block size and block mode.
//       Use get_weight_count_for_block_mode() to determine the required weight count.
bool manual_construct_astc_block(uint8_t* compressed, 
                                 int block_width, int block_height,
                                 int block_mode, int partition, 
                                 const uint8_t* endpoints, int endpoint_count, 
                                 const uint8_t* weights, int weight_count,
                                 bool has_alpha);

// Get the required weight count for a specific block size and mode
// block_width, block_height: block dimensions
// block_mode: block mode index
// Returns the number of weights required, or 0 if the block mode is invalid
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
// decimation_mode: weight distribution pattern (0-255)
// quant_mode: weight quantization level (0-20, see quant_method enum)
// is_dual_plane: whether to use dual weight planes
// Returns the calculated block_mode, or -1 if invalid combination
int calculate_block_mode(int block_width, int block_height, int decimation_mode, int quant_mode, bool is_dual_plane);

// Get block_mode parameters from a given block_mode
// block_width, block_height: block dimensions
// block_mode: the block mode to analyze
// decimation_mode: output decimation mode
// quant_mode: output quantization mode
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
bool handle_special_commands(const ASTCParameterParser::Parameters& params, const char* program_name);

// Execute the main ASTC compression test
bool execute_compression_test(const ASTCParameterParser::Parameters& params);

// Validate parameters for compression test
bool validate_compression_parameters(const ASTCParameterParser::Parameters& params);

// Print compression test parameters
void print_compression_parameters(const ASTCParameterParser::Parameters& params, int required_weight_count);

// Execute compression and display results
bool execute_compression(const ASTCParameterParser::Parameters& params, int required_weight_count);

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
    int decimation_mode;
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
