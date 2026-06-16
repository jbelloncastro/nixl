# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

subdir('common')

disable_gds_backend = get_option('disable_gds_backend')
disable_rocm_ais_backend = get_option('disable_rocm_ais_backend')

build_gds_mt = enabled_plugins.get('GDS_MT') and not disable_gds_backend and cuda_dep.found() and taskflow_proj.found()
build_ais_mt = enabled_plugins.get('AIS_MT') and not disable_rocm_ais_backend and hip_dep.found() and taskflow_proj.found()

if build_gds_mt
    subdir('gds')
endif

if build_ais_mt
    subdir('ais')
endif
