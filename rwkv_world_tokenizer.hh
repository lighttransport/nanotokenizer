// SPDX-License-Identifier: Apache 2.0

/*!
 *  Copyright (c) 2023 by Contributors daquexian
 *  Modification by (C) 2024 - Present, Light Transport Entertainment Inc.
 * \file rwkv_world_tokenizer.h
 * \brief Implementation of llm chat.
 */
#ifndef RWKV_WORLD_TOKENIZER_H_
#define RWKV_WORLD_TOKENIZER_H_

#if defined(RWKV_ENABLE_EXCEPTION)
#include <exception>
#include <stdexcept>
#endif
#include <sstream>
#include <string>

#define STRINGIFY(...) STRINGIFY_(__VA_ARGS__)
#define STRINGIFY_(...) #__VA_ARGS__

#if defined(RWKV_ENABLE_EXCEPTION)

#define RV_CHECK(...)                                                         \
  for (bool _rv_check_status = (__VA_ARGS__); !_rv_check_status;)             \
  FRException() << ("Check \"" STRINGIFY(__VA_ARGS__) "\" failed at " + \
                          std::to_string(__LINE__) + " in " __FILE__ "\n  > Error msg: ")

struct FRException : public std::runtime_error {
  FRException() : std::runtime_error("") {}
  const char* what() const noexcept override { return msg.c_str(); }
  template <typename T>
  FRException& operator<<(const T& s) {
    std::stringstream ss;
    ss << s;
    msg += ss.str();
    return *this;
  }
  std::string msg;
};
#else

#define RV_CHECK(...)                                                         \
  for (bool _rv_check_status = (__VA_ARGS__); !_rv_check_status;)             \
  _err_ss << ("Check \"" STRINGIFY(__VA_ARGS__) "\" failed at " + \
                          std::to_string(__LINE__) + " in " __FILE__ "\n  > Error msg: ")
#endif


#endif  // RWKV_WORLD_TOKENIZER_H_
