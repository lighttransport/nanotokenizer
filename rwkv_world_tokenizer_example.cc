#include <fstream>
#include <iostream>

#define MINIJSON_IMPLEMENTATION
#include "hat-trie/include/tsl/htrie_map.h"
#include "minijson.h"

//
#include "rwkv_world_tokenizer.hh"

#include "ccedar_core.h"

namespace ccedar {

  static inline int u8_len (const char *p) {
    static const uint8_t u8bytes[256] = { // must be static to tame compilers
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
      2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
      3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3, 4,4,4,4,4,4,4,4,5,5,5,5,6,6,6,6
    };
    return u8bytes[static_cast <uint8_t> (*p)];
  }

#if 0
  // examine UTF8 sequence p consist of only num / alpha / kana characters
  static inline int char_type (const char* p, const char* end, const ccedar::da <char, int>& chars) {
    int b (u8_len (p)), n (chars.exactMatchSearch <int> (p, b));
    if (n == -1) return 3;
    while ((p += b) != end)
      if (chars.exactMatchSearch <int> (p, b = u8_len (p)) != n) return 3;
    return n;
  }
#endif

  // convert UTF-8 char to code point
  static inline int unicode (const char* p, int& b) {
    const unsigned char *p_ = reinterpret_cast <const unsigned char*> (p);
    //const int p0 (p_[0]), p1 (p_[1]), p2 (p_[2]), p3 (p_[3]);
    int p0, p1, p2, p3;
    
    switch (b = u8_len (p)) {
      case 1: { p0 = p_[0]; return   p0 & 0x7f; }
      case 2: { p0 = p_[0]; p1 = p_[1]; return ((p0 & 0x1f) << 6)  |  (p1 & 0x3f); }
      case 3: { p0 = p_[0]; p1 = p_[1]; p2 = p_[2];return ((p0 & 0xf)  << 12) | ((p1 & 0x3f) << 6)  |  (p2 & 0x3f); }
      case 4: { p0 = p_[0]; p1 = p_[1]; p2 = p_[2]; p3 = p_[3]; return ((p0 & 0x7)  << 18) | ((p1 & 0x3f) << 12) | ((p2 & 0x3f) << 6)  | (p3 & 0x3f); }
      default: return 0;
    }
    return 0;
  }

//constexpr size_t MAX_KEY_BITS = 14;
constexpr size_t MAX_KEY_BITS = 16; // 65536

class da_ : public ccedar::da<int, int, MAX_KEY_BITS> {
 public:


  struct utf8_feeder {  // feed one UTF-8 character by one while mapping codes
    const char *p, *const end;
    utf8_feeder(const char *key_, const char *end_) : p(key_), end(end_) {}
    int read(int &b) const { return p == end ? 0 : unicode(p, b); }
    void advance(const int b) { p += b; }
  };
#if 0
  int longestPrefixSearchWithPOS(const char *key, const char *const end,
                                 int fi_prev, const uint16_t *const c2i,
                                 size_t from = 0) const {
    size_t from_ = 0;
    int n(0), i(0), b(0);
    for (utf8_feeder f(key, end); (i = c2i[f.read(b)]); f.advance(b)) {
      size_t pos = 0;
      const int n_ = traverse(&i, from, pos, pos + 1);
      if (n_ == CEDAR_NO_VALUE) continue;
      if (n_ == CEDAR_NO_PATH) break;
      from_ = from;
      n = n_;
    }
    // ad-hock matching at the moment; it prefers POS-ending patterns
    if (!fi_prev) return n;
    for (const node *const array_ = reinterpret_cast<const node *>(array());;
         from = array_[from].check) {  // hopefully, in the cache
      const int n_ = exactMatchSearch<int>(&fi_prev, 1, from);
      if (n_ != CEDAR_NO_VALUE) return n_;
      if (from == from_) return n;
    }
  }
#else
  // Returns ID of lognest match string in vocabs.
  int longestPrefixSearch(const char *key, const char *const end,
                          size_t from = 0) const {
    size_t from_ = 0;
    int n(0), i(0), b(0);
    for (utf8_feeder f(key, end); (i = f.read(b)); f.advance(b)) {
      size_t pos = 0;
      const int n_ = traverse(&i, from, pos, pos + 1);
      if (n_ == CEDAR_NO_VALUE) continue;
      if (n_ == CEDAR_NO_PATH) break;
      from_ = from;
      n = n_;
    }
    // ad-hock matching at the moment; it prefers POS-ending patterns
    //if (!fi_prev) return n;
    int fi_prev =0;
    for (const node *const array_ = reinterpret_cast<const node *>(array());;
         from = array_[from].check) {  // hopefully, in the cache
      const int n_ = exactMatchSearch<int>(&fi_prev, 1, from);
      if (n_ != CEDAR_NO_VALUE) return n_;
      if (from == from_) return n;
    }

    // shuold never reach here.
    return CEDAR_NO_VALUE;
  }
#endif
};



} // namespace ccedar

// Trie tokenizer based on ccedar
class CedarTrieTokenizer
{
 public:
  CedarTrieTokenizer() = default;
  ~CedarTrieTokenizer() {
   // free memory in cedar
    da.clear(/* reuse */false);
    if (da.array()) { // work around for _array is not free'ed in ccedar
      std::free(const_cast<void *>(da.array())); 
    }
  }

  bool load_vocab(const std::map<std::string, int> &str_to_id_map) {
    _str_to_id_map = str_to_id_map;

    int max_id{0};
    for (const auto &it : str_to_id_map) {
      // ignore empty key(zero-length char).
      if (it.first.empty()) {
        _empty_char_id = it.second;
      } else {
        _id_to_str_map[it.second] = it.first;
      }
      max_id = (std::max)(max_id, it.second);
    }

    _utf8_fallback_token_id = max_id + 1;
    if (_utf8_fallback_token_id > 65535) {
      return false;
    }
    _utf8_id_offset = 1; // ASCII character is +1'ed in RWKV world vocab

    for (const auto &it : str_to_id_map) {

      const char *str = it.first.c_str();
      const size_t slen = strlen(str);

      // cedar does not accept empty char(zero-length char).
      if (slen == 0) {
        continue;
      }

      // UTF-8 string to int(unicode) array
      std::vector<int> ikey;

      int charlen;
      for (size_t i = 0; i < slen; i += charlen) {
        int code = ccedar::unicode(it.first.c_str(), charlen);
        ikey.push_back(code);
      }

      da.update(ikey.data(), ikey.size()) = it.second;
    }

    return true;
  }

  
  bool encode(const std::string &s, std::vector<int> &output_ids) {
    std::vector<int> dst;

    if (s.empty()) {
      return true;
    }

    const char *s_ptr = s.c_str();
    const char *p = s.c_str();
    const char *e_ptr = s.c_str() + s.size();

    s_ptr += ccedar::u8_len(s_ptr);
    while (p < e_ptr) {
      int n = da.longestPrefixSearch(p, e_ptr, /* from */0);

      if ((n < 1) || !_id_to_str_map.count(n)) {
        // utf-8 fallback
        int u8len{0};
        std::string u8char = extract_utf8_char(s, p - s_ptr, u8len);
        if (u8len == 0) {
          std::cerr << "invalid utf8 char found.\n";
          return false;
        }

        dst.push_back(_utf8_fallback_token_id);

        for (size_t i = 0; i < u8char.size(); i++) {
          dst.push_back(int(uint8_t(u8char[i])) + _utf8_id_offset);

        }
        p += u8len;
      } else {

        dst.push_back(n);

        // TODO: precalculate str len
        const std::string &tok = _id_to_str_map.at(n);

        int charlen = ccedar::u8_len(tok.c_str());
        if (charlen < 1) {
          // this should not happen though. just in case.
          return false;
        }
        p += charlen;
      }
    }

    output_ids = dst;
    return true;
  }

  bool decode(const std::vector<int> &input_ids, std::string &output_str) {

    std::string dst;

    for (size_t i = 0; i < input_ids.size(); i++) {
      if (input_ids[i] == _utf8_fallback_token_id) {
        std::string u8char;
        if (!utf8_char_from_ids(input_ids.data(), i+1, input_ids.size(), u8char, _utf8_id_offset)) {
          std::cerr << "utf8 reconstruct failed.\n";
          return false;
        }

        i += u8char.size();

        dst += u8char;

        continue;
      }

      if (!_id_to_str_map.count(input_ids[i])) {
        std::cerr << "id not found: " << input_ids[i] << "\n";
        return false;
      }

      dst += _id_to_str_map[input_ids[i]];
    }

    output_str = dst;

    return true;
  }

  std::string str_from_id(int id) {
    if (_id_to_str_map.count(id)) {
      return _id_to_str_map.at(id);
    }
    if (id == _utf8_fallback_token_id) {
      return "[[ut8_fallback_id]]";
    }
    if (id > 0 && id < 257) { // ASCII or UTF-8 byte
      return "[[byte]]";
    }
    return std::string();
  }

 private:
  ccedar::da_ da;

  std::map<std::string, int> _str_to_id_map;
  std::map<std::vector<int>, int> _unicode_to_id_map;
  std::map<int, std::string> _id_to_str_map;

  int _utf8_fallback_token_id{-1};
  int _utf8_id_offset{1}; // ASCII character is +1'ed in RWKV world vocab
  int _empty_char_id{3319};

  inline uint32_t utf8_len(const uint8_t c) {
    if (c <= 127) {
      // ascii
      return 1;
    } else if ((c & 0xE0) == 0xC0) {
      return 2;
    } else if ((c & 0xF0) == 0xE0) {
      return 3;
    } else if ((c & 0xF8) == 0xF0) {
      return 4;
    }

    // invalid
    return 0;
  }

  // Reconstruct UTF-8 bytes from int sequence(UTF-8 encoded)
  inline bool utf8_char_from_ids(const int *addr, size_t loc, size_t n, std::string &str, int id_offset = 1) {
    if (loc >= n) {
      return false;
    }

    int start_c = addr[loc] - id_offset;
    if ((start_c < 0) || (start_c > 255)) {
      return false;
    }

    uint32_t len = utf8_len(uint8_t(start_c));

    if (len == 0) {
      return false;
    }

    if ((loc + len) > n) {
      return false;
    }

    str = "";
    std::vector<uint8_t> buf;
    for (size_t i = 0; i < len; i++) {
      int ic = addr[loc + i] - id_offset;
      if ((ic < 0) || (ic > 255)) {
        return false;
      }
      buf.push_back(uint8_t(ic));
    }

    str = std::string(reinterpret_cast<const char *>(buf.data()), buf.size());

    return true;
  }

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
};

// Up to 65534 vocabs
class TrieTokenizer
{
 public:

  bool load_vocab(const std::map<std::string, int> &str_to_id_map) {
    _str_to_id_map = str_to_id_map;

    int max_id{0};
    for (const auto &it : str_to_id_map) {
      _id_to_str_map[it.second] = it.first;
      max_id = (std::max)(max_id, it.second);
    }

    for (const auto &it : str_to_id_map) {
      _trie_map[it.first] = it.second;
    }

    _utf8_fallback_token_id = max_id + 1;
    if (_utf8_fallback_token_id > 65535) {
      return false;
    }
    _utf8_id_offset = 1; // ASCII character is +1'ed in RWKV world vocab

    return true;
  }

  bool encode(const std::string &_input_str, std::vector<int> &output_ids) {
    std::vector<int> dst;

    std::string buf = _input_str;

    while (!buf.empty()) {

      auto longest_prefix = _trie_map.longest_prefix(buf);
      // 3319 = empty string.
      if ((longest_prefix != _trie_map.end()) && !longest_prefix.key().empty()) {
        dst.push_back(*longest_prefix);

        buf.erase(0, longest_prefix.key().size());
      } else {
        int u8len{0};
        std::string u8char = extract_utf8_char(buf, 0, u8len);
        if (u8len == 0) {
          std::cerr << "invalid utf8 char found.\n";
          exit(-1);
        }

        dst.push_back(_utf8_fallback_token_id);

        for (size_t i = 0; i < u8char.size(); i++) {
          dst.push_back(int(uint8_t(u8char[i])) + _utf8_id_offset);
        }
        buf.erase(0, u8len);
      }

    }

    output_ids = dst;
    return true;
  }

  bool decode(const std::vector<int> input_ids, std::string &output_str) {

    std::string dst;

    for (size_t i = 0; i < input_ids.size(); i++) {
      if (input_ids[i] == _utf8_fallback_token_id) {
        std::string u8char;
        if (!utf8_char_from_ids(input_ids.data(), i+1, input_ids.size(), u8char, _utf8_id_offset)) {
          std::cerr << "utf8 reconstruct failed.\n";
          return false;
        }

        i += u8char.size();

        dst += u8char;

        continue;
      }

      if (!_id_to_str_map.count(input_ids[i])) {
        std::cerr << "id not found: " << input_ids[i] << "\n";
        return false;
      }

      dst += _id_to_str_map[input_ids[i]];
    }

    output_str = dst;

    return true;
  }

 private:

  // We can use uint16_t as value type.
  tsl::htrie_map<char, int> _trie_map;

  std::map<std::string, int> _str_to_id_map;
  std::map<int, std::string> _id_to_str_map;

  int _utf8_fallback_token_id{-1};
  int _utf8_id_offset{1}; // ASCII character is +1'ed in RWKV world vocab

  inline uint32_t utf8_len(const uint8_t c) {
    if (c <= 127) {
      // ascii
      return 1;
    } else if ((c & 0xE0) == 0xC0) {
      return 2;
    } else if ((c & 0xF0) == 0xE0) {
      return 3;
    } else if ((c & 0xF8) == 0xF0) {
      return 4;
    }

    // invalid
    return 0;
  }

  // Reconstruct UTF-8 bytes from int sequence(UTF-8 encoded)
  inline bool utf8_char_from_ids(const int *addr, size_t loc, size_t n, std::string &str, int id_offset = 1) {
    if (loc >= n) {
      return false;
    }

    int start_c = addr[loc] - id_offset;
    if ((start_c < 0) || (start_c > 255)) {
      return false;
    }

    uint32_t len = utf8_len(uint8_t(start_c));

    if (len == 0) {
      return false;
    }

    if ((loc + len) > n) {
      return false;
    }

    str = "";
    std::vector<uint8_t> buf;
    for (size_t i = 0; i < len; i++) {
      int ic = addr[loc + i] - id_offset;
      if ((ic < 0) || (ic > 255)) {
        return false;
      }
      buf.push_back(uint8_t(ic));
    }

    str = std::string(reinterpret_cast<const char *>(buf.data()), buf.size());

    return true;
  }

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


};

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

#if 0
  TrieTokenizer tokenizer;

  if (!tokenizer.load_vocab(str_to_id_map)) {
    std::cerr << "Vocab seems too large(65535 or more): "
              << vocab_json_filename << "\n";
    return -1;
  }

  // encode UTF-8 string
  std::string input_str = u8"吾輩は猫である。🤩";
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
#endif




#if 0
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
    }
  }
#endif

  CedarTrieTokenizer tokenizer;

  if (!tokenizer.load_vocab(str_to_id_map)) {
    std::cerr << "Vocab seems too large(65535 or more): "
              << vocab_json_filename << "\n";
    return -1;
  }

  std::string input_str = u8"吾輩は猫である。🤩";

  std::vector<int> input_ids;
  if (!tokenizer.encode(input_str, input_ids)) {
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
  }

  std::cout << "input_ids.len = " << input_ids.size() << "\n";

  return EXIT_SUCCESS;
}
