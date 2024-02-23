#include <fstream>
#include <iostream>

#define MINIJSON_IMPLEMENTATION
#include "minijson.h"

//
#include "rwkv_world_tokenizer_trie.hh"
#include "rwkv_world_tokenizer_hat.hh"
#include "rwkv_world_tokenizer_cedar.hh"

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

  // naiive trie tree
  {
    nanotokenizer::TrieTokenizer tokenizer;

    std::string err;
    if (!tokenizer.load_vocab(str_to_id_map, err)) {
      std::cerr << "Load vocab failed: " << vocab_json_filename << " err = " << err
                << "\n";
      return -1;
    }

    // encode UTF-8 string
    std::string input_str = u8"吾輩は猫である。🤩";
    // HACK
    size_t nrepeat = 2;

    for (size_t i = 0; i < nrepeat; i++) {
      input_str += "名前はまだない。にゃん。";
    }

    std::cout << "input: " << input_str << "\n";

    std::vector<int> output_ids;

    if (!tokenizer.encode(input_str, output_ids)) {
      std::cerr << "encode failed.\n";
      return -1;
    }

    std::cout << "ids = [";
    for (size_t i = 0; i < output_ids.size(); i++) {
      if (i > 0) {
        std::cout << ", ";
      }
      std::cout << output_ids[i];
    }
    std::cout << "]\n";

    std::string output_str;
    if (!tokenizer.decode(output_ids, output_str)) {
      std::cerr << "decode failed.\n";
      return -1;
    }
    std::cout << "decoded: " << output_str << "\n";
  }
  
  // hat-trie
  {
    nanotokenizer::HatTrieTokenizer tokenizer;

    std::string err;
    if (!tokenizer.load_vocab(str_to_id_map, err)) {
      std::cerr << "Load vocab failed: " << vocab_json_filename << " err = " << err
                << "\n";
      return -1;
    }

    // encode UTF-8 string
    std::string input_str = u8"吾輩は猫である。🤩";
    // HACK
    size_t nrepeat = 2;

    for (size_t i = 0; i < nrepeat; i++) {
      input_str += "名前はまだない。にゃん。";
    }

    std::cout << "input: " << input_str << "\n";

    std::vector<int> output_ids;

    if (!tokenizer.encode(input_str, output_ids)) {
      std::cerr << "encode failed.\n";
      return -1;
    }

    std::cout << "ids = [";
    for (size_t i = 0; i < output_ids.size(); i++) {
      if (i > 0) {
        std::cout << ", ";
      }
      std::cout << output_ids[i];
    }
    std::cout << "]\n";

    std::string output_str;
    if (!tokenizer.decode(output_ids, output_str)) {
      std::cerr << "decode failed.\n";
      return -1;
    }
    std::cout << "decoded: " << output_str << "\n";
  }


  // cedar
  {
    bool use_codepoint{false};
    if (argc > 2) {
      use_codepoint = bool(argv[2]);
    }

    nanotokenizer::CedarTrieTokenizer tokenizer(use_codepoint);

    std::string err;
    if (!tokenizer.load_vocab(str_to_id_map, err)) {
      std::cerr << "Load vocab failed: " << vocab_json_filename << " err = " << err
                << "\n";
      return -1;
    }

    // encode UTF-8 string
    std::string input_str = u8"吾輩は猫である。🤩";
    // HACK
    size_t nrepeat = 2;

    for (size_t i = 0; i < nrepeat; i++) {
      input_str += "名前はまだない。にゃん。";
    }

    std::vector<int> input_ids;
    if (!tokenizer.encode(input_str, input_ids)) {
      std::cerr << "Failed to encode\n";
      return -1;
    }

    std::cout << "[";
    for (size_t i = 0; i < input_ids.size(); i++) {
      if (i > 0) {
        std::cout << ", ";
      }
      std::cout << tokenizer.str_from_id(input_ids[i]) << " : " << input_ids[i];
    }
    std::cout << "]\n";

    std::string output_str;
    if (!tokenizer.decode(input_ids, output_str)) {
      std::cerr << "decode failed.\n";
      return -1;
    }
    std::cout << "decoded: " << output_str << "\n";
  }

  return EXIT_SUCCESS;
}
