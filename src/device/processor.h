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

#include <verilated.h>
#include <verilated_vcd_c.h>
#include <memory>
#include <queue>
#include "VGPGPU_top.h"
#include "memory.h"
#include "ventus.h"

namespace vt {

constexpr static uint64_t RAM_RANGE = (4096ull * 1024ull * 1024ull);
constexpr static uint8_t BLOCK_SIZE = 64;
constexpr static uint8_t MAX_CONTEXT = 4;
constexpr static uint8_t MAX_KERNEL = 4;
constexpr static uint8_t NUM_SM = 2;
constexpr static uint8_t MAX_BLOCK_PER_SM = 32;
constexpr static uint8_t RUN_DELAY = 100;
constexpr static uint8_t IMPLEMENTATION_ID = 0;
constexpr static uint8_t NUM_CTA = 2;
constexpr static uint8_t NUM_WARP = 4;
constexpr static uint8_t THREAD_PER_WARP = 8;

inline uint64_t aligned_size(uint64_t size, uint64_t align_block) {
  assert(0 == (align_block & (align_block - 1)));
  return (size + align_block - 1) & ~(align_block - 1);
}
inline bool is_aligned(uint64_t addr, uint64_t align_block) {
  assert(0 == (align_block & (align_block - 1)));
  return (0 == ((addr) & (align_block - 1)));
}

constexpr static uint8_t NUM_THREAD = 8;
class Processor {
 public:
  Processor()
      : m_context_(std::make_unique<VerilatedContext>()),
        m_device_(std::make_unique<VGPGPU_top>()),
        m_root_(0x20000000),
        m_mem_ctrl_(NUM_THREAD),
        m_ram_(std::make_unique<Memory>(RAM_RANGE)) {
    m_context_->debug(0);
    m_context_->randReset(2);
    m_context_->assertOn(false);
  }

  std::queue<int> wait(uint64_t cycle) {
    for (int i = 0; i < cycle; i++) {
      this->tick();
    }
    return finished_block();
  }

  std::queue<int> finished_block() {
    auto tmp = m_finished_block_queue_;
    while (!m_finished_block_queue_.empty())
      m_finished_block_queue_.pop();
    return tmp;
  }

  std::unique_ptr<Memory>& ram() { return m_ram_; }

  // call rtl SM to run a single block.
  int run(uint64_t root, host_port_t* input_sig) {
    m_root_ = root;
    if (!m_device_->clock)
      this->eval();
    m_device_->io_host_req_bits_host_wg_id = input_sig->host_req_wg_id;
    m_device_->io_host_req_bits_host_num_wf = input_sig->host_req_num_wf;
    m_device_->io_host_req_bits_host_wf_size = input_sig->host_req_wf_size;
    m_device_->io_host_req_bits_host_kernel_size_3d_0 =
        input_sig->host_req_kernel_size_3d_0;
    m_device_->io_host_req_bits_host_kernel_size_3d_1 =
        input_sig->host_req_kernel_size_3d_1;
    m_device_->io_host_req_bits_host_kernel_size_3d_2 =
        input_sig->host_req_kernel_size_3d_2;
    m_device_->io_host_req_bits_host_vgpr_size_total =
        input_sig->host_req_vgpr_size_total;
    m_device_->io_host_req_bits_host_sgpr_size_total =
        input_sig->host_req_sgpr_size_total;
    m_device_->io_host_req_bits_host_gds_size_total =
        input_sig->host_req_gds_size_total;
    m_device_->io_host_req_bits_host_vgpr_size_per_wf =
        input_sig->host_req_vgpr_size_per_wf;
    m_device_->io_host_req_bits_host_sgpr_size_per_wf =
        input_sig->host_req_sgpr_size_per_wf;
    m_device_->io_host_req_bits_host_start_pc = input_sig->host_req_start_pc;
    m_device_->io_host_req_bits_host_pds_baseaddr =
        input_sig->host_req_pds_baseaddr;
    m_device_->io_host_req_bits_host_csr_knl = input_sig->host_req_csr_knl;
    m_device_->io_host_req_bits_host_lds_size_total =
        input_sig->host_req_lds_size_total;
    m_device_->io_host_req_bits_host_gds_baseaddr =
        input_sig->host_req_gds_baseaddr;
    m_device_->io_host_req_valid = 1;
    m_device_->io_host_rsp_ready = 1;

    while (true) {
      this->tick();
      if (m_device_->io_host_req_ready == 1) {
        this->tick();
        m_device_->io_host_req_valid = 0;
        break;
      }
    }
    return 0;
  }

  void tick() {
    eval();
    eval();
    if (m_device_->io_host_rsp_valid) {
      m_finished_block_queue_.push(
          m_device_->io_host_rsp_bits_inflight_wg_buffer_host_wf_done_wg_id);
    }
  }
  void get_ram_bits_port() {
    /// out_a signals, update at high voltage
    if (m_device_->clock) {
      m_mem_ctrl_.m_req_->address =
          m_ram_ ? m_ram_->convert(m_root_, m_device_->io_out_a_bits_address)
                 : m_device_->io_out_a_bits_address;
      m_mem_ctrl_.m_req_->opcode = m_device_->io_out_a_bits_opcode;
      m_mem_ctrl_.m_req_->size = m_device_->io_out_a_bits_opcode;
      m_mem_ctrl_.m_req_->source = m_device_->io_out_a_bits_source;
      m_mem_ctrl_.m_req_->mask = m_device_->io_out_a_bits_mask;

      for (int i = 0; i < NUM_THREAD; ++i) {
        m_mem_ctrl_.m_req_->data[i] = m_device_->io_out_a_bits_data[i];
      }
      m_mem_ctrl_.m_req_->valid = m_device_->io_out_a_valid;
      m_mem_ctrl_.m_rsp_->ready = m_device_->io_out_d_ready;

      m_device_->io_out_a_ready = m_mem_ctrl_.m_req_->ready;
      m_device_->io_out_d_bits_opcode = m_mem_ctrl_.m_rsp_->opcode;
      m_device_->io_out_d_bits_size = m_mem_ctrl_.m_rsp_->size;
      m_device_->io_out_d_bits_source = m_mem_ctrl_.m_rsp_->source;

      for (int i = 0; i < NUM_THREAD; ++i) {
        m_device_->io_out_d_bits_data[i] = m_mem_ctrl_.m_rsp_->data[i];
      }
      m_device_->io_out_d_valid = m_mem_ctrl_.m_rsp_->valid;
    }
  }

  void eval() {
    m_context_->timeInc(1);
    get_ram_bits_port();
    m_device_->clock = !m_device_->clock;
    m_device_->eval();
    m_mem_ctrl_.eval(m_device_->clock, m_ram_);
  }

 private:
  std::unique_ptr<Memory> m_ram_{nullptr};
  std::unique_ptr<VGPGPU_top> m_device_;
  uint64_t m_root_;
  MemController m_mem_ctrl_;

  std::unique_ptr<VerilatedContext> m_context_{nullptr};
  std::queue<int> m_finished_block_queue_;
};
}  // namespace vt
