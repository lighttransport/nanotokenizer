EXTRA_CXXFLAGS = -fsanitize=address

all:
	clang++ -o example_rwkv_world -g -O1 -std=c++17 $(EXTRA_CXXFLAGS) -DRWKV_ENABLE_EXCEPTION rwkv_world_tokenizer_example.cc rwkv_world_tokenizer.cc
