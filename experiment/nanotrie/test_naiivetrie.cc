// Simple Naiive Trie tree implementation
#include <map>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <vector>
#include <iostream>

template<typename KeyType, typename ValueType>
class NaiiveTrie
{
 public:
  struct NaiiveTrieNode
  {
    std::map<KeyType, NaiiveTrieNode> children;
    bool has_value{false};
    ValueType value{};
  };

  bool update(const KeyType *key, const size_t key_len, const ValueType &value) {
    if (!key) {
      return false;
    }

    if (key_len == 0) {
      return false;
    }

    NaiiveTrieNode *node = &_root;

    for (size_t i = 0; i < key_len; i++) {
      const KeyType token = key[i];
      if (!node->children.count(token)) {
        node->children[token] = NaiiveTrieNode();
      }

      node = &node->children[token];
    }
    node->has_value = true;
    node->value = value;

    return true;
  }

  bool exactMatch(const KeyType *key, const size_t key_len, ValueType &value_out) {
    if (!key) {
      return false;
    }

    if (key_len == 0) {
      return false;
    }

    NaiiveTrieNode *node = &_root;

    for (size_t i = 0; i < key_len; i++) {

      const KeyType token = key[i];
      if (!node->children.count(token)) {
        return false;
      }
      node = &node->children[token];

    }

    if (node->has_value) {
      value_out = node->value;
      return true;
    }

    return false;
  }

  // TODO: erase()

  size_t num_nodes_rec(const NaiiveTrieNode &node) {

    size_t n = node.children.size();
    for (const auto &child : node.children) {
      n += num_nodes_rec(child.second);
    }

    return n;
  }

  size_t num_nodes() {

    size_t n = num_nodes_rec(_root);

    return n;
  }


 private:
  NaiiveTrieNode _root;
};

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  NaiiveTrie<char, uint32_t> trie;

  trie.update("aa", 2, 0);
  trie.update("aaaa", 4, 1);
  trie.update("b", 1, 2);

  std::cout << "# of nodes: " << trie.num_nodes() << "\n";

  std::string key;
  uint32_t result{0};

  key = "a";
  if (trie.exactMatch(key.c_str(), key.size(), result)) {
    std::cout << "match: " << key << " : " << result << "\n";
  }

  key = "aa";
  if (trie.exactMatch(key.c_str(), key.size(), result)) {
    std::cout << "match: " << key << " : " << result << "\n";
  }

  key = "aaa";
  if (trie.exactMatch(key.c_str(), key.size(), result)) {
    std::cout << "match: " << key << " : " << result << "\n";
  }

  key = "aaaa";
  if (trie.exactMatch(key.c_str(), key.size(), result)) {
    std::cout << "match: " << key << " : " << result << "\n";
  }

  key = "b";
  if (trie.exactMatch(key.c_str(), key.size(), result)) {
    std::cout << "match: " << key << " : " << result << "\n";
  }

  key = "cc";
  if (trie.exactMatch(key.c_str(), key.size(), result)) {
    std::cout << "match: " << key << " : " << result << "\n";
  }

  return 0;
}

