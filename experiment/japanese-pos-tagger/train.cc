#include <cstdio>
#include <cstdlib>
#include <limits>
#include <unordered_map>

#define NANOCSV_IMPLEMENTATION
#include "nanocsv.h"


// Zenkaku digit/alpha/katanaka
const std::string kDigit = u8"０１２３４５６７８９〇一二三四五六七八九十百千万億兆京数・";
const std::string kAlphabet = u8"ａｂｃｄｅｆｇｈｉｊｋｌｍｎｏｐｑｒｓｔｕｖｗｘｙｚＡＢＣＤＥＦＧＨＩＪＫＬＭＮ  ＯＰＱＲＳＴＵＶＷＸＹＺ＠：／．";
const std::string kKatakana = u8"ァアィイゥウェエォオカガキギクグケゲコゴサザシジスズセゼソゾタダチヂッツヅテデトドナニヌネノハバパヒビピフブプヘベペホボポマミムメモャヤ  ュユョヨラリルレロヮワヰヱヲンヴヵヶヷヸヹヺーヽヾヿァアィイゥウェエォオカガキギクグケゲコゴサザシジスズセゼソゾタダチヂッツヅテデトドナニヌネノハバパヒビピフブプヘベペホボポマミムメモャヤ  ュユョヨラリルレロヮワヰヱヲンヴヵヶヷヸヹヺーヽヾヿ";

// Char kind is used to assist detecting word boundary.
enum class CharKind {
  KIND_DIGIT = 0,
  KIND_ALPHABET = 1,
  KIND_KATAKANA = 2,
  KIND_OTHER = 3, // Kanji, hiragana, emoji, etc.
};

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



// Simple String <-> Id, Id -> String map
struct StrIdMap
{
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

  void add(const std::string &str, int val) {
    str_to_id[str] = val;
    id_to_str[val] = str;
  }

  bool has(const std::string &str) const {
    return str_to_id.count(str);
  }

  bool has(const int id) const {
    return id_to_str.count(id);
  }

  bool get(const std::string &str, int &result) const {
    if (str_to_id.count(str)) {
      result = str_to_id.at(str);
      return true;
    }

    return false;
  }

  bool get(int &id, std::string &result) const {
    if (id_to_str.count(id)) {
      result = id_to_str.at(id);
      return true;
    }

    return false;
  }
  
  size_t size() const {
    return str_to_id.size();
  }
  
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

inline std::string join(const std::vector<std::string> &strs, const size_t s_idx, const size_t e_idx, const char delimiter = ',', const char quote = '"') {

  if (s_idx >= e_idx) {
    return std::string();
  }

  if (s_idx >= strs.size()) {
    return std::string();
  }

  if (e_idx > strs.size()) {
    return std::string();
  }

  for (size_t i = s_idx; i < e_idx; i++
  

}

// Support quoted string'\"' (do not consider `delimiter` character in quoted string)
// delimiter must be a ASCII char.
// quote_char must be a single UTF-8 char.
inline std::vector<std::string> parse_line(const char *p, const size_t len, const char delimiter = ',', const char *quote_char = "\"")
{
  std::vector<std::string> tokens;

  if (len == 0) {
    return tokens;
  }

  size_t quote_size = utf8_len(uint8_t(quote_char[0]));

  bool in_quoted_string = false;
  size_t s_start = 0;

  const char *curr_p = p;

  for (size_t i = 0; i < len; i += utf8_len(uint8_t(*curr_p))) {

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
      //std::cout << "s_start = " << s_start << ", (i-1) = " << i-1 << "\n";
      //std::cout << "p[i] = " << p[i] << "\n";
      if (s_start < i) {
        std::string tok(p + s_start, i - s_start);

        tokens.push_back(tok);
      } else {
        // Add empty string
        tokens.push_back(std::string());
      }

      s_start = i + 1; // next to delimiter char
    }
  }

  // the remainder
  //std::cout << "remain: s_start = " << s_start << ", len - 1 = " << len-1 << "\n";

  if (s_start <= (len - 1)) {
    std::string tok(p + s_start, len - s_start);
    tokens.push_back(tok);
  }

  return tokens;
}

// 3 = Mecab dict.
bool train(const std::vector<std::string> &lines,
  const char delimiter, const uint32_t num_pos_field_skips = 3) {

  StrIdMap chars_table;
  StrIdMap feature_table;
  StrIdMap word_table;
  
  for (const auto &it : lines) {
    std::vector<std::string> fields = parse_line(it.c_str(), it.size(), delimiter);

    if (fields.size() < num_pos_field_skips + 1 + 1) {
      std::cerr << "Invalid line: " << it << "\n";
      return false;
    }

    const std::string &surface = fields[0]; 
    const std::string &pos = fields[num_pos_field_skips + 1]; // e.g. '動詞', '助詞'

    const std::string feature = join(fields, num_pos_field_skips, fields.size(), ',', '"');


    std::vector<std::string> tags = parse_feature(feature.c_str(), feature.size(), delimiter);
    if (tags.size() < (num_pos_field_skips + 1)) {
    }

    feature_table.put(surface);

  }

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
  bool ret = nanocsv::ParseTextCSVFromFile(vocab_filename, csv_option, &csv, &warn, &err);
  if (warn.size()) {
    std::cout << "CSV read warn: " << warn << "\n";
  }
  if (!ret) {
    std::cerr << "CSV read err: " << err << "\n";
    exit(-1);
  }

  std::cout << "# of rows = " << csv.num_records << ", # of columns = " << csv.num_fields << "\n";
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
