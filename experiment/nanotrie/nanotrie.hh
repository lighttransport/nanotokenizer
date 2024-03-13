// SPDX-License-Identifier: MIT
// Copyright 2024-Present, Light Transport Entertainment Inc.
#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat"
#endif

// Simple array repsentation of Trie tree.
// TODO:
// - [ ] Support std::string_view(Use alternative implementation of string_view
// for < C++17)

#define PUSH_ERROR_AND_RETURN(s)                         \
  do {                                                   \
    std::ostringstream ss_e;                             \
    ss_e << "[nanotrie error]"                           \
         << ":" << __func__ << "():" << __LINE__ << " "; \
    ss_e << s << "\n";                                   \
    PushError(ss_e.str());                               \
    return false;                                        \
  } while (0)

#define PushError(msg) \
  {                    \
    if (err) {         \
      (*err) += msg;   \
    }                  \
  }

namespace nanotrie {

// Usualy KeyType = const char(const char *) or uint(UTF-8 codepoint)
template <typename KeyType, typename ValueType>
class Trie {
 public:
  static_assert(
      sizeof(ValueType) >= sizeof(int32_t),
      "sizeof(ValueType) must be equal to or larger than sizeof(int)");

  struct node {
    size_t depth{0};
    size_t left{0};
    size_t right{0};

    std::vector<node> children;
  };

  struct inode {
    ValueType offset{0};  // offset or value
    int parent{-1};       // parent node id(up to 2GB). -1 = root
  };

  static_assert(sizeof(inode) == (sizeof(ValueType) + sizeof(int32_t)),
                "inode struct must be tightly packed.");

  ///
  /// Get RAW bytes of Trie data.
  ///
  const uint8_t *data() const { return _data.data(); }

  ///
  /// # of bytes for Trie data.
  ///
  size_t size() const { return _data.size(); }

  size_t clear() { _data.clear(); }

  ///
  /// Build Trie tree.
  ///
  /// Key must be sorted lexcographically a priori.
  ///
  /// @param[in] num_keys The number of keys
  /// @param[in] keys Pointer array of keys(len = num_keys)
  /// @param[in] key_lens Array of key lengths(len = num_keys)
  /// @param[in] values Array of values(values[i] corresponds to keys[i])
  /// @param[out] err Error message when error.
  ///
  /// NOTE: empty key is not allowed.
  ///
  /// @return true Upon success.
  bool build(const size_t num_keys, const KeyType *keys[],
             const size_t *key_lens, const ValueType *values,
             std::string *err = nullptr) {
    (void)num_keys;
    (void)keys;
    (void)key_lens;
    (void)values;

    PUSH_ERROR_AND_RETURN("TODO");
  }

  ///
  /// Deserialize Trie data from bytes.
  /// Do validation of byte data
  ///
  bool deserialize(const uint8_t *data, const size_t nbytes);

  ///
  /// Do exact match search.
  ///
  ValueType exactMatchSearch(const KeyType *key, size_t key_len,
                             size_t node_pos = 0);

#if 0  // TODO
  ///
  /// Do common prefix search.
  ///
  size_t commonPrefixSearch(const KeyType *key, size_t key_len, ValueType *results, size_t num_results, size_t node_pos = 0);

  ///
  /// Do traverseh.
  ///
  ValueType traverse(const KeyType *key, size_t key_len, size_t &node_pos, size_t &key_pos);
#endif

 private:
  std::vector<uint8_t> _data;
  size_t _input_size{0};

  size_t *_key_lens{nullptr};
  KeyType *_keys{nullptr};
  size_t *_values{nullptr};

  // right_index is inclusive
  bool build_tree_rec_impl(const size_t depth, const node &parent,
    size_t left_index, size_t right_index, std::string *err) {

    if (left_index >= _input_size) {
      PUSH_ERROR_AND_RETURN("index out-of-range.");
    }

    if (right_index >= _input_size) {
      PUSH_ERROR_AND_RETURN("index out-of-range.");
    }

    if (right_index <= left_index) {
      PUSH_ERROR_AND_RETURN("internal error.");
    }

    size_t curr_depth = _key_lens[left_index];
    if (curr_depth != depth) {
      PUSH_ERROR_AND_RETURN("keys are not lexicographically sorted.");
    }

    size_t child_depth = depth + 1;
    size_t child_left = left_index;
    size_t child_right = right_index;

    // find child_left
    for (size_t i = left_index; i <= right_index; i++) {

      if (_key_lens[i] < depth) {
        PUSH_ERROR_AND_RETURN("keys are not lexicographically sorted.");
        continue;
      }

      // skip until length change.
      if (_key_lens[i] == depth) {
        continue;
      }

      child_left = i;
      child_depth = _key_lens[i];
      break;
    }

    // find sibling_right
    for (size_t i = child_left; i <= right_index; i++) {

      if (_key_lens[i] < child_depth) {
        PUSH_ERROR_AND_RETURN("keys are not lexicographically sorted.");
        continue;
      }

      // skip until length change.
      if (_key_lens[i] == child_depth) {
        continue;
      }

      child_right = i;
      break;
    }

    if (child_right - child_left > 0) {
      // recurse
      node child; // TODO
      if (!build_tree_rec_impl(child_depth, child, child_left, child_right, err)) {
        return false;
      }
    }
    
    return true;
  }

  bool validate() {
    // TODO
    return false;
  }
};

#ifdef __clang__
#pragma clang diagnostic pop
#endif

}  // namespace nanotrie

