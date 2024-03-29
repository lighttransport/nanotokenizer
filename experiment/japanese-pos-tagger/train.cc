#include <cstdio>
#include <cstdlib>
#include <limits>
#include <unordered_map>

// use common/zstd.h
#include "zstd.h"

//
#include "safetensors.hh"

//
#define NANOCSV_IMPLEMENTATION
#include "nanocsv.h"

#define ERROR_AND_RETURN(s)                              \
  do {                                                   \
    std::ostringstream ss_e;                             \
    ss_e << "[error]"                                    \
         << ":" << __func__ << "():" << __LINE__ << " "; \
    ss_e << s << "\n";                                   \
    std::cerr << ss_e.str();                             \
    return false;                                        \
  } while (0)

#define DCOUT(s)                              \
  do {                                                   \
    std::ostringstream ss_e;                             \
    ss_e << __func__ << "():" << __LINE__ << " "; \
    ss_e << s << "\n";                                   \
    std::cout << ss_e.str();                             \
  } while (0)

//
// Simple Naiive Implementation of Trie tree.
//
template<typename KeyType, typename ValueType>
class NaiiveTrie
{
 public:

  enum class TraverseResult {
    Success = 0,
    FailAtLeaf = -1,
    FailAtIntermediate = -2,
    InvalidArg = -3,
  };

  struct NaiiveTrieNode
  {
    std::map<KeyType, NaiiveTrieNode> children;
    bool has_value{false};
    ValueType value{};
  };

  struct TraverseNode {
    const NaiiveTrieNode *node{nullptr};
    int depth{0}; // corresponding Trie tree depth(0 = root)
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

  ///
  /// Traverse Trie tree.
  ///
  /// When `from_node` is provided, traverse Trie tree from that.
  /// It returns the pointer of the last traversed node.
  ///
  /// @param[in] key Key
  /// @param[in] key_len Key length
  /// @param[out] value_out Value of corresponding key(When TraverseResult::Succes)
  /// @param[out] last_traversed_node Lastly traversed node.
  /// @param[in] from_node Traverse Trie trie from given node. nullptr = Traverse from root
  ///
  /// Return TraverseResult code.
  ///
  /// NOTE: the pointer address of `last_traversed_node`  would change when `update` is called subsequently.
  /// Thus the app should not reuse the pointer address when the code is calling `traverse() -> update() -> traverse()`.
  ///
  TraverseResult traverse(const KeyType *key, const size_t key_len, ValueType &value_out, TraverseNode &last_traversed_node, const TraverseNode *from_node = nullptr) {

    if (!key) {
      return TraverseResult::InvalidArg;
    }

    if (key_len == 0) {
      return TraverseResult::InvalidArg;
    }

    const NaiiveTrieNode *node = (from_node && from_node->node) ? from_node->node : &_root;
    size_t depth = from_node ? from_node->depth : 0;

    if (depth >= key_len) {
      return TraverseResult::InvalidArg;
    }

    last_traversed_node.node = node;
    last_traversed_node.depth = depth;

    for (size_t i = depth; i < key_len; i++) {

      const KeyType token = key[i];
      if (!node->children.count(token)) {
        return TraverseResult::FailAtIntermediate;
      }
      node = &node->children.at(token);

      last_traversed_node.node = node;
      last_traversed_node.depth++;
    }

    if (node->has_value) {
      value_out = node->value;
      return TraverseResult::Success;
    }

    return TraverseResult::FailAtLeaf;


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

struct feature_t {
  int id{-1};
  uint16_t pos_str_len{0};      // Up to 16bit
  uint16_t feature_str_len{0};  // Up to 16bit
  // Assume buffer is up to 4GB
  uint32_t pos_offset{0};      // POS string offset bytes in the buffer
  uint32_t feature_offset{0};  // Feature string offset bytes in the buffer
};

struct pattern_t {
  std::string surface;
  int prev_pos_id{-1};
  int count{0};
  int32_t shift{0};
  int32_t char_kind{0}; // we can use uint8
  int32_t feature_id{-1};
};

static const char *kPOSDigit =
    "\x09\xE5\x90\x8D\xE8\xA9\x9E\x2C\xE6\x95\xB0\xE8\xA9\x9E\x2C\x2A\x2C\x2A";

static const char *kPOSUnknown =
    "\x09\xE5\x90\x8D\xE8\xA9\x9E\x2C\xE6\x99\xAE\xE9\x80\x9A\xE5\x90\x8D\xE8"
    "\xA  9\x9E\x2C\x2A\x2C\x2A";
static const char *kPOSSymbol =
    "\x09\xE7\x89\xB9\xE6\xAE\x8A\x2C\xE8\xA8\x98\xE5\x8F\xB7\x2C\x2A\x2C\x2A "
    " ";

// digit/alpha/katanaka
const std::string kDigit =
    u8"0123456789０１２３４５６７８９〇一二三四五六七八九十百千万億兆京数・";
const std::string kAlphabet =
    u8"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZａｂｃｄｅｆｇｈｉｊ"
    u8"ｋｌｍｎｏｐｑｒｓｔｕｖｗｘｙｚＡＢＣＤＥＦＧＨＩＪ"
    u8"ＫＬＭＮＯＰＱＲＳＴＵＶＷＸＹＺ＠：／．";
const std::string kKatakana =
    u8"ァアィイゥウェエォオカガキギクグケゲコゴサザシジスズセゼソゾタダチヂッツ"
    u8"ヅテデトドナニヌネノハバパヒビピフブプヘベペホボポマミムメモャヤ  "
    u8"ュユョヨラリルレロヮワヰヱヲンヴヵヶヷヸヹヺーヽヾヿァアィイゥウェエォオ"
    u8"カガキギクグケゲコゴサザシジスズセゼソゾタダチヂッツヅテデトドナニヌネノ"
    u8"ハバパヒビピフブプヘベペホボポマミムメモャヤュユョヨラリルレロヮワヰヱヲ"
    u8"ンヴヵヶヷヸヹヺーヽヾヿ";

constexpr uint32_t kMaxCodePoint = 0x10ffff;

// Char kind is used to assist detecting word boundary.
enum class CharKind {
  KIND_OTHER = 0,
  KIND_DIGIT = 1,
  KIND_ALPHABET = 1 << 1,
  KIND_KATAKANA = 1 << 2,
  KIND_ANY = 0x7,  // mixture of digit/alphabet/katakana
};

inline uint32_t utf8_len(const char c_) {
  const uint8_t c = uint8_t(c_);
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

uint32_t to_codepoint(const char *s, uint32_t &len) {
  if (!s) {
    len = 0;
    return ~0u;
  }

  uint32_t char_len = utf8_len(*s);

  uint32_t code = 0;
  if (char_len == 1) {
    unsigned char s0 = static_cast<unsigned char>(s[0]);
    if (s0 > 0x7f) {
      len = 0;
      return ~0u;
    }
    code = uint32_t(s0) & 0x7f;
  } else if (char_len == 2) {
    // 11bit: 110y-yyyx 10xx-xxxx
    unsigned char s0 = static_cast<unsigned char>(s[0]);
    unsigned char s1 = static_cast<unsigned char>(s[1]);

    if (((s0 & 0xe0) == 0xc0) && ((s1 & 0xc0) == 0x80)) {
      code = (uint32_t(s0 & 0x1f) << 6) | (s1 & 0x3f);
    } else {
      len = 0;
      return ~0u;
    }
  } else if (char_len == 3) {
    // 16bit: 1110-yyyy 10yx-xxxx 10xx-xxxx
    unsigned char s0 = static_cast<unsigned char>(s[0]);
    unsigned char s1 = static_cast<unsigned char>(s[1]);
    unsigned char s2 = static_cast<unsigned char>(s[2]);
    if (((s0 & 0xf0) == 0xe0) && ((s1 & 0xc0) == 0x80) &&
        ((s2 & 0xc0) == 0x80)) {
      code =
          (uint32_t(s0 & 0xf) << 12) | (uint32_t(s1 & 0x3f) << 6) | (s2 & 0x3f);
    } else {
      len = 0;
      return ~0u;
    }
  } else if (char_len == 4) {
    // 21bit: 1111-0yyy 10yy-xxxx 10xx-xxxx 10xx-xxxx
    unsigned char s0 = static_cast<unsigned char>(s[0]);
    unsigned char s1 = static_cast<unsigned char>(s[1]);
    unsigned char s2 = static_cast<unsigned char>(s[2]);
    unsigned char s3 = static_cast<unsigned char>(s[3]);
    if (((s0 & 0xf8) == 0xf0) && ((s1 & 0xc0) == 0x80) &&
        ((s2 & 0xc0) == 0x80) && ((s2 & 0xc0) == 0x80)) {
      code = (uint32_t(s0 & 0x7) << 18) | (uint32_t(s1 & 0x3f) << 12) |
             (uint32_t(s2 & 0x3f) << 6) | uint32_t(s3 & 0x3f);
    } else {
      len = 0;
      return ~0u;
    }
  } else {
    len = 0;
    return ~0u;
  }

  len = char_len;
  return code;
}

inline std::string extract_utf8_char(const std::string &str, uint32_t start_i,
                                     uint32_t &len) {
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

inline std::vector<std::string> split(
    const std::string &str, const std::string &sep,
    const uint32_t kMaxItems = (std::numeric_limits<int32_t>::max)() / 100) {
  size_t s;
  size_t e = 0;

  size_t count = 0;
  std::vector<std::string> result;

  while ((s = str.find_first_not_of(sep, e)) != std::string::npos) {
    e = str.find(sep, s);
    result.push_back(str.substr(s, e - s));
    if (++count > kMaxItems) {
      break;
    }
  }

  return result;
}

// Simple T <-> id map
// NOTE: size() = max_id + 1
// No erase() for simplicity.
template <typename T>
struct IdMap {
  std::map<T, int> t_to_id;
  std::map<int, T> id_to_t;

  void clear() {
    t_to_id.clear();
    id_to_t.clear();
  }

  // assign id automatically
  bool put(const T &p) {
    if (t_to_id.count(p)) {
      return true;
    }

    size_t id = id_to_t.size();

    if (id > (std::numeric_limits<int>::max)()) {
      return false;
    }

    t_to_id[p] = int(id);
    id_to_t[int(id)] = p;

    return true;
  }

  // assign id automatically
  bool put(const T &p, int &result) {
    if (t_to_id.count(p)) {
      result = t_to_id.at(p);
      return true;
    }

    size_t id = id_to_t.size();

    if (id > (std::numeric_limits<int>::max)()) {
      return false;
    }

    t_to_id[p] = int(id);
    id_to_t[int(id)] = p;

    result = id;

    return true;
  }

  bool has(const T &p) const { return t_to_id.count(p); }

  bool has(const int id) const { return id_to_t.count(id); }

  bool get(const T &p, int &result) const {
    if (t_to_id.count(p)) {
      result = t_to_id.at(p);
      return true;
    }

    return false;
  }

  bool get(const int id, T &result) const {
    if (id_to_t.count(id)) {
      result = id_to_t.at(id);
      return true;
    }

    return false;
  }

  size_t size() const { return t_to_id.size(); }
};

using StrIdMap = IdMap<std::string>;
using PatternIdMap = IdMap<std::pair<std::string, int>>;  // <str, prev_pos_id>
using FeatureIdMap = IdMap<std::pair<int, int>>;  // <feature_id, pos_id>

static inline bool is_line_ending(const char *p, size_t i, size_t end_i) {
  if (p[i] == '\0') return true;
  if (p[i] == '\n') return true;  // this includes \r\n
  if (p[i] == '\r') {
    if (((i + 1) < end_i) && (p[i + 1] != '\n')) {  // detect only \r case
      return true;
    }
  }
  return false;
}

inline std::string join(const std::vector<std::string> &strs,
                        const size_t s_idx, const size_t e_idx,
                        const char delimiter = ',', const char quote = '"') {
  if (s_idx >= e_idx) {
    return std::string();
  }

  if (s_idx >= strs.size()) {
    return std::string();
  }

  if (e_idx > strs.size()) {
    return std::string();
  }

  std::string dst;

  for (size_t i = s_idx; i < e_idx; i++) {
    if (i > s_idx) {
      dst += delimiter;
    }
    if (strs[i].find(delimiter) != std::string::npos) {
      dst += quote + strs[i] + quote;
    } else {
      dst += strs[i];
    }
  }

  return dst;
}

// Support quoted string'\"' (do not consider `delimiter` character in quoted
// string) delimiter must be a ASCII char. quote_char must be a single UTF-8
// char.
inline std::vector<std::string> parse_line(const char *p, const size_t len,
                                           const char delimiter = ',',
                                           const char *quote_char = "\"") {
  std::vector<std::string> tokens;

  if (len == 0) {
    return tokens;
  }

  size_t quote_size = utf8_len(quote_char[0]);

  bool in_quoted_string = false;
  size_t s_start = 0;

  const char *curr_p = p;

  for (size_t i = 0; i < len; i += utf8_len(*curr_p)) {
    curr_p = &p[i];

    if (is_line_ending(p, i, len - 1)) {
      break;
    }

    if ((i + quote_size) < len) {
      if (memcmp(curr_p, quote_char, quote_size) == 0) {
        in_quoted_string = !in_quoted_string;
        continue;
      }
    }

    if (!in_quoted_string && (p[i] == delimiter)) {
      // std::cout << "s_start = " << s_start << ", (i-1) = " << i-1 << "\n";
      // std::cout << "p[i] = " << p[i] << "\n";
      if (s_start < i) {
        std::string tok(p + s_start, i - s_start);

        tokens.push_back(tok);
      } else {
        // Add empty string
        tokens.push_back(std::string());
      }

      s_start = i + 1;  // next to delimiter char
    }
  }

  // the remainder
  // std::cout << "remain: s_start = " << s_start << ", len - 1 = " << len-1 <<
  // "\n";

  if (s_start <= (len - 1)) {
    std::string tok(p + s_start, len - s_start);
    tokens.push_back(tok);
  }

  return tokens;
}

// Test if chars in input string has all the same charkind.
// If not or input string does not exist in `chars_table`, return KIND_OTHER,
static inline CharKind classify_char_kind(
    const std::string &s, const std::map<std::string, int> &chars_table) {
  int n = int(CharKind::KIND_ANY);

  uint32_t char_len{0};
  for (size_t i = 0; i < s.size(); i += char_len) {
    std::string u8_char = extract_utf8_char(s, i, char_len);

    if (!chars_table.count(u8_char)) {
      return CharKind::KIND_OTHER;
    }

    n &= chars_table.at(u8_char);

    if (!n) {
      break;  // KIND_OTHER
    }
  }

  return static_cast<CharKind>(n);
}

struct token_and_pos_tag {
  // std::string token;
  int token_len{-1};
  int pos_id{-1};
  int feature_id{-1};
};

bool extract_pos(const std::string &feature, std::string &pos,
                 int num_pos_fields = 4, char delimiter = ',') {
  std::vector<std::string> fields =
      parse_line(feature.c_str(), feature.size(), delimiter);

  if (fields.size() < num_pos_fields) {
    std::cerr << "Insufficient POS fields:" << feature << "\n";
    return false;
  }

  pos = join(fields, 0, num_pos_fields + 1, ',', '"');
  return true;
}

class Trainer {
 public:
  Trainer(const char delimiter, const uint32_t num_pos_fields = 4)
      : _delimiter(delimiter), _num_pos_fields(num_pos_fields) {}

  bool train(const std::vector<std::string> &lines,
             const std::vector<std::string> &pos_tagged_lines) {
    std::map<std::string, int> chars_table;
    StrIdMap token_table;
    PatternIdMap pattern_table;

    // Maximum word length(in bytes) in vocab.
    size_t max_word_length = 0;

    // key: feature_id, value: counts
    std::map<int, int> feature_counts;

    // key: pos_id, value: counts
    std::map<int, int> pos_counts;

    // key: pattern_id, value = (key: POS_id, value = feature_id)
    std::map<int, std::map<int, int>> pattern_to_pos_and_feature_map;

    _pos_table.clear();
    _feature_table.clear();

    // hardcoded pos id
    _pos_table.put("\tBOS");      // must be id 0
    _pos_table.put(kPOSUnknown);  // id 1
    _pos_table.put(kPOSDigit);    // id 2
    _pos_table.put(kPOSSymbol);   // id 3
    {
      int id{-1};
      _pos_table.get("\tBOS", id);
      if (id != 0) {
        ERROR_AND_RETURN("BOS id must be 0");
      }
      _pos_table.get(kPOSUnknown, id);
      if (id != 1) {
        ERROR_AND_RETURN("Unknown POS id must be 1");
      }
      _pos_table.get(kPOSDigit, id);
      if (id != 2) {
        ERROR_AND_RETURN("Digit POS id must be 2");
      }
      _pos_table.get(kPOSSymbol, id);
      if (id != 3) {
        ERROR_AND_RETURN("Symbol POS id must be 3");
      }
    }

    for (const auto &it : lines) {
      std::vector<std::string> fields =
          parse_line(it.c_str(), it.size(), _delimiter);

      // at lest num_pos_fields + 1 fields must exist.
      if (fields.size() < _num_pos_fields + 1) {
        ERROR_AND_RETURN("Insufficient fields in line: " << it);
      }

      const std::string &surface = fields[0];

      int pattern_id;
      if (!pattern_table.put({surface, -1}, pattern_id)) {
        ERROR_AND_RETURN("Too many patterns.");
      }

      max_word_length = (std::max)(surface.size(), max_word_length);

      // POS fields.
      // e.g. 動詞,*,母音動詞,語幹
      const std::string pos = join(fields, 1, _num_pos_fields + 1, ',', '"');

      // full feature string.
      // e.g.
      // 動詞,*,母音動詞,語幹,い置付ける,いちづけ,代表表記:位置付ける/いちづける
      const std::string feature = join(fields, 1, fields.size(), ',', '"');

      // add pos and feature
      _pos_table.put(pos);
      _feature_table.put(feature);

      int pos_id{-1};
      if (!_pos_table.get(pos, pos_id)) {
        ERROR_AND_RETURN("POS not found: {" << pos);
      }

      int feature_id{-1};
      if (!_feature_table.get(feature, feature_id)) {
        ERROR_AND_RETURN("Feature not found: {" << feature);
      }

      pattern_to_pos_and_feature_map[pattern_id][pos_id] = feature_id;
    }
    size_t num_seed_patterns = pattern_table.size();
    std::cout << "# of seed patterns : " << num_seed_patterns << "\n";

    std::cout << "Max word length: " << max_word_length << "\n";

    // Register base characters.
    uint32_t char_len = 0;
    for (size_t i = 0; i < kDigit.size(); i += char_len) {
      std::string s = extract_utf8_char(kDigit, i, char_len);
      chars_table[s] = int(CharKind::KIND_DIGIT);
      if (!pattern_table.put({s, -1})) {
        ERROR_AND_RETURN("Too many words.");
      }
    }

    for (size_t i = 0; i < kAlphabet.size(); i += char_len) {
      std::string s = extract_utf8_char(kAlphabet, i, char_len);
      chars_table[s] = int(CharKind::KIND_ALPHABET);
      if (!pattern_table.put({s, -1})) {
        ERROR_AND_RETURN("Too many words.");
      }
    }

    for (size_t i = 0; i < kKatakana.size(); i += char_len) {
      std::string s = extract_utf8_char(kKatakana, i, char_len);
      chars_table[s] = int(CharKind::KIND_KATAKANA);
      if (!pattern_table.put({s, -1})) {
        ERROR_AND_RETURN("Too many words.");
      }
    }

    // std::vector<token_and_pos_tag> tokens;
    std::vector<std::pair<std::string, std::string>>
        token_and_features;  // <token, feature>

    // 0: pattern_id, 1: feature_id, 2: pattern length(shift)
    // std::vector<std::array<int, 3>> pis;
    std::string sentence;

    // key = pattern_id, value = (key = (pattern len(shift), feature_id), value
    // = count)
    std::map<int, std::map<std::pair<int, int>, int>>
        pattern_to_shift_feature_counts;

    for (const auto &line : pos_tagged_lines) {
      if (line.empty()) {
        continue;
      }

      if (line.compare("EOS\n") == 0) {
        //
        // Example:
        //
        // tokens = ['吾輩', 'は', '猫', 'である']
        // sentence = '吾輩は猫である'
        //
        // pattern_to_shift_feature_counts =
        //
        // [{'吾輩', -1} , feature_id, tokens[0].len]
        // [{'吾輩', pos0_id}', feature_id, tokens[0].len]
        // [{'吾輩は', -1}, feature_id, tokens[0].len]
        // [{'吾輩は', pos0_id}, feature_id, tokens[0].len]
        // [{'吾輩は猫', -1}, feature_id, tokens[0].len]
        // [{'吾輩は猫', pos0_id}, feature_id, tokens[0].len]
        // [{'吾輩は猫で', -1}, feature_id, tokens[0].len]
        // [{'吾輩は猫で', pos0_id}, feature_id, tokens[0].len]
        // [{'吾輩は猫であ', -1}, feature_id, tokens[0].len]
        // [{'吾輩は猫であ' pos0_id}, feature_id, tokens[0].len]
        // [{'吾輩は猫である', -1} feature_id, tokens[0].len]
        // [{'吾輩は猫である', pos0_id}, feature_id, tokens[0].len]
        // [{'は', -1}, feature_id, tokens[1].len]
        // [{'は', pos1_id}, feature_id, tokens[1].len]
        // [{'は猫', -1}, feature_id, tokens[1].len]
        // [{'は猫', pos1_id}, feature_id, tokens[1].len]
        // [{'は猫で', -1}, feature_id, tokens[1].len]
        // [{'は猫で', pos1_id}, feature_id, tokens[1].len]
        // [{'は猫であ', -1}, feature_id, tokens[1].len]
        // [{'は猫であ' pos1_id}, feature_id, tokens[1].len]
        // [{'は猫である', -1} feature_id, tokens[1].len]
        // [{'は猫である', pos1_id}, feature_id, tokens[1].len]
        // ...

        size_t sent_loc{0};
        size_t prev_pos_len{0};
        int prev_pos_id{0};
        for (const auto &ts : token_and_features) {
          const std::string &token = ts.first;
          const std::string &feature = ts.second;
          int feature_id{-1};
          if (!_feature_table.put(feature, feature_id)) {
            ERROR_AND_RETURN("Too many features");
          }

          // TODO: limit shift amount.
          size_t shift = token.size();

          for (size_t sent_len = shift;
               (sent_loc + sent_len <= sentence.size()) &&
               (sent_len <= max_word_length);) {
            std::string fragment = sentence.substr(sent_loc, sent_len);

            bool fragment_exists = pattern_table.has({fragment, -1});

            int fragment_id{-1};
            if (!pattern_table.put({fragment, -1}, fragment_id)) {
              ERROR_AND_RETURN("Failed to add fragment: " << fragment);
            }

            int pattern_id{-1};
            if (!pattern_table.put({fragment, prev_pos_len}, pattern_id)) {
              ERROR_AND_RETURN("Failed to add pattern: {"
                               << fragment << ", " << prev_pos_len << "}");
            }

            pattern_to_shift_feature_counts[fragment_id][{shift, feature_id}]++;
            pattern_to_shift_feature_counts[pattern_id][{shift, feature_id}]++;

            if (!fragment_exists) {  // new pattern
              break;
            }

            sent_len += utf8_len(sentence[sent_loc + sent_len]);
          }

          int tok_id{-1};
          if (!pattern_table.put({token, -1}, tok_id)) {
            ERROR_AND_RETURN("Failed to add pattern: {" << token << ", -1}");
          }

          // extract POS part from feature string.
          std::string pos;
          if (!extract_pos(feature, pos)) {
            ERROR_AND_RETURN("Failed to extract POS string from feature");
          }

          int pos_id{-1};
          if (!_pos_table.put(pos, pos_id)) {
            ERROR_AND_RETURN("Failed to add POS: {" << pos);
          }

          // token is only seen in training data
          // Add it also to vocabs
          if ((tok_id >= num_seed_patterns) &&
              (classify_char_kind(token, chars_table) !=
               CharKind::KIND_DIGIT)) {
            pos_counts[pos_id]++;
            int pi{-1};
            if (!pattern_table.put({"", prev_pos_id}, pi)) {
              ERROR_AND_RETURN("Failed to add pattern: {\"\", " << prev_pos_id
                                                                << "}");
            }

            std::string feature_str = pos + ",*,*,*\n";  // TODO: strip newline?

            int feature_id{-1};
            if (!_feature_table.put(feature_str, feature_id)) {
              ERROR_AND_RETURN("Too many features");
            }

            pattern_to_shift_feature_counts[pi][{0, feature_id}]++;
          }

          sent_loc += token.size();
          prev_pos_id = pos_id;
        }

        token_and_features.clear();
        sentence.clear();

      } else {
        // Parse POS tagged line: SURAFACE\tFEATURE
        std::vector<std::string> tup = split(line, "\t");
        if (tup.size() != 2) {
          ERROR_AND_RETURN("Invalid POS Tagged line:" << line);
        }

        token_and_features.push_back({tup[0], tup[1]});
        sentence += tup[0];
      }
    }

    // prune redundant patterns
    {
      NaiiveTrie<char, int> pattern_trie;

      for (const auto &item : chars_table) {
        uint32_t len;
        uint32_t cp = to_codepoint(item.first.c_str(), len);
        if ((cp == ~0) || (len == 0) || (cp > kMaxCodePoint)) {
          // ???
          ERROR_AND_RETURN("Invalid UTF8 character.");
        }

        uint32_t id = uint32_t(_counters.size());
        _counters[cp] = {0, id};
      }

      for (const auto &item : _pos_table.t_to_id) {
        uint32_t id = uint32_t(_counters.size());
        _counters[kMaxCodePoint + 1 + item.second] = {0, id};
      }

      size_t max_pos_id{0};
      for (const auto &item : _pos_table.t_to_id) {
        max_pos_id = (std::max)(size_t(item.second), max_pos_id);
      }

      for (const auto &item : pattern_table.t_to_id) {
        const std::pair<std::string, int> &pattern = item.first; // <pattern_str, prev_pos_id>
        const std::string &pattern_str = pattern.first;

        int prev_pos_id = pattern.second;
        int p_id = item.second;
        int shift = int(pattern_str.size());
        int count = 0;

        int feature_id{0};

        if (!pattern_to_shift_feature_counts.count(p_id)) {
          // Pattern is not seen in training data.
          if (p_id < num_seed_patterns) {  // Pattern in input vocabs.
            const std::map<int, int> &ps_map =
                pattern_to_pos_and_feature_map.at(p_id);
            int pos_id{0};

            // Find the lowest count
            for (const auto &ps : ps_map) {
              if (_counters[ps.first] >= _counters[pos_id]) {
                pos_id = ps.first;
              }
            }
            feature_id = ps_map.find(pos_id)->second;
          } else if (classify_char_kind(pattern_str, chars_table) ==
                     CharKind::KIND_DIGIT) {
            std::string feature = std::string(kPOSDigit) + ",*,*,*\n";

            if (!_feature_table.put(feature, feature_id)) {
              ERROR_AND_RETURN("Too many features.");
            }
          } else if (classify_char_kind(pattern_str, chars_table) !=
                     CharKind::KIND_OTHER) {
            std::string pos_str;
            if (!_pos_table.get(int(max_pos_id), pos_str)) {
              ERROR_AND_RETURN("POS str not found for id: " << max_pos_id);
            }

            std::string feature = pos_str + "," + pattern_str + "," + pattern_str + ",*\n";
            if (!_feature_table.put(feature, feature_id)) {
              ERROR_AND_RETURN("Too many features.");
            }
          } else {
            std::string feature = std::string(kPOSSymbol) + ",*,*,*\n";

            if (!_feature_table.put(feature, feature_id)) {
              ERROR_AND_RETURN("Too many features.");
            }
          }

        } else {
          // Unseen pattern
          const auto &m = pattern_to_shift_feature_counts.at(p_id);
          std::vector<int> shift_counts;
          shift_counts.assign(max_word_length + 1, 0);

          for (const auto &item : m) {
            int _shift = std::get<0>(item.first);

            shift_counts[_shift] += item.second;
          }

          shift = -std::distance(shift_counts.rend(),  std::max_element(shift_counts.rbegin(), shift_counts.rend())) - 1;

          for (const auto &item : m) {
            if ((std::get<0>(item.first) == shift) && (item.second > count)) {
              count = item.second;
              feature_id = std::get<1>(item.first);
            }
          }

          // Traverse Trie for each single char.
          NaiiveTrie<char, int>::TraverseNode from_node;
          from_node.node = nullptr; // = root
          int pattern_id{-1};

          for (size_t key_pos = 0; key_pos < pattern_str.size(); key_pos++) {
            from_node.depth = key_pos;

            NaiiveTrie<char, int>::TraverseNode traversed_node;
            int value;
            size_t key_len = key_pos + 1;

            //
            // traverse [key_pos, key_pos+1) Trie node.
            //
            auto trav_result = pattern_trie.traverse(pattern_str.c_str(), key_len, /* out */value, /* out */traversed_node, &from_node);
            if (trav_result == NaiiveTrie<char, int>::TraverseResult::InvalidArg) {
              ERROR_AND_RETURN("Invalid call of NaiiveTrie::traverse().");
            } else if (trav_result == NaiiveTrie<char, int>::TraverseResult::FailAtLeaf) {
              from_node.node = traversed_node.node;
              continue;
            } else if (trav_result == NaiiveTrie<char, int>::TraverseResult::FailAtIntermediate) {
              break;
            } else {
              // Success
              pattern_id = value;
            }
          }

          if (pattern_id > -1) {
            const pattern_t &pat = _patterns[size_t(pattern_id)];
            if ((shift == pat.shift) && (feature_id == pat.feature_id)) {
              continue;
            }
          }

        }

        // Count each character of pattern string.
        for (size_t s = 0; s < pattern_str.size(); ) {
          uint32_t char_len{0};
          uint32_t cp = to_codepoint(&pattern_str[s], char_len);
          if ((cp == ~0) || (char_len == 0)) {
            ERROR_AND_RETURN("Invalid UTF8 character in pattern string.");
          }
          _counters[cp].first += count + 1;
          s += char_len;
        }

        if (prev_pos_id != -1) {
          _counters[kMaxCodePoint + 1 + prev_pos_id].first += count + 1;
        } else {
          // surface only(BOS) pattern
          int pattern_id = int(_patterns.size());
          if (!pattern_trie.update(pattern_str.c_str(), pattern_str.size(), pattern_id)) {
            ERROR_AND_RETURN("Internal error: Patten Trie update failed.");
          }
        }

        CharKind char_kind = CharKind::KIND_OTHER;
        if (shift > 0) {
          std::string prefix = pattern_str.substr(0, shift);
          char_kind = classify_char_kind(prefix, chars_table);
        }

        pattern_t pt;
        pt.surface = pattern_str;
        pt.prev_pos_id = prev_pos_id;
        pt.char_kind = int(char_kind);
        pt.count = count;
        pt.shift = shift;
        pt.feature_id = feature_id;
        _patterns.emplace_back(pt);

      }
    }

    return true;
  }

  bool save_patterns(std::ofstream &ofs) {

    for (const auto &it : _patterns) {
      // separator: \t
      // count, surface, prev_pos, shift, char_kind, feature
      std::string line;
      line = std::to_string(it.count);
      line += "\t" + it.surface;
      if (it.prev_pos_id < 0) {
        line += "\t";
      } else {
        std::string pos_str;
        if (!_pos_table.get(it.prev_pos_id, pos_str)) {
          ERROR_AND_RETURN("Unknown POS string id: " << it.prev_pos_id);
        }
        line += pos_str;
      }
      line += "\t" + std::to_string(it.shift);
      std::string feature_str;
      if (!_feature_table.get(it.feature_id, feature_str)) {
        ERROR_AND_RETURN("Unknown feature string id: " << it.feature_id);
      }
      line += "\t" + std::to_string(static_cast<int>(it.char_kind));
      // TODO: add '\t' separator?
      line += feature_str; // feature_str includes '\n'

      ofs << line;
    }

    return true;
  }

  bool save_pretrained(const std::string &base_filename) {
    size_t data_offset = 0;
    safetensors::safetensors_t st;

    FeatureIdMap feature_id_map;
    feature_id_map.put({0, 1});  // (unknown, kPOSUnknown) -> id 0

    // Single UTF-8 character to id table.
    // We use uint32 insted of uint16(in jagger), since it may not fit in 16bit.
    std::vector<int32_t> char_to_id;
    char_to_id.assign(kMaxCodePoint + 1 + _pos_table.size(),
                      -1);  // -1 = invalid
    char_to_id[0] = 0;      // BOS
    for (size_t i = 1; i < char_to_id.size(); i++) {
      if (_counters.count(i)) {
        const auto &item = _counters.at(i);
        if (item.first) {  // has counts
          char_to_id[item.second] = int(i);
        }
      }
    }

    { // patterns file
      // Sort patterns based on count, then string(lexicologically)
      std::sort(_patterns.rbegin(), _patterns.rend(), [](const pattern_t &a, const pattern_t &b) {
        if (a.count == b.count) {
          return a.surface <  b.surface;
        } else {
          return a.count < b.count;
        }
      });

      std::ofstream ofs(base_filename);
      if (!ofs) {
        ERROR_AND_RETURN("Failed to open file for write: " << base_filename);
      }

      if (!save_patterns(ofs)) {
        ERROR_AND_RETURN("Failed to save `patterns` to file: " << base_filename);
      }
    }


    {
      size_t sz = char_to_id.size() * sizeof(int32_t);
      st.storage.resize(data_offset + sz);
      memcpy(st.storage.data() + data_offset, char_to_id.data(), sz);

      safetensors::tensor_t tensor;
      tensor.dtype = safetensors::dtype::kUINT8;  // Use UINT32?
      tensor.data_offsets[0] = data_offset;
      tensor.data_offsets[1] = data_offset + sz;
      tensor.shape.resize(1);
      tensor.shape[0] = sz;
      st.tensors.insert("char_to_id", tensor);

      data_offset += sz;
    }

    //
    // Serialize feature(+ pos) strings
    // TODO: Concatenate string with null char for better portablity?
    //
    std::string feature_str_buf;
    std::vector<size_t> pos_offsets;
    std::vector<size_t> pos_str_lens;
    pos_offsets.assign(_pos_table.size(), ~0);
    pos_str_lens.assign(_pos_table.size(), ~0);
    for (const auto &item : _pos_table.t_to_id) {
      pos_offsets[item.second] = feature_str_buf.size();
      pos_str_lens[item.second] = item.first.size();

      feature_str_buf += item.first;
    }

    {
      size_t sz = feature_str_buf.size();
      st.storage.resize(data_offset + sz);
      memcpy(st.storage.data() + data_offset, feature_str_buf.data(), sz);

      safetensors::tensor_t tensor;
      tensor.dtype = safetensors::dtype::kUINT8;
      tensor.data_offsets[0] = data_offset;
      tensor.data_offsets[1] = data_offset + sz;
      tensor.shape.resize(1);
      tensor.shape[0] = sz;
      st.tensors.insert("feature_strings", tensor);

      data_offset += sz;
    }

    std::vector<size_t> feature_offsets;
    std::vector<size_t> feature_str_lens;
    feature_offsets.assign(_feature_table.size(), ~0);
    feature_str_lens.assign(_feature_table.size(), 0);

    for (const auto &item : _feature_table.t_to_id) {
      feature_offsets[item.second] = feature_str_buf.size();
      feature_str_lens[item.second] = item.first.size();
      feature_str_buf += item.first;
    }
    //DCOUT("feature_offsets.size = " << feature_offsets.size());
    size_t num_feature_offsets = feature_offsets.size();

    std::vector<feature_t> features;
    features.assign(_feature_table.size(), {});
    for (const auto &item : feature_id_map.t_to_id) {
      const int pos_id = std::get<1>(item.first);
      const int feature_id = std::get<0>(item.first);
      if (pos_id < 0) {
        ERROR_AND_RETURN("Invalid pos_id: " << pos_id);
      } else if (pos_id >= pos_offsets.size()) {
        ERROR_AND_RETURN("pos_id out-of-range: " << feature_id << ", sz " << pos_offsets.size());
      }

      if (feature_id < 0) {
        ERROR_AND_RETURN("Invalid feature_id: " << feature_id);
      } else if (feature_id >= int(num_feature_offsets)) {
        ERROR_AND_RETURN("feature_id out-of-range: " << feature_id << ", sz " << num_feature_offsets);
      }

      feature_t &wf = features[size_t(item.second)];
      wf.id = feature_id;

      wf.pos_offset = pos_offsets[size_t(pos_id)];
      wf.pos_str_len = pos_str_lens[size_t(pos_id)];

      wf.feature_offset = feature_offsets[size_t(feature_id)];
      wf.feature_str_len = feature_str_lens[size_t(feature_id)];
    }

    {
      size_t sz = features.size() * sizeof(feature_t);
      st.storage.resize(data_offset + sz);
      memcpy(st.storage.data() + data_offset, features.data(), sz);

      safetensors::tensor_t tensor;
      tensor.dtype = safetensors::dtype::kUINT8;
      tensor.data_offsets[0] = data_offset;
      tensor.data_offsets[1] = data_offset + sz;
      tensor.shape.resize(1);
      tensor.shape[0] = sz;
      st.tensors.insert("features", tensor);

      data_offset += sz;
    }

    // TODO: Save Trie data

    {
      st.metadata.insert("creator", "nanotokenizer");
      st.metadata.insert("num_pos_fields", std::to_string(_num_pos_fields));
    }

    return true;
  }

 private:
  StrIdMap _pos_table;
  StrIdMap _feature_table;
  char _delimiter{','};
  int _num_pos_fields{4};


  std::vector<pattern_t> _patterns;

  // key: single UTF-8 char id or POS id, value = (count, unique_id))
  std::unordered_map<int, std::pair<int, int>> _counters;
};

int main(int argc, char **argv) {
  if (argc < 4) {
    std::cout
        << "Need input.vocab(csv) train.txt(POS tagged) output_basename [num_pos_fields]\n";
    exit(-1);
  }

  std::string vocab_filename = argv[1];
  std::string pos_tagged_filename = argv[2];
  std::string output_basename = argv[3];

  // 4 = Mecab dict style: 品詞,品詞細分類1,品詞細分類2,品詞細分類3
  int num_pos_fields = 4;

  if (argc > 4) {
    num_pos_fields = std::stoi(argv[4]);
  }

  nanocsv::ParseTextOption csv_option;
  csv_option.ignore_header = true;
  csv_option.req_num_threads = 1;
  csv_option.delimiter = ',';
  nanocsv::TextCSV csv;
  std::string warn;
  std::string err;
  bool ret = nanocsv::ParseTextCSVFromFile(vocab_filename, csv_option, &csv,
                                           &warn, &err);
  if (warn.size()) {
    std::cout << "CSV read warn: " << warn << "\n";
  }
  if (!ret) {
    std::cerr << "CSV read err: " << err << "\n";
    exit(-1);
  }

  std::cout << "# of rows = " << csv.num_records
            << ", # of columns = " << csv.num_fields << "\n";

  // construct string[](validated by nanocsv)
  std::vector<std::string> lines;
  for (size_t row = 0; row < csv.num_records; row++) {
    std::string line;
    for (size_t col = 0; col < csv.num_fields; col++) {
      if (col > 0) {
        line += ",";
      }

      const std::string &field = csv.values[row * csv.num_fields + col];
      if (field.find(",")) {
        line += "\"" + field + "\"";
      } else {
        line += field;
      }
    }

    line += "\n";  // TODO: do not include newline.
    lines.emplace_back(std::move(line));
  }

  std::vector<std::string> pos_tagged_lines;
  {
    std::ifstream ifs(pos_tagged_filename);
    std::string s;
    while (std::getline(ifs, s)) {
      // TODO: do not include newline
      pos_tagged_lines.push_back(s + "\n");
    }
  }

  Trainer trainer(',', num_pos_fields);

  if (!trainer.train(lines, pos_tagged_lines)) {
    std::cerr << "Train failed.\n";
    exit(-1);
  }

  if (!trainer.save_pretrained(output_basename)) {
    std::cerr << "Failed to save pretrained data.\n";
    exit(-1);
  }

  std::cout << "Train DONE!\n";

  return 0;
}
