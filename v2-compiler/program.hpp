#pragma once

#include <vector>

#include "ir.hpp"
#include "config.hpp"

using instruction_list_t = std::vector<ir::Instruction>;
class Program {
public:
  Program() = default;
  Program(const Program&) = default;
  Program(Program&&) noexcept = default;
  Program& operator=(const Program&) = default;
  Program& operator=(Program&&) noexcept = default;
  ~Program() = default;

  explicit Program(const Config& config) : config(config) {}

  inline std::size_t size() const noexcept { return instructions.size(); }

  inline void reserve_capacity(const std::size_t i) { instructions.reserve(i); }
  inline void compress_capacity() { instructions.shrink_to_fit(); }

  inline ir::Instruction& get_instruction(const std::size_t i) noexcept { return instructions[i]; }
  inline const ir::Instruction& get_instruction(const std::size_t i) const noexcept { return instructions[i]; }

  inline instruction_list_t& get_instructions() noexcept { return instructions; }
  inline const instruction_list_t& get_instructions() const noexcept { return instructions; }

  inline void update_instructions(instruction_list_t&& instructions) noexcept {
    this->instructions = std::move(instructions);
  }

  void print_intermediate_representation() const noexcept {
    const std::size_t num_digits = (size() == 0) ? 1 : (std::log10(size()) + 1);
    std::size_t loop_depth = 0;
    for (std::size_t i = 0; i < size(); ++i) {
      const ir::Instruction& instruction = get_instruction(i);
      if (instruction.opcode == lexer::OpCode::JUMP_IF_NOT_ZERO) {
        loop_depth -= 1;
      }
      std::cout << std::setw(num_digits) << std::setfill('0') << std::right << i
                << std::string(loop_depth, '\t') << ' ' << ir::instruction_to_str(instruction)
                << std::endl;
      if (instruction.opcode == lexer::OpCode::JUMP_IF_ZERO) {
        loop_depth += 1;
      }
    }
  }

  // TODO :: program.push(..), program.back(), program[index]
  ir::Instruction& push(const ir::Instruction& instruction) noexcept {
    instructions.push_back(instruction);
    return back();
  }

  ir::Instruction& back() {
    if (instructions.empty()) {
      throw std::runtime_error("Cannot call back() on empty instruction list!");
    }
    return instructions.back();
  }

  const ir::Instruction& back() const {
    if (instructions.empty()) {
      throw std::runtime_error("Cannot call back() on empty instruction list!");
    }
    return instructions.back();
  }

  ir::Instruction& operator[](const std::size_t index) {
    return get_instruction(index);
  }

  const ir::Instruction& operator[](const std::size_t index) const {
    return get_instruction(index);
  }

  ir::Instruction pop_back() {
    ir::Instruction instruction = back();
    instructions.pop_back();
    return instruction;
  }

  void build_intermediate_representation(const std::string_view raw_program_tokens) {
    // Reset instructions for a new program IR
    instructions.clear();
    instructions.reserve(raw_program_tokens.size());

    // Maintain memory addresses for jumps
    std::vector<std::size_t> loop_stack{};

    for (const char ch : raw_program_tokens) {
      switch (static_cast<lexer::Tokens>(ch)) {
        case lexer::Tokens::MOVE_RIGHT:
          emit_instruction(instructions, lexer::OpCode::MOVE, 1);
          break;
        case lexer::Tokens::MOVE_LEFT:
          emit_instruction(instructions, lexer::OpCode::MOVE, -1);
          break;
        case lexer::Tokens::INCREMENT:
          emit_instruction(instructions, lexer::OpCode::ADD, 1);
          break;
        case lexer::Tokens::DECREMENT:
          emit_instruction(instructions, lexer::OpCode::ADD, -1);
          break;
        case lexer::Tokens::OUTPUT:
          emit_instruction(instructions, lexer::OpCode::OUTPUT);
          break;
        case lexer::Tokens::INPUT:
          emit_instruction(instructions, lexer::OpCode::INPUT);
          break;
        case lexer::Tokens::JUMP_IF_ZERO:
          {
            // set default jump index to zero, will update later
            const std::size_t open_loop_index = emit_instruction(instructions, lexer::OpCode::JUMP_IF_ZERO);
            // store index for matching ']'
            loop_stack.push_back(open_loop_index);
          }
          break;
        case lexer::Tokens::JUMP_IF_NOT_ZERO:
          {
            if (loop_stack.empty()) {
              throw std::runtime_error("There are unmatched ']' instructions, which produces an invalid program!");
            }
            // set jump from ']' to matching '['
            const std::size_t open_loop_index = loop_stack.back();
            loop_stack.pop_back();
            const std::size_t close_loop_index = emit_instruction(instructions, lexer::OpCode::JUMP_IF_NOT_ZERO, open_loop_index);
            // set jump from '[' to matching ']'
            instructions[open_loop_index].operand = close_loop_index;
          }
          break;
        default:
          throw std::runtime_error(std::format("Invalid input '{}' was encountered! This should never happen!", ch));
      }
    }
    if (!loop_stack.empty()) {
      throw std::runtime_error("There are unmatched '[' instructions, which produces an invalid program!");
    }
    instructions.shrink_to_fit();
  }

  void run() {
    std::array<std::uint8_t, 30'000> buffer{};
    std::size_t memory_pointer{};
    std::size_t instruction_pointer{};

    const auto is_valid_move = [&](const ir::operand_t operand) -> void {
      if (operand > 0 && (memory_pointer >= buffer.size() - operand)) {
        throw std::runtime_error(
            std::format("Error: Program data pointer increment from instruction "
                        "at (index: {}) caused out-of-bounds indexing!",
                        instruction_pointer));
      }
      const ir::operand_t cell_index = static_cast<ir::operand_t>(memory_pointer);
      if (operand < 0 && (cell_index <= operand - 1)) {
        throw std::runtime_error(
            std::format("Error: Program data pointer decrement from instruction "
                        "at (index: {}) caused out-of-bounds indexing!",
                        instruction_pointer));
      }
    };

    const auto read_input = [&]() -> void {
      // Read single byte of user input and ignore any additional user input
      char input;
      if (std::cin.get(input)) {
        buffer[memory_pointer] = static_cast<std::uint8_t>(input);
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      }
    };

    while (instruction_pointer < get_instructions().size()) {
      const ir::Instruction& instruction = get_instruction(instruction_pointer);
      switch (instruction.opcode) {
        case lexer::OpCode::MOVE:
          is_valid_move(instruction.operand);
          memory_pointer += instruction.operand;
          break;
        case lexer::OpCode::ADD:
          buffer[memory_pointer] += instruction.operand;
          break;
        case lexer::OpCode::OUTPUT:
          std::cout << static_cast<char>(buffer[memory_pointer]);
          break;
        case lexer::OpCode::INPUT:
          read_input();
          break;
        case lexer::OpCode::JUMP_IF_ZERO:
          if (buffer[memory_pointer] == 0) {
            instruction_pointer = instruction.operand;
            continue;
          }
          break;
        case lexer::OpCode::JUMP_IF_NOT_ZERO:
          if (buffer[memory_pointer] != 0) {
            instruction_pointer = instruction.operand;
            continue;
          }
          break;
        default: throw std::logic_error("Unknown opcode detected in compiled program!");
      }
      ++instruction_pointer;
    }
  }

private:
  instruction_list_t instructions;
  Config config;
};