#include <cstdio>
#include <cstdlib>

#define NANOCSV_IMPLEMENTATION
#include "nanocsv.h"


int main(int argc, char **argv) {
  if (argc < 3) {
    std::cout << "Need input.vocab(csv) train.txt(POS tagged)\n";
    exit(-1);
  }

  std::string vocab_filename = argv[1];
  std::string pos_tagged_filename = argv[2];

  nanocsv::ParseOptionStr csv_option;
  nanocsv::StrCSV csv;
  std::string warn;
  std::string err;
  bool ret = nanocsv::ParseStrCSVFromFile(vocab_filename, csv_option, &csv, &warn, &err);
  if (warn.size()) {
    std::cout << "CSV read warn: " << warn << "\n";
  }
  if (!ret) {
    std::cerr << "CSV read err: " << err << "\n";
    exit(-1);
  }

  return 0;
}
