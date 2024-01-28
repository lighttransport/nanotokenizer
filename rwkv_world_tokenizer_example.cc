#include <iostream>
#include <fstream>

#define MINIJSON_IMPLEMENTATION
#include "minijson.h"

#include "hat-trie/include/tsl/htrie_map.h"

//
#include "rwkv_world_tokenizer.hh"

int main(int argc, char **argv) {
  
  std::string vocab_json_filename = "rwkv_vocab_v20230424.json";

  if (argc > 1) {
    vocab_json_filename = argv[1];
  }

  std::ifstream ifs(vocab_json_filename);
  if (!ifs) {
    std::cerr << "file not found or open file failed: " << vocab_json_filename << "\n";
    return -1;
  }

  std::stringstream buf;
  buf << ifs.rdbuf();
  std::string s = buf.str();

  if (s.size() < 2) {
    std::cerr << "Invalid JSON file or file is invalid(maybe directory or special device?): " << vocab_json_filename << "\n";
    return -1;
  }

  const char *startp = s.c_str();
  const char *p = s.c_str();

  minijson::value v;
  minijson::error e = minijson::parse(p, v);
  if (e != minijson::no_error) {
    std::cerr << minijson::errstr(e) << " at byte offset (" << int(p - startp) << "):" << p << std::endl;
    return -1;
  }

  std::cout << "Read vocab OK: " << vocab_json_filename << "\n";

  std::map<std::string, int> str_to_id_map;

  int max_id = 0;
  // key = string, value = id
  if (const auto *po = v.as<minijson::object>()) {
    // id 0(<endoftext>) is not included in JSON. so add +1
    std::cout << "nvocab = " << po->size() + 1 << "\n";
    for (size_t i = 0; i < po->size(); i++) {
      std::string key = po->keys()[i];

      minijson::value num_v;
      if (!po->at(key, &num_v)) {
        std::cerr << "Invalid JSON. value for `" << key << "` not found: " << vocab_json_filename << "\n";
        return -1;
      }

      if (const auto *pv = num_v.as<minijson::number>()) {
        int id = int(*pv); 
        if ((id < 0) || (id > 65535)) {
          std::cerr << "Invalid id value for `" << key << "`. must be in range[0, 65536) but got " << id << "\n";
          return -1; 
        }
        str_to_id_map[key] = id;
        max_id = (std::max)(id, max_id);
      } else {
        std::cerr << "Invalid JSON. value is not number type for `" << key << "` : " << vocab_json_filename << "\n";
        return -1;
      }
    }
    std::cout << "max id value = " << max_id << "\n";

  } else {
    std::cerr << "Invalid JSON. Root element must be object: " << vocab_json_filename << "\n";
    return -1;    
  }

  // We can use uint16_t as value type.
  tsl::htrie_map<char, int> trie_map;

  for (const auto &it : str_to_id_map) {
    trie_map[it.first] = it.second;
  }


  // encode UTF-8 string
  std::string input_str = u8"吾輩は猫である。";

  while (!input_str.empty()) {

    auto longest_prefix = trie_map.longest_prefix(input_str);
    if (longest_prefix != trie_map.end()) {
      std::cout << "{" << longest_prefix.key() << ", " << *longest_prefix << "}\n";
      input_str.erase(0, longest_prefix.key().size());
    } else {
      // TODO: fallback to UTF-8
    }
  }

  return EXIT_SUCCESS;
}
