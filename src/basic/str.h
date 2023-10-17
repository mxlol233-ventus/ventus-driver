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

#include <array>
#include <sstream>
#include <string>
#include <vector>

namespace vt::basic::str {

namespace details {

template <size_t N>
std::ostream& operator<<(std::ostream& s, const std::array<char, N>& arr) {
  if (arr.empty()) {
    return s;
  }
  s << std::string(arr.begin(), arr.end() - 1);
  return s;
}

template <typename T>
std::ostream& operator<<(std::ostream& s, const std::vector<T>& arr) {
  if (arr.empty()) {
    return s;
  }
  const size_t max = 32;
  size_t max_idx = std::min(max, arr.size());
  s << "[";
  for (size_t i = 0; i < max_idx; i++) {
    if (i < max_idx - 1)
      s << arr[i] << ", ";
    else {
      if (arr.size() > max_idx) {
        s << arr[i] << ", ...";
      } else {
        s << arr[i];
      }
    }
  }
  s << "]";
  return s;
}

struct Empty {
  explicit operator const std::string&() const {
    static std::string empty;
    return empty;
  }
  explicit operator const char*() const { return ""; }
};

inline void str(std::ostream& s) {}

template <typename T>
inline void str(std::ostream& s, const T& t) {
  s << t;
}

template <>
inline void str<Empty>(std::ostream& s, const Empty& /*a*/) {}

template <typename T, typename... Args>
inline void str(std::ostream& s, const T& t, const Args&... args) {
  str(s, t);
  str(s, args...);
}

template <typename T>
struct Canonicalize {
  using type = const T&;
};

template <size_t N>
struct Canonicalize<char[N]> {
  using type = const char*;
};

template <typename... Args>
struct Warper final {
  inline static std::string call(const Args&... args) {
    std::ostringstream ss;
    details::str(ss, args...);
    return ss.str();
  }
};

template <>
struct Warper<std::string> final {
  static const std::string& call(const std::string& str) { return str; }
};

template <>
struct Warper<const char*> final {
  static const char* call(const char* str) { return str; }
};

template <>
struct Warper<> final {
  static Empty call() { return Empty(); }
};

}  // namespace details

template <typename... Args>
inline decltype(auto) from(const Args&... args) {
  return details::Warper<typename details::Canonicalize<Args>::type...>::call(
      args...);
}
}  // namespace vt::basic::str
