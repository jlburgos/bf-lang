// C libs
#include <cstdint>

// Containers
#include <array>
#include <string>
#include <string_view>
#include <ranges>
#include <vector>

// I/O
#include <iostream>
#include <format>
#include <fstream>
#include <limits>

#include <filesystem>
namespace fs = std::filesystem;

// Educational experiment:
//
// Dense table:
//   + O(1) direct indexing
//   + Excellent cache locality
//   - One entry per instruction
//
// Sparse table:
//   + Stores only loop instructions
//   + Lower memory usage for programs with few loops
//   - Hash lookup on every jump
#if defined(USE_DENSE_JUMP_TABLE)
using jump_table = std::vector<std::size_t>;
#elif defined(USE_SPARSE_JUMP_TABLE)
#include <unordered_map>
using jump_table = std::unordered_map<std::size_t, std::size_t>;
#else
#error "Need to pass either USE_DENSE_JUMP_TABLE or USE_SPARSE_JUMP_TABLE build parameters"
#endif

static jump_table mk_jump_table(const std::string_view program) {
#if defined(USE_DENSE_JUMP_TABLE)
  jump_table jumps(program.size());
#elif defined(USE_SPARSE_JUMP_TABLE)
  jump_table jumps{};
#endif
  std::vector<std::uint64_t> loop_index{};

  for (std::size_t i = 0; i < program.size(); ++i) {
    const char instruction = program[i];
    if (instruction == '[') {
      loop_index.push_back(i);
    } else if (instruction == ']') {
      if (loop_index.empty()) {
        throw std::runtime_error("There are unmatched ']' instructions, which produces an invalid program!");
      }
      jumps[loop_index.back()] = i; // jump from '[' to matching ']'
      jumps[i] = loop_index.back(); // jump from ']' to matching '['
      loop_index.pop_back();
    }
  }

  if (loop_index.empty()) {
    return jumps;
  }
  throw std::runtime_error("There are unmatched '[' instructions, which produces an invalid program!");
}

static std::string filter_program(const std::string_view program) {
  constexpr std::string_view valid_instructions = "><+-.,[]";
  const auto is_valid_instruction = [&valid_instructions](const char c) { return valid_instructions.contains(c); };
  return program
    | std::ranges::views::filter(is_valid_instruction)
    | std::ranges::to<std::string>();
}

static std::string read_program_file(const std::string_view filepath) {
  const fs::path path{filepath};
  if (!fs::exists(path)) {
    throw std::runtime_error(std::format("File {} does not exist!", filepath));
  }
  if (!fs::is_regular_file(path)) {
    throw std::runtime_error(std::format("Cannot open irregular file {}!", filepath));
  }

  std::ifstream ifs(path, std::ifstream::binary);
  if (!ifs.is_open()) {
    throw std::runtime_error(std::format("Failed to open file {}!", filepath));
  }

  std::string buffer(fs::file_size(path), '\0');
  if (ifs.read(buffer.data(), buffer.size())) {
    return filter_program(buffer);
  }
  throw std::runtime_error(std::format("Failed to read any data from file {}!", filepath));
}

static void interpret(const std::string_view program) {
  std::array<std::uint8_t, 30'000> buffer{};
  std::size_t memory_pointer{};

  // Defines the jump index for handling looping between matching '[' and ']' instructions
  jump_table jumps = mk_jump_table(program);

  // Interpret and run BF program
  for (std::size_t i = 0; i < program.size(); ++i) {
    switch (program[i]) {
      case '>':
        if (memory_pointer == buffer.size() - 1) {
          throw std::runtime_error(std::format("Error: Program data pointer increment from instruction '{}' (index: {}) caused out-of-bounds indexing!", program[i], i));
        }
        ++memory_pointer;
        break;
      case '<':
        if (memory_pointer == 0) {
          throw std::runtime_error(std::format("Error: Program data pointer decrement from instruction '{}' (index: {}) caused out-of-bounds indexing!", program[i], i));
        }
        --memory_pointer;
        break;
      case '+':
        ++buffer[memory_pointer];
        break;
      case '-':
        --buffer[memory_pointer];
        break;
      case '.':
        std::cout << static_cast<char>(buffer[memory_pointer]);
        break;
      case ',':
        // Read single byte of user input and ignore any additional user input
        char input;
        if (std::cin.get(input)) {
          buffer[memory_pointer] = static_cast<std::uint8_t>(input);
          std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
        break;
      case '[':
        if (buffer[memory_pointer] == 0) {
          i = jumps[i];
        }
        break;
      case ']':
        if (buffer[memory_pointer] != 0) {
          i = jumps[i];
        }
        break;
      default:
        throw std::runtime_error(std::format("Invalid input '{}' was encountered! This should never happen!", program[i]));
    }
  }
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Need to provide filepath!" << std::endl;
    return 1;
  }
  try {
    interpret(read_program_file(argv[1]));
  } catch (const std::exception& e) {
    std::cerr << "Failed with error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
