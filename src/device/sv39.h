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

#include <cstdint>

namespace vt {

// https://rcore-os.cn/rCore-Tutorial-Book-v3/chapter4/3sv39-implementation-1.html
class SV39 {
 public:
  constexpr static uint64_t MaxPhyRange = (1ull << 56ull);
  constexpr static uint64_t PageBits = 12ull;
  constexpr static uint64_t PageSize = 1ull << PageBits;  // 4KiB
  constexpr static uint64_t PageElem = 512ull;
  constexpr static uint64_t MegaPageSize = 1ull << 21ull;  // 2MiB
  constexpr static uint64_t GigaPageSize = 1ull << 30ull;  // 1GiB
  constexpr static uint64_t VALowerCeil = 0x0000004000000000;
  constexpr static uint64_t VAUpperFloor = 0xffffffc000000000;

  constexpr static uint64_t V = 1ull;
  constexpr static uint64_t R = 2ull;
  constexpr static uint64_t W = 4ull;
  constexpr static uint64_t X = 8ull;
  constexpr static uint64_t U = 16ull;
  constexpr static uint64_t G = 32ull;
  constexpr static uint64_t A = 64ull;
  constexpr static uint64_t D = 128ull;

  static uint64_t VAextract(uint64_t VA, uint32_t level) {
    if (level == 0)
      return (VA >> 30ull) & 0x1ffull;
    else if (level == 1)
      return (VA >> 21ull) & 0x1ffull;
    else
      return (VA >> 12ull) & 0x1ffull;
  }
  static bool VAcheck(uint64_t VA) {
    if (VA >= VALowerCeil && VA < VAUpperFloor)
      return false;
    else
      return true;
  }
  static uint64_t PTE2PA(uint64_t PTE) {
    return (PTE & 0x003ffffffffffc00ull) << 2ull;
  }
  static uint64_t SetPTE(uint64_t PA, uint64_t mods) {
    return ((PA & 0x00fffffffffff000ull) >> 2ull) | mods;
  }
};
}  // namespace vt
