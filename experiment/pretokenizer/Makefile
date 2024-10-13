all:
	clang++ -g -DTEST_MAIN=1 nanopretokenizer.cc unicode-data.cc unicode-util.cc

fuzz:
	clang++ -g -O2 -fsanitize=address,fuzzer -DFUZZ_MAIN=1 nanopretokenizer.cc unicode-data.cc unicode-util.cc
