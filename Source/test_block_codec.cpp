#include "astc_block_codec.h"
#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <algorithm>
#include <iomanip> // Required for std::setw and std::setfill

/*
Example usage:

1. Test with default configuration (loads example_config.ini):
   ./test_block_codec

2. Test with 6x6 block:
   ./test_block_codec -b 6x6

3. Test with 8x8 block and custom parameters:
   ./test_block_codec -b 8x8 -m 1 -p 2

4. Test with custom endpoints:
   ./test_block_codec -b 4x4 -e 255,255,255,255,128,128,128,128

5. Test with custom weights (number depends on block size and mode):
   ./test_block_codec -b 6x6 -w 128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128

6. List supported block sizes:
   ./test_block_codec -l

7. Test public functions:
   ./test_block_codec -t

8. Explain block modes for 6x6:
   ./test_block_codec -x 6x6

9. List decimation modes for 6x6:
   ./test_block_codec -d 6x6

10. List quantization levels:
    ./test_block_codec -q

11. Calculate block_mode from parameters (6x6, decimation=0, quant=4, single plane):
    ./test_block_codec -c 6x6 0 4 0

12. Analyze block_mode parameters:
    ./test_block_codec -a 6x6 5

13. Validate HLSL preset configuration and execute compression test:
    ./test_block_codec -v 4x4_rgba_standard

14. Run all HLSL preset tests:
    ./test_block_codec --test-all-hlsl

15. List available HLSL configuration presets:
    ./test_block_codec --list-hlsl-presets

16. Load parameters from configuration file:
    ./test_block_codec -f my_config.ini

17. Create example configuration file:
    ./test_block_codec --create-config example.ini

18. Show help:
    ./test_block_codec -h

Default Configuration:
- When no arguments are provided, the program automatically loads default_config.ini
- The default config file path can be customized via CMake using DEFAULT_CONFIG_FILE_PATH
- Example CMake configuration:
  cmake .. -DUSE_CUSTOM_DEFAULT_CONFIG=ON -DDEFAULT_CONFIG_PATH="/path/to/custom_config.ini"

Supported block sizes: 4x4, 5x4, 5x5, 6x5, 6x6, 8x5, 8x6, 8x8, 10x5, 10x6, 10x8, 10x10, 12x10, 12x12

Note: The number of weights required varies by block size and block mode.
      The program will automatically determine the required weight count and
      fill missing weights with default values (128).

Block Mode Parameters:
- Decimation Mode: Determines weight distribution pattern (0-255)
- Quantization Mode: Determines weight precision (0-20, see -q for details)
- Dual Plane: Whether to use separate weight planes (0=no, 1=yes)

Code Structure:
- ASTCParameterParser: Handles all command line argument parsing
- handle_special_commands(): Processes help, list, test, and analysis commands
- execute_compression_test(): Main compression workflow
- validate_compression_parameters(): Parameter validation
- print_compression_parameters(): Display test parameters
- execute_compression(): Perform actual compression and show results
- test_public_utilities(): Internal test function (in astc_block_codec.cpp)

The main() function is now clean and focused on high-level program flow.
test_public_functions() has been moved to astc_block_codec.cpp as test_public_utilities().
*/


int main(int argc, char* argv[])
{
    // Parse command line arguments
    ASTCParameterParser::Parameters params;
    if (!ASTCParameterParser::parse(argc, argv, params))
    {
        return 1;
    }

    // Handle special commands first
    if (handle_special_commands(params, argv[0]))
    {
        return 0;
    }

    // Execute main compression test
    if (!execute_compression_test(params))
    {
        return 1;
    }

    return 0;
}
