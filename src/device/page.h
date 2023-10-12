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

#include <cstdio>
#include <cstring>
#include <list>
#include <memory>
#include "basic/exception.h"
#include "sv39.h"

namespace vt {
class Block;
class Page {
 public:
  friend class Block;

  Page(uint64_t size) { m_data_ = new uint8_t[size](); }
  ~Page() {
    if (m_data_ != nullptr)
      delete[] m_data_;
  }

 private:
  uint8_t* m_data_ = nullptr;
};

class Block {
 public:
  Block(uint64_t addr, uint64_t size, bool create_pages = true)
      : m_addr_(addr), m_size_(size) {
    if (create_pages) {
      uint64_t num = (size + SV39::PageSize - 1) / SV39::PageSize;
      m_pages_ = std::make_unique<Page>(num * SV39::PageSize);
    }
  }

  Block(const Block& blk)
      : m_addr_(blk.m_addr_), m_size_(blk.m_size_), m_pages_(nullptr) {
    if (blk.m_pages_ != nullptr) {
      auto num = (m_size_ + SV39::PageSize - 1) / SV39::PageSize;
      m_pages_ = std::make_unique<Page>(num * SV39::PageSize);
      memcpy(m_pages_->m_data_, blk.m_pages_->m_data_, num * SV39::PageSize);
    }
  }

  Block(Block&& blk)
      : m_addr_(blk.m_addr_),
        m_size_(blk.m_size_),
        m_pages_(std::move(blk.m_pages_)) {}

  ~Block() {}

  bool operator<(const Block& another) const {
    return this->end() <= another.m_addr_;
  }
  bool operator>(const Block& another) {
    return this->m_addr_ >= another.end();
  }
  bool operator<(const uint64_t& pos) const {
    uint64_t end = this->m_addr_ + this->m_size_;
    return end <= pos;
  }
  bool operator>(const uint64_t& pos) const { return this->m_addr_ > pos; }
  bool operator==(const uint64_t& pos) const {
    return this->m_addr_ <= pos && (this->m_addr_ + this->m_size_) > pos;
  }
  Block& operator=(Block&& blk) noexcept {
    if (this != &blk) {
      m_addr_ = blk.m_addr_;
      m_size_ = blk.m_size_;
      m_pages_ = std::move(blk.m_pages_);
    }
    return *this;
  }

  uint64_t end() const { return m_addr_ + m_size_; }

  int write(uint64_t base, uint64_t length,
            const void* data) {  // 0 <= base < size
    if (base + length > m_size_) {
      printf("Input 0x%016lx + 0x%016lx is too long for block @0x%016lx\n",
             base, length, m_addr_);
      return -1;
    }
    uint8_t* in = (uint8_t*)data;
    auto sz = std::min(length, m_size_ - base);
    memcpy(m_pages_->m_data_ + base, in, sz);
    return 0;
  }
  int read(uint64_t base, uint64_t length, void* data) {
    if (base + length > m_size_) {
      printf("Input 0x%016lx + 0x%016lx is too long for block @0x%016lx\n",
             base, length, m_addr_);
      return -1;
    }
    uint8_t* out = (uint8_t*)data;
    auto sz = std::min(length, m_size_ - base);
    memcpy(out, m_pages_->m_data_ + base, sz);
    return 0;
  }

  uint64_t addr() const { return m_addr_; }
  uint64_t size() const { return m_size_; }
  bool empty() const { return m_pages_ == nullptr; }

 private:
  std::unique_ptr<Page> m_pages_{nullptr};
  uint64_t m_addr_{0};
  uint64_t m_size_{0};
};

class PhysicalMemory {
 public:
  PhysicalMemory(uint64_t range) {
    range = (range <= SV39::MaxPhyRange) ? range : SV39::MaxPhyRange;
    m_max_range_ =
        (range + SV39::PageSize - 1) / SV39::PageSize * SV39::PageSize;
    m_blocks_.emplace_back(0, 0x20000000, false);
    m_current_block_ = m_blocks_.begin();
    m_blocks_.emplace_back(m_max_range_, SV39::PageSize, false);
  }
  PhysicalMemory(const PhysicalMemory& p) {
    if (!p.m_blocks_.empty()) {
      for (auto it : p.m_blocks_) {
        m_blocks_.push_back(it);
      }
      m_max_range_ = p.m_max_range_;
      m_current_block_ = this->m_blocks_.begin();
    }
  }

  ~PhysicalMemory() {}

  template <typename... Args>
  int insert(Args&&... args) {
    Block blk(std::forward<Args>(args)...);
    if (*m_current_block_ > blk) {
      while (m_current_block_ != m_blocks_.begin()) {
        m_current_block_--;
        if (*m_current_block_ < blk) {
          m_blocks_.emplace(std::next(m_current_block_), std::move(blk));
          return 0;
        }
      }
    } else if (*m_current_block_ < blk) {
      while (m_current_block_ != m_blocks_.end()) {
        m_current_block_++;
        if (*m_current_block_ > blk) {
          m_blocks_.emplace(m_current_block_, std::move(blk));
          return 0;
        }
      }
    } else {
      printf("Block address conflict: 0x%016lx\n", blk.addr());
      return -1;
    }
    printf("Insert failed by unknown reason\n");
    return -1;
  }
  int remove(uint64_t addr) {
    for (auto iter = m_blocks_.begin(); iter != m_blocks_.end(); iter++) {
      if (iter->addr() <= addr && iter->addr() + iter->size() > addr) {
        m_blocks_.erase(iter);
        return 0;
      }
      if (iter->addr() > addr)
        return -1;
    }
    return -1;
  }

  int write_data(uint64_t addr, uint64_t length, const void* data) {
    for (auto& blk : m_blocks_) {
      if (addr >= blk.addr() && addr + length <= blk.addr() + blk.size()) {
        if (blk.empty()) {
          printf("No pages in this block: 0x%016lx\n", addr);
          return -1;
        }
        blk.write(addr - blk.addr(), length, data);
        return 0;
      }
    }
    printf("Invalid address or too long data length: 0x%016lx\n", addr);
    return -1;
  }

  template <class T>
  int write_value(uint64_t addr, const T in) {
    T dat = in;
    return write_data(addr, sizeof(T), &dat);
  }

  template <class T>
  int write(uint64_t addr, uint64_t num, const T* in,
            const void* mask = nullptr) {
    if (mask == nullptr) {
      write_data(addr, num * sizeof(T), in);
    } else {
      for (uint64_t i = 0; i < num; i++) {
        uint8_t maskbit = (((uint8_t*)mask)[i / 8] >> (i % 8)) & '\x01';
        if (maskbit == '\x01') {
          write_value<T>(addr + i * sizeof(T), in[i]);
        }
      }
    }
    return 0;
  }

  int read_data(uint64_t addr, uint64_t length, void* data) {
    for (auto& iter : m_blocks_) {
      if (addr >= iter.addr() && addr + length <= iter.addr() + iter.size()) {
        if (iter.empty()) {
          printf("No pages in this block: 0x%016lx\n", addr);
          return -1;
        }
        iter.read(addr - iter.addr(), length, data);
        return 0;
      }
    }
    printf("Invalid address or too long data length: 0x%016lx\n", addr);
    return -1;
  }

  template <class T>
  T read_value(uint64_t addr) {
    T out;
    int flag = read_data(addr, sizeof(T), &out);
    if (!flag)
      return out;
    else
      return 0;
  }

  template <class T>
  int read(uint64_t addr, uint64_t num, T* data, const void* mask = nullptr) {
    read_data(addr, num * sizeof(T), data);
    if (mask != nullptr) {
      for (uint64_t i = 0; i < num; i++) {
        uint8_t maskbit = (((uint8_t*)mask)[i / 8] >> (i % 8)) & '\x01';
        if (maskbit == '\x00') {
          data[i] = 0;
        }
      }
    }
    return 0;
  }

  uint64_t usable(uint64_t size) {
    size = (size + SV39::PageSize - 1) / SV39::PageSize * SV39::PageSize;
    uint64_t usable = 0ull;
    for (auto iter = m_blocks_.begin(); std::next(iter) != m_blocks_.end();
         iter++) {
      usable = usable <= iter->end() ? iter->end() : usable;
      if (*std::next(iter) > usable + size - 1) {
        if (usable + size <= m_max_range_) {
          return usable;
        } else {
          break;
        }
      }
    }
    return 0ull;
  }

  uint64_t length(uint64_t addr) {
    uint64_t length = 0ull;
    for (const auto& block : m_blocks_) {
      if (block == addr) {
        length = block.size();
        break;
      }
    }
    return length;
  }

 private:
  uint64_t m_max_range_;
  std::list<Block> m_blocks_;
  std::list<Block>::iterator m_current_block_;
};

}  // namespace vt
