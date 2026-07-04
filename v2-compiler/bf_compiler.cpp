// C libs
#include <cmath>
#include <cstdint>

// Containers
#include <array>
#include <string>
#include <string_view>
#include <ranges>
#include <vector>
#include <unordered_set>

// Algorithm
#include <algorithm>

// I/O
#include <iostream>
#include <format>
#include <fstream>
#include <limits>

#include <filesystem>
namespace fs = std::filesystem;

// TODO :: Use the new C++ std tooling for printing file/line information in debug output!
#ifdef DEBUG
#define DBG(expr)                                                 \
    do {                                                          \
        std::cout << '\n' << __FILE__ << ':' << __LINE__ << ": "; \
        expr;                                                     \
    } while (0)
#else
#define DBG(expr) do { } while (0)
#endif

namespace lexer {
  enum struct Tokens : char {
    TOKEN_MOVE_LEFT         = '<',
    TOKEN_MOVE_RIGHT        = '>',
    TOKEN_INCREMENT         = '+',
    TOKEN_DECREMENT         = '-',
    TOKEN_OUTPUT            = '.',
    TOKEN_INPUT             = ',',
    TOKEN_JUMP_IF_ZERO      = '[',
    TOKEN_JUMP_IF_NOT_ZERO  = ']'
  };

  static bool is_valid_token(const char ch) {
    switch (static_cast<Tokens>(ch)) {
      case Tokens::TOKEN_MOVE_LEFT:
      case Tokens::TOKEN_MOVE_RIGHT:
      case Tokens::TOKEN_INCREMENT:
      case Tokens::TOKEN_DECREMENT:
      case Tokens::TOKEN_OUTPUT:
      case Tokens::TOKEN_INPUT:
      case Tokens::TOKEN_JUMP_IF_ZERO:
      case Tokens::TOKEN_JUMP_IF_NOT_ZERO:
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
    // IDEA :: Combine into a single COND_JUMP instruction?
    JUMP_IF_ZERO,     // '[',
    JUMP_IF_NOT_ZERO, // ']',

    // Complex (IR) Instruction Set
    //CLEAR_CELL // For collapsing loops such as "[-]"
    //MOVE_VALUE // For something like "[->+<]" which transfers a value between cells
  };

  static constexpr std::string_view opcode_to_str(const OpCode opcode) {
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
        throw std::logic_error("Unknown opcode detected!");
    }
  }

  [[maybe_unused]]
  inline static bool is_move(const OpCode opcode) {
    return opcode == OpCode::MOVE;
  }
  [[maybe_unused]]
  inline static bool is_arith(const OpCode opcode) {
    return opcode == OpCode::ADD;
  }
  [[maybe_unused]]
  inline static bool is_stdio(const OpCode opcode) {
    return (opcode == OpCode::OUTPUT) || (opcode == OpCode::INPUT);
  }
  [[maybe_unused]]
  inline static bool is_jump(const OpCode opcode) {
    return (opcode == OpCode::JUMP_IF_ZERO) || (opcode == OpCode::JUMP_IF_NOT_ZERO);
  }
  [[maybe_unused]]
  inline static bool is_foldable(const OpCode opcode) {
    return is_arith(opcode) || is_move(opcode);
  }
  [[maybe_unused]]
  inline static bool has_usable_operand(const OpCode opcode) {
    constexpr std::array<bool(*)(OpCode), 3> funcs = {is_move, is_arith, is_jump};
    return std::any_of(funcs.cbegin(), funcs.cend(), [opcode](auto func) { return func(opcode); });
  }
  [[maybe_unused]]
  inline static bool is_same_foldable(const OpCode opcode1, const OpCode opcode2) {
    return is_foldable(opcode1) && (opcode1 == opcode2);
  }
}

namespace ir {
  using operand_t = std::int64_t;
  struct Instruction {
      Instruction() = delete;
      Instruction(lexer::OpCode opcode, operand_t operand)
        : operand(operand), opcode(opcode) {}

      operand_t operand;
      lexer::OpCode opcode;
  };
  using instruction_list = std::vector<Instruction>;

  static std::string instruction_to_str(const Instruction& instruction) {
    std::string str = std::string{lexer::opcode_to_str(instruction.opcode)};
    if (lexer::has_usable_operand(instruction.opcode)) {
      return str + "(" + std::to_string(instruction.operand) + ")";
    }
    return str;
  }

  inline static std::size_t emit_instruction(instruction_list& instructions, const lexer::OpCode opcode, operand_t operand = 0) {
    instructions.emplace_back(opcode, operand);
    return instructions.size() - 1;  // instruction index
  }

  inline static std::size_t emit_instruction(instruction_list& instructions, const Instruction& instruction) {
    return emit_instruction(instructions, instruction.opcode, instruction.operand);
  }

  [[maybe_unused]]
  static std::size_t try_emit_folded_instruction(instruction_list& instructions, Instruction& instruction) {
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
}

// TODO :: Organize FFs in a better way later!
enum struct FeatureFlag : std::uint8_t {
  COMPRESS_INSTRUCTIONS
  // TODO :: Add more FFs!
};

class Config {
 public:
  bool is_enabled(const FeatureFlag feature_flag) const noexcept {
    return feature_flags.contains(feature_flag);
  }
  void set(const FeatureFlag feature_flag) noexcept {
    feature_flags.emplace(feature_flag);
  }
  void unset(const FeatureFlag feature_flag) noexcept {
    feature_flags.erase(feature_flag);
  }

 private:
  std::unordered_set<FeatureFlag> feature_flags;
};

using instruction_list = std::vector<ir::Instruction>;
class Program {
public:
  Program() = delete;
  ~Program() = default;
  explicit Program(std::size_t size) { instructions.reserve(size); }
  explicit Program(Program& program)
      : instructions(program.get_instructions()) {}
  explicit Program(Program&& program)
      : instructions(std::move(program.get_instructions())) {}

  inline ir::Instruction get_instruction(const std::size_t i) noexcept { return instructions[i]; }
  inline const ir::Instruction& get_instruction(const std::size_t i) const noexcept { return instructions[i]; }
  inline instruction_list& get_instructions() noexcept { return instructions; }
  inline const instruction_list& get_instructions() const noexcept { return instructions; }
  inline std::size_t size() const noexcept { return instructions.size(); }

  void update_instructions(instruction_list&& instructions) noexcept {
    this->instructions = std::move(instructions);
    this->instructions.shrink_to_fit();
  }

  void print_intermediate_representation() const {
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
  ir::Instruction& push(const ir::Instruction& instruction) {
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
    return instructions.at(index);
  }

  const ir::Instruction& operator[](const std::size_t index) const {
    return instructions.at(index);
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
        case lexer::Tokens::TOKEN_MOVE_RIGHT:
          emit_instruction(instructions, lexer::OpCode::MOVE, 1);
          break;
        case lexer::Tokens::TOKEN_MOVE_LEFT:
          emit_instruction(instructions, lexer::OpCode::MOVE, -1);
          break;
        case lexer::Tokens::TOKEN_INCREMENT:
          emit_instruction(instructions, lexer::OpCode::ADD, 1);
          break;
        case lexer::Tokens::TOKEN_DECREMENT:
          emit_instruction(instructions, lexer::OpCode::ADD, -1);
          break;
        case lexer::Tokens::TOKEN_OUTPUT:
          emit_instruction(instructions, lexer::OpCode::OUTPUT);
          break;
        case lexer::Tokens::TOKEN_INPUT:
          emit_instruction(instructions, lexer::OpCode::INPUT);
          break;
        case lexer::Tokens::TOKEN_JUMP_IF_ZERO:
          {
            // set default jump index to zero, will update later
            const std::size_t open_loop_index = emit_instruction(instructions, lexer::OpCode::JUMP_IF_ZERO);
            // store index for matching ']'
            loop_stack.push_back(open_loop_index);
          }
          break;
        case lexer::Tokens::TOKEN_JUMP_IF_NOT_ZERO:
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

private:
  instruction_list instructions;
};

namespace optimization {

  std::size_t compress_instructions(Program& program) {
    auto& uncompressed_instructions = program.get_instructions();
    std::vector<std::size_t> loop_stack{};

    instruction_list compressed_instructions{};
    compressed_instructions.reserve(uncompressed_instructions.size());

    // TODO :: BUG :: Compression does not properly account for multiple '[' and ']' operations with compression in-between!
    for (std::size_t i = 0; i < uncompressed_instructions.size(); ++i) {
      auto& instruction = uncompressed_instructions[i];
      switch(instruction.opcode) {
        case lexer::OpCode::JUMP_IF_ZERO:
          {
            // back-patch later to correct the operand value since further compressions will invalidate the existing value
            const std::size_t open_bracket = emit_instruction(compressed_instructions, instruction);
            loop_stack.push_back(open_bracket);
          }
          break;
        case lexer::OpCode::JUMP_IF_NOT_ZERO:
          {
            const std::size_t open_bracket = loop_stack.back();
            loop_stack.pop_back();
            const std::size_t closed_bracket = emit_instruction(compressed_instructions, instruction.opcode, open_bracket);
            compressed_instructions[open_bracket].operand = closed_bracket;
            break;
          }
        default:
          try_emit_folded_instruction(compressed_instructions, instruction);
      }
    }

    const std::size_t diff = program.size() - compressed_instructions.size();
    program.update_instructions(std::move(compressed_instructions));
    return diff;
  }

  void optimize(Program& program, const Config& config) {
    if (config.is_enabled(FeatureFlag::COMPRESS_INSTRUCTIONS)) {
      optimization::compress_instructions(program);
    }
  }
}

namespace io {
  std::string filter_program(const std::string_view raw_program) {
    return raw_program
      | std::ranges::views::filter(lexer::is_valid_token)
      | std::ranges::to<std::string>();
  }

  std::string read_program_file(const std::string_view file_path) {
    const fs::path path{file_path};
    if (!fs::exists(path)) {
      throw std::runtime_error(std::format("File {} does not exist!", file_path));
    }
    if (!fs::is_regular_file(path)) {
      throw std::runtime_error(std::format("Cannot open irregular file {}!", file_path));
    }

    std::ifstream ifs(path, std::ifstream::binary);
    if (!ifs.is_open()) {
      throw std::runtime_error(std::format("Failed to open file {}!", file_path));
    }

    std::string buffer(fs::file_size(path), '\0');
    if (ifs.read(buffer.data(), buffer.size())) {
      return filter_program(buffer);
    }
    throw std::runtime_error(std::format("Failed to read any data from file {}!", file_path));
  }
}

static void run(const Program& program) {
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

  while (instruction_pointer < program.get_instructions().size()) {
    const ir::Instruction& instruction = program.get_instruction(instruction_pointer);
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

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Need to provide file path!" << std::endl;
    return 1;
  }
  try {
    const std::string raw_program = io::read_program_file(argv[1]);
    Program program{raw_program.size()};
    program.build_intermediate_representation(raw_program);
    DBG(std::cout << "Prior to optimization:" << std::endl; program.print_intermediate_representation());

    // TODO :: Accept FFs as input later. For now, we manually enable them all for testing purposes!
    Config config;
    config.set(FeatureFlag::COMPRESS_INSTRUCTIONS);
    optimization::optimize(program, config);
    DBG(std::cout << "After optimization:" << std::endl; program.print_intermediate_representation());

    run(program);
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::cerr << "Failed with error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
