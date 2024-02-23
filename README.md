# Nanoscale tokenizer in C++

Nanoscale tokenizer in C++.
Currently RWKV world tokenizer is implemented.

## Features

* Easy to embed
* Read vocab from JSON(through minijson)

## Variants

* Naiive Trie tree implementation : rwkv_world_tokenizer_trie.hh
* Efficient version using hat-trie : rwkv_world_tokenizer_hat.hh
* Efficient version using cedar : rwkv_world_tokenizer_cedar.hh

If you want to run tokenizer with no C++ exception(e.g. WASM), naiive or cedar version recommended to use.

### Additional feature to original RWKV world tokenizer.

* [x] UTF-8 byte fallback

## TODO

* [ ] Make C++ Exception free

## Third party libraries

* minijson : MIT license https://github.com/syoyo/minijson
* hat-trie: MIT license https://github.com/Tessil/hat-trie
* rwkv_world_tokenizer : Apache 2.0 license https://github.com/mlc-ai/tokenizers-cpp
* rwkv_vocab_v20230424.json : Not sure, but would be Apache 2.0 also. https://github.com/BlinkDL/ChatRWKV
* cedar/ccedar_core.h : GPLv2, LGPLv2.1 or BSD license. https://www.tkl.iis.u-tokyo.ac.jp/~ynaga/cedar/

