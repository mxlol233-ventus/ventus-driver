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

#ifndef __VENTUS_H__
#define __VENTUS_H__

#include <stdint.h>

typedef unsigned int inst_len;
typedef struct {
  inst_len host_req_wg_id;            // workgroup id
  inst_len host_req_num_wf;           // number of warp (wavefront)
  inst_len host_req_wf_size;          // number of threads in a warp
  inst_len host_req_start_pc;         // start pc address
  inst_len host_req_vgpr_size_total;  // total number of vector registers
  inst_len host_req_sgpr_size_total;  // total number of scalar registers
  inst_len host_req_lds_size_total;   // local memory size
  inst_len host_req_gds_size_total;   // global memory size
  inst_len
      host_req_vgpr_size_per_wf;  // number of vector registers for each of warp
  inst_len
      host_req_sgpr_size_per_wf;  // number of scalar registers for each of warp
  inst_len host_req_gds_baseaddr;
  inst_len host_req_pds_baseaddr;
  inst_len host_req_csr_knl;
  inst_len host_req_kernel_size_3d_0;
  inst_len host_req_kernel_size_3d_1;
  inst_len host_req_kernel_size_3d_2;
} host_port_t;

#ifdef __cplusplus
extern "C" {
#endif
typedef void* vt_device_h;
typedef void* vt_buffer_h;

int vt_set_device(vt_device_h*);

int vt_release(vt_device_h*);

#define VT_CAPS_VERSION 0x0
#define VT_CAPS_MAX_CORES 0x1
#define VT_CAPS_MAX_WARPS 0x2
#define VT_CAPS_MAX_THREADS 0x3
#define VT_CAPS_CACHE_LINE_SIZE 0x4
#define VT_CAPS_LOCAL_MEM_SIZE 0x5
#define VT_CAPS_ALLOC_BASE_ADDR 0x6
#define VT_CAPS_KERNEL_BASE_ADDR 0x7

int vt_dev_caps(vt_device_h* hdevice, uint64_t caps_id, uint64_t* value);

int vt_root_mem_alloc(vt_device_h hdevice, int taskID);

int vt_root_mem_free(vt_device_h hdevice, int taskID);

int vt_malloc(vt_device_h hdevice, uint64_t size, uint64_t* vaddr, int BUF_TYPE,
              uint64_t taskID, uint64_t kernelID);

int vt_free(vt_device_h hdevice, uint64_t size, uint64_t* vaddr,
            uint64_t taskID, uint64_t kernelID);

enum MemCopyType {
  HOST_TO_DEVICE = 0,
  DEVICE_TO_HOST,
};

int vt_memcopy(vt_device_h hdevice, uint64_t dev_vaddr, uint64_t src_addr,
               uint64_t size, uint64_t taskID, uint64_t kernelID, MemCopyType);

int vt_start(vt_device_h hdevice, void* metaData, uint64_t taskID);

int vt_ready_wait(vt_device_h hdevice, uint64_t timeout);

int vt_upload_kernel_bytes(vt_device_h device, const void* content,
                           uint64_t size, int taskID);

#ifdef __cplusplus
}
#endif

#endif  // __VENTUS_H__
