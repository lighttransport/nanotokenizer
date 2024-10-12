#include "nanopretokenizer.hh"

#ifdef TEST_MAIN

int main(int argc, char **argv)
{
  std::vector<size_t> offsets{};
  std::string input = " Hello World 123@\nbora.";
  std::vector<size_t> ret;
  ret = nanotokenizer::pretokenize_qwen2(input, offsets);

}
#endif
