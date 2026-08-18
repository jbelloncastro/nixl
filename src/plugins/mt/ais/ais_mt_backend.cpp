/* Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <nixl.h>
#include <nixl_types.h>
#include <backend/backend_engine.h>
#include <hip/hip_runtime.h>
#include <hipfile.h>
#include <thread>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <string>
#include <exception>
#include <cstring>
#include <variant>
#include <future>
#include <atomic>
#include "common/nixl_log.h"
#include "ais_mt_backend.h"
#include "ais_mt_utils.h"
#include "file/file_utils.h"
#include <taskflow/taskflow.hpp>
#include <unordered_map>
#include <unordered_set>

namespace {
const size_t default_thread_count = std::max(1u, std::thread::hardware_concurrency() / 2);

struct FileSegData {
    std::shared_ptr<aisMtFileHandle> handle;

    FileSegData(std::shared_ptr<aisMtFileHandle> h) : handle(std::move(h)) {}
};

struct MemSegData {
    aisMtMemBuf buf;

    MemSegData(void *addr, size_t size, int flags)
        : buf(addr, size, flags) {}
};

struct AisMtTransferRequestH {
    AisMtTransferRequestH(void *address,
                          size_t size,
                          size_t buffer_offset,
                          size_t file_offset,
                          hipFileHandle_t handle,
                          hipFileOpcode_t operation,
                          int device_id)
        : addr{address},
          size{size},
          buffer_offset{buffer_offset},
          file_offset{file_offset},
          fh{handle},
          op{operation},
          dev_id{device_id}
    {
    }

    void *addr;
    size_t size;
    size_t buffer_offset;
    size_t file_offset;
    hipFileHandle_t fh;
    hipFileOpcode_t op;
    int dev_id;
};

class nixlAisMtMetadata final : public nixlBackendMD {
public:
    explicit nixlAisMtMetadata(std::shared_ptr<aisMtFileHandle> file_handle)
        : nixlBackendMD(true),
          data_(std::in_place_type<FileSegData>, std::move(file_handle)) {}

    explicit nixlAisMtMetadata(void *addr, size_t size, int flags)
        : nixlBackendMD(true),
          data_(std::in_place_type<MemSegData>, addr, size, flags) {}

    ~nixlAisMtMetadata() = default;

    nixlAisMtMetadata(const nixlAisMtMetadata &) = delete;
    nixlAisMtMetadata &
    operator=(const nixlAisMtMetadata &) = delete;

    nixlAisMtMetadata(nixlAisMtMetadata &&) = default;
    nixlAisMtMetadata &
    operator=(nixlAisMtMetadata &&) = default;

    std::variant<FileSegData, MemSegData> data_;
};

class nixlAisMtBackendReqH final : public nixlBackendReqH {
public:
    explicit nixlAisMtBackendReqH(std::vector<AisMtTransferRequestH> list);
    ~nixlAisMtBackendReqH();

    std::vector<AisMtTransferRequestH> request_list;
    tf::Taskflow taskflow;
    std::future<nixl_status_t> running_transfer;

    nixl_status_t run();
};

size_t
getThreadCount(const nixlBackendInitParams *init_params) {
    size_t thread_count = default_thread_count;

    nixl_b_params_t *custom_params = init_params->customParams;
    if (custom_params) {
        if (custom_params->count("thread_count") > 0) {
            try {
                size_t tcount = std::stoul((*custom_params)["thread_count"]);
                if (tcount != 0) {
                    thread_count = tcount;
                }
            }
            catch (const std::exception &e) {
                throw std::runtime_error("AIS_MT: invalid thread_count parameter: " +
                                         std::string(e.what()));
            }
        }
    }
    return thread_count;
}

void reportOpError(const char *msg, hipError_t error) {
  NIXL_ERROR << msg << hipGetErrorString(error);
}

void reportOpError(const char *msg, ssize_t error) {
  if (error == -1) {
    NIXL_ERROR << msg << strerror(errno);
  } else {
    reportOpError(msg, static_cast<hipFileOpError_t>(abs(error)));
  }
}

nixl_status_t
runHipFileOp(AisMtTransferRequestH &req) {
    if (req.dev_id >= 0) {
        const hipError_t dev_err = hipSetDevice(req.dev_id);
        if (dev_err != hipSuccess) {
            reportOpError("AIS_MT: hipSetDevice failed: ", dev_err);
            return NIXL_ERR_BACKEND;
        }
    }

    ssize_t nbytes = 0;
    if (req.op == hipFileBatchRead) {
        nbytes = hipFileRead(req.fh, req.addr, req.size, req.file_offset, 0);
        if (nbytes < 0) {
            reportOpError("AIS_MT: hipFileRead failed: ", nbytes);
            return NIXL_ERR_BACKEND;
        }
    } else if (req.op == hipFileBatchWrite) {
        nbytes = hipFileWrite(req.fh, req.addr, req.size, req.file_offset, 0);
        if (nbytes < 0) {
            reportOpError("AIS_MT: hipFileWrite failed: ", nbytes);
            return NIXL_ERR_BACKEND;
        }
    } else {
        return NIXL_ERR_INVALID_PARAM;
    }

    if (size_t(nbytes) != req.size) {
        NIXL_ERROR << "AIS_MT: error: short "
                   << ((req.op == hipFileBatchRead) ? "read: " : "write: ") << nbytes << " out of "
                   << req.size << " bytes - address=" << req.addr;
        return NIXL_ERR_BACKEND;
    }

    return NIXL_SUCCESS;
}

static
const FileSegData &getRegisteredFile(const nixlMetaDesc &desc)
{
  auto *ais_metadata = static_cast<const nixlAisMtMetadata *>(desc.metadataP);
  auto *ais_fileseg = std::get_if<FileSegData>(ais_metadata? &ais_metadata->data_ : nullptr);
  if (!ais_fileseg)
    throw std::runtime_error("Unexpected: file metadata was not found in transfer request");
  return *ais_fileseg;
}

static
const MemSegData &getRegisteredBuffer(const nixlMetaDesc &desc)
{
  auto *ais_metadata = static_cast<const nixlAisMtMetadata *>(desc.metadataP);
  auto *ais_memseg = std::get_if<MemSegData>(ais_metadata? &ais_metadata->data_ : nullptr);
  if (!ais_memseg)
    throw std::runtime_error("Unexpected: mem metadata was not found in transfer request");
  return *ais_memseg;
}

} // namespace

nixlAisMtBackendReqH::nixlAisMtBackendReqH(std::vector<AisMtTransferRequestH> list) :
  request_list(std::move(list)),
  taskflow(),
  running_transfer()
{
}

nixlAisMtBackendReqH::~nixlAisMtBackendReqH() {
    if (running_transfer.valid()) {
        running_transfer.wait();
    }
}

nixl_status_t
runHipFileOp(AisMtTransferRequestH &req);

nixl_status_t
nixlAisMtBackendReqH::run() {
  for (AisMtTransferRequestH &req : request_list) {
    nixl_status_t status = runHipFileOp(req);
    if (status != NIXL_SUCCESS) {
      return status;
    }
  }
  return NIXL_SUCCESS;
}

nixlAisMtEngine::nixlAisMtEngine(const nixlBackendInitParams *init_params)
    : FileMtEngineBase<nixlAisMtEngine>(init_params),
      ais_mt_utils_(),
      thread_count_(getThreadCount(init_params)),
      executor_(std::make_unique<tf::Executor>(thread_count_)) {
    NIXL_DEBUG << "AIS_MT: thread count=" << thread_count_;
}

nixl_status_t
nixlAisMtEngine::registerMem(const nixlBlobDesc &mem,
                             const nixl_mem_t &nixl_mem,
                             nixlBackendMD *&out) {
    switch (nixl_mem) {
    case FILE_SEG: {
        auto it = ais_mt_file_map_.find(mem.devId);
        std::shared_ptr<aisMtFileHandle> handle;
        if (it != ais_mt_file_map_.end()) {
            handle = it->second.lock();
            if (handle) {
                out = new nixlAisMtMetadata(handle);
                return NIXL_SUCCESS;
            }
            ais_mt_file_map_.erase(it);
        }

        try {
            handle = std::make_shared<aisMtFileHandle>(mem.devId);
        }
        catch (const std::exception &e) {
            NIXL_ERROR << "AIS_MT: failed to create file handle: " << e.what();
            return NIXL_ERR_BACKEND;
        }
        ais_mt_file_map_[mem.devId] = handle;
        out = new nixlAisMtMetadata(handle);
        return NIXL_SUCCESS;
    }

    case VRAM_SEG: {
        const hipError_t error_id = hipSetDevice(mem.devId);
        if (error_id != hipSuccess) {
            NIXL_ERROR << "AIS_MT: error: hipSetDevice returned "
                       << hipGetErrorString(error_id) << " for device ID " << mem.devId;
            return NIXL_ERR_BACKEND;
        }
        [[fallthrough]];
    }

    case DRAM_SEG: {
        try {
            out = new nixlAisMtMetadata(std::bit_cast<void *>(mem.addr), mem.len, 0);
            return NIXL_SUCCESS;
        }
        catch (const std::exception &e) {
            NIXL_ERROR << "AIS_MT: failed to create memory buffer: " << e.what();
            return NIXL_ERR_BACKEND;
        }
    }

    default:
        return NIXL_ERR_BACKEND;
    }
}

nixl_status_t
nixlAisMtEngine::deregisterMem(nixlBackendMD *meta) {
    std::unique_ptr<nixlAisMtMetadata> md(static_cast<nixlAisMtMetadata *>(meta));

    if (auto *file_data = std::get_if<FileSegData>(&md->data_)) {
        if (file_data->handle) {
            int key = file_data->handle->fd;
            md.reset();

            auto it = ais_mt_file_map_.find(key);
            if (it != ais_mt_file_map_.end() && it->second.expired()) {
                ais_mt_file_map_.erase(it);
            }
        }
    }

    return NIXL_SUCCESS;
}

nixl_status_t
nixlAisMtEngine::prepXfer(const nixl_xfer_op_t &operation,
                                 const nixl_meta_dlist_t &local,
                                 const nixl_meta_dlist_t &remote,
                                 const std::string &remote_agent,
                                 nixlBackendReqH *&handle,
                                 const nixl_opt_b_args_t *opt_args) const {
    size_t buf_cnt = local.descCount();
    size_t file_cnt = remote.descCount();

    if ((buf_cnt != file_cnt) || ((operation != NIXL_READ) && (operation != NIXL_WRITE))) {
        NIXL_ERROR << "AIS_MT: error: incorrect count or operation selection";
        return NIXL_ERR_INVALID_PARAM;
    }

    if ((remote.getType() != FILE_SEG) && (local.getType() != FILE_SEG)) {
        NIXL_ERROR << "AIS_MT: error: backend only supports I/O between memory "
                      "(DRAM/VRAM_SEG) and "
                      "files (FILE_SEG)";
        return NIXL_ERR_INVALID_PARAM;
    }

    std::vector<AisMtTransferRequestH> request_list;
    bool is_local_file = (local.getType() == FILE_SEG);
    for (size_t i = 0; i < buf_cnt; i++) {
        const nixlMetaDesc &mem_desc = is_local_file? remote[i] : local[i];
        const nixlMetaDesc &file_desc = is_local_file? local[i] : remote[i];

        const int dev_id = mem_desc.devId;
        const MemSegData &mem_segment = getRegisteredBuffer(mem_desc);
        const FileSegData &file_segment = getRegisteredFile(file_desc);

        size_t buffer_offset = std::bit_cast<const char *>(mem_desc.addr)
                             - static_cast<const char *>(mem_segment.buf->getBaseAddr());
        request_list.emplace_back(
            mem_segment.buf->getBaseAddr(),
            mem_desc.len,
            buffer_offset,
            file_desc.addr,
            file_segment.handle->hip_fhandle,
            (operation == NIXL_READ) ? hipFileBatchRead : hipFileBatchWrite,
            dev_id);
    }

    if (request_list.empty()) {
        return NIXL_ERR_INVALID_PARAM;
    }

    handle = new nixlAisMtBackendReqH(std::move(request_list));
    return NIXL_SUCCESS;
}

nixl_status_t
nixlAisMtEngine::postXfer(const nixl_xfer_op_t &operation,
                                 const nixl_meta_dlist_t &local,
                                 const nixl_meta_dlist_t &remote,
                                 const std::string &remote_agent,
                                 nixlBackendReqH *&handle,
                                 const nixl_opt_b_args_t *opt_args) const {
    nixlAisMtBackendReqH *ais_mt_handle = (nixlAisMtBackendReqH *)handle;
    ais_mt_handle->running_transfer = executor_->async(std::bind(&nixlAisMtBackendReqH::run, ais_mt_handle));
    return NIXL_IN_PROG;
}

nixl_status_t
nixlAisMtEngine::checkXfer(nixlBackendReqH *handle) const {
    nixlAisMtBackendReqH *ais_mt_handle = (nixlAisMtBackendReqH *)handle;
    if (ais_mt_handle->running_transfer.wait_for(nixlTime::seconds(0)) !=
        std::future_status::ready) {
        return NIXL_IN_PROG;
    }
    
    nixl_status_t result = ais_mt_handle->running_transfer.get();

    std::unordered_set<int> devices;
    for (const AisMtTransferRequestH &req : ais_mt_handle->request_list) {
        if (req.dev_id >= 0) {
            devices.insert(req.dev_id);
        }
    }
    for (int dev_id : devices) {
        const hipError_t dev_err = hipSetDevice(dev_id);
        if (dev_err != hipSuccess) {
            NIXL_ERROR << "AIS_MT: hipSetDevice failed during sync: "
                       << hipGetErrorString(dev_err);
            return NIXL_ERR_BACKEND;
        }
        const hipError_t sync_err = hipDeviceSynchronize();
        if (sync_err != hipSuccess) {
            NIXL_ERROR << "AIS_MT: hipDeviceSynchronize failed: "
                       << hipGetErrorString(sync_err);
            return NIXL_ERR_BACKEND;
        }
    }

    return result;
}

nixl_status_t
nixlAisMtEngine::releaseReqH(nixlBackendReqH *handle) const {
    std::unique_ptr<nixlAisMtBackendReqH> ais_mt_handle(
        (nixlAisMtBackendReqH *)handle);
    return NIXL_SUCCESS;
}

nixl_status_t
nixlAisMtEngine::queryMem(const nixl_reg_dlist_t &descs,
                                 std::vector<nixl_query_resp_t> &resp) const {
    std::vector<nixl_blob_t> metadata(descs.descCount());
    for (int i = 0; i < descs.descCount(); ++i) {
        metadata[i] = descs[i].metaInfo;
    }

    return nixl::queryFileInfoList(metadata, resp);
}
