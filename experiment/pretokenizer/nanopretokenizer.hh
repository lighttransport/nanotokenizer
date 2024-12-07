// SPDX-License-Identifier: MIT
#pragma once

#include <vector>
#include <string>

namespace nanotokenizer {

// Split unicode string with GPT2 rule.
std::vector<size_t> pretokenize_gpt2(const std::string & text, const std::vector<size_t> & offsets);

// Split unicode string with Qwen2 rule.
std::vector<size_t> pretokenize_qwen2(const std::string & text, const std::vector<size_t> & offsets);

// Split unicode string with llama3 rule.
std::vector<size_t> pretokenize_llama3(const std::string & text, const std::vector<size_t> & offsets);

} // namespace nanotokenizer


