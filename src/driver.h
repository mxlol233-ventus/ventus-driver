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

#include <future>
#include <map>
#include <memory>
#include "basic/exception.h"
#include "device/processor.h"

namespace vt {

namespace krl {

struct Meta {
  uint64_t kernel_id;
  uint64_t kernel_size[3];
  uint64_t wf_size;
  uint64_t wg_size;
  uint64_t base_addr;
  uint64_t lds_size;
  uint64_t pds_size;
  uint64_t sgpr_usage;
  uint64_t vgpr_usage;
  uint64_t pds_base_addr;
};

enum State { UNFINISH, FINISH };

struct Info {
  using StateList = std::map<int, State>;
  StateList blk_list;
  State state;
  Info(StateList&& input_blk_list, State s)
      : blk_list(std::move(input_blk_list)), state(s) {}
};
}  // namespace krl

namespace ctx {
struct Info {
  uint64_t ctx_id;
  std::map<uint64_t, krl::Info> krl_list;
  uint64_t root;
  Info(uint64_t task_id) {
    ctx_id = task_id;
    root = 0;
  }

  bool context_finished() {
    for (auto const& it : krl_list) {
      if (it.second.state == krl::UNFINISH)
        return false;
    }
    return true;
  }
};

}  // namespace ctx
namespace addr {
struct Item {
  Item* prev_ctx_item;
  Item* succ_ctx_item;
  Item* prev_krl_item;
  Item* succ_krl_item;
  uint64_t krl_id;
  uint64_t task_id;
  uint64_t vaddr;
  uint64_t paddr;
  uint64_t size;
  Item(uint64_t in_kernelID, uint64_t in_taskID, uint64_t in_vaddr,
       uint64_t in_size)
      : prev_ctx_item(nullptr),
        succ_ctx_item(nullptr),
        prev_krl_item(nullptr),
        succ_krl_item(nullptr),
        krl_id(in_kernelID),
        task_id(in_taskID),
        vaddr(in_vaddr),
        size(in_size) {}
};

enum class BufferType { READ_WRITE = 0, READ_ONLY, KERNEL_MEM };

constexpr static uint64_t GLOBALMEM_BASE = 0x40000000;
constexpr static uint64_t GLOBALMEM_SIZE = 0x40000000;
constexpr static uint64_t RODATA_BASE = GLOBALMEM_BASE;
constexpr static uint64_t RWDATA_BASE = 0x60000000;
constexpr static uint64_t BUF_PARA_BASE = 0x80000000;
constexpr static uint64_t LOCALMEM_BASE = 0x10000000;
constexpr static uint64_t BUFSIZE = 0x10000000;
constexpr static uint64_t PAGESIZE = 1ull << 12ull;

class Manager {
 public:
  Manager(){};
  ~Manager() {
    for (auto it : m_cxt_mem_) {
      Item* curItem = it.second;
      while (curItem != nullptr) {
        auto tmp = curItem;
        curItem = curItem->succ_ctx_item;
        delete tmp;
      }
    }
  }

  void createNewContext(uint64_t contextID) {
    for (auto it : m_cxt_mem_) {
      if (it.first == contextID) {
        VZ_ERROR("Manager::createNewContext", "A context of ID ", contextID,
                 " exists, error!");
      }
    }
    m_cxt_mem_.emplace(contextID, nullptr);
  }
  void allocMemory(uint64_t contextID, uint64_t kernelID, uint64_t* vaddr,
                   uint64_t size, BufferType BUF_TYPE) {
    if (size == 0 || vaddr == nullptr) {
      VZ_ERROR("Manager::allocMemory", "vaddr or size not valid!");
      return;
    }

    size = aligned_size(size, BLOCK_SIZE);
    Item* currentItem = nullptr;

    if (m_cxt_mem_.find(contextID) == m_cxt_mem_.end()) {
      VZ_ERROR("Manager::allocMemory", contextID, " is not valid");
    }

    switch (BUF_TYPE) {
      case BufferType::READ_ONLY:
        if (size < RWDATA_BASE - RODATA_BASE) {
          *vaddr = RODATA_BASE;
          break;
        } else {
          VZ_ERROR("Manager::allocMemory", "buffer size too large");
        }
      case BufferType::READ_WRITE:
        if (size < RWDATA_BASE - RODATA_BASE) {
          *vaddr = RWDATA_BASE;
          break;
        } else {
          VZ_ERROR("Manager::allocMemory", "buffer size too large");
        }
      case BufferType::KERNEL_MEM:
        if (size < GLOBALMEM_SIZE / 2) {
          *vaddr = BUF_PARA_BASE;
          break;
        } else {
          VZ_ERROR("Manager::allocMemory", "buffer size too large");
        }
      default:
        break;
    }
    if (m_cxt_mem_.at(contextID) == nullptr) {
      currentItem = new Item(kernelID, contextID, *vaddr, size);
      m_cxt_mem_.at(contextID) = currentItem;
    } else {
      currentItem = m_cxt_mem_.at(contextID);
      if (!allocVaddr(&currentItem, vaddr, size, BUF_TYPE))
        insertNewItem(currentItem, contextID, kernelID, vaddr, size);
      else {
        VZ_ERROR("Manager::allocMemory", "allocating virtual addr failed !");
      }
    }
  }
  int releaseMemory(uint64_t contextID, uint64_t kernelID, uint64_t* vaddr,
                    uint64_t size) {
    bool b_contextExist = false;
    bool b_vaddrExist = false;

    b_contextExist = true;
    auto tmp = m_cxt_mem_.at(contextID);
    while (tmp != nullptr) {
      if (tmp->vaddr == *vaddr) {
        if (tmp->prev_ctx_item == nullptr && tmp->succ_ctx_item == nullptr)
          ;
        else if (tmp->prev_ctx_item == nullptr)
          tmp->succ_ctx_item->prev_ctx_item = nullptr;
        else if (tmp->succ_ctx_item == nullptr)
          tmp->prev_ctx_item->succ_ctx_item = nullptr;
        else {
          tmp->prev_ctx_item->succ_ctx_item = tmp->succ_ctx_item;
          tmp->succ_ctx_item->prev_ctx_item = tmp->prev_ctx_item;
        }
        m_cxt_mem_.erase(contextID);
        b_vaddrExist = true;
        break;
      }
      tmp = tmp->succ_ctx_item;
    }
    if (!b_contextExist) {
      VZ_ERROR("Manager", "context ID of [", contextID, "] not exist");
      return -1;
    }
    if (!b_vaddrExist) {
      VZ_ERROR("Manager", "addr [", vaddr, "] not valid");

      return -1;
    }
    return 0;
  }

  // assign physical address to item
  int attachPaddr(uint64_t kernelID, uint64_t contextID, uint64_t vaddr,
                  uint64_t paddr) {
    auto tmp = m_cxt_mem_.at(contextID);
    while (tmp != nullptr) {
      if (tmp->vaddr == vaddr) {
        tmp->paddr = paddr;
        break;
      }
      tmp = tmp->succ_ctx_item;
    }
    if (!tmp) {
      VZ_ERROR("Manager", "failed");
      return -1;
    }
    return 0;
  }
  int findVaByPa(uint64_t kernelID, uint64_t contextID, uint64_t* vaddr,
                 uint64_t* paddr) {
    if (m_cxt_mem_.find(contextID) == m_cxt_mem_.end()) {
      VZ_ERROR("Manager", "Context of ID [", contextID, "] has not created");
      return -1;
    }
    auto tmp = m_cxt_mem_.at(contextID);
    while (tmp != nullptr) {
      if (tmp->paddr == *paddr) {
        *vaddr = tmp->vaddr;
        break;
      }
      tmp = tmp->succ_ctx_item;
    }
    return 0;
  }
  bool findContextID(uint64_t contextID) {
    for (auto it : m_cxt_mem_) {
      if (it.first == contextID)
        return true;
    }
    return false;
  }
  bool findKernelID(uint64_t kernelID) {
    for (auto it : krl_lst_) {
      if (it == kernelID)
        return true;
    }
    return false;
  }

 private:
  // alloc vaddr to segments(RO, RW, PARA)
  int allocVaddr(Item** rootItem, uint64_t* vaddr, uint64_t size,
                 BufferType BUF_TYPE) {
    uint64_t curAddr;
    switch (BUF_TYPE) {
      case BufferType::READ_ONLY:
        if ((*rootItem)->vaddr == RODATA_BASE) {
          *vaddr =
              aligned_size((*rootItem)->vaddr + (*rootItem)->size, PAGESIZE);
          while ((*rootItem)->vaddr < RWDATA_BASE &&
                 (*rootItem)->succ_ctx_item != nullptr) {
            if (*vaddr + size <= (*rootItem)->succ_ctx_item->vaddr) {
              break;
            }
            *rootItem = (*rootItem)->succ_ctx_item;
            *vaddr =
                aligned_size((*rootItem)->vaddr + (*rootItem)->size, PAGESIZE);
          }

          if ((*rootItem)->succ_ctx_item == nullptr ||
              (*rootItem)->succ_ctx_item->vaddr >= RWDATA_BASE) {
            if (*vaddr + size <= RWDATA_BASE)
              break;
            else {
              VZ_ERROR("Manager::allocMemory",
                       "memory needs to allocate of size of 0x", std::hex, size,
                       "failed! No enough space!");
              return -1;
            }
          }
        } else {
          *vaddr = RODATA_BASE;
        }
        break;

      case BufferType::READ_WRITE:
        if ((*rootItem)->vaddr == RODATA_BASE) {
          while ((*rootItem)->vaddr < RWDATA_BASE) {
            if ((*rootItem)->succ_ctx_item == nullptr) {
              *vaddr = RWDATA_BASE;
              return 0;
            }
            *rootItem = (*rootItem)->succ_ctx_item;
          }
        }
        *vaddr = aligned_size((*rootItem)->vaddr + (*rootItem)->size, PAGESIZE);
        while ((*rootItem)->vaddr < BUF_PARA_BASE &&
               (*rootItem)->succ_ctx_item != nullptr) {
          if (*vaddr + size <= (*rootItem)->succ_ctx_item->vaddr) {
            break;
          }
          *rootItem = (*rootItem)->succ_ctx_item;
          *vaddr =
              aligned_size((*rootItem)->vaddr + (*rootItem)->size, PAGESIZE);
        }
        if ((*rootItem)->succ_ctx_item == nullptr &&
            (*vaddr + size > BUF_PARA_BASE)) {
          VZ_ERROR("Manager::allocMemory",
                   "memory needs to allocate of size of 0x", std::hex, size,
                   "failed! No enough space!");
          return -1;
        }
        break;
      case BufferType::KERNEL_MEM:
        if ((*rootItem)->vaddr == RODATA_BASE ||
            (*rootItem)->vaddr == RWDATA_BASE) {
          while ((*rootItem)->vaddr < BUF_PARA_BASE) {
            if ((*rootItem)->succ_ctx_item == nullptr) {
              *vaddr = BUF_PARA_BASE;
              return 0;
            }
            *rootItem = (*rootItem)->succ_ctx_item;
          }
        }
        *vaddr = aligned_size((*rootItem)->vaddr + (*rootItem)->size, PAGESIZE);
        while ((*rootItem)->vaddr < BUF_PARA_BASE + GLOBALMEM_SIZE / 2 &&
               (*rootItem)->succ_ctx_item != nullptr) {
          if (*vaddr + size <= (*rootItem)->succ_ctx_item->vaddr) {
            break;
          }
          *rootItem = (*rootItem)->succ_ctx_item;
          *vaddr =
              aligned_size((*rootItem)->vaddr + (*rootItem)->size, PAGESIZE);
        }
        if ((*rootItem)->succ_ctx_item == nullptr &&
            (*vaddr + size > BUF_PARA_BASE + GLOBALMEM_SIZE / 2)) {
          VZ_ERROR("Manager::allocMemory",
                   "memory needs to allocate of size of 0x", std::hex, size,
                   "failed! No enough space!");
          return -1;
        }
        break;
    }

    return 0;
  }

  void insertNewItem(Item* currentItem, uint64_t contextID, uint64_t kernelID,
                     uint64_t* vaddr, uint64_t size) {
    auto tmp = new Item(kernelID, contextID, *vaddr, size);
    if (tmp->vaddr == RODATA_BASE && currentItem->vaddr == RWDATA_BASE) {
      tmp->succ_ctx_item = currentItem;
      currentItem->prev_ctx_item = tmp;
      m_cxt_mem_.at(contextID) = tmp;
      return;
    }
    tmp->succ_ctx_item = (currentItem)->succ_ctx_item;
    tmp->prev_ctx_item = currentItem;
    if (currentItem->succ_ctx_item != nullptr)
      currentItem->succ_ctx_item->prev_ctx_item = tmp;
    (currentItem)->succ_ctx_item = tmp;
  }

  std::map<uint64_t, Item*> m_cxt_mem_;
  std::list<uint64_t> krl_lst_;
};

}  // namespace addr

class Driver {

 public:
  Driver() {}

  int create_device_mem(uint64_t taskID) {
    if (m_ctx_lst_.find(taskID) != m_ctx_lst_.end()) {
      VZ_ERROR("vz::Device", "the taskID of ", taskID,
               " has been created, check your input!");
      return -1;
    }
    m_addr_mng_.createNewContext(taskID);
    m_ctx_lst_.emplace(taskID, ctx::Info(taskID));
    auto it = m_ctx_lst_.find(taskID);
    uint64_t ret1 = m_processor_.ram()->create_root_pt();
    it->second.root = ret1;
    return !ret1;
  }

  int delete_device_mem(int taskID) {
    if (m_ctx_lst_.find(taskID) != m_ctx_lst_.end()) {
      VZ_ERROR("vz::Device", "the taskID of ", taskID,
               " has not been created, check your input!");
      return -1;
    }
    m_ctx_lst_.erase(taskID);
    return 0;
  }

  int alloc_local_mem(uint64_t size, uint64_t* vaddr, addr::BufferType BUF_TYPE,
                      uint64_t taskID, uint64_t kernelID) {
    auto it = m_ctx_lst_.find(taskID);
    if (size <= 0 || vaddr == nullptr || it == m_ctx_lst_.end()) {
      VZ_ERROR("vz::Device", "alloc_local_mem failed");
      return -1;
    }
    uint64_t paddr;
    m_addr_mng_.allocMemory(taskID, kernelID, vaddr, size,
                            BUF_TYPE);  // virtual mem
    if (!vaddr) {
      VZ_ERROR("vz::Device",
               "alloc_local_mem failed, can not alloc virtual mem.");
      return -1;
    }
    paddr = m_processor_.ram()->allocate(it->second.root, *vaddr,
                                         size);  // physical mem
    if (!paddr) {
      VZ_ERROR("vz::Device",
               "alloc_local_mem failed, can not alloc physical mem. ");
      return -1;
    }
    m_addr_mng_.attachPaddr(taskID, kernelID, *vaddr, paddr);
    VZ_NOTE("vz::Driver", "allocating memory at vaddr of 0x", std::hex, *vaddr,
            ", associated paddr of 0x", std::hex, paddr, ", size of ", std::dec,
            size, " bytes");
    return !*vaddr;
  }

  int free_local_mem(uint64_t size, uint64_t* vaddr, uint64_t taskID,
                     uint64_t kernelID) {
    if (size <= 0 || vaddr == nullptr || !m_addr_mng_.findContextID(taskID)) {
      VZ_ERROR("vz::Device", "free_local_mem failed");
      return -1;
    }
    m_addr_mng_.releaseMemory(taskID, kernelID, vaddr, size);
    auto it = m_ctx_lst_.find(taskID);
    int flg = m_processor_.ram()->release(it->second.root, *vaddr);
    if (!flg) {
      return 0;
    }
    VZ_ERROR("vz::Device", "upload free_local_mem");
    return -1;
  }

  int upload(uint64_t dev_vaddr, const void* src_addr, uint64_t size,
             uint64_t taskID, uint64_t kernelID) {
    auto it = m_ctx_lst_.find(taskID);
    if (size <= 0 || src_addr == nullptr || it == m_ctx_lst_.end()) {
      VZ_ERROR("vz::Device", "upload failed");
      return -1;
    }

    auto flg = m_processor_.ram()->write_virt(it->second.root, dev_vaddr, size,
                                              src_addr);
    if (!flg) {
      return 0;
    }
    VZ_ERROR("vz::Device", "upload failed");
    return -1;
  }

  int download(uint64_t dev_vaddr, void* dst_addr, uint64_t size,
               uint64_t taskID, uint64_t kernelID) {
    auto it = m_ctx_lst_.find(taskID);

    if (size <= 0 || dst_addr == nullptr || it == m_ctx_lst_.end()) {
      VZ_ERROR("vz::Device", "download failed");
      return -1;
    }

    auto flg = m_processor_.ram()->read_virt(it->second.root, dev_vaddr, size,
                                             dst_addr);
    if (!flg) {
      return 0;
    }
    VZ_ERROR("vz::Device", "download failed");
    return -1;
  }

  int start(int taskID, void* metaData) {

    host_port_t* devicePort = new host_port_t;
    auto inputData = (krl::Meta*)metaData;

    uint64_t wgNum = inputData->kernel_size[0] * inputData->kernel_size[1] *
                     inputData->kernel_size[2];
    uint64_t pdsParam =
        inputData->pds_size * inputData->wf_size * inputData->wg_size;
    devicePort->host_req_num_wf = inputData->wg_size;
    devicePort->host_req_wf_size = inputData->wf_size;
    devicePort->host_req_kernel_size_3d_0 = inputData->kernel_size[0];
    devicePort->host_req_kernel_size_3d_1 = inputData->kernel_size[1];
    devicePort->host_req_kernel_size_3d_2 = inputData->kernel_size[2];
    devicePort->host_req_vgpr_size_total =
        inputData->wg_size * inputData->vgpr_usage;
    devicePort->host_req_sgpr_size_total =
        inputData->wg_size * inputData->sgpr_usage;
    devicePort->host_req_gds_size_total = 0;
    devicePort->host_req_vgpr_size_per_wf = inputData->vgpr_usage;
    devicePort->host_req_sgpr_size_per_wf = inputData->sgpr_usage;
    devicePort->host_req_start_pc = 0x80000000;
    devicePort->host_req_pds_baseaddr = inputData->pds_base_addr;
    devicePort->host_req_csr_knl = inputData->base_addr;
    devicePort->host_req_lds_size_total = inputData->lds_size;
    devicePort->host_req_gds_baseaddr = 0;

    if (m_ctx_lst_.find(taskID) == m_ctx_lst_.end()) {
      VZ_ERROR("vz::Device", "the taskID of ", taskID,
               " has not been created, check your input!");
      return -1;
    }
    uint64_t kernelID = inputData->kernel_id;

    for (int i = 0; i < wgNum; ++i) {
      devicePort->host_req_pds_baseaddr =
          inputData->pds_base_addr + i * pdsParam;
      devicePort->host_req_wg_id =
          (inst_len)(((kernelID << (int)ceil(log2(MAX_CONTEXT)) | taskID)
                      << ((int)ceil(log2(MAX_KERNEL)) | kernelID))
                     << ((int)ceil(log2(NUM_SM * MAX_BLOCK_PER_SM)) | i))
          << ((int)ceil(log2(NUM_SM)));

      devicePort->host_req_num_wf = 2;
      devicePort->host_req_wf_size = 0x8;
      devicePort->host_req_kernel_size_3d_0 = 0;
      devicePort->host_req_kernel_size_3d_1 = 0;
      devicePort->host_req_kernel_size_3d_2 = 0;
      devicePort->host_req_vgpr_size_total = 0x040;
      devicePort->host_req_sgpr_size_total = 0x040;
      devicePort->host_req_gds_size_total = 0;
      devicePort->host_req_vgpr_size_per_wf = 0x020;
      devicePort->host_req_sgpr_size_per_wf = 0x020;
      devicePort->host_req_start_pc = 0x80000000;
      devicePort->host_req_pds_baseaddr = 0x80001000;
      devicePort->host_req_csr_knl = 0x80023000;
      devicePort->host_req_lds_size_total = 0x80;
      devicePort->host_req_gds_baseaddr = 0x00000000;

      m_processor_.run(m_ctx_lst_.find(taskID)->second.root, devicePort);
      std::map<int, krl::State> firedBlk;
      firedBlk.emplace((int)(devicePort->host_req_wg_id), krl::UNFINISH);
      m_ctx_lst_.find(taskID)->second.krl_list.emplace(
          kernelID, krl::Info(std::move(firedBlk), krl::UNFINISH));
    }
    return 0;
  }

  int wait(uint64_t time) {
    if (m_last_task_.valid()) {

      uint64_t timeout = time / 1000;
      std::chrono::seconds wait_time(1);
      for (;;) {
        auto status = m_last_task_.wait_for(wait_time);
        if (status == std::future_status::ready || timeout-- == 0)
          break;
      }
    }

    auto finished_block = m_processor_.wait(time);

    while (!finished_block.empty()) {
      bool block_legal = true;
      uint64_t blkID = (finished_block.front() >> (int)ceil(log2(NUM_SM))) &
                       (1 << (int)ceil(log2(NUM_SM * MAX_BLOCK_PER_SM)));
      uint64_t kernelID =
          (finished_block.front() >>
           (int)ceil(log2(NUM_SM * MAX_BLOCK_PER_SM * NUM_SM))) &
          (1 << (int)ceil(log2(MAX_KERNEL)));
      uint64_t contextID =
          (finished_block.front() >>
           (int)ceil(log2(NUM_SM * MAX_BLOCK_PER_SM * NUM_SM * MAX_KERNEL))) &
          (1 << (int)ceil(log2(MAX_CONTEXT)));
      auto contextItem = m_ctx_lst_.find(contextID);
      if (contextItem == m_ctx_lst_.end())
        block_legal = false;
      else {
        auto it = contextItem->second.krl_list.find(kernelID);
        if (it == contextItem->second.krl_list.end()) {
          block_legal = false;
        } else {
          if (it->second.blk_list.find(blkID) == it->second.blk_list.end())
            block_legal = false;
          else {
            it->second.blk_list[blkID] = krl::FINISH;
            finished_block.pop();
            bool kernel_all_block_finished = true;
            for (auto& it_map : it->second.blk_list) {
              if (!it_map.second) {
                kernel_all_block_finished = false;
                break;
              }
            }
            if (kernel_all_block_finished) {
              m_finished_kernel_lst_.push(
                  contextID << (int)ceil(log2(MAX_CONTEXT)) | kernelID);
              it->second.state = krl::FINISH;
            }
          }
        }
      }
      if (!block_legal) {
        VZ_ERROR("vz::Driver", "wait failed");
        return -1;
      }
    }
    return 0;
  }

  std::queue<int> get_finished_kernel() {
    std::queue<int> tmp;
    while (!m_finished_kernel_lst_.empty()) {
      tmp.push(m_finished_kernel_lst_.front());
      m_finished_kernel_lst_.pop();
    }
    return tmp;
  }

  std::queue<int> execute_all_kernel() {
    std::queue<int> tmp;
    int cnt = 0;
    while (!all_context_finished()) {
      while (!m_finished_kernel_lst_.empty()) {
        tmp.push(m_finished_kernel_lst_.front());
        m_finished_kernel_lst_.pop();
      }
      wait(RUN_DELAY);
      cnt++;
      if (cnt > 30)
        break;
    }
    return tmp;
  }

  bool all_context_finished() {
    auto it = m_ctx_lst_.begin();
    while (it != m_ctx_lst_.end()) {
      if (!it->second.context_finished())
        return false;
    }
    return true;
  }

 private:
  Processor m_processor_;
  std::future<int> m_last_task_;
  std::queue<int> m_finished_kernel_lst_;
  addr::Manager m_addr_mng_;
  std::map<uint64_t, ctx::Info> m_ctx_lst_;
};

}  // namespace vt
