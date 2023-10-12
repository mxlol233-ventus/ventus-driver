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

#include <functional>
#include <memory>
#include "page.h"

namespace vt {

class Memory {
 public:
  Memory(uint64_t max_range) {
    m_pm_ = std::make_unique<PhysicalMemory>(max_range);
  }

  ~Memory() {}
  uint64_t create_root_pt() {
    uint64_t root = m_pm_->usable(SV39::PageSize);
    if (root != 0)
      m_pm_->insert(root, SV39::PageSize, true);
    return root;
  }
  uint64_t allocate(uint64_t root, uint64_t v_addr, uint64_t length) {
    length = (length + SV39::PageSize - 1) / SV39::PageSize * SV39::PageSize;
    uint64_t p_addr = m_pm_->usable(length);
    if (p_addr == 0ull)
      return 0ull;
    if (m_pm_->insert(p_addr, length, true)) {
      return 0ull;
    }
    uint64_t pt_idx[3] = {~0ull, ~0ull, ~0ull};
    for (uint64_t iter = 0ull; iter < length; iter += SV39::PageSize) {
      // level-0
      if (SV39::VAextract((v_addr + iter), 0) != pt_idx[0]) {
        pt_idx[0] = SV39::VAextract((v_addr + iter), 0);
        if ((m_pm_->read_value<uint64_t>(root + pt_idx[0] * sizeof(uint64_t)) &
             SV39::V) == 0ull) {
          auto pt1_addr = m_pm_->usable(SV39::PageSize);
          if (!pt1_addr) {
            return 0;
          }
          m_pm_->insert(pt1_addr, SV39::PageSize, true);
          uint64_t root_pt_entry = SV39::SetPTE(pt1_addr, SV39::V);
          m_pm_->write_value<uint64_t>(root + pt_idx[0] * sizeof(uint64_t),
                                       root_pt_entry);
        }
      }

      uint64_t pt1_addr = SV39::PTE2PA(
          m_pm_->read_value<uint64_t>(root + pt_idx[0] * sizeof(uint64_t)));

      // level-1
      if (SV39::VAextract(v_addr + iter, 1) != pt_idx[1]) {
        pt_idx[1] = SV39::VAextract((v_addr + iter), 1);
        if ((m_pm_->read_value<uint64_t>(pt1_addr +
                                         pt_idx[1] * sizeof(uint64_t)) &
             SV39::V) == 0ull) {
          auto pt2_addr = m_pm_->usable(SV39::PageSize);
          if (!pt2_addr) {
            return 0;
          }
          m_pm_->insert(pt2_addr, SV39::PageSize, true);
          uint64_t pt1_entry = SV39::SetPTE(pt2_addr, SV39::V);
          m_pm_->write_value<uint64_t>(pt1_addr + pt_idx[1] * sizeof(uint64_t),
                                       pt1_entry);
        }
      }

      uint64_t pt2_addr = SV39::PTE2PA(
          m_pm_->read_value<uint64_t>(pt1_addr + pt_idx[1] * sizeof(uint64_t)));

      // level-2
      if (SV39::VAextract(v_addr + iter, 2) != pt_idx[2]) {
        pt_idx[2] = SV39::VAextract((v_addr + iter), 2);
        if ((m_pm_->read_value<uint64_t>(pt2_addr +
                                         pt_idx[2] * sizeof(uint64_t)) &
             SV39::V) == 0ull) {
          uint64_t pt2_entry = SV39::SetPTE(
              p_addr + iter, SV39::R | SV39::W | SV39::X | SV39::V);
          m_pm_->write_value<uint64_t>(pt2_addr + pt_idx[2] * sizeof(uint64_t),
                                       pt2_entry);
        }
      }
    }
    return p_addr;
  }
  // virtual address -> physical address
  uint64_t convert(uint64_t root, uint64_t v_addr) {
    auto pt1_addr = SV39::PTE2PA(m_pm_->read_value<uint64_t>(
        root + SV39::VAextract(v_addr, 0) * sizeof(uint64_t)));
    if (!pt1_addr)
      return 0ull;
    auto pt2_addr = SV39::PTE2PA(m_pm_->read_value<uint64_t>(
        pt1_addr + SV39::VAextract(v_addr, 1) * sizeof(uint64_t)));
    if (!pt2_addr)
      return 0ull;
    auto p_addr = SV39::PTE2PA(m_pm_->read_value<uint64_t>(
        pt2_addr + SV39::VAextract(v_addr, 2) * sizeof(uint64_t)));
    if (!p_addr)
      return 0ull;
    return p_addr | (v_addr & 0xfffull);
  }

  template <typename PtrType>
  int act_virt(uint64_t root, uint64_t v_addr, uint64_t size, PtrType addr,
               const std::function<void(uint64_t, uint64_t, PtrType)>& action) {
    uint64_t vpn = 0ull;
    uint64_t len = 0ull;
    uint64_t p_addr = convert(root, v_addr);
    if (!p_addr) {
      return -1;
    }
    for (uint64_t it = 0ull; it < size; it++) {
      if (vpn != ((v_addr + it) & 0x0000007ffffff000ull)) {
        if (len) {
          action(p_addr, len, (uint8_t*)addr + it - len);
          p_addr = convert(root, v_addr + it);
        }
        len = 0ull;
        vpn = (v_addr + it) & 0x0000007ffffff000ull;
      }
      if (!p_addr)
        return -1;
      len++;
    }
    action(p_addr, len, (uint8_t*)addr + size - len);
    return 0;
  }

  // read data from virtual address
  int read_virt(uint64_t root, uint64_t v_addr, uint64_t size, void* out) {
    return act_virt<void*>(root, v_addr, size, out,
                           [this](uint64_t addr, uint64_t len, void* p) {
                             m_pm_->read_data(addr, len, p);
                           });
  }

  // read data from physical address
  int read_phy(uint64_t p_addr, uint64_t length, void* data) {
    m_pm_->read_data(p_addr, length, data);
    return 0;
  }
  template <class T>
  T read_phy(uint64_t p_addr) {
    return m_pm_->read_value<T>(p_addr);
  }
  template <class T>
  int read_phy(uint64_t p_addr, uint64_t num, T* data,
               const void* mask = nullptr) {
    m_pm_->read<T>(p_addr, num, data, mask);
    return 0;
  }

  // write data by virtual address
  int write_virt(uint64_t root, uint64_t v_addr, uint64_t size,
                 const void* in) {
    return act_virt<const void*>(
        root, v_addr, size, in,
        [this](uint64_t addr, uint64_t len, const void* p) {
          m_pm_->write_data(addr, len, p);
        });
  }

  // write data by physical address
  int write_phy(uint64_t p_addr, uint64_t length, const void* data) {
    return m_pm_->write_data(p_addr, length, data);
  }
  template <class T>
  int write_phy(uint64_t p_addr, const T in) {
    return m_pm_->write_value<T>(p_addr, in);
  }
  template <class T>
  int write_phy(uint64_t p_addr, uint64_t num, const T* in,
                const void* mask = nullptr) {
    return m_pm_->write<T>(p_addr, num, in, mask);
  }

  int release(uint64_t root, uint64_t v_addr) {
    uint64_t length = 0;
    uint64_t p_addr = convert(root, v_addr);
    if (!m_pm_->length(p_addr)) {
      return -1;
    }

    m_pm_->remove(p_addr);

    uint64_t pt_idx[3] = {~0ull, ~0ull, ~0ull};
    for (uint64_t iter = 0ull; iter < length; iter += SV39::PageSize) {
      auto pt1_addr = SV39::PTE2PA(m_pm_->read_value<uint64_t>(
          root + SV39::VAextract(v_addr, 0) * sizeof(uint64_t)));
      auto pt2_addr = SV39::PTE2PA(m_pm_->read_value<uint64_t>(
          pt1_addr + SV39::VAextract(v_addr, 1) * sizeof(uint64_t)));
      pt_idx[2] = SV39::VAextract(v_addr + iter, 2);
      m_pm_->write_value<uint64_t>(pt2_addr + pt_idx[2] * sizeof(uint64_t),
                                   0ull);
    }
    return 0;
  }

  uint64_t clean_pt(uint64_t root) {
    for (uint64_t it0 = 0; it0 < SV39::PageSize; it0 += sizeof(uint64_t)) {
      if ((m_pm_->read_value<uint64_t>(root + it0) & SV39::V) == 0ull)
        continue;
      bool pt1_empty = true;
      auto pt1_addr = SV39::PTE2PA(m_pm_->read_value<uint64_t>(root + it0));
      for (uint64_t it1 = 0; it1 < SV39::PageSize; it1 += sizeof(uint64_t)) {
        if ((m_pm_->read_value<uint64_t>(pt1_addr + it1) & SV39::V) == 0ull)
          continue;
        bool pt2_empty = true;
        auto pt2_addr =
            SV39::PTE2PA(m_pm_->read_value<uint64_t>(pt1_addr + it1));
        for (uint64_t it2 = 0; it2 < SV39::PageSize; it2 += sizeof(uint64_t)) {
          if ((m_pm_->read_value<uint64_t>(pt2_addr + it2) & SV39::V) == 0ull)
            continue;
          else {
            pt2_empty = false;
            break;
          }
        }
        if (pt2_empty) {
          m_pm_->remove(pt2_addr);
          m_pm_->write_value<uint64_t>(pt1_addr + it1, 0ull);
          continue;
        } else {
          pt1_empty = false;
        }
      }
      if (pt1_empty) {
        m_pm_->remove(pt1_addr);
        m_pm_->write_value<uint64_t>(root + it0, 0ull);
      }
    }
    return 0;
  }

  uint64_t clean_task(uint64_t root) {
    for (uint64_t it0 = 0; it0 < SV39::PageSize; it0 += sizeof(uint64_t)) {
      if ((m_pm_->read_value<uint64_t>(root + it0) & SV39::V) == 0ull)
        continue;
      auto pt1_addr = SV39::PTE2PA(m_pm_->read_value<uint64_t>(root + it0));
      for (uint64_t it1 = 0; it1 < SV39::PageSize; it1 += sizeof(uint64_t)) {
        if ((m_pm_->read_value<uint64_t>(pt1_addr + it1) & SV39::V) == 0ull)
          continue;
        auto pt2_addr =
            SV39::PTE2PA(m_pm_->read_value<uint64_t>(pt1_addr + it1));
        for (uint64_t it2 = 0; it2 < SV39::PageSize; it2 += sizeof(uint64_t)) {
          if ((m_pm_->read_value<uint64_t>(pt2_addr + it2) & SV39::V) == 0ull)
            continue;
          else {
            auto p_addr =
                SV39::PTE2PA(m_pm_->read_value<uint64_t>(pt2_addr + it2));
            m_pm_->remove(p_addr);
          }
        }
        m_pm_->remove(pt2_addr);
      }
      m_pm_->remove(pt1_addr);
    }
    m_pm_->remove(root);
    return 0ull;
  }

 private:
  std::unique_ptr<PhysicalMemory> m_pm_;
};

struct TLBundleA {  // req
  uint8_t opcode;
  uint32_t size;
  uint32_t source;
  uint32_t address;
  uint32_t mask;
  uint8_t ready;  // controller -> gpu, output
  uint8_t valid;  // gpu -> controller, input
  uint32_t* data;

  TLBundleA(int n) { data = new uint32_t[n]; }

  ~TLBundleA() { delete[] data; }
};

struct TLBundleD {  // rsp
  uint8_t opcode;
  uint32_t size;
  uint32_t source;
  uint8_t ready;  // gpu -> controller, input
  uint8_t valid;  // controller -> gpu, output
  uint32_t* data;

  TLBundleD(int n) { data = new uint32_t[n]; }

  ~TLBundleD() { delete[] data; }
};

class Processor;
class MemController {
 public:
  friend class Processor;
  MemController(uint32_t num_thread) {
    m_req_ = std::make_unique<TLBundleA>(2);
    m_rsp_ = std::make_unique<TLBundleD>(2);
    m_num_ = num_thread;
    m_data_ = new uint32_t[num_thread];
  }

  ~MemController() { delete[] m_data_; }

  void reset() {

    m_rsp_valid_ = 0;
    m_req_ready_ = 0;
    // request
    m_req_->ready = m_req_ready_;
    // response
    m_rsp_->valid = m_rsp_valid_;
    m_rsp_->size = 0;
    m_rsp_->opcode = 0;
    m_rsp_->source = 0;
    for (auto i = 0; i < m_num_; i++)
      m_data_[i] = 0;

    m_time_remain_ = 0;
    m_rw_ = 0;
    m_in_rsp_ = 0;
  }

  void eval(uint8_t clk, const std::unique_ptr<Memory>& mem) {
    if (clk) {
      m_rsp_ready_ = m_rsp_->ready;
      m_req_valid_ = m_req_->valid;
      m_rsp_valid_ = m_rsp_->valid;
      m_req_ready_ = m_req_->ready;
      return;
    }
    if (m_time_remain_ != 0) {
      m_req_->ready = 0;
      m_rsp_->valid = 0;
      --m_time_remain_;
      return;
    } else {
      m_rsp_->valid = 1;
    }
    // response
    if (m_in_rsp_) {
      if (m_rsp_->ready && m_rsp_->valid) {
        m_rsp_->size = m_size_;
        m_rsp_->source = m_source_;
        m_rsp_->opcode = 0;
        if (m_rw_ == 4) {
          m_rsp_->opcode = 1;
          mem->read_phy<uint32_t>(m_addr_, m_num_, m_rsp_->data);
          m_in_rsp_ = 0;
        }  // read
        if (m_rw_ == 0) {
          m_rsp_->opcode = 0;
          mem->write_phy<uint32_t>(m_addr_, m_num_, m_data_, &m_mask_);
          m_in_rsp_ = 0;
        }  // write
        m_rw_ = 0;
        m_rsp_valid_ = 1;
        m_req_ready_ = 1;
      }

    } else {  // request
      m_rsp_valid_ = 0;
      m_req_ready_ = 1;
      if (m_req_->valid && m_req_->ready) {
        m_rw_ = m_req_->opcode;
        m_addr_ = m_req_->address;
        m_size_ = m_req_->size;
        m_source_ = m_req_->source;
        m_mask_ = m_req_->mask;
        m_in_rsp_ = 1;
        if (m_rw_ == 0) {
          for (auto i = 0; i < m_num_; i++)
            m_data_[i] = m_req_->data[i];
        }
        m_time_remain_ = m_default_delay_;
      }
    }
    m_rsp_->valid = m_rsp_valid_;
    m_req_->ready = m_req_ready_;
  }

 private:
  std::unique_ptr<TLBundleA> m_req_;
  std::unique_ptr<TLBundleD> m_rsp_;
  uint32_t* m_data_;
  int m_default_delay_{0};

  uint32_t m_time_remain_, m_in_rsp_;
  uint8_t m_rw_, m_num_;
  uint8_t m_req_valid_, m_rsp_ready_;  // i
  uint8_t m_req_ready_, m_rsp_valid_;  // o
  uint64_t m_addr_;
  uint32_t m_size_, m_source_, m_mask_;
};

}  // namespace vt
