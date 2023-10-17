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

#include "ventus.h"
#include "basic/exception.h"
#include "driver.h"

int vt_set_device(vt_device_h* hdevice) {
  if (hdevice == nullptr) {
    VT_ERROR("vz::api", "input not valid");
    return -1;
  }
  *hdevice = new vt::Driver();
  vt_root_mem_alloc(*hdevice, 0);
  return 0;
}

int vt_release(vt_device_h* hdevice) {
  if (hdevice == nullptr) {
    VT_ERROR("vz::api", "input not valid");
    return -1;
  }
  auto* device = (vt::Driver*)hdevice;
  delete device;
  return 0;
}

int vt_dev_caps(vt_device_h* hdevice, uint64_t caps_id, uint64_t* value) {
  if (hdevice == nullptr) {
    VT_ERROR("vz::api", "input not valid");
    return -1;
  }
  switch (caps_id) {
    case VT_CAPS_VERSION:
      *value = vt::IMPLEMENTATION_ID;
      break;
    case VT_CAPS_MAX_CORES:
      *value = vt::NUM_CTA;
      break;
    case VT_CAPS_MAX_WARPS:
      *value = vt::NUM_WARP;
      break;
    case VT_CAPS_MAX_THREADS:
      *value = vt::NUM_THREAD;
      break;
    default:
      VT_ERROR("vz::api", "input not valid");
      return -1;
  }

  return 0;
}

int vt_root_mem_alloc(vt_device_h hdevice, int taskID) {
  if (hdevice == nullptr) {
    VT_ERROR("vz::api", "input not valid");
    return -1;
  }
  return ((vt::Driver*)hdevice)->create_device_mem(taskID);
}

int vt_root_mem_free(vt_device_h hdevice, int taskID) {
  if (hdevice == nullptr) {
    VT_ERROR("vz::api", "input not valid");
    return -1;
  }
  return ((vt::Driver*)hdevice)->delete_device_mem(taskID);
}

int vt_malloc(vt_device_h hdevice, uint64_t size, uint64_t* vaddr, int BUF_TYPE,
              uint64_t taskID, uint64_t kernelID) {
  if (size <= 0 || hdevice == nullptr) {
    VT_ERROR("vz::api", "input not valid");
    return -1;
  }
  return ((vt::Driver*)hdevice)
      ->alloc_local_mem(size, vaddr, (vt::addr::BufferType)BUF_TYPE, taskID,
                        kernelID);
}

int vt_free(vt_device_h hdevice, uint64_t size, uint64_t* vaddr,
            uint64_t taskID, uint64_t kernelID) {
  if (size <= 0 || hdevice == nullptr) {
    VT_ERROR("vz::api", "input not valid");
    return -1;
  }
  return ((vt::Driver*)hdevice)->free_local_mem(size, vaddr, taskID, kernelID);
}

static int vt_copy_to_dev(vt_device_h hdevice, uint64_t dev_vaddr,
                          uint64_t src_addr, uint64_t size, uint64_t taskID,
                          uint64_t kernelID) {
  if (size <= 0 || hdevice == nullptr) {
    VT_ERROR("vz::api", "input not valid");
    return -1;
  }
  return ((vt::Driver*)hdevice)
      ->upload(dev_vaddr, (void*)src_addr, size, taskID, kernelID);
}

static int vt_copy_from_dev(vt_device_h hdevice, uint64_t dev_vaddr,
                            uint64_t dst_addr, uint64_t size, uint64_t taskID,
                            uint64_t kernelID) {
  if (size <= 0 || hdevice == nullptr) {
    VT_ERROR("vz::api", "input not valid");
    return -1;
  }
  return ((vt::Driver*)hdevice)
      ->download(dev_vaddr, (void*)dst_addr, size, taskID, kernelID);
}

int vt_memcopy(vt_device_h hdevice, uint64_t dst_addr, uint64_t src_addr,
               uint64_t size, uint64_t taskID, uint64_t kernelID,
               MemCopyType type) {
  switch (type) {
    case MemCopyType::DEVICE_TO_HOST: {
      vt_copy_from_dev(hdevice, src_addr, dst_addr, size, taskID, kernelID);
      break;
    }
    case MemCopyType::HOST_TO_DEVICE: {
      vt_copy_to_dev(hdevice, dst_addr, src_addr, size, taskID, kernelID);
      break;
    }
    default: {
      VT_ERROR("vz::api", "input not valid");
      return -1;
    }
  }
  unreachable("vz::api");
  return -1;
}

int vt_start(vt_device_h hdevice, void* metaData, uint64_t taskID) {
  if (hdevice == nullptr) {
    VT_ERROR("vz::api", "input not valid");
    return -1;
  }
  ((vt::Driver*)hdevice)->start(taskID, metaData);
  return 0;
}

int vt_ready_wait(vt_device_h hdevice, uint64_t timeout) {
  if (hdevice == nullptr) {
    VT_ERROR("vz::api", "input not valid");
    return -1;
  }
  ((vt::Driver*)hdevice)->wait(timeout);
  return 0;
}

int vt_upload_kernel_bytes(vt_device_h device, const void* content,
                           uint64_t size, int taskID) {
  int err = 0;

  if (NULL == content || 0 == size)
    return -1;
  uint32_t buffer_transfer_size = 65536;
  uint64_t kernel_base_addr = vt::addr::BUF_PARA_BASE;
  uint64_t dev_mem_addr;
  uint64_t offset = 0;

  int numValues = size / 8;

  uint32_t values[numValues];

  for (int i = 0; i < numValues; i++) {
    std::string substring = ((std::string*)content)->substr(i * 8, 8);
    unsigned int value = std::stoul(substring, nullptr, 16);
    std::memcpy(values + i, &value, sizeof(uint32_t));
  }
  void* const buffer = malloc(buffer_transfer_size);
  while (offset < size) {
    auto chunk_size = std::min<uint64_t>(buffer_transfer_size, size - offset);
    std::memcpy(buffer, values + offset, chunk_size);

    err = vt_malloc(device, buffer_transfer_size, &dev_mem_addr,
                    (int)vt::addr::BufferType::KERNEL_MEM, taskID, 0);
    if (err != 0)
      return -1;

    printf("***  Upload Kernel to 0x%0x: data=", kernel_base_addr + offset);
    for (int i = 0; i < chunk_size; ++i) {
      printf("%08x", ((values + offset))[i]);
    }
    printf("\n");

    err = vt_copy_to_dev(device, dev_mem_addr, (uint64_t)buffer, chunk_size,
                         taskID, 0);
    if (err != 0) {
      //      vt_buf_free(device, buffer_transfer_size, &dev_mem_addr, taskID, 0);
      return err;
    }
    offset += chunk_size;
  }
  free(buffer);
  return 0;
}
