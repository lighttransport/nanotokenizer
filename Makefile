#EXTRA_CXXFLAGS=-fsanitize=address
EXTRA_CXXFLAGS=

all:
	clang++ -Wall -o example_rwkv_world -g -O2 -std=c++14 $(EXTRA_CXXFLAGS) -DRWKV_ENABLE_EXCEPTION rwkv_world_tokenizer_example.cc
