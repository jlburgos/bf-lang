#pragma once

// C Libs
#include <cstdint>

// Containers
#include <string>
#include <vector>

#include "lexer.hpp"

/*
namespace ir {
  using operand_t = std::int64_t;
  struct Instruction;
  using instruction_list = std::vector<Instruction>;

  static std::string instruction_to_str(const Instruction& instruction);
  inline static std::size_t emit_instruction(instruction_list& instructions, const lexer::OpCode opcode, operand_t operand = 0);
  inline static std::size_t emit_instruction(instruction_list& instructions, const Instruction& instruction);
  inline static std::size_t emit_instruction(Program& program, const Instruction& instruction);
}
*/

namespace ir {
  using operand_t = std::int64_t;
  struct Instruction {
      Instruction() = delete;
      Instruction(lexer::OpCode opcode, operand_t operand)
        : operand(operand), opcode(opcode) {}

      operand_t operand;
      lexer::OpCode opcode;
  };
  using instruction_list_t = std::vector<Instruction>;

  std::string instruction_to_str(const Instruction& instruction) {
    std::string str = std::string{lexer::opcode_to_str(instruction.opcode)};
    if (lexer::has_usable_operand(instruction.opcode)) {
      return str + "(" + std::to_string(instruction.operand) + ")";
    }
    return str;
  }

  inline std::size_t emit_instruction(instruction_list_t& instructions, const lexer::OpCode opcode, operand_t operand = 0) {
    instructions.emplace_back(opcode, operand);
    return instructions.size() - 1;  // instruction index
  }

  inline std::size_t emit_instruction(instruction_list_t& instructions, const Instruction& instruction) {
    return emit_instruction(instructions, instruction.opcode, instruction.operand);
  }

  std::size_t try_emit_folded_instruction(instruction_list_t& instructions, Instruction& instruction) {
      auto opcode = instruction.opcode;
      auto operand = instruction.operand;
      if (!instructions.empty() && is_same_foldable(opcode, instructions.back().opcode)) {
        instructions.back().operand += operand;
        if (instructions.back().operand == 0) {
          instructions.pop_back();
          return instructions.size(); // avoid potential underflow if all instructions are removed
        }
        return instructions.size() - 1;  // instruction index
      } else {
        return emit_instruction(instructions, instruction);
      }
  }

  /*
  std::size_t try_emit_folded_instruction(Program& program, Instruction& instruction) {
    return try_emit_folded_instruction(program.get_instructions(), instruction);
  }
  */
}