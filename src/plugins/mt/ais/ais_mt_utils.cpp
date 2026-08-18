/* Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include "common/nixl_log.h"
#include "ais_mt_utils.h"

namespace {
bool
aisMtCompatModeAllowed() {
    const char *v = std::getenv("HIPFILE_ALLOW_COMPAT_MODE");
    if (v == nullptr || v[0] == '\0') {
        return false;
    }
    return std::strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0 || strcasecmp(v, "yes") == 0;
}
} // namespace

aisMtUtil::aisMtUtil() {
    const hipFileError_t status = hipFileDriverOpen();
    if (status.err != hipFileSuccess) {
        throw std::runtime_error(
            "AIS_MT: error initializing AMD Infinity Storage driver: error=" +
            std::to_string(status.err));
    }
}

aisMtUtil::~aisMtUtil() {
    (void)hipFileDriverClose();
}

aisMtMemBuf::aisMtMemBuf(void *ptr, size_t sz, int flags) : base_(ptr) {
    hipError_t err;
    hipPointerAttribute_t attr;
    err = hipPointerGetAttributes(&attr, ptr);
    if (err != hipSuccess || attr.type == hipMemoryTypeUnregistered) {
        // Unregistered memory is host memory
        err = hipHostRegister(ptr, sz, hipHostRegisterDefault);

        type = hipMemoryTypeHost;
    }
    else {
        type = hipMemoryTypeDevice;

        hipFileError_t status = hipFileBufRegister(ptr, sz, flags);
        if (status.err != hipFileSuccess) {
            const char *err_description{HIPFILE_ERRSTR(status.err)};
            if (aisMtCompatModeAllowed()) {
                NIXL_WARN << "AIS_MT: buffer registration failed - compat mode: "
                    << err_description;
            } else {
                throw std::runtime_error(
                        "AIS_MT: hipFileBufRegister failed (" + std::string(err_description) +
                        "); set HIPFILE_ALLOW_COMPAT_MODE=true to allow fallback");
            }
        }
    }
}

aisMtMemBuf::~aisMtMemBuf() {
    if (type == hipMemoryTypeHost) {
        const hipError_t err = hipHostUnregister(base_);
        if (err != hipSuccess) {
            NIXL_WARN << "AIS_MT: warning: deregistering buffer error: " << hipGetErrorString(err)
                      << "; ptr=" << base_;
        }
    }
    if (type == hipMemoryTypeDevice) {
        const hipFileError_t status = hipFileBufDeregister(base_);
        if (status.err != hipFileSuccess) {
            NIXL_WARN << "AIS_MT: warning: deregistering buffer error: " << hipFileGetOpErrorString(status.err)
                      << "; ptr=" << base_;
        }
    }
}

aisMtFileHandle::aisMtFileHandle(int file_fd) : fd(file_fd) {
    hipFileDescr_t descr = {};
    descr.handle.fd = fd;
    descr.type = hipFileHandleTypeOpaqueFD;

    const hipFileError_t status = hipFileHandleRegister(&hip_fhandle, &descr);
    if (status.err != hipFileSuccess) {
        throw std::runtime_error("AIS_MT: file register error: error=" +
                                 std::to_string(status.err) + ", fd=" + std::to_string(fd));
    }
}

aisMtFileHandle::~aisMtFileHandle() {
    (void)hipFileHandleDeregister(hip_fhandle);
}
