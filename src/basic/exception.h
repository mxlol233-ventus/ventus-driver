/*
* MIT License
* Copyright (c) 2023 mxlol233 (mxlol233@outlook.com)

* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:

* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.

* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include "str.h"
#include "tui.h"

namespace vz::basic::exception {

class Error : public std::exception {
 public:
  explicit Error(std::string&& msg) : msg_(std::move(msg)) {
    msg_.erase(std::remove(msg_.begin(), msg_.end(), 0), msg_.end());
  }
  [[nodiscard]] const char* what() const noexcept override {
    return msg_.c_str();
  }

 private:
  std::string msg_;
};
namespace details {

template <typename... Args>
std::string msg(const char* tag_name, const char* file_name, uint32_t line,
                const char* func_name, const Args&... args) {
  std::string msg;
  if (std::strlen(tag_name) == 0) {
    msg = tui::color::Shell::colorize(
        vz::basic::str::from("[File:", file_name,
                             ","
                             "Line:",
                             line, ", Func: ", func_name, "]: "),
        vz::basic::tui::color::Shell::fgreen());

  } else {
    auto m0 = tui::color::Shell::colorize(
        vz::basic::str::from(tag_name), vz::basic::tui::color::Shell::fblue());

    auto m1 = tui::color::Shell::colorize(
        vz::basic::str::from(", File:", file_name,
                             ","
                             "Line:",
                             line, ", Func: ", func_name, "]: "),
        vz::basic::tui::color::Shell::fgreen());
    msg = tui::color::Shell::colorize(vz::basic::str::from("[Tag:", m0, m1),
                                      vz::basic::tui::color::Shell::fgreen());
  }

  msg = vz::basic::str::from(msg, args...);
  return msg;
}
}  // namespace details

template <typename... Args>
inline void warning(const char* tag_name, const char* file_name, uint32_t line,
                    const char* func_name, const Args&... args) {

  std::cout << details::msg<Args...>(tag_name, file_name, line, func_name,
                                     args...)
            << std::endl;
}

template <typename... Args>
inline void note(const char* tag_name, const char* file_name, uint32_t line,
                 const char* func_name, const Args&... args) {

  std::cout << details::msg<Args...>(tag_name, file_name, line, func_name,
                                     args...)
            << std::endl;
}

template <typename... Args>
inline void fail(const char* tag_name, const char* file_name, uint32_t line,
                 const char* func_name, const Args&... args) {
  throw Error(
      details::msg<Args...>(tag_name, file_name, line, func_name, args...));
}

inline void do_assert(bool condition, const char* tag_name,
                      const char* file_name, uint32_t line,
                      const char* func_name, std::string&& msg) {
  if (!condition)
    throw Error(details::msg(tag_name, file_name, line, func_name, msg));
}

}  // namespace vz::basic::exception

#define VZ_CHECK_ERROR(TagName, COND, ...)                            \
  if (!(COND)) {                                                      \
    vz::basic::exception::fail(                                       \
        TagName, __FILE__, __LINE__, __func__,                        \
        vz::basic::tui::color::Shell::colorize(                       \
            " [`" #COND "`] ", vz::basic::tui::color::Shell::fred()), \
        "not true. ", ##__VA_ARGS__);                                 \
  }

#define VZ_CHECK_WARNING(TagName, COND, ...)                          \
  if (!(COND)) {                                                      \
    vz::basic::exception::warning(                                    \
        vz::basic::tui::color::Shell::colorize(                       \
            TagName, __FILE__, __LINE__, __func__, " [`" #COND "`] ", \
            vz::basic::tui::color::Shell::fyellow()),                 \
        "not true. ", ##__VA_ARGS__);                                 \
  }

#define VZ_ERROR(TagName, ...)                                   \
  {                                                              \
    vz::basic::exception::fail(                                  \
        TagName, __FILE__, __LINE__, __func__,                   \
        vz::basic::tui::color::Shell::colorize(                  \
            " [ERROR]: ", vz::basic::tui::color::Shell::fred()), \
        ##__VA_ARGS__);                                          \
  }

#define VZ_WARNING(TagName, ...)                                    \
  {                                                                 \
    vz::basic::exception::warning(                                  \
        TagName, __FILE__, __LINE__, __func__,                      \
        vz::basic::tui::color::Shell::colorize(                     \
            " [ERROR]: ", vz::basic::tui::color::Shell::fyellow()), \
        ##__VA_ARGS__);                                             \
  }

#define VZ_NOTE(TagName, ...)                                         \
  {                                                                   \
    vz::basic::exception::note(TagName, __FILE__, __LINE__, __func__, \
                               " [NOTE]: ", ##__VA_ARGS__);           \
  }

#define unreachable(TagName) VZ_ERROR(TagName, "unreachable")

#define vz_assert(TagName, COND)                                          \
  vz::basic::exception::do_assert((COND), TagName, __FILE__, __LINE__,    \
                                  __func__,                               \
                                  vz::basic::tui::color::Shell::colorize( \
                                      " [ASSERT FAILED]: [`" #COND "`] ", \
                                      vz::basic::tui::color::Shell::fred()));
