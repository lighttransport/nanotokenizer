#include <fstream>
#include <iostream>

#define MINIJSON_IMPLEMENTATION
#include "hat-trie/include/tsl/htrie_map.h"
#include "minijson.h"

//
#include "rwkv_world_tokenizer.hh"

#if defined(USE_CEDAR)

#include "ccedar_core.h"

namespace ccedar {

static inline int u8_len(const char *p) {
  static const uint8_t u8bytes[256] = {
      // must be static to tame compilers
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6};
  return u8bytes[static_cast<uint8_t>(*p)];
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
static inline int unicode(const char *p, int &b) {
  const unsigned char *p_ = reinterpret_cast<const unsigned char *>(p);
  // const int p0 (p_[0]), p1 (p_[1]), p2 (p_[2]), p3 (p_[3]);
  int p0, p1, p2, p3;

  switch (b = u8_len(p)) {
    case 1: {
      p0 = p_[0];
      return p0 & 0x7f;
    }
    case 2: {
      p0 = p_[0];
      p1 = p_[1];
      return ((p0 & 0x1f) << 6) | (p1 & 0x3f);
    }
    case 3: {
      p0 = p_[0];
      p1 = p_[1];
      p2 = p_[2];
      return ((p0 & 0xf) << 12) | ((p1 & 0x3f) << 6) | (p2 & 0x3f);
    }
    case 4: {
      p0 = p_[0];
      p1 = p_[1];
      p2 = p_[2];
      p3 = p_[3];
      return ((p0 & 0x7) << 18) | ((p1 & 0x3f) << 12) | ((p2 & 0x3f) << 6) |
             (p3 & 0x3f);
    }
    default:
      return 0;
  }
  return 0;
}

// constexpr size_t MAX_KEY_BITS = 14;
constexpr size_t MAX_KEY_BITS = 16;  // 65536

class da_ : public ccedar::da<int, int, MAX_KEY_BITS> {
 public:
#if 0  // TODO:
  struct utf8_feeder {  // feed one UTF-8 character by one while mapping codes
    const char *p, *const end;
    utf8_feeder(const char *key_, const char *end_) : p(key_), end(end_) {}
    int read(int &b) const { return p == end ? 0 : unicode(p, b); }
    void advance(const int b) { p += b; }
  };
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
  // @param[in] s_begin : Starting address of UTF-8 string.
  // @param[in] s_end : End addres of UTF-8 string.
  int longestPrefixSearch(const char *s_begin, const char *const s_end) const {
    // Assuming key is sorted
    // (i.e, the last candidate is the longest one)

    // TODO
    return -1;
  }
#endif
};

}  // namespace ccedar

// Trie tokenizer based on ccedar
class CedarTrieTokenizer {
 public:
  CedarTrieTokenizer() = default;
  ~CedarTrieTokenizer() {
    // free memory in cedar
    da.clear(/* reuse */ false);
    if (da.array()) {  // work around for _array is not free'ed in ccedar
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
    _utf8_id_offset = 1;  // ASCII character is +1'ed in RWKV world vocab

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
      int n = da.longestPrefixSearch(p, e_ptr);

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
        if (!utf8_char_from_ids(input_ids.data(), i + 1, input_ids.size(),
                                u8char, _utf8_id_offset)) {
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
    if (id > 0 && id < 257) {  // ASCII or UTF-8 byte
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
  int _utf8_id_offset{1};  // ASCII character is +1'ed in RWKV world vocab
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
  inline bool utf8_char_from_ids(const int *addr, size_t loc, size_t n,
                                 std::string &str, int id_offset = 1) {
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
#endif

// hat-trie version of Tokenizer.
// Up to 65535 vocab id
// - token id 0 is reserved for empty(zero)
// - token ids in [127, 256] are reserved for UTF-8 byte fallback(+1'ed)
// - max_id + 1 is used for UTF-8 fallback token id
class TrieTokenizer {
 public:
  bool load_vocab(const std::map<std::string, int> &str_to_id_map) {
    _str_to_id_map = str_to_id_map;

    int max_id{0};
    for (const auto &it : str_to_id_map) {
      if (it.second == 0) {
        return false;
      }
      // reserved for UTF-8 byte fallback
      if ((it.second >= 127) && (it.second <= 256)) {
        return false;
      }
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
    _utf8_id_offset = 1;  // ASCII character is +1'ed in RWKV world vocab

    return true;
  }

  bool encode(const std::string &_input_str, std::vector<int> &output_ids) {
    std::vector<int> dst;

    const size_t s_len = _input_str.size();

    if (s_len == 0) {
      // empty input
      return false;
    }

    size_t char_idx = 0;
    int prev_id = -1;  // Track previously matched result.
    size_t key_size = 0;

    // Find match for each UTF-8 character,
    // Since `longest_prefix` is quite slow for larger input string.

    while ((char_idx + key_size) < s_len) {
      // Extract UTF-8 char.
      uint32_t charlen = utf8_len(_input_str[char_idx]);
      if (charlen == 0) {
        // Found invalid UTF-8 string.
        return false;
      }

      key_size += charlen;

      auto it = _trie_map.find_ks(&_input_str[char_idx], key_size);
      if (it == _trie_map.cend()) {
        if (prev_id > 0) {
          // prev_id = id of longest matched key
          dst.push_back(prev_id);

          // pop current UTF-8 character.
          key_size -= charlen;

        } else {
          // UTF-8 byte fallback
          // Should be single UTF-8 character
          if (key_size != charlen) {
            // This should not happen. Just in case.
            return false;
          }

          dst.push_back(_utf8_fallback_token_id);

          for (size_t i = 0; i < charlen; i++) {
            dst.push_back(int(uint8_t(_input_str[char_idx + i])) +
                          _utf8_id_offset);
          }
        }

        prev_id = -1;

        char_idx += key_size;
        key_size = 0;
      } else {
        prev_id = *(it);

        // Continue search
      }
    }

    // Remainder
    if (prev_id) {
      dst.push_back(prev_id);
    }

    output_ids = dst;
    return true;
  }

  bool decode(const std::vector<int> input_ids, std::string &output_str) {
    std::string dst;

    for (size_t i = 0; i < input_ids.size(); i++) {
      if (input_ids[i] == _utf8_fallback_token_id) {
        std::string u8char;
        if (!utf8_char_from_ids(input_ids.data(), i + 1, input_ids.size(),
                                u8char, _utf8_id_offset)) {
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
  int _utf8_id_offset{1};  // ASCII character is +1'ed in RWKV world vocab

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
  inline bool utf8_char_from_ids(const int *addr, size_t loc, size_t n,
                                 std::string &str, int id_offset = 1) {
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

#if 1
  TrieTokenizer tokenizer;

  if (!tokenizer.load_vocab(str_to_id_map)) {
    std::cerr << "Vocab seems too large(65535 or more): " << vocab_json_filename
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

#else

  CedarTrieTokenizer tokenizer;

  if (!tokenizer.load_vocab(str_to_id_map)) {
    std::cerr << "Vocab seems too large(65535 or more): " << vocab_json_filename
              << "\n";
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
#endif

  return EXIT_SUCCESS;
}
