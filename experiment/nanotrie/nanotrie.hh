// SPDX-License-Identifier: MIT
// Copyright 2024-Present, Light Transport Entertainment Inc.
#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>
#include <array>

#include "nanohashmap.hh"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat"
#endif

// Simple array repsentation of Trie tree.
//
// Up to 2GB items.
//
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

template<typename KeyType>
inline bool has_prefix(const KeyType *s, const size_t s_len, const KeyType *prefix, const size_t prefix_len){
  if (s_len < prefix_len) {
    return false;
  }

  // TODO: Use memcmp for faster compare?
  for (size_t i = 0; i < prefix_len; i++) {
    if (s[i] != prefix[i]) {
      return false;
    }
  }

  return true;
}

// Usualy KeyType = const char(const char *) or uint(UTF-8 codepoint)
template <typename KeyType, typename ValueType>
class Trie {
 public:
  // MSB 3 bit
  enum class NodeType {
    NODE_LEAF = 0x4 << 28, // 100
    NODE_ARRAY = 0x5 << 28, // 101: for small sized items. items are not sorted.
    NODE_SORTED_ARRAY = 0x6 << 28, // 110: for middle sized items. items are sorted. so use binary search to find an item.
    NODE_HASHMAP = 0x7 << 28  // 111: for large sized items. Use nanohashmap.
  };

  static_assert(
      sizeof(ValueType) >= sizeof(int32_t),
      "sizeof(ValueType) must be equal to or larger than sizeof(int)");

  struct node {
    size_t depth{0};
    size_t left{0};
    size_t right{0};

    std::vector<node> children;
  };

  static constexpr int32_t kLeafNode = -2;
  static constexpr int32_t kSingleTokenIntermediateNode = -1;

  struct inode {
    //ValueType offset{0}; // offset or value
    //
    uint32_t offset_to_hashes{0}; // array index to `hashes`

    // for negative values.
    //
    // +----+--------------+
    // | ty | payload      |
    // +----+--------------+
    //
    // MSB 3 bit  : NodeType
    // LSB 29 bit : Payload(e.g. # of array items)
    //
    // for 0 or positive values
    //
    // - index to sibling(neighbor) node

    int jump{-1};
  };

  std::vector<TokenHashMap<KeyType, ValueType, 64>> hashes;

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

    node root;
    std::vector<int32_t> jumps;
    jumps.assign(num_keys, kLeafNode);

    return build_tree_rec_impl(keys, key_lens, values, 0, root, 0, num_keys - 1, jumps, err);
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


  // NOTE: right_index is inclusive
  bool build_tree_rec_impl(
    const KeyType *keys[], const size_t *key_lens, const ValueType *values,
    const size_t depth, const node &parent,
    const uint32_t curr_index,
    const uint32_t left_index, const uint32_t right_index,
    uint32_t &next_index_out,
    std::vector<int32_t> &jumps, std::string *err) {

    //
    // Type of node is based on # of siblings and has child or not.
    //
    // - Terminal(leaf node):
    // - No siblings but has child: Create node with single token.
    // - Small # of siblings: Use simple array and do linear search.
    // - Other: Use tokenhashmap.
    //

    // right_idx is exclusive
    // return false when invalid left_idx/right_idx combination.
    auto get_next_subtree = [&keys, &key_lens](const uint32_t left_idx, const uint32_t right_idx, uint32_t &subtree_idx) -> bool {

      if (left_idx >= right_idx) {
        return false;
      }

      uint32_t i = left_idx + 1;
      for (; i < right_idx; i++) {
        if (!has_prefix(keys[i], key_lens[i], keys[right_idx], key_lens[right_idx])) {
          subtree_idx = i;
          return true;
        }
      }

      subtree_idx = i;
      return true;
    };

    if (right_index > left_index) {
      return false;
    }

    size_t n = right_index - left_index;

    uint32_t end_index = right_index + 1; // end is exclusive
    uint32_t parent_index = left_index;

    for (uint32_t next_idx = parent_index; parent_index < end_index; parent_index = next_idx) {

      uint32_t next_subtree_idx;
      if (!get_next_subtree(parent_index, end_index, next_subtree_idx)) {
        PUSH_ERROR_AND_RETURN("keys are not sorted.");
      }

      next_idx++;


      bool has_child{false};
      bool has_sibling{false};

      if (next_subtree_idx > right_index) {
        // Just in case
        PUSH_ERROR_AND_RETURN("Invalid next_subtree_idx.");
      }

      if (next_idx != next_subtree_idx) {

        if (next_idx > right_index) {
          // Just in case
          PUSH_ERROR_AND_RETURN("Invalid next_idx.");
        }


        // TODO: has_child
      }

      if (next_subtree_idx != end_idx) {

        // TODO: has_sibling

      }


      uint32_t prev_idx = curr_idx;

      if (has_child) {
        uint32_t next_idx_out;


      }

      if (has_sibling && has_child) {
        jumps[prev_idx] = int32_t(curr_idx - prev_idx);
      } else if (has_sibling) {
        jumps[prev_idx] = 0;
      } else if (has_child) {
        jumps[prev_idx] = -1;
      } else {
        jumps[prev_idx] = -2; // leaf node
      }

    }


    PUSH_ERROR_AND_RETURN("TODO");

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

