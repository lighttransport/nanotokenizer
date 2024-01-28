# Nanoscale tokenizer in C++

Nanoscale tokenizer in C++.
Currently RWKV world tokenizer is implemented.

## Features

* Easy to embed
* Read vocab from JSON(through minijson)

### Additional feature to RWKV world tokenizer

* UTF-8 fallback(with fallback token_id. default 65530)

## TODO

* [ ] Make C++ Exception free

## Third party libraries

* minijson : MIT license https://github.com/syoyo/minijson
* hat-trie: MIT license https://github.com/Tessil/hat-trie
* rwkv_world_tokenizer : Apache 2.0 license https://github.com/mlc-ai/tokenizers-cpp
* rwkv_vocab_v20230424.json : Not sure, but would be Apache 2.0 also. https://github.com/BlinkDL/ChatRWKV
  
