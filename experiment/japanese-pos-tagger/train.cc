#include <cstdio>
#include <cstdlib>
#include <limits>
#include <unordered_map>

#define NANOCSV_IMPLEMENTATION
#include "nanocsv.h"

// Zenkaku digit/alpha/katanaka
const std::string kDigit =
    u8"０１２３４５６７８９〇一二三四五六七八九十百千万億兆京数・";
const std::string kAlphabet =
    u8"ａｂｃｄｅｆｇｈｉｊｋｌｍｎｏｐｑｒｓｔｕｖｗｘｙｚＡＢＣＤＥＦＧＨＩＪ"
    u8"ＫＬＭＮ  ＯＰＱＲＳＴＵＶＷＸＹＺ＠：／．";
const std::string kKatakana =
    u8"ァアィイゥウェエォオカガキギクグケゲコゴサザシジスズセゼソゾタダチヂッツ"
    u8"ヅテデトドナニヌネノハバパヒビピフブプヘベペホボポマミムメモャヤ  "
    u8"ュユョヨラリルレロヮワヰヱヲンヴヵヶヷヸヹヺーヽヾヿァアィイゥウェエォオ"
    u8"カガキギクグケゲコゴサザシジスズセゼソゾタダチヂッツヅテデトドナニヌネノ"
    u8"ハバパヒビピフブプヘベペホボポマミムメモャヤュユョヨラリルレロヮワヰヱヲ"
    u8"ンヴヵヶヷヸヹヺーヽヾヿ";

// Char kind is used to assist detecting word boundary.
enum class CharKind {
  KIND_DIGIT = 0,
  KIND_ALPHABET = 1,
  KIND_KATAKANA = 2,
  KIND_OTHER = 3,  // Kanji, hiragana, emoji, etc.
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

// Simple String <-> Id, Id -> String map
struct StrIdMap {
  std::unordered_map<std::string, int> str_to_id;
  std::unordered_map<int, std::string> id_to_str;

  // assign id automatically
  bool put(const std::string &str) {
    if (str_to_id.count(str)) {
      return true;
    }

    size_t id = id_to_str.size();

    if (id < (std::numeric_limits<int>::max)()) {
      return false;
    }

    str_to_id[str] = int(id);
    id_to_str[int(id)] = str;

    return true;
  }

  // assign id automatically
  bool put(const std::string &str, int &result) {
    if (str_to_id.count(str)) {
      result = str_to_id.at(str);
      return true;
    }

    size_t id = id_to_str.size();

    if (id < (std::numeric_limits<int>::max)()) {
      return false;
    }

    str_to_id[str] = int(id);
    id_to_str[int(id)] = str;

    result = id;

    return true;
  }

  void add(const std::string &str, int val) {
    str_to_id[str] = val;
    id_to_str[val] = str;
  }

  bool has(const std::string &str) const { return str_to_id.count(str); }

  bool has(const int id) const { return id_to_str.count(id); }

  bool get(const std::string &str, int &result) const {
    if (str_to_id.count(str)) {
      result = str_to_id.at(str);
      return true;
    }

    return false;
  }

  bool get(const int id, std::string &result) const {
    if (id_to_str.count(id)) {
      result = id_to_str.at(id);
      return true;
    }

    return false;
  }

  size_t size() const { return str_to_id.size(); }
};

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
// If not, return KIND_OTHER,
static inline CharKind classify_char_kind(const std::string &s,
  const StrIdMap &chars_table) {

  int prev_kind{-1};
  
  uint32_t char_len{0};
  for (size_t i = 0; i < s.size(); i += char_len) {
    std::string u8_char = extract_utf8_char(s, i, char_len);

    int kind{-1};
    if (!chars_table.get(u8_char, kind)) {
      return CharKind::KIND_OTHER;
    }

    if (i > 0) {
      if (prev_kind != kind) {
        return CharKind::KIND_OTHER;
      }
    }

    prev_kind = kind;
  }

  return static_cast<CharKind>(prev_kind);
}

struct token_and_pos_tag {
  //std::string token;
  int token_len{-1};
  int pos_id{-1};
  int feature_id{-1};
};

// 4 = Mecab dict: 品詞,品詞細分類1,品詞細分類2,品詞細分類3
bool train(const std::vector<std::string> &lines,
           const std::vector<std::string> &pos_tagged_lines,
           const char delimiter, const uint32_t num_pos_fields = 4) {
  StrIdMap chars_table;
  StrIdMap feature_table;
  StrIdMap word_table;

  // Maximum word length(in bytes) in vocab.
  size_t max_word_length = 0;

  // key: feature_id, value: counts
  std::map<int, int> feature_counts;

  // key: pos_id, value: counts
  std::map<int, int> pos_counts;

  // key: word_id, value = (key: POS_id, value = feature_id)
  std::map<int, std::map<int, int>> word_to_pos_and_feature_map;

  for (const auto &it : lines) {
    std::vector<std::string> fields =
        parse_line(it.c_str(), it.size(), delimiter);

    // at lest num_pos_fields + 1 fields must exist.
    if (fields.size() < num_pos_fields + 1) {
      std::cerr << "Insufficient fields in line: " << it << "\n";
      return false;
    }

    const std::string &surface = fields[0];

    if (!word_table.put(surface)) {
      std::cerr << "Too many words.\n";
      return false;
    }

    max_word_length = (std::max)(surface.size(), max_word_length);

    int word_id;
    if (!word_table.get(surface, word_id)) {
      // This should not happen though.
      std::cerr << "Internal error: word " << surface
                << " not found in the table.\n";
      return false;
    }

    // POS fields.
    // e.g. 動詞,*,母音動詞,語幹
    const std::string pos = join(fields, 1, num_pos_fields + 1, ',', '"');

    // full feature string.
    // e.g.
    // 動詞,*,母音動詞,語幹,い置付ける,いちづけ,代表表記:位置付ける/いちづける
    const std::string feature = join(fields, 1, fields.size(), ',', '"');

    // add pos and feature
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

    word_to_pos_and_feature_map[word_id][pos_id] = feature_id;
  }

  std::cout << "Max word length: " << max_word_length << "\n";

  // Register base characters.
  uint32_t char_len = 0;
  for (size_t i = 0; i < kDigit.size(); i += char_len) {
    std::string s = extract_utf8_char(kDigit, i, char_len);
    chars_table.add(s, int(CharKind::KIND_DIGIT));
    if (!word_table.put(s)) {
      std::cerr << "Too many words.\n";
      return false;
    }
  }

  for (size_t i = 0; i < kAlphabet.size(); i += char_len) {
    std::string s = extract_utf8_char(kAlphabet, i, char_len);
    chars_table.add(s, int(CharKind::KIND_ALPHABET));
    if (!word_table.put(s)) {
      std::cerr << "Too many words.\n";
      return false;
    }
  }

  for (size_t i = 0; i < kKatakana.size(); i += char_len) {
    std::string s = extract_utf8_char(kKatakana, i, char_len);
    chars_table.add(s, int(CharKind::KIND_KATAKANA));
    if (!word_table.put(s)) {
      std::cerr << "Too many words.\n";
      return false;
    }
  }

  size_t num_seed_words = word_table.size();
  std::cout << "# of seed words : " << num_seed_words << "\n";

  std::vector<token_and_pos_tag> tokens;

  // 0: word_id, 1: feature_id, 2: word length
  std::vector<std::array<int, 3>> pis;
  std::string sentence;

  // key = word_id, value = (key = feature_id, value = (token_len, count))
  std::map<int, std::map<int, std::pair<int, int>>> word_to_feature_counts;

  for (const auto &line : pos_tagged_lines) {
    if (line.empty()) {
      continue;
    }

    if (line.compare("EOS\n") == 0) {

      std::string prev_pos = "\tBOS";

      // limit # of chars in sentence to the max word length in vocabs 
      std::string sent_truncated = sentence;
      if (sent_truncated.size() > max_word_length) {
        sent_truncated.erase(max_word_length - sentence.size() - 1);
      }

      size_t sent_loc{0};

      for (const auto &tok : tokens) {

        //
        // Example:
        //
        // tokens = ['吾輩', 'は', '猫', 'である']
        // sentence = '吾輩は猫である'
        //
        // pis = 
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

        for (size_t next_char_loc = tok.token_len; next_char_loc < sent_truncated.size(); ) {

          std::string fragment = sent_truncated.substr(0, next_char_loc);

          bool fragment_exists = word_table.has(fragment);

          int fragment_id{-1};
          if (!word_table.put(fragment, fragment_id)) {
            std::cerr << "Failed to add fragment: " << fragment << "\n";
            return false;
          }

          pis.push_back({fragment_id, tok.feature_id, tok.token_len});

          // patten = append feature tag to fragment.
          std::string pattern = fragment + prev_pos;
          int pattern_id{-1};
          if (!word_table.put(pattern, pattern_id)) {
            std::cerr << "Failed to add pattern: " << pattern << "\n";
            return false;
          }

          pis.push_back({pattern_id, tok.feature_id, tok.token_len});

          if (!fragment_exists) { // new pattern
            break;
          }

          next_char_loc += utf8_len(sent_truncated[next_char_loc]);
        }

        int tok_id;
        // first token in sentence.
        std::string token = std::string(&sentence[sent_loc], tok.token_len);
        bool has_word = word_table.get(token, tok_id);

        if ((!has_word || (tok_id > num_seed_words)) && (classify_char_kind(token, chars_table) != CharKind::KIND_DIGIT)) {
          // TODO
          pos_counts[tok.pos_id] += 1;

          std::string pos_str;
          if (!feature_table.get(tok.pos_id, pos_str)) {
            std::cerr << "Internal error: POS string not found for id " << std::to_string(tok.pos_id) << "\n";
            return false;
          }

          std::string feature_str = pos_str + ",*,*,*";
          
          int feature_id{-1};
          if (!feature_table.put(feature_str, feature_id)) {
            std::cerr << "Too many features\n";
            return false;
          }

          // add POS string as word
          int prev_pos_id{-1};
          if (!word_table.put(prev_pos, prev_pos_id)) {
            std::cerr << "Too many words\n";
            return false;
          }

          pis.push_back({prev_pos_id, feature_id, 0});
        }

        for (const auto &it : pis) {

          int word_id{-1};
          if (!word_table.get(it.token, word_id)) {
            std::cerr << "Id not found for word: " << it.token << "\n";
            return false;
          }

          auto &m = word_to_feature_counts[word_id];

          if (!m.count(it.feature_id)) {
            m[it.feature_id] = 
          }

          word_to_feature_counts[it.token]
          it.feature_id
          it.token
        }

        std::string pos_str;
        if (!feature_table.get(tok.pos_id, pos_str)) { 
          std::cerr << "Internal error: POS string not found for id " << std::to_string(tok.pos_id) << "\n";
          return false;
        }
        prev_pos = "\t" + pos_str;

        sent_loc += tok.token_ken;
      }

      tokens.clear();
      sentence.clear();

    } else {
      // Parse POS tagged line: SURAFACE\tFEATURE
      std::vector<std::string> tup = split(line, "\t");
      if (tup.size() != 2) {
        std::cerr << "Invalid POS Tagged line:" << line << "\n";
        return false;
      }

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

      sentence += tok.token;
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
