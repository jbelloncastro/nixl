#!/bin/bash
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
# See the License for the specific permissions and limitations under the
# License.
#
# Run inside a gpu-test (or similar) image to sanity-check NIXL install layout,
# nixlbench binaries, the UCX plugin, UCX CUDA/ROCm transport libraries, hipFile
# libraries, the AIS_MT plugin, and the ais-stats CLI from rocm-systems hipFile.
#
# Usage (from repo host, image need not include this file yet):
#   docker run --rm -v "$PWD/.ci/scripts/verify-gpu-test-contents.sh:/tmp/v.sh:ro" \
#     YOUR_TAG bash /tmp/v.sh
#
# After a gpu-test build with COPY . ., the script also lives under:
#   /workspace/nixl/.ci/scripts/verify-gpu-test-contents.sh
#
# Optional environment (inside the container):
#   VERIFY_HIPFILE_LIB_ROOTS — space-separated prefixes (each has lib/ scanned
#     for libhipfile.so*). Default: ${ROCM_PATH:-/opt/rocm} /opt/rocs-ais /usr/local
#   VERIFY_AIS_STATS_BIN — explicit path to ais-stats when not in PATH

set -euo pipefail

P="${NIXL_INSTALL_DIR:-/opt/nixl}"
fail=0
warn=0

section() {
    printf '\n=== %s ===\n' "$1"
}

note_warn() {
    echo "WARN: $*"
    warn=$((warn + 1))
}

note_fail() {
    echo "FAIL: $*"
    fail=$((fail + 1))
}

arch="$(uname -m)"
case "$arch" in
    aarch64) libarch="aarch64-linux-gnu" ;;
    x86_64) libarch="x86_64-linux-gnu" ;;
    *) libarch="$arch-linux-gnu" ;;
esac

ais_mt_plugin="${P}/lib/${libarch}/plugins/libplugin_AIS_MT.so"
PLUG="${P}/lib/${libarch}/plugins"
ROCM="${ROCM_PATH:-/opt/rocm}"

# Optional: space-separated list of install prefixes to search for libhipfile.so*
# under <prefix>/lib (Meson AIS_MT search: ROCM_PATH, /opt/rocs-ais, /usr/local).
if [[ -n "${VERIFY_HIPFILE_LIB_ROOTS:-}" ]]; then
    # shellcheck disable=SC2206
    HIPFILE_LIB_ROOTS=( ${VERIFY_HIPFILE_LIB_ROOTS} )
else
    HIPFILE_LIB_ROOTS=( "${ROCM}" /opt/rocs-ais /usr/local )
fi

_find_ais_stats_bin() {
    if [[ -n "${VERIFY_AIS_STATS_BIN:-}" && -x "${VERIFY_AIS_STATS_BIN}" ]]; then
        printf '%s\n' "${VERIFY_AIS_STATS_BIN}"
        return 0
    fi
    local p
    for p in "${ROCM}/bin/ais-stats" "/opt/rocs-ais/bin/ais-stats"; do
        if [[ -x "$p" ]]; then
            printf '%s\n' "$p"
            return 0
        fi
    done
    local _path="${ROCM}/bin:/opt/rocs-ais/bin:${PATH:-}"
    if PATH="${_path}" command -v ais-stats >/dev/null 2>&1; then
        PATH="${_path}" command -v ais-stats
        return 0
    fi
    return 1
}

section "Install prefix"
if [[ ! -d "$P" ]]; then
    note_fail "missing directory: $P"
else
    echo "OK: $P"
fi

section "Core NIXL libraries"
for f in "${P}/lib/${libarch}/libnixl.so" "${P}/lib/${libarch}/libnixl_build.so"; do
    if [[ -f "$f" ]]; then
        echo "OK: $f"
    else
        note_fail "missing: $f"
    fi
done

section "Plugin directory"
if [[ ! -d "$PLUG" ]]; then
    note_fail "missing: $PLUG"
else
    echo "OK: $PLUG ($(find "$PLUG" -maxdepth 1 -name 'libplugin_*.so' | wc -l) plugins)"
    ls -la "$PLUG"/libplugin_*.so 2>/dev/null || note_warn "no libplugin_*.so in $PLUG"
fi

_have_rocm_hip=0
if ls "${ROCM}/lib"/libamdhip64.so* >/dev/null 2>&1; then
    _have_rocm_hip=1
fi

section "hipFile libraries (libhipfile)"
_have_hipfile=0
for root in "${HIPFILE_LIB_ROOTS[@]}"; do
    [[ -d "${root}/lib" ]] || continue
    while IFS= read -r -d '' hf; do
        _have_hipfile=1
        echo "OK: $hf"
        ls -la "$hf" 2>/dev/null || true
    done < <(find "${root}/lib" -maxdepth 1 \( -name 'libhipfile.so' -o -name 'libhipfile.so.*' \) -print0 2>/dev/null || true)
done
if [[ "$_have_hipfile" -eq 0 ]]; then
    echo "OK: no libhipfile under searched roots (${HIPFILE_LIB_ROOTS[*]})"
fi

section "UCX backend plugin"
ucx_plugin="${PLUG}/libplugin_UCX.so"
if [[ ! -f "$ucx_plugin" ]]; then
    note_fail "missing: $ucx_plugin"
else
    echo "OK: $ucx_plugin"
    echo "--- readelf NEEDED (first level) ---"
    if command -v readelf >/dev/null 2>&1; then
        readelf -d "$ucx_plugin" | grep NEEDED || true
    else
        echo "(readelf not installed; skipping)"
    fi
    echo "--- ldd (full; CUDA/ROCm names may be indirect) ---"
    ldd "$ucx_plugin" 2>&1 | sed 's/^/    /' || note_fail "ldd failed on $ucx_plugin"
fi

section "UCX transport modules (CUDA / ROCm hints)"
# UCX install prefix matches NIXL in CI (same --prefix).
found_cuda=0
found_rocm=0
while IFS= read -r -d '' so; do
    case "$so" in
        *cuda*) found_cuda=1 ;;
        *rocm*) found_rocm=1 ;;
    esac
    echo "  $so"
done < <(find "${P}/lib" -maxdepth 4 \( -iname '*cuda*.so*' -o -iname '*rocm*.so*' \) -print0 2>/dev/null || true)

if [[ "$found_cuda" -eq 0 ]]; then
    note_warn "no *cuda*.so under ${P}/lib (UCX may be CPU-only or layout differs)"
fi
if [[ -d "${ROCM}/lib" ]] && ls "${ROCM}/lib"/libamdhip64.so* >/dev/null 2>&1; then
    if [[ "$found_rocm" -eq 0 ]]; then
        note_warn "ROCm present but no *rocm*.so under ${P}/lib (check UCX --with-rocm build)"
    fi
fi

section "nixlbench binaries"
if [[ -x "${P}/bin/nixlbench" ]]; then
    echo "OK: ${P}/bin/nixlbench"
    file "${P}/bin/nixlbench" || true
else
    note_fail "missing or not executable: ${P}/bin/nixlbench"
fi

if [[ -x "${P}/bin/nixlbench-rocm" ]]; then
    echo "OK: ${P}/bin/nixlbench-rocm (dual HIP build)"
    file "${P}/bin/nixlbench-rocm" || true
else
    if ls "${ROCM}/lib"/libamdhip64.so* >/dev/null 2>&1; then
        note_warn "no ${P}/bin/nixlbench-rocm (expected when CUDA+ROCm dual nixlbench ran)"
    else
        echo "OK: no nixlbench-rocm (ROCm HIP not expected in this image)"
    fi
fi

section "AIS_MT plugin (ROCm / hipFile)"
if [[ -f "$ais_mt_plugin" ]]; then
    echo "OK: $ais_mt_plugin"
    file "$ais_mt_plugin" || true
elif [[ "$_have_rocm_hip" -eq 1 ]] && [[ "$_have_hipfile" -eq 1 ]]; then
    note_fail "missing $ais_mt_plugin but HIP and libhipfile are present (expect AIS_MT built)"
elif [[ "$_have_rocm_hip" -eq 1 ]] && [[ "$_have_hipfile" -eq 0 ]]; then
    note_warn "ROCm HIP present but no libhipfile (e.g. aarch64 stub); AIS_MT not expected"
else
    echo "OK: AIS_MT not required (no full ROCm + hipFile layout)"
fi

section "ais-stats (hipFile stats CLI)"
# Built when rocm-systems hipFile is configured with AIS_INSTALL_TOOLS (see
# upstream tools/ais-stats). VERIFY_AIS_STATS_BIN overrides discovery.
_ais_stats_bin=
if _ais_stats_bin="$(_find_ais_stats_bin)" && [[ -n "$_ais_stats_bin" ]]; then
    echo "OK: ${_ais_stats_bin}"
    file "$_ais_stats_bin" || true
else
    if [[ -f "$ais_mt_plugin" ]]; then
        note_fail "AIS_MT present but ais-stats not found (install hipFile tools; set VERIFY_AIS_STATS_BIN=...)"
    elif [[ "$_have_rocm_hip" -eq 1 ]] && [[ "$_have_hipfile" -eq 1 ]]; then
        note_warn "HIP + libhipfile present but ais-stats not on PATH or under ${ROCM}/bin (optional tooling)"
    else
        echo "OK: ais-stats not required (no AIS_MT / hipFile test stack)"
    fi
fi

section "UCX introspection (ucx_info)"
# cuda-dl-base images put HPC-X UCX on PATH first; always prefer UCX installed
# next to NIXL (${P}) so "Configured with" matches the build from .gitlab/build.sh.
_ucx_info="${P}/bin/ucx_info"
_ucx_ldpath="${P}/lib:${P}/lib/${libarch}"
if [[ -x "${_ucx_info}" ]]; then
    echo "Using: ${_ucx_info}"
    env \
        PATH="${P}/bin:${PATH}" \
        LD_LIBRARY_PATH="${_ucx_ldpath}:${LD_LIBRARY_PATH:-}" \
        UCX_TLS="${UCX_TLS:-^cuda_ipc}" \
        "${_ucx_info}" -v 2>&1 | head -40 || true
else
    echo "(skipped: ${_ucx_info} not executable — UCX may use a different layout)"
fi

section "Summary"
echo "warnings: $warn  failures: $fail"
if [[ "$fail" -gt 0 ]]; then
    exit 1
fi
exit 0
