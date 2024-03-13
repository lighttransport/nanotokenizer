#include <iostream>
#include <algorithm>

#include "nanotrie.hh"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wc++98-compat"
#endif

static void test_char() {
  // key = const char*
  nanotrie::Trie<const char, uint32_t> trie;

  std::vector<std::pair<std::string, uint32_t>> input_kvs;

  input_kvs.push_back({"he", 0});
  input_kvs.push_back({"hello", 1});
  input_kvs.push_back({"you", 2});
  input_kvs.push_back({"your", 3});
  input_kvs.push_back({"word", 4});
  input_kvs.push_back({"world", 5});

  // Key must be sorted(lexicographically).
  std::sort(input_kvs.begin(), input_kvs.end(), [](const std::pair<std::string, uint32_t> &a,
    const std::pair<std::string, uint32_t> &b) {
    return a.first < b.first;
  });

  std::vector<const char *> keys;
  std::vector<size_t> key_lens;
  std::vector<uint32_t> values;

  keys.assign(input_kvs.size(), nullptr);
  key_lens.assign(input_kvs.size(), 0);
  values.assign(input_kvs.size(), 0);

  for (size_t i = 0; i < input_kvs.size(); i++) {
    keys[i] = input_kvs[i].first.c_str();
    key_lens[i] = input_kvs[i].first.size();
    values[i] = input_kvs[i].second;
  }

  std::string err;
  if (!trie.build(keys.size(), keys.data(), key_lens.data(), values.data(),
                  &err)) {
    std::cerr << "(test_char) Failed to Build trie: " << err << "\n";
    exit(-1);
  }
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  test_char();

  return 0;
}
