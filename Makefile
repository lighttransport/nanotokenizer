all:
	clang++ -std=c++17 -DRWKV_ENABLE_EXCEPTION rwkv_world_tokenizer_example.cc rwkv_world_tokenizer.cc
