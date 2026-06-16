/* Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef __AIS_MT_BACKEND_H
#define __AIS_MT_BACKEND_H

#include <nixl.h>
#include <nixl_types.h>
#include <backend/backend_engine.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <hipfile.h>
#include "file_mt_engine_base.hpp"
#include "ais_mt_utils.h"
#include "taskflow/core/executor.hpp"

class nixlAisMtEngine : public FileMtEngineBase<nixlAisMtEngine> {
public:
    nixlAisMtEngine(const nixlBackendInitParams *init_params);
    ~nixlAisMtEngine() = default;

    nixlAisMtEngine(const nixlAisMtEngine &) = delete;
    nixlAisMtEngine &
    operator=(const nixlAisMtEngine &) = delete;

    nixl_status_t
    registerMem(const nixlBlobDesc &mem, const nixl_mem_t &nixl_mem, nixlBackendMD *&out) override;
    nixl_status_t
    deregisterMem(nixlBackendMD *meta) override;

    nixl_status_t
    prepXfer(const nixl_xfer_op_t &operation,
             const nixl_meta_dlist_t &local,
             const nixl_meta_dlist_t &remote,
             const std::string &remote_agent,
             nixlBackendReqH *&handle,
             const nixl_opt_b_args_t *opt_args = nullptr) const override;

    nixl_status_t
    postXfer(const nixl_xfer_op_t &operation,
             const nixl_meta_dlist_t &local,
             const nixl_meta_dlist_t &remote,
             const std::string &remote_agent,
             nixlBackendReqH *&handle,
             const nixl_opt_b_args_t *opt_args = nullptr) const override;

    nixl_status_t
    checkXfer(nixlBackendReqH *handle) const override;
    nixl_status_t
    releaseReqH(nixlBackendReqH *handle) const override;

    nixl_status_t
    queryMem(const nixl_reg_dlist_t &descs, std::vector<nixl_query_resp_t> &resp) const override;

private:
    aisMtUtil ais_mt_utils_;
    std::unordered_map<int, std::weak_ptr<aisMtFileHandle>> ais_mt_file_map_;
    size_t thread_count_;
    std::unique_ptr<tf::Executor> executor_;
};
#endif
