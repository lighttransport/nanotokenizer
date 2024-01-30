#include <fstream>
#include <iostream>

#define MINIJSON_IMPLEMENTATION
#include "hat-trie/include/tsl/htrie_map.h"
#include "minijson.h"

//
#include "rwkv_world_tokenizer.hh"

inline std::string extract_utf8_char(const std::string &str, uint32_t start_i,
                                     int &len) {
  len = 0;

  if ((start_i + 1) > str.size()) {
    len = 0;
    return std::string();
  }

  unsigned char c = static_cast<unsigned char>(str[start_i]);

  if (c <= 127) {
    // ascii
    len = 1;
    return str.substr(start_i, 1);
  } else if ((c & 0xE0) == 0xC0) {
    if ((start_i + 2) > str.size()) {
      len = 0;
      return std::string();
    }
    len = 2;
    return str.substr(start_i, 2);
  } else if ((c & 0xF0) == 0xE0) {
    if ((start_i + 3) > str.size()) {
      len = 0;
      return std::string();
    }
    len = 3;
    return str.substr(start_i, 3);
  } else if ((c & 0xF8) == 0xF0) {
    if ((start_i + 4) > str.size()) {
      len = 0;
      return std::string();
    }
    len = 4;
    return str.substr(start_i, 4);
  } else {
    // invalid utf8
    len = 0;
    return std::string();
  }
}

int main(int argc, char **argv) {
  std::string vocab_json_filename = "rwkv_vocab_v20230424.json";

  if (argc > 1) {
    vocab_json_filename = argv[1];
  }

  std::ifstream ifs(vocab_json_filename);
  if (!ifs) {
    std::cerr << "file not found or open file failed: " << vocab_json_filename
              << "\n";
    return -1;
  }

  std::stringstream buf;
  buf << ifs.rdbuf();
  std::string s = buf.str();

  if (s.size() < 2) {
    std::cerr << "Invalid JSON file or file is invalid(maybe directory or "
                 "special device?): "
              << vocab_json_filename << "\n";
    return -1;
  }

  const char *startp = s.c_str();
  const char *p = s.c_str();

  minijson::value v;
  minijson::error e = minijson::parse(p, v);
  if (e != minijson::no_error) {
    std::cerr << minijson::errstr(e) << " at byte offset (" << int(p - startp)
              << "):" << p << std::endl;
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
        std::cerr << "Invalid JSON. value for `" << key
                  << "` not found: " << vocab_json_filename << "\n";
        return -1;
      }

      if (const auto *pv = num_v.as<minijson::number>()) {
        int id = int(*pv);
        if ((id < 0) || (id > 65535)) {
          std::cerr << "Invalid id value for `" << key
                    << "`. must be in range[0, 65536) but got " << id << "\n";
          return -1;
        }
        str_to_id_map[key] = id;
        max_id = (std::max)(id, max_id);
      } else {
        std::cerr << "Invalid JSON. value is not number type for `" << key
                  << "` : " << vocab_json_filename << "\n";
        return -1;
      }
    }
    std::cout << "max id value = " << max_id << "\n";

  } else {
    std::cerr << "Invalid JSON. Root element must be object: "
              << vocab_json_filename << "\n";
    return -1;
  }

  int fallback_token_id = max_id + 1;
  if (fallback_token_id > 65535) {
    std::cerr << "Too many tokens in JSON\n";
    return -1;
  }

  // We can use uint16_t as value type.
  tsl::htrie_map<char, int> trie_map;

  for (const auto &it : str_to_id_map) {
    trie_map[it.first] = it.second;
  }

  // encode UTF-8 string
  std::string input_str = u8"吾輩は猫である。🤩";

  while (!input_str.empty()) {
    auto longest_prefix = trie_map.longest_prefix(input_str);
    // 3319 = empty string.
    if ((longest_prefix != trie_map.end()) && !longest_prefix.key().empty()) {
      std::cout << "{" << longest_prefix.key() << ", " << *longest_prefix
                << "}\n";
      input_str.erase(0, longest_prefix.key().size());
    } else {
      int u8len{0};
      std::string u8char = extract_utf8_char(input_str, 0, u8len);
      if (u8len == 0) {
        std::cerr << "invalid utf8 char found.\n";
        exit(-1);
      }

      std::cout << "{utf8 byte fallback : " << fallback_token_id << ", ";
      for (size_t i = 0; i < u8char.size(); i++) {
        if (i > 0) {
          std::cout << ", ";
        }
        std::cout << int(u8char[i]);
      }
      std::cout << "}\n";
      input_str.erase(0, u8len);

      // TODO: fallback to UTF-8
    }
  }

  return EXIT_SUCCESS;
}
