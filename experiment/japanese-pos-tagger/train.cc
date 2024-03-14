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
