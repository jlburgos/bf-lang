#pragma once

#include <cstdint>

#include <unordered_set>

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