#include <cstdio>
#include <cstdlib>
#include <limits>
#include <unordered_map>

#define NANOCSV_IMPLEMENTATION
#include "nanocsv.h"

static const char* kPOSDigit = "\x09\xE5\x90\x8D\xE8\xA9\x9E\x2C\xE6\x95\xB0\xE8\xA9\x9E\x2C\x2A\x2C\x2A";

static const char* kPOSUnknown = "\x09\xE5\x90\x8D\xE8\xA9\x9E\x2C\xE6\x99\xAE\xE9\x80\x9A\xE5\x90\x8D\xE8\xA  9\x9E\x2C\x2A\x2C\x2A";
static const char* kPOSSymbol = "\x09\xE7\x89\xB9\xE6\xAE\x8A\x2C\xE8\xA8\x98\xE5\x8F\xB7\x2C\x2A\x2C\x2A  ";

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
// No erase()
template <typename T>
struct IdMap {
  std::map<T, int> t_to_id;
  std::map<int, T> id_to_t;

  // assign id automatically
  bool put(const T &p) {
    if (t_to_id.count(p)) {
      return true;
    }

    size_t id = id_to_t.size();

    if (id < (std::numeric_limits<int>::max)()) {
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

    if (id < (std::numeric_limits<int>::max)()) {
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
using PatternIdMap = IdMap<std::pair<std::string, int>>;  // <str, len>
using FeatureIdMap = IdMap<std::pair<int, int>>;  // <feature_str_id, len>

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

// 4 = Mecab dict: 品詞,品詞細分類1,品詞細分類2,品詞細分類3
bool train(const std::vector<std::string> &lines,
           const std::vector<std::string> &pos_tagged_lines,
           const char delimiter, const uint32_t num_pos_fields = 4) {
  std::map<std::string, int> chars_table;
  StrIdMap token_table;
  StrIdMap pos_table;
  StrIdMap feature_table;
  // StrIdMap word_table;
  PatternIdMap pattern_table;

  // Maximum word length(in bytes) in vocab.
  size_t max_word_length = 0;

  // key: feature_id, value: counts
  std::map<int, int> feature_counts;

  // key: pos_id, value: counts
  std::map<int, int> pos_counts;

  // key: pattern_id, value = (key: POS_id, value = feature_id)
  std::map<int, std::map<int, int>> pattern_to_pos_and_feature_map;

  for (const auto &it : lines) {
    std::vector<std::string> fields =
        parse_line(it.c_str(), it.size(), delimiter);

    // at lest num_pos_fields + 1 fields must exist.
    if (fields.size() < num_pos_fields + 1) {
      std::cerr << "Insufficient fields in line: " << it << "\n";
      return false;
    }

    const std::string &surface = fields[0];

    int pattern_id;
    if (!pattern_table.put({surface, -1}, pattern_id)) {
      std::cerr << "Too many patterns.\n";
      return false;
    }

    max_word_length = (std::max)(surface.size(), max_word_length);

    // POS fields.
    // e.g. 動詞,*,母音動詞,語幹
    const std::string pos = join(fields, 1, num_pos_fields + 1, ',', '"');

    // full feature string.
    // e.g.
    // 動詞,*,母音動詞,語幹,い置付ける,いちづけ,代表表記:位置付ける/いちづける
    const std::string feature = join(fields, 1, fields.size(), ',', '"');

    // add pos and feature
    pos_table.put(pos);
    feature_table.put(feature);

    int pos_id;
    if (!pos_table.get(pos, pos_id)) {
      std::cerr << "Internal error: POS " << pos
                << " not found in the table.\n";
      return false;
    }

    int feature_id;
    if (!feature_table.get(feature, feature_id)) {
      std::cerr << "Internal error: feature " << feature
                << " not found in the table.\n";
      return false;
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
      std::cerr << "Too many words.\n";
      return false;
    }
  }

  for (size_t i = 0; i < kAlphabet.size(); i += char_len) {
    std::string s = extract_utf8_char(kAlphabet, i, char_len);
    chars_table[s] = int(CharKind::KIND_ALPHABET);
    if (!pattern_table.put({s, -1})) {
      std::cerr << "Too many words.\n";
      return false;
    }
  }

  for (size_t i = 0; i < kKatakana.size(); i += char_len) {
    std::string s = extract_utf8_char(kKatakana, i, char_len);
    chars_table[s] = int(CharKind::KIND_KATAKANA);
    if (!pattern_table.put({s, -1})) {
      std::cerr << "Too many words.\n";
      return false;
    }
  }

  // std::vector<token_and_pos_tag> tokens;
  std::vector<std::pair<std::string, std::string>>
      token_and_features;  // <token, feature>

  // 0: pattern_id, 1: feature_id, 2: pattern length(shift)
  // std::vector<std::array<int, 3>> pis;
  std::string sentence;

  // key = pattern_id, value = (key = (pattern len(shift), feature_id), value =
  // count)
  std::map<int, std::map<std::pair<int, int>, int>>
      pattern_to_shift_feature_counts;

  for (const auto &line : pos_tagged_lines) {
    if (line.empty()) {
      continue;
    }

    if (line.compare("EOS\n") == 0) {
      std::string prev_pos = "\tBOS";

      //
      // Example:
      //
      // tokens = ['吾輩', 'は', '猫', 'である']
      // sentence = '吾輩は猫である'
      //
      // ['吾輩', feature_id, tokens[0].len]
      // ['吾輩\tBOS', feature_id, tokens[0].len]
      // ['吾輩は', feature_id, tokens[0].len]
      // ['吾輩は\tBOS', feature_id, tokens[0].len]
      // ['吾輩は猫', feature_id, tokens[0].len]
      // ['吾輩は猫\tBOS', feature_id, tokens[0].len]
      // ['吾輩は猫で', feature_id, tokens[0].len]
      // ['吾輩は猫で\tBOS', feature_id, tokens[0].len]
      // ['吾輩は猫であ', feature_id, tokens[0].len]
      // ['吾輩は猫であ\tBOS', feature_id, tokens[0].len]
      // ['吾輩は猫である', feature_id, tokens[0].len]
      // ['吾輩は猫である\tBOS', feature_id, tokens[0].len]
      //

      size_t sent_loc{0};
      size_t prev_pos_len{0};
      int prev_pos_id{0};
      for (const auto &ts : token_and_features) {
        const std::string &token = ts.first;
        const std::string &feature = ts.second;
        int feature_id{-1};
        if (!feature_table.get(feature, feature_id)) {
          std::cerr << "id not found for feature string: " << feature << "\n";
          return false;
        }

        size_t shift = token.size();

        // TODO: limit shift amount.

        for (size_t sent_len = shift;
             (sent_loc + sent_len <= sentence.size()) &&
             (sent_len <= max_word_length);) {
          std::string fragment = sentence.substr(sent_loc, sent_len);

          bool fragment_exists = pattern_table.has({fragment, -1});

          int fragment_id{-1};
          if (!pattern_table.put({fragment, -1}, fragment_id)) {
            std::cerr << "Failed to add fragment: " << fragment << "\n";
            return false;
          }

          int pattern_id{-1};
          if (!pattern_table.put({fragment, prev_pos_len}, pattern_id)) {
            std::cerr << "Failed to add pattern: {" << fragment << ", "
                      << prev_pos_len << "}\n";
            return false;
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
          std::cerr << "Failed to add pattern: {" << token << ", -1}\n";
          return false;
        }

        // extract POS part from feature string.
        std::string pos;
        if (!extract_pos(feature, pos)) {
          return false;
        }

        int pos_id{-1};
        if (!pos_table.put(pos, pos_id)) {
          std::cerr << "Failed to add POS: " << pos << "\n";
          return false;
        }

        // token is only seen in training data
        // Add it also to vocabs
        if ((tok_id >= num_seed_patterns) &&
            (classify_char_kind(token, chars_table) != CharKind::KIND_DIGIT)) {
          pos_counts[pos_id]++;
          int pi{-1};
          if (!pattern_table.put({"", prev_pos_id}, pi)) {
            std::cerr << "Failed to add pattern: {\"\", " << prev_pos_id
                      << "}\n";
            return false;
          }

          std::string feature_str = pos + ",*,*,*\n";  // TODO: strip newline?

          int feature_id{-1};
          if (!feature_table.put(feature_str, feature_id)) {
            std::cerr << "Too many features\n";
            return false;
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
        std::cerr << "Invalid POS Tagged line:" << line << "\n";
        return false;
      }

#if 0
      std::vector<std::string> fields =
          parse_line(tup[1].c_str(), tup[1].size(), delimiter);

      if (fields.size() < num_pos_fields) {
        std::cerr << "Insufficient POS fields:" << tup[1] << "\n";
        return false;
      }

      const std::string pos = join(fields, 1, num_pos_fields + 1, ',', '"');
      const std::string feature = join(fields, 1, fields.size(), ',', '"');

      feature_table.put(pos);
      feature_table.put(feature);

      int pos_id;
      if (!feature_table.get(pos, pos_id)) {
        std::cerr << "Internal error: POS " << pos
                  << " not found in the table.\n";
        return false;
      }

      int feature_id;
      if (!feature_table.get(feature, feature_id)) {
        std::cerr << "Internal error: feature " << feature
                  << " not found in the table.\n";
        return false;
      }

      token_and_pos_tag tok;
      tok.token_len = tup[0].size();
      tok.pos_id = pos_id;
      tok.feature_id = feature_id;

#else
      token_and_features.push_back({tup[0], tup[1]});
      sentence += tup[0];
#endif
    }
  }

  // prune redundant patterns
  {
    // key: single char id + POS id, value = (count, unique_id))
    std::unordered_map<int, std::pair<int, int>> counters;

    for (const auto &item : chars_table) {
      uint32_t len;
      uint32_t cp = to_codepoint(item.first.c_str(), len);
      if ((cp == ~0) || (len == 0) || (cp > kMaxCodePoint)) {
        // ???
        std::cerr << "Invalid UTF8 character.\n";
        return false;
      }

      uint32_t id = uint32_t(counters.size());
      counters[cp] = {0, id};
    }

    for (const auto &item : pos_table.t_to_id) {
      uint32_t id = uint32_t(counters.size());
      counters[kMaxCodePoint + 1 + item.second] = {0, id};
    }

    size_t max_pos_id;
    for (const auto &item : pos_table.t_to_id) {
      max_pos_id = (std::max)(size_t(item.second), max_pos_id);
    }

    for (const auto &item : pattern_table.t_to_id) {
      const std::pair<std::string, int> &pattern = item.first;
      const std::string &p_str = pattern.first;

      int prev_pos_id = pattern.second;
      int p_id = item.second;
      int shift = int(p_str.size());

      int feature_id{0};

      if (!pattern_to_shift_feature_counts.count(p_id)) {
        // Pattern is not seen in training data.
        if (p_id < num_seed_patterns) { // Pattern in input vocabs.
          const std::map<int, int>& ps_map = pattern_to_pos_and_feature_map.at(p_id);
          int pos_id{0};

          // Find the lowest count one
          for (const auto &ps : ps_map) {
            if (counters[ps.first] >= counters[pos_id]) {
              pos_id = ps.first;
            }
          }
          feature_id = ps_map.find(pos_id)->second;
        } else if (classify_char_kind(p_str, chars_table) == CharKind::KIND_DIGIT) {
          std::string feature = std::string(kPOSDigit) + ",*,*,*\n";

          if (!feature_table.put(feature, feature_id)) {
            std::cerr << "Too many features.\n";
            return false;
          }
        } else if (classify_char_kind(p_str, chars_table) != CharKind::KIND_OTHER) {
          // TODO
        } else {
          // TODO

        }
      }

    }

  }

  return true;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    std::cout << "Need input.vocab(csv) train.txt(POS tagged)\n";
    exit(-1);
  }

  std::string vocab_filename = argv[1];
  std::string pos_tagged_filename = argv[2];

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
  for (size_t row = 0; row < csv.num_records; row++) {
    for (size_t col = 0; col < csv.num_fields; col++) {
      if (col > 0) {
        std::cout << ", ";
      }

      std::cout << csv.values[row * csv.num_fields + col];
    }

    std::cout << "\n";
  }

  return 0;
}
