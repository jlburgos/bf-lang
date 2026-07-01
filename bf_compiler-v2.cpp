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

enum class OpCode : std::uint8_t {
  INVALID_CODE      = 0, // undefined state
  MOVE_LEFT         = '<',
  MOVE_RIGHT        = '>',
  INCREMENT         = '+',
  DECREMENT         = '-',
  OUTPUT            = '.',
  INPUT             = ',',
  JUMP_IF_ZERO      = '[',
  JUMP_IF_NOT_ZERO  = ']'
};

struct Instruction {
  std::size_t operand{};
  OpCode opcode{OpCode::INVALID_CODE};
};

struct Program {
  std::vector<Instruction> instructions;
};

static Program compile(const std::string_view program) {
  std::vector<std::size_t> loop_index{};

  Program parsed_program{};
  parsed_program.instructions.reserve(program.size());

  const auto emit = [&parsed_program](const std::size_t operand, const OpCode op) -> void {
    parsed_program.instructions.emplace_back(operand, op);
  };

  for (std::size_t i = 0; i < program.size(); ++i) {
    const char ch = program[i];
    switch (ch) {
      case static_cast<std::uint8_t>(OpCode::MOVE_RIGHT):
        emit(1, OpCode::MOVE_RIGHT);
        break;
      case static_cast<std::uint8_t>(OpCode::MOVE_LEFT):
        emit(1, OpCode::MOVE_LEFT);
        break;
      case static_cast<std::uint8_t>(OpCode::INCREMENT):
        emit(1, OpCode::INCREMENT);
        break;
      case static_cast<std::uint8_t>(OpCode::DECREMENT):
        emit(1, OpCode::DECREMENT);
        break;
      case static_cast<std::uint8_t>(OpCode::OUTPUT):
        emit(0, OpCode::OUTPUT);
        break;
      case static_cast<std::uint8_t>(OpCode::INPUT):
        emit(0, OpCode::INPUT);
        break;
      case static_cast<std::uint8_t>(OpCode::JUMP_IF_ZERO):
        loop_index.push_back(parsed_program.instructions.size()); // store index for matching ']'
        emit(0, OpCode::JUMP_IF_ZERO); // default jump index to zero, will update later
        break;
      case static_cast<std::uint8_t>(OpCode::JUMP_IF_NOT_ZERO):
        if (loop_index.empty()) {
          throw std::runtime_error("There are unmatched ']' instructions, which produces an invalid program!");
        }
        parsed_program.instructions[loop_index.back()].operand = parsed_program.instructions.size(); // jump from '[' to matching ']'
        emit(loop_index.back(), OpCode::JUMP_IF_NOT_ZERO); // jump from ']' to matching '['
        loop_index.pop_back();
        break;
      default:
        throw std::runtime_error(std::format("Invalid input '{}' was encountered! This should never happen!", ch));
    }
  }
  if (loop_index.empty()) {
    return parsed_program;
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

static void run(const Program& program) {
  std::array<std::uint8_t, 30'000> buffer{};
  std::size_t memory_pointer{};

  // Interpret and run BF program
  for (std::size_t i = 0; i < program.instructions.size(); ++i) {
    const Instruction& instruction = program.instructions[i];
    switch (instruction.opcode) {
      case OpCode::MOVE_RIGHT:
        if (memory_pointer == buffer.size() - instruction.operand) {
          throw std::runtime_error(std::format("Error: Program data pointer increment from instruction at (index: {}) caused out-of-bounds indexing!", i));
        }
        memory_pointer += instruction.operand;
        break;
      case OpCode::MOVE_LEFT:
        if (memory_pointer == instruction.operand - 1) {
          throw std::runtime_error(std::format("Error: Program data pointer decrement from instruction at (index: {}) caused out-of-bounds indexing!", i));
        }
        memory_pointer -= instruction.operand;
        break;
      case OpCode::INCREMENT:
        buffer[memory_pointer] += instruction.operand;
        break;
      case OpCode::DECREMENT:
        buffer[memory_pointer] -= instruction.operand;
        break;
      case OpCode::OUTPUT:
        std::cout << static_cast<char>(buffer[memory_pointer]);
        break;
      case OpCode::INPUT:
        // Read single byte of user input and ignore any additional user input
        char input;
        if (std::cin.get(input)) {
          buffer[memory_pointer] = static_cast<std::uint8_t>(input);
          std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
        break;
      case OpCode::JUMP_IF_ZERO:
        if (buffer[memory_pointer] == 0) {
          i = instruction.operand;
        }
        break;
      case OpCode::JUMP_IF_NOT_ZERO:
        if (buffer[memory_pointer] != 0) {
          i = instruction.operand;
        }
        break;
      case OpCode::INVALID_CODE:
        throw std::runtime_error(std::format("Invalid input was encountered! This should never happen!"));
    }
  }
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Need to provide filepath!" << std::endl;
    return 1;
  }
  try {
    run(compile(read_program_file(argv[1])));
  } catch (const std::exception& e) {
    std::cerr << "Failed with error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
