#include <iostream>
#include "nanohashmap.hh"

int main(int argc, char **argv) {

  (void)argc;
  (void)argv;

  nanotrie::TokenHashMap<char, uint32_t, 256> m;

  if (!m.update('a', 0)) {
    return -1;
  }

  if (!m.update('a', 1)) {
    return -1;
  }

  if (!m.update('b', 2)) {
    return -1;
  }

  uint32_t val;
  if (!m.find('a', val)) {
    return -1;
  }
  std::cout << "a = " << val << "\n";
  if (!m.find('b', val)) {
    return -1;
  }
  std::cout << "b = " << val << "\n";
  if (m.find('c', val)) {
    return -1;
  }

  if (!m.update('a', 3)) {
    return -1;
  }
  if (!m.find('a', val)) {
    return -1;
  }
  std::cout << "a = " << val << "\n";


  std::cout << "ok\n";

  return 0;

}
