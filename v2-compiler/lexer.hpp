#pragma once

// Containers
#include <array>
#include <string_view>

// IO
#include <format>

// Utilities
#include <utility> // std::to_underling

namespace lexer {
  enum struct Tokens : char {
    MOVE_LEFT         = '<',
    MOVE_RIGHT        = '>',
    INCREMENT         = '+',
    DECREMENT         = '-',
    OUTPUT            = '.',
    INPUT             = ',',
    JUMP_IF_ZERO      = '[',
    JUMP_IF_NOT_ZERO  = ']'
  };

  bool is_valid_token(const char ch) {
    switch (static_cast<Tokens>(ch)) {
      case Tokens::MOVE_LEFT:
      case Tokens::MOVE_RIGHT:
      case Tokens::INCREMENT:
      case Tokens::DECREMENT:
      case Tokens::OUTPUT:
      case Tokens::INPUT:
      case Tokens::JUMP_IF_ZERO:
      case Tokens::JUMP_IF_NOT_ZERO:
        return true;
      default:
        return false;
    }
  }

  enum struct OpCode : std::uint8_t {
    // Standard Language Instruction Set
    MOVE,             // '<','>'
    ADD,              // '+','-'
    OUTPUT,           // '.'
    INPUT,            // ','
    JUMP_IF_ZERO,     // '[',
    JUMP_IF_NOT_ZERO, // ']',

    // Complex (IR) Instruction Set
    //CLEAR_CELL // For collapsing loops such as "[-]"
    //MOVE_VALUE // For something like "[->+<]" which transfers a value between cells
  };

  constexpr std::string_view opcode_to_str(const OpCode opcode) {
    switch (opcode) {
      case OpCode::MOVE:
        return "MOVE";
      case OpCode::ADD:
        return "ADD";
      case OpCode::OUTPUT:
        return "OUTPUT";
      case OpCode::INPUT:
        return "INPUT";
      case OpCode::JUMP_IF_ZERO:
        return "JUMP_IF_ZERO";
      case OpCode::JUMP_IF_NOT_ZERO:
        return "JUMP_IF_NOT_ZERO";
      default:
        throw std::runtime_error(std::format("Unknown opcode detected: {}",
                                             std::to_underlying(opcode)));
    }
  }

  inline bool is_move(const OpCode opcode) {
    return opcode == OpCode::MOVE;
  }

  inline bool is_arith(const OpCode opcode) {
    return opcode == OpCode::ADD;
  }

  inline bool is_stdio(const OpCode opcode) {
    return (opcode == OpCode::OUTPUT) || (opcode == OpCode::INPUT);
  }

  inline bool is_jump(const OpCode opcode) {
    return (opcode == OpCode::JUMP_IF_ZERO) || (opcode == OpCode::JUMP_IF_NOT_ZERO);
  }

  inline bool is_foldable(const OpCode opcode) {
    return is_arith(opcode) || is_move(opcode);
  }

  inline bool is_same_foldable(const OpCode opcode1, const OpCode opcode2) {
    return is_foldable(opcode1) && (opcode1 == opcode2);
  }

  inline bool has_usable_operand(const OpCode opcode) {
    return is_move(opcode) || is_arith(opcode) || is_jump(opcode);
  }
}