#define NANOCSV_IMPLEMENTATION
#include "nanocsv.h"

int main(int argc, char **argv) {

  std::string vocab_filename = "vocab.csv";

  if (argc > 1) {
    vocab_filename = argv[1];
  }

  nanocsv::ParseTextOption csv_option;
  csv_option.delimiter = ',';
  nanocsv::TextCSV csv;
  std::string warn, err;

  bool ret = nanocsv::ParseTextCSVFromFile(vocab_filename, csv_option, &csv, &warn, &err);
  if (warn.size()) {
    std::cout << "CSV read warn: " << warn << "\n";
  }

  if (!ret) {
    std::cerr << "CSV read err: " << err << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}


