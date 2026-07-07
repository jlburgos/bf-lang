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

// Algorithms / Utilities
#include <algorithm>

// IO
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
    // IDEA :: Combine into a single COND_JUMP instruction?
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
        throw std::logic_error("Unknown opcode detected!");
    }
  }

  [[maybe_unused]]
  inline bool is_move(const OpCode opcode) {
    return opcode == OpCode::MOVE;
  }
  [[maybe_unused]]
  inline bool is_arith(const OpCode opcode) {
    return opcode == OpCode::ADD;
  }
  [[maybe_unused]]
  inline bool is_stdio(const OpCode opcode) {
    return (opcode == OpCode::OUTPUT) || (opcode == OpCode::INPUT);
  }
  [[maybe_unused]]
  inline bool is_jump(const OpCode opcode) {
    return (opcode == OpCode::JUMP_IF_ZERO) || (opcode == OpCode::JUMP_IF_NOT_ZERO);
  }
  [[maybe_unused]]
  inline bool is_foldable(const OpCode opcode) {
    return is_arith(opcode) || is_move(opcode);
  }
  [[maybe_unused]]
  inline bool has_usable_operand(const OpCode opcode) {
    return is_move(opcode) || is_arith(opcode) || is_jump(opcode);
  }
  [[maybe_unused]]
  inline bool is_same_foldable(const OpCode opcode1, const OpCode opcode2) {
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
  using instruction_list_t = std::vector<Instruction>;

  std::string instruction_to_str(const Instruction& instruction) {
    std::string str = std::string{lexer::opcode_to_str(instruction.opcode)};
    if (lexer::has_usable_operand(instruction.opcode)) {
      return str + "(" + std::to_string(instruction.operand) + ")";
    }
    return str;
  }

  inline std::size_t emit_instruction(ir::instruction_list_t& instructions, const lexer::OpCode opcode, operand_t operand = 0) {
    instructions.emplace_back(opcode, operand);
    return instructions.size() - 1;  // instruction index
  }

  inline std::size_t emit_instruction(ir::instruction_list_t& instructions, const Instruction& instruction) {
    return emit_instruction(instructions, instruction.opcode, instruction.operand);
  }

  std::size_t try_emit_folded_instruction(ir::instruction_list_t& instructions, Instruction& instruction) {
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

struct Config {
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

  inline ir::instruction_list_t& get_instructions() noexcept { return instructions; }
  inline const ir::instruction_list_t& get_instructions() const noexcept { return instructions; }

  inline void update_instructions(ir::instruction_list_t&& instructions) noexcept {
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

    std::int64_t mismatched_brackets_counter = 0;

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
            // emit jump instruction with no target, resolve later
            mismatched_brackets_counter += 1;
            emit_instruction(instructions, lexer::OpCode::JUMP_IF_ZERO);
          }
          break;
        case lexer::Tokens::JUMP_IF_NOT_ZERO:
          {
            // emit jump instruction with no target, resolve later
            mismatched_brackets_counter -= 1;
            if (mismatched_brackets_counter < 0) {
              throw std::runtime_error(
                  "There are unmatched ']' instructions, which "
                  "produces an invalid program!");
            }
            emit_instruction(instructions, lexer::OpCode::JUMP_IF_NOT_ZERO);
          }
          break;
        default:
          throw std::runtime_error(std::format(
              "Invalid input '{}' was encountered! This should never happen!",
              ch));
      }
    }
    if (mismatched_brackets_counter != 0) {
      throw std::runtime_error(
          "There are unmatched '[' instructions, which produces an "
          "invalid program!");
    }
    resolve_jump_instructions(); // back patch jump instructions
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

  void resolve_jump_instructions() {
    std::vector<std::size_t> loop_stack{};
    for (std::size_t i = 0; i < size(); ++i) {
      std::cout << "idx: " << i << ", size: " << size() << std::endl;
      switch(get_instruction(i).opcode) {
        case lexer::OpCode::JUMP_IF_ZERO:
          // store open-bracket index for back-patching
          loop_stack.push_back(i);
          break;
        case lexer::OpCode::JUMP_IF_NOT_ZERO:
          // reset jump targets
          get_instruction(i).operand = loop_stack.back();
          get_instruction(loop_stack.back()).operand = i;
          loop_stack.pop_back();
          break;
        default:
          // Ignore
      }
    }
  }

private:
  ir::instruction_list_t instructions;
  Config config;
};

namespace optimization {

  std::size_t compress_instructions(Program& program) {
    std::vector<std::size_t> loop_stack{};
    Program compressed_program{};
    compressed_program.reserve_capacity(program.size());

    for (std::size_t i = 0; i < program.size(); ++i) {
      auto& instruction = program[i];
      try_emit_folded_instruction(compressed_program.get_instructions(), instruction);
    }

    // resolve potential jump instruction invalidations from compression
    compressed_program.resolve_jump_instructions();
    // remove excess capacity from initial reserve operation
    compressed_program.compress_capacity();
    const std::size_t diff = program.size() - compressed_program.size(); // compute difference in program instruction size
    program.update_instructions(std::move(compressed_program.get_instructions()));
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

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Need to provide file path!" << std::endl;
    return 1;
  }
  try {
    // TODO :: Accept FFs as input later. For now, we manually enable them all for testing purposes!
    Config config;
    config.set(FeatureFlag::COMPRESS_INSTRUCTIONS);

    Program program{config};
    program.build_intermediate_representation(io::read_program_file(argv[1]));
    DBG(std::cout << "Prior to optimization:" << std::endl; program.print_intermediate_representation());

    optimization::optimize(program, config);
    DBG(std::cout << "After optimization:" << std::endl; program.print_intermediate_representation());
    DBG(std::cout << "Size of program obj: " << sizeof(program) << std::endl;);
    program.run();
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::cerr << "Failed with error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
