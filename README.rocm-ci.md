# ROCm and CUDA Based NIXL CI Setup and Build

## Build Process

To build the docker-based CI setup and build for both CUDA and ROCm versions
of NIXL and nixlbench do the following. Note the first step is needed if you
cannot access the Mellanox registry which houses the infinia stub.
```
docker build -f .ci/dockerfiles/Dockerfile.infinia-stub -t infinia-libs:stub .
docker build -f .ci/dockerfiles/Dockerfile.base --build-arg NIXL_INSTALL_DIR=/opt/nixl --build-arg INFINIA_LIBS_IMAGE=infinia-libs:stub --build-arg PRE_INSTALLED_NIXL_ENV=1 -t nixl-ci-base:latest .
docker build -f .ci/dockerfiles/Dockerfile.gpu-test --build-arg PRE_INSTALLED_ENV=1 --build-arg BASE_IMAGE=docker.io/library/nixl-ci-base:latest -t nixl-gpu-test:latest .
```

`PRE_INSTALLED_ENV=1` skips the large `apt-get` block in `.gitlab/build.sh`; the
image must already provide `python3`, `meson`, `ninja`, `git`, and a C++
compiler (as after `Dockerfile.base`). The default `BASE_IMAGE` on
`Dockerfile.gpu-test` is the bare CUDA stack—always pass
`--build-arg BASE_IMAGE=...` when using `PRE_INSTALLED_ENV`, or omit
`PRE_INSTALLED_ENV` for a full bootstrap on that CUDA image.

## Build Validation

Verify what landed in the image (the script runs **inside** the container).
From the repo root, bind-mount the checker, or use the path below after a
`gpu-test` build that used `COPY . .`:

```bash
docker run --rm -v "$PWD/.ci/scripts/verify-gpu-test-contents.sh:/tmp/v.sh:ro" \
  nixl-gpu-test:latest bash /tmp/v.sh
```

### Validation results (example)

The transcript below is from an older image; plugin counts and paths may not
match your build.

Sample output from `verify-gpu-test-contents.sh`:

```bash
$ docker run --rm -v "$PWD/.ci/scripts/verify-gpu-test-contents.sh:/tmp/v.sh:ro" \
  nixl-gpu-test:latest bash /tmp/v.sh

=== Install prefix ===
OK: /opt/nixl

=== Core NIXL libraries ===
OK: /opt/nixl/lib/x86_64-linux-gnu/libnixl.so
OK: /opt/nixl/lib/x86_64-linux-gnu/libnixl_build.so

=== Plugin directory ===
OK: /opt/nixl/lib/x86_64-linux-gnu/plugins (12 plugins)
-rwxr-xr-x 1 svc-nixl dip  4634408 Jun 17 20:48 /opt/nixl/lib/x86_64-linux-gnu/plugins/libplugin_AZURE_BLOB.so
-rwxr-xr-x 1 svc-nixl dip  1364512 Jun 17 20:48 /opt/nixl/lib/x86_64-linux-gnu/plugins/libplugin_GDS.so
-rwxr-xr-x 1 svc-nixl dip  4235528 Jun 17 20:48 /opt/nixl/lib/x86_64-linux-gnu/plugins/libplugin_GDS_MT.so
-rwxr-xr-x 1 svc-nixl dip 25269360 Jun 17 20:48 /opt/nixl/lib/x86_64-linux-gnu/plugins/libplugin_GPUNETIO.so
-rwxr-xr-x 1 svc-nixl dip  1406296 Jun 17 20:48 /opt/nixl/lib/x86_64-linux-gnu/plugins/libplugin_GUSLI.so
-rwxr-xr-x 1 svc-nixl dip  7301448 Jun 17 20:48 /opt/nixl/lib/x86_64-linux-gnu/plugins/libplugin_LIBFABRIC.so
-rwxr-xr-x 1 svc-nixl dip  5650000 Jun 17 20:48 /opt/nixl/lib/x86_64-linux-gnu/plugins/libplugin_MOCK_BACKEND.so
-rwxr-xr-x 1 svc-nixl dip  1664368 Jun 17 20:48 /opt/nixl/lib/x86_64-linux-gnu/plugins/libplugin_Mooncake.so
-rwxr-xr-x 1 svc-nixl dip  6317496 Jun 17 20:48 /opt/nixl/lib/x86_64-linux-gnu/plugins/libplugin_OBJ.so
-rwxr-xr-x 1 svc-nixl dip  2221160 Jun 17 20:48 /opt/nixl/lib/x86_64-linux-gnu/plugins/libplugin_POSIX.so
-rwxr-xr-x 1 svc-nixl dip  2182032 Jun 17 20:48 /opt/nixl/lib/x86_64-linux-gnu/plugins/libplugin_UCCL.so
-rwxr-xr-x 1 svc-nixl dip  5145336 Jun 17 20:48 /opt/nixl/lib/x86_64-linux-gnu/plugins/libplugin_UCX.so

=== UCX backend plugin ===
OK: /opt/nixl/lib/x86_64-linux-gnu/plugins/libplugin_UCX.so
--- readelf NEEDED (first level) ---
 0x0000000000000001 (NEEDED)             Shared library: [libnixl_common.so]
 0x0000000000000001 (NEEDED)             Shared library: [libnixl_build.so]
 0x0000000000000001 (NEEDED)             Shared library: [libserdes.so]
 0x0000000000000001 (NEEDED)             Shared library: [libabsl_log_internal_message.so.2508.0.0]
 0x0000000000000001 (NEEDED)             Shared library: [libabsl_log_internal_nullguard.so.2508.0.0]
 0x0000000000000001 (NEEDED)             Shared library: [libabsl_vlog_config_internal.so.2508.0.0]
 0x0000000000000001 (NEEDED)             Shared library: [libabsl_strings.so.2508.0.0]
 0x0000000000000001 (NEEDED)             Shared library: [libucp.so.0]
 0x0000000000000001 (NEEDED)             Shared library: [libucs.so.0]
 0x0000000000000001 (NEEDED)             Shared library: [libstdc++.so.6]
 0x0000000000000001 (NEEDED)             Shared library: [libgcc_s.so.1]
 0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
 0x0000000000000001 (NEEDED)             Shared library: [ld-linux-x86-64.so.2]
--- ldd (full; CUDA/ROCm names may be indirect) ---
        linux-vdso.so.1 (0x00007aa369df2000)
        libnixl_common.so => /opt/nixl/lib/x86_64-linux-gnu/plugins/../libnixl_common.so (0x00007aa369bf5000)
        libnixl_build.so => /opt/nixl/lib/x86_64-linux-gnu/plugins/../libnixl_build.so (0x00007aa369b68000)
        libserdes.so => /opt/nixl/lib/x86_64-linux-gnu/plugins/../libserdes.so (0x00007aa369b5a000)
        libabsl_log_internal_message.so.2508.0.0 => /opt/nixl/lib/libabsl_log_internal_message.so.2508.0.0 (0x00007aa369b4b000)
        libabsl_log_internal_nullguard.so.2508.0.0 => /opt/nixl/lib/libabsl_log_internal_nullguard.so.2508.0.0 (0x00007aa369b46000)
        libabsl_vlog_config_internal.so.2508.0.0 => /opt/nixl/lib/libabsl_vlog_config_internal.so.2508.0.0 (0x00007aa369b3b000)
        libabsl_strings.so.2508.0.0 => /opt/nixl/lib/libabsl_strings.so.2508.0.0 (0x00007aa369b0f000)
        libucp.so.0 => /opt/nixl/lib/libucp.so.0 (0x00007aa369a0c000)
        libucs.so.0 => /opt/nixl/lib/libucs.so.0 (0x00007aa369992000)
        libstdc++.so.6 => /lib/x86_64-linux-gnu/libstdc++.so.6 (0x00007aa369704000)
        libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x00007aa3696d6000)
        libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007aa3694c2000)
        /lib64/ld-linux-x86-64.so.2 (0x00007aa369df4000)
        libabsl_log_globals.so.2508.0.0 => not found
        libabsl_hash.so.2508.0.0 => not found
        libabsl_throw_delegate.so.2508.0.0 => not found
        libabsl_raw_logging_internal.so.2508.0.0 => not found
        libabsl_log_initialize.so.2508.0.0 => not found
        libabsl_raw_hash_set.so.2508.0.0 => not found
        libabsl_examine_stack.so.2508.0.0 => /opt/nixl/lib/libabsl_examine_stack.so.2508.0.0 (0x00007aa3694bb000)
        libabsl_log_internal_format.so.2508.0.0 => /opt/nixl/lib/libabsl_log_internal_format.so.2508.0.0 (0x00007aa3694b6000)
        libabsl_log_internal_structured_proto.so.2508.0.0 => /opt/nixl/lib/libabsl_log_internal_structured_proto.so.2508.0.0 (0x00007aa3694b1000)
        libabsl_strerror.so.2508.0.0 => /opt/nixl/lib/libabsl_strerror.so.2508.0.0 (0x00007aa3694aa000)
        libabsl_log_internal_log_sink_set.so.2508.0.0 => /opt/nixl/lib/libabsl_log_internal_log_sink_set.so.2508.0.0 (0x00007aa3694a3000)
        libabsl_log_internal_globals.so.2508.0.0 => /opt/nixl/lib/libabsl_log_internal_globals.so.2508.0.0 (0x00007aa36949e000)
        libabsl_log_globals.so.2508.0.0 => /opt/nixl/lib/libabsl_log_globals.so.2508.0.0 (0x00007aa369498000)
        libabsl_log_internal_proto.so.2508.0.0 => /opt/nixl/lib/libabsl_log_internal_proto.so.2508.0.0 (0x00007aa369493000)
        libabsl_time.so.2508.0.0 => /opt/nixl/lib/libabsl_time.so.2508.0.0 (0x00007aa36947a000)
        libabsl_strings_internal.so.2508.0.0 => /opt/nixl/lib/libabsl_strings_internal.so.2508.0.0 (0x00007aa369474000)
        libabsl_base.so.2508.0.0 => /opt/nixl/lib/libabsl_base.so.2508.0.0 (0x00007aa36946d000)
        libabsl_raw_logging_internal.so.2508.0.0 => /opt/nixl/lib/libabsl_raw_logging_internal.so.2508.0.0 (0x00007aa369468000)
        libabsl_log_internal_fnmatch.so.2508.0.0 => /opt/nixl/lib/libabsl_log_internal_fnmatch.so.2508.0.0 (0x00007aa369463000)
        libabsl_synchronization.so.2508.0.0 => /opt/nixl/lib/libabsl_synchronization.so.2508.0.0 (0x00007aa36944f000)
        libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007aa369366000)
        libuct.so.0 => /opt/nixl/lib/libuct.so.0 (0x00007aa369325000)
        libucm.so.0 => /opt/nixl/lib/libucm.so.0 (0x00007aa369307000)
        libabsl_stacktrace.so.2508.0.0 => /opt/nixl/lib/libabsl_stacktrace.so.2508.0.0 (0x00007aa369301000)
        libabsl_symbolize.so.2508.0.0 => /opt/nixl/lib/libabsl_symbolize.so.2508.0.0 (0x00007aa3692f6000)
        libabsl_str_format_internal.so.2508.0.0 => /opt/nixl/lib/libabsl_str_format_internal.so.2508.0.0 (0x00007aa3692d6000)
        libabsl_log_sink.so.2508.0.0 => /opt/nixl/lib/libabsl_log_sink.so.2508.0.0 (0x00007aa3692d1000)
        libabsl_spinlock_wait.so.2508.0.0 => /opt/nixl/lib/libabsl_spinlock_wait.so.2508.0.0 (0x00007aa3692cc000)
        libabsl_hash.so.2508.0.0 => /opt/nixl/lib/libabsl_hash.so.2508.0.0 (0x00007aa3692c7000)
        libabsl_time_zone.so.2508.0.0 => /opt/nixl/lib/libabsl_time_zone.so.2508.0.0 (0x00007aa3692a2000)
        libabsl_kernel_timeout_internal.so.2508.0.0 => /opt/nixl/lib/libabsl_kernel_timeout_internal.so.2508.0.0 (0x00007aa36929d000)
        libabsl_tracing_internal.so.2508.0.0 => /opt/nixl/lib/libabsl_tracing_internal.so.2508.0.0 (0x00007aa369298000)
        libabsl_malloc_internal.so.2508.0.0 => /opt/nixl/lib/libabsl_malloc_internal.so.2508.0.0 (0x00007aa369291000)
        libabsl_debugging_internal.so.2508.0.0 => /opt/nixl/lib/libabsl_debugging_internal.so.2508.0.0 (0x00007aa36928a000)
        libabsl_demangle_internal.so.2508.0.0 => /opt/nixl/lib/libabsl_demangle_internal.so.2508.0.0 (0x00007aa369277000)
        libabsl_int128.so.2508.0.0 => /opt/nixl/lib/libabsl_int128.so.2508.0.0 (0x00007aa36926e000)
        libabsl_city.so.2508.0.0 => /opt/nixl/lib/libabsl_city.so.2508.0.0 (0x00007aa369269000)
        libabsl_demangle_rust.so.2508.0.0 => /opt/nixl/lib/libabsl_demangle_rust.so.2508.0.0 (0x00007aa369262000)
        libabsl_decode_rust_punycode.so.2508.0.0 => /opt/nixl/lib/libabsl_decode_rust_punycode.so.2508.0.0 (0x00007aa36925d000)
        libabsl_utf8_for_code_point.so.2508.0.0 => /opt/nixl/lib/libabsl_utf8_for_code_point.so.2508.0.0 (0x00007aa369256000)

=== UCX transport modules (CUDA / ROCm hints) ===
  /opt/nixl/lib/ucx/libuct_rocm.so
  /opt/nixl/lib/ucx/libucx_perftest_rocm.so.0
  /opt/nixl/lib/ucx/libuct_rocm.so.0
  /opt/nixl/lib/ucx/libucx_perftest_rocm.so
  /opt/nixl/lib/ucx/libucx_perftest_rocm.so.0.0.0
  /opt/nixl/lib/ucx/libucm_rocm.so
  /opt/nixl/lib/ucx/libucm_rocm.so.0.0.0
  /opt/nixl/lib/ucx/libucm_rocm.so.0
  /opt/nixl/lib/ucx/libuct_rocm.so.0.0.0
  /opt/nixl/lib/ucx/libuct_cuda.so.0
  /opt/nixl/lib/ucx/libucm_cuda.so.0.0.0
  /opt/nixl/lib/ucx/libuct_cuda.so.0.0.0
  /opt/nixl/lib/ucx/libucx_perftest_cuda.so.0.0.0
  /opt/nixl/lib/ucx/libucx_perftest_cuda.so.0
  /opt/nixl/lib/ucx/libucm_cuda.so.0
  /opt/nixl/lib/ucx/libucx_perftest_cuda.so
  /opt/nixl/lib/ucx/libucm_cuda.so
  /opt/nixl/lib/ucx/libuct_cuda.so

=== nixlbench binaries ===
OK: /opt/nixl/bin/nixlbench
/opt/nixl/bin/nixlbench: ELF 64-bit LSB pie executable, x86-64, version 1 (GNU/Linux), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=d34f7913843e22efd52572d8356180d4d5745b46, for GNU/Linux 3.2.0, not stripped
OK: /opt/nixl/bin/nixlbench-rocm (dual HIP build)
/opt/nixl/bin/nixlbench-rocm: ELF 64-bit LSB pie executable, x86-64, version 1 (GNU/Linux), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=74e6e79bc9d80fa3e359062386a75e71f8b81aab, for GNU/Linux 3.2.0, not stripped

=== UCX introspection (ucx_info) ===
Using: /opt/nixl/bin/ucx_info
# Library path should be under /opt/nixl/lib (not /opt/hpcx/ucx).
# "Configured with" should match the UCX built in .gitlab/build.sh.

=== Summary ===
warnings: 0  failures: 0
```

## AIS_MT on AMD GPU (docker + nixlbench-rocm)

For an interactive AIS_MT run on VRAM with an NVMe-backed directory mounted
into the container, hipFile stats (`HIPFILE_STATS_LEVEL`), and `ais-stats`
wrapping `nixlbench-rocm`, use
[`benchmark/nixlbench/contrib/run-ais-mt-amd-gpu-test.sh`](benchmark/nixlbench/contrib/run-ais-mt-amd-gpu-test.sh).

Print a host-ready `docker run` template (mount, devices, env):

```bash
./benchmark/nixlbench/contrib/run-ais-mt-amd-gpu-test.sh print-docker-run
```

Then set `HOST_NVME_DIR`, `IMAGE`, and `TESTFILE` in your shell and run the
printed command. On the host, prefer mounting a directory on a filesystem that
sits on a stable NVMe identity path under `/dev/disk/by-id/`. Inside the
container, set `FILEPATH` to a **directory** under the mount (nixlbench creates
`nixlbench_ais_mt_test_file_*` files inside it; a single pre-created `.bin`
file path will fail with “Not a directory”). The script runs
**single-process** AIS_MT (null runtime; no etcd) which matches nixlbench’s
storage-backend layout.

If the container reports `No such file or directory` for
`run-ais-mt-amd-gpu-test.sh`, rebuild the gpu-test image from a tree that
includes that script, or bind-mount it from the host (see the `NIXL_SRC` line in
`print-docker-run`).

If `--init-file` fails with `mkdir: … Permission denied`, the image user
(typically UID 148069 from `Dockerfile.base`) cannot create directories on your
host-owned bind mount. On the host run `mkdir -p` for the path you use as
`FILEPATH`, then run **without** `--init-file`, or add
`--user "$(id -u):$(id -g)"` to `docker run` (and `--group-add` for `video` /
`render` if `/dev/dri` needs those groups). Do not paste placeholder lines (for
example `...same devices`); use a full command. From repo root, bind-mounting
the contrib script (omit that `-v` if the image already contains it):

```bash
docker run --rm -it \
  --user "$(id -u):$(id -g)" \
  --group-add "$(getent group video  | cut -d: -f3)" \
  --group-add "$(getent group render | cut -d: -f3)" \
  --device=/dev/kfd \
  --device=/dev/dri \
  --ipc=host \
  --security-opt seccomp=unconfined \
  -v /mnt/nvme-nixlbench:/mnt/nvme-nixlbench:rw \
  -v "$PWD/benchmark/nixlbench/contrib/run-ais-mt-amd-gpu-test.sh:/workspace/nixl/benchmark/nixlbench/contrib/run-ais-mt-amd-gpu-test.sh:ro" \
  -e FILEPATH=/mnt/nvme-nixlbench/nixlbench-ais-mt \
  -e HIPFILE_STATS_LEVEL=1 \
  nixl-gpu-test:latest \
  bash /workspace/nixl/benchmark/nixlbench/contrib/run-ais-mt-amd-gpu-test.sh run
```

The contrib script sets `HOME=/tmp` when the image `HOME` is not executable for
the current UID (typical with `docker --user` while `ENV HOME` still points at
`/home/svc-nixl`), so NIXL does not resolve `$HOME/.nixl.cfg` under that path.
Alternatively set `NIXL_CONFIG_FILE` to a readable file or pass `-e HOME=/tmp`
before the benchmark runs.

If nixlbench fails with `Permission denied` while **creating** files under
`FILEPATH`, the directory exists but is not writable for the container UID
(same UID mismatch as above). Grant write access, for example on the host
`sudo chown -R 148069:30 /mnt/.../nixlbench-ais-mt` (use your image’s `_UID` /
`_GID` from `Dockerfile.base` if you changed them), or use the `docker run`
example block above with `--user "$(id -u):$(id -g)"`.

```bash
stebates@snoc-thinkstation:~/Projects/nixl$ docker run --rm -it   --user "$(id -u):$(id -g)"   --group-add "$(getent group video  | cut -d: -f3)"   --group-add "$(getent group render | cut -d: -f3)"   --device=/dev/kfd   --device=/dev/dri   --ipc=host   --security-opt seccomp=unconfined   -v /mnt/nvme-nixlbench:/mnt/nvme-nixlbench:rw   -v "$PWD/benchmark/nixlbench/contrib/run-ais-mt-amd-gpu-test.sh:/workspace/nixl/benchmark/nixlbench/contrib/run-ais-mt-amd-gpu-test.sh:ro"   -e FILEPATH=/mnt/nvme-nixlbench/nixlbench-ais-mt   -e HIPFILE_STATS_LEVEL=1   nixl-gpu-test:latest   bash /workspace/nixl/benchmark/nixlbench/contrib/run-ais-mt-amd-gpu-test.sh run
HIPFILE_STATS_LEVEL=1 (ais-stats + nixlbench log: /tmp/ais-mt-ais-stats.uYulz6.log)
Running: /opt/nixl/bin/nixlbench-rocm --backend AIS_MT --filepath /mnt/nvme-nixlbench/nixlbench-ais-mt --initiator_seg_type VRAM --target_seg_type VRAM --gds_mt_num_threads 4 --total_buffer_size 67108864 --start_block_size 65536 --max_block_size 65536 --start_batch_size 1 --max_batch_size 1 --num_iter 200 --warmup_iter 40 --op_type WRITE --check_consistency
WARNING: Adjusting num_iter to 208 to allow equal distribution to 1 threads
WARNING: Adjusting warmup_iter to 48 to allow equal distribution to 1 threads
Using null runtime for storage backend without ETCD
AIS_MT backend
AIS_MT thread_count: 4
Waiting for all processes to start... (expecting 1 total: 1 initiators and 1 targets)
All processes are ready to proceed
Creating file: /mnt/nvme-nixlbench/nixlbench-ais-mt/nixlbench_ais_mt_test_file_initiator_0
****************************************************************************************************************************************************************
NIXLBench Configuration
****************************************************************************************************************************************************************
Runtime (--runtime_type=[ETCD,ASIO])                        : ETCD
ETCD Endpoint                                               : disabled (storage backend)
Worker type (--worker_type=[nixl,nvshmem])                  : nixl
Backend (--backend=[UCX,GDS,GDS_MT,AIS_MT,POSIX,Mooncake,HF3FS,OBJ,AZURE_BLOB]): AIS_MT
Enable pt (--enable_pt=[0,1])                               : 0
Progress threads (--progress_threads=N)                     : 0
Device list (--device_list=dev1,dev2,...)                   : all
Enable VMM (--enable_vmm=[0,1])                             : 0
Recreate xfer each iteration (--recreate_xfer=[0,1])        : 0
Re-register memory each iteration (--reregister_mem=[0,1])  : 0
Prepared xfer (prep+make) (--prepared_xfer=[0,1])           : 0
Pipeline depth (--pipeline_depth=N)                         : 1
Use hugepages (--use_hugepages=[0,1])                       : 0
GDS_MT / AIS_MT thread pool (--gds_mt_num_threads=N)        : 4
filepath (--filepath=path)                                  : /mnt/nvme-nixlbench/nixlbench-ais-mt
filenames (--filenames=filename1,filename2,...)             :
Number of files (--num_files=N)                             : 1
Storage enable direct (--storage_enable_direct=[0,1])       : 0
Initiator seg type (--initiator_seg_type=[DRAM,VRAM])       : VRAM
Target seg type (--target_seg_type=[DRAM,VRAM])             : VRAM
Scheme (--scheme=[pairwise,manytoone,onetomany,tp])         : pairwise
Mode (--mode=[SG,MG])                                       : SG
Op type (--op_type=[READ,WRITE])                            : WRITE
Check consistency (--check_consistency=[0,1])               : 1
Total buffer size (--total_buffer_size=N)                   : 67108864
Num initiator dev (--num_initiator_dev=N)                   : 1
Num target dev (--num_target_dev=N)                         : 1
Start block size (--start_block_size=N)                     : 65536
Max block size (--max_block_size=N)                         : 65536
Start batch size (--start_batch_size=N)                     : 1
Max batch size (--max_batch_size=N)                         : 1
Num iter (--num_iter=N)                                     : 208
Warmup iter (--warmup_iter=N)                               : 48
Large block iter factor (--large_blk_iter_ftr=N)            : 16
Num threads (--num_threads=N)                               : 1
----------------------------------------------------------------------------------------------------------------------------------------------------------------

Block Size (B)      Batch Size     B/W (GB/Sec)   Avg Lat. (us)  Avg Prep (us)  P99 Prep (us)  Avg Post (us)  P99 Post (us)  Avg Tx (us)    P99 Tx (us)
----------------------------------------------------------------------------------------------------------------------------------------------------------------
65536               1              1.236753       53.0           16.0           16.0           4.0            6.0            48.9           64.0
AIS-STATS Version: 1
HipFile Stats Level: 1
File Handle Registrations: 1
Buffer Registrations: 1
Fastpath Rejections: 0

Total Fastpath Read Size (B): 0
Average Fastpath Read Bandwidth (GiB/s): 0
Average Fastpath Read Latency (us): 0
Total Fastpath Read Errors: 0

Total Fastpath Write Size (B): 16777216
Average Fastpath Write Bandwidth (GiB/s): 1.14452
Average Fastpath Write Latency (us): 53.3281
Total Fastpath Write Errors: 0

Total Fallback Read Size (B): 0
Average Fallback Read Bandwidth (GiB/s): 0
Average Fallback Read Latency (us): 0
Total Fallback Read Errors: 0

Total Fallback Write Size (B): 0
Average Fallback Write Bandwidth (GiB/s): 0
Average Fallback Write Latency (us): 0
Total Fallback Write Errors: 0

GPU 0:
IO Size Histogram
IO Size (KiB)               Fastpath Read Size (B)           Fastpath Write Size (B)            Fallback Read Size (B)           Fallback Write Size (B)
0-4                                              0                                 0                                 0                                 0
4-8                                              0                                 0                                 0                                 0
8-16                                             0                                 0                                 0                                 0
16-32                                            0                                 0                                 0                                 0
32-64                                            0                                 0                                 0                                 0
64-128                                           0                          16777216                                 0                                 0
128-256                                          0                                 0                                 0                                 0
256-512                                          0                                 0                                 0                                 0
512-1024                                         0                                 0                                 0                                 0
1024-2048                                        0                                 0                                 0                                 0
2048-4096                                        0                                 0                                 0                                 0
4096-8192                                        0                                 0                                 0                                 0
8192-16384                                       0                                 0                                 0                                 0
16384-32768                                      0                                 0                                 0                                 0
32768-65536                                      0                                 0                                 0                                 0
65536-...                                        0                                 0                                 0                                 0
IO Bandwidth Histogram
IO Size (KiB)      Fastpath Read Bandwidth (GiB/s)  Fastpath Write Bandwidth (GiB/s)   Fallback Read Bandwidth (GiB/s)  Fallback Write Bandwidth (GiB/s)
0-4                                              0                                 0                                 0                                 0
4-8                                              0                                 0                                 0                                 0
8-16                                             0                                 0                                 0                                 0
16-32                                            0                                 0                                 0                                 0
32-64                                            0                                 0                                 0                                 0
64-128                                           0                           1.14452                                 0                                 0
128-256                                          0                                 0                                 0                                 0
256-512                                          0                                 0                                 0                                 0
512-1024                                         0                                 0                                 0                                 0
1024-2048                                        0                                 0                                 0                                 0
2048-4096                                        0                                 0                                 0                                 0
4096-8192                                        0                                 0                                 0                                 0
8192-16384                                       0                                 0                                 0                                 0
16384-32768                                      0                                 0                                 0                                 0
32768-65536                                      0                                 0                                 0                                 0
65536-...                                        0                                 0                                 0                                 0
IO Latency Histogram
IO Size (KiB)           Fastpath Read Latency (us)       Fastpath Write Latency (us)        Fallback Read Latency (us)       Fallback Write Latency (us)
0-4                                              0                                 0                                 0                                 0
4-8                                              0                                 0                                 0                                 0
8-16                                             0                                 0                                 0                                 0
16-32                                            0                                 0                                 0                                 0
32-64                                            0                                 0                                 0                                 0
64-128                                           0                           53.3281                                 0                                 0
128-256                                          0                                 0                                 0                                 0
256-512                                          0                                 0                                 0                                 0
512-1024                                         0                                 0                                 0                                 0
1024-2048                                        0                                 0                                 0                                 0
2048-4096                                        0                                 0                                 0                                 0
4096-8192                                        0                                 0                                 0                                 0
8192-16384                                       0                                 0                                 0                                 0
16384-32768                                      0                                 0                                 0                                 0
32768-65536                                      0                                 0                                 0                                 0
65536-...                                        0                                 0                                 0                                 0
IO Errors Histogram
IO Size (KiB)            Fastpath Read Error Count        Fastpath Write Error Count         Fallback Read Error Count        Fallback Write Error Count
0-4                                              0                                 0                                 0                                 0
4-8                                              0                                 0                                 0                                 0
8-16                                             0                                 0                                 0                                 0
16-32                                            0                                 0                                 0                                 0
32-64                                            0                                 0                                 0                                 0
64-128                                           0                                 0                                 0                                 0
128-256                                          0                                 0                                 0                                 0
256-512                                          0                                 0                                 0                                 0
512-1024                                         0                                 0                                 0                                 0
1024-2048                                        0                                 0                                 0                                 0
2048-4096                                        0                                 0                                 0                                 0
4096-8192                                        0                                 0                                 0                                 0
8192-16384                                       0                                 0                                 0                                 0
16384-32768                                      0                                 0                                 0                                 0
32768-65536                                      0                                 0                                 0                                 0
65536-...                                        0                                 0                                 0                                 0
OK: ais-stats / hipFile output mentions fastpath (see /tmp/ais-mt-ais-stats.uYulz6.log).
```