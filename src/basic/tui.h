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

#include <sstream>

namespace vz::basic {
namespace tui {
namespace color {
enum Code {
  kFgRed = 31,
  kFgGreen = 32,
  kFgYellow = 33,
  kFgBlue = 34,
  kFgDefault = 39,
  kBgRed = 41,
  kBgGreen = 42,
  kBgBlue = 44,
  kBgDefault = 49
};
class Shell {
  Code code_;

 public:
  [[nodiscard]] Code code() const { return code_; }
  explicit Shell(Code pCode) : code_(pCode) {}
  explicit Shell() : code_(kFgDefault) {}
  static Shell fred() { return Shell(Code::kFgRed); }
  static Shell fgreen() { return Shell(Code::kFgGreen); }
  static Shell fyellow() { return Shell(Code::kFgYellow); }
  static Shell fblue() { return Shell(Code::kFgBlue); }
  static Shell bred() { return Shell(Code::kBgRed); }
  static Shell bgreen() { return Shell(Code::kBgGreen); }
  static Shell bblue() { return Shell(Code::kBgBlue); }
  static Shell bdefault() { return Shell(Code::kBgDefault); }
  static Shell fdefault() { return Shell(Code::kFgDefault); }
  static std::string colorize(std::string&& a, Shell&& color) {
    std::ostringstream ss;
    ss << "\033[" << color.code() << "m" << std::move(a) << "\033[0m";
    return ss.str();
  }
};

}  // namespace color
}  // namespace tui

namespace str::details {
inline std::ostream& operator<<(std::ostream& os,
                                const tui::color::Shell& mod) {
  return os << "\033[" << mod.code() << "m";
}

}  // namespace str::details

}  // namespace vz::basic
