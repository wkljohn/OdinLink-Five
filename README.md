# OdinLink-Five

**Thunderbolt 5 RDMA for Linux — kernel driver, libibverbs provider, NCCL/RCCL plugins**

OdinLink turns a Thunderbolt cable into a high-speed RDMA interconnect between machines. It provides the full `ibv_*` verbs API so any verbs-aware application (NCCL, MPI, PyTorch DDP) can use Thunderbolt DMA without code changes.

```
80 Gbps  ·  sub-µs latency  ·  zero-copy GPU  ·  standard ibv_verbs API
```


## About this fork

> **Engineering by [paicon](https://paix-navigator.paicon.com)**
> This work is part of the [paix-navigator.paicon.com](https://paix-navigator.paicon.com) effort.

This is an additive fork of [Geramy/OdinLink-Five](https://github.com/Geramy/OdinLink-Five), crediting upstream rather than claiming superiority or a rewrite.
It keeps the upstream design and adds focused driver and RDMA verbs fixes found while bringing up real cross-node workloads.
The `main` branch is based on upstream `ed60505` ([full diff](https://github.com/Geramy/OdinLink-Five/compare/ed60505...wkljohn:main)) and was measured on two AMD Ryzen AI MAX+ 395 systems (Strix Halo, `gfx1151`) running Ubuntu 26.04 and kernel 7.0.0-30.

```bash
git clone https://github.com/wkljohn/OdinLink-Five.git
cd OdinLink-Five
cmake -B build -DBUILD_VERBS=ON -DBUILD_TRAY=OFF && cmake --build build -j$(nproc)
make -C driver
```

Recipes, the bug ledger, and raw measurements are in [wkljohn/llama.cpp-strix-halo-RCCL-RDMA](https://github.com/wkljohn/llama.cpp-strix-halo-RCCL-RDMA/tree/master/odinlink).

### What this branch fixes

These are transport-wide correctness fixes, not AMD-specific fixes; only the discovery bridge was tested on Strix Halo. Both ends must run the same build because the stream header grew a fragment index.

| Fix | Consequence |
|---|---|
| **Standard RDMA discovery** | An `LD_PRELOAD` shim makes OdinLink visible to verbs applications; it is a practical bridge, not complete kernel-provider integration, because OdinLink registers no kernel `ib_device`. |
| **Full-size frame headroom** | Full fragments no longer exceed their receive buffers and disappear. |
| **Posted receive queue** | `ibv_post_recv` now queues buffers for later arrivals instead of waiting immediately. |
| **Safe sends and completion IDs** | Host sends keep a private copy, and completions return `wr_id`. |
| **Non-blocking completion polling** | Producers can publish completions while consumers poll. |
| **Independent send and receive progress** | Two-way traffic no longer stalls because one direction reserves the other's buffers. |
| **Fragment sequencing** | Sequence gaps report loss and drop damaged messages; they detect loss but do not retransmit. |
| **Byte-verifying stress test** | `tests/odl_rdma_stress.c` catches truncation, reordering, stale data, and loss in one-way, `--bidir`, and `--latency` runs. |
| **Fail-closed compatibility** | Mismatched OdinLink wire-protocol versions are rejected before either peer enters `READY`; deploy the same commit on both machines. |
| **Link diagnosis** | `odl_tb5_cli diag` identifies the first failing USB4, peer-discovery, driver, handshake, or device layer. |

### Measured results on Strix Halo

These two-node measurements used a USB4v1 cable; the driver reported 10 Gb/s × 2 lanes.

| Test | Measured result | Context |
|---|---:|---|
| Round-trip latency | **22.0 µs median** | **286 µs TCP; ~13×** |
| Median reproducibility | **±0.19 µs** | p95/p99 swing **~3×** |
| Byte-verified bulk | **21 GiB at 8.38 Gb/s** | **86016/86016** |
| Byte-verified full duplex | **2 GiB each way at 9.84 Gb/s** | **8192/8192** |
| llama.cpp 27B Q6_K, 2 nodes, `-sm layer`, tg128 | **9.16 t/s** | **9.07 t/s** thunderbolt_ibverbs; **8.83 t/s** TCP |
| llama.cpp 27B Q6_K, 1 node, tg128 | **9.50 t/s** | Single node |
| Inline send, 1 KiB | **min −1.93 µs; stddev −52%** | median **22.58 off / 22.50 on** |
| DS4 DeepSeek V4 Flash Q4_K, TP=2 prefill | **270.34 t/s** | Cache-free 2,048-token prefill; exact-fingerprint validation run |
| DS4 DeepSeek V4 Flash Q4_K, TP=2 decode | **20.52 t/s** | 300 generated tokens; exact fingerprint and zero provider fallback |

The median is reproducible, but p95/p99 are not; the bulk results are byte-verified. A single node at 9.50 t/s remains faster than two nodes, which are for capacity rather than speed.

The current DS4 result used two Ryzen AI MAX+ 395 nodes, identical binaries and
model files, cache-free Q4_K weights, a 2,048-token prefill plus 300-token
greedy decode, and OdinLink RDMA with no fallback traffic. It matched the
expected token fingerprint and passed the arithmetic and long-retrieval smoke
cases. Each rank recorded 61,264 optimized WC sends and 3,849,794,176 bytes;
see the [verbs provider manual](verbs/VERBS_PROVIDER.md#wc-mapped-host-send-buffers).

---

## Progress

| Layer | Component | Status |
|-------|-----------|--------|
| 🟢 | Kernel module (`odl_tb5.ko`) | NHI ring DMA, XDomain handshake, loopback mode |
| 🟢 | Userspace library (`libodl_tb5.so`) | C API, stream I/O, mmap, DMA-buf |
| 🟢 | Verbs provider (`libodl_tb5_verbs.so`) | `ibv_open_device`, `ibv_reg_dmabuf_mr`, QP/CQ lifecycle |
| 🟢 | rdma-core plugin (`libodl_tb5-rdmav34.so`) | Auto-discovered by `ibv_devinfo` |
| 🟢 | Async I/O | `poll()` + `O_NONBLOCK` ioctls end-to-end |
| 🟢 | No-cable testing | `loopback=1` module param + mock library |
| 🟢 | NCCL verbs transport | NCCL's built-in `NCCL_NET_PLUGIN=IB` transport auto-discovers ODL via `ibv_get_device_list` |
| 🟡 | NCCL custom plugin | DMA-buf zero-copy path (legacy, use verbs transport instead) |
| 🟡 | Async DMA-buf | Needs callback-based cleanup — stream path is already async via poll() |

## Quick Start

### Build & Run

```bash
sudo apt install build-essential cmake linux-headers-$(uname -r) libibverbs-dev rdma-core pkg-config gcc-14
git clone https://github.com/wkljohn/OdinLink-Five.git
cd OdinLink-Five && mkdir build && cd build
cmake .. -DBUILD_VERBS=ON && make -j$(nproc) odl_tb5_verbs odl_tb5_verbs_provider

# Test without cable:
sudo insmod driver/odl_tb5.ko loopback=1
ibv_devinfo                     # Should show odl_tb5 device
build/verbs/tests/test_verbs_basic
```

### Point-to-Point (two machines)

```bash
# Machine A:
sudo insmod driver/odl_tb5.ko
build/cli/odl_tb5_cli server -d 0

# Machine B:
sudo insmod driver/odl_tb5.ko
build/cli/odl_tb5_cli client -d 0 -t bandwidth

# Either machine: verify the complete link before starting a workload
build/cli/odl_tb5_cli diag -v
```

Install the same commit on both machines. The driver deliberately refuses a
peer with an incompatible wire protocol instead of silently entering an unsafe
`READY` state. Leave the DMA ring size at its default: if one machine cannot
allocate the preferred size, both drivers negotiate the smaller working size
during login. Run `diag -v` on both machines and confirm they report `READY`
with matching local and peer packet-slot counts before starting a workload.
If the device does not appear, the same command reports the first failing
layer.

Without an override, the standalone verbs provider looks for an IPv4 address
on `bond0` first and `thunderbolt0` second. Set `ODL_RDMA_GID_IFACE` on both
machines when using a different interface, or `ODL_RDMA_GID_IP` to supply each
machine's address directly. Explicit values are strict: an invalid address or
an interface without IPv4 produces no GID rather than silently choosing
another interface.

Full install guide → [`docs/INSTALL.md`](docs/INSTALL.md)

## Architecture

```
┌────────────────────────────────────────────────────┐
│  Application (NCCL, MPI, PyTorch, ibv_* API)       │
├────────────────────────────────────────────────────┤
│             libibverbs (libibverbs.so.1)            │
├────────────────────────────────────────────────────┤
│  libodl_tb5-rdmav34.so  (verbs provider plugin)    │
├────────────────────────────────────────────────────┤
│  libodl_tb5.so  (OdinLink C API)                   │
├────────────────────────────────────────────────────┤
│  odl_tb5.ko  (kernel module — NHI DMA)             │
├────────────────────────────────────────────────────┤
│  Thunderbolt 5 NHI DMA Engine                       │
└────────────────────────────────────────────────────┘
```

## Components

| Component | Binary | Description |
|-----------|--------|-------------|
| Kernel driver | `odl_tb5.ko` | NHI ring DMA, XDomain handshake, char device |
| Library | `libodl_tb5.so` | C API wrapping ioctls, streams, mmap |
| Verbs provider | `libodl_tb5_verbs.so` | Standalone `ibv_*` via symbol interposition |
| Verbs plugin | `libodl_tb5-rdmav34.so` | rdma-core provider plugin (`ibv_devinfo`) |
| NCCL plugin | `libnccl-net-ODL_TB5.so` | NVIDIA GPU collectives |
| RCCL plugin | `librccl_net_odl_tb5.so` | AMD GPU collectives |
| CLI tool | `odl_tb5_cli` | Bandwidth, latency, jitter, MIMO tests |
| Loopback module | `loopback=1` param | Fake peer for no-cable testing |
| Mock library | `libodl_tb5_mock.so` | LD_PRELOAD simulation (no kernel needed) |

### GPU and daemon/tray → [`docs/GPU.md`](docs/GPU.md), [`docs/INSTALL.md`](docs/INSTALL.md)

## Verbs API Coverage

| Operation | Status | Notes |
|-----------|--------|-------|
| `ibv_open_device` | ✅ | Symbol interposition + rdma-core plugin |
| `ibv_query_device` | ✅ | Attributes from peer info |
| `ibv_query_port` | ✅ | Port state from peer connection |
| `ibv_alloc_pd` / `ibv_dealloc_pd` | ✅ | Protection domains |
| `ibv_reg_mr` / `ibv_dereg_mr` | ✅ | Host memory registration |
| `ibv_reg_dmabuf_mr` | ✅ | Zero-copy GPU memory (Linux DMA-buf) |
| `ibv_create_cq` / `ibv_destroy_cq` | ✅ | Eventfd-based completion queues |
| `ibv_poll_cq` / `ibv_req_notify_cq` | ✅ | Poll + eventfd notification |
| `ibv_create_qp` / `ibv_destroy_qp` | ✅ | RC QP → stream mapping |
| `ibv_modify_qp` | ✅ | RESET → INIT → RTR → RTS |
| `ibv_post_send` | ✅ | Async via workqueue + poll() |
| `ibv_post_recv` | ✅ | Non-blocking via poll() |
| `ibv_query_qp` | ✅ | State + capabilities |

## Async I/O Model

```
ibv_post_send(qp, wr, NULL)
    │
    ▼  (non-blocking, returns immediately)
Enqueue WR → per-QP submission queue
    │
    ▼  (worker thread)
poll(fd, POLLOUT)  ← kernel signals TX readiness
    │
    ▼
ioctl(STREAM_SEND) ← O_NONBLOCK, never blocks
    │
    ├── -EAGAIN → re-queue WR, poll again
    └── success → post struct ibv_wc → CQ → eventfd
```

## Testing Without Hardware

```bash
# Option 1: kernel loopback (real module, fake peer)
sudo insmod driver/odl_tb5.ko loopback=1
build/verbs/tests/test_verbs_basic

# Option 2: user-space mock (no kernel module at all)
mkfifo /dev/odl_tb5_0
LD_PRELOAD=verbs/tests/libodl_tb5_mock.so \
  LD_LIBRARY_PATH=build/verbs:build/lib \
  build/verbs/tests/test_verbs_mock_loopback
```

## Smoke Tests (with hardware)

Verbose logging is essential for diagnosing failures. Use the provided helper
script which captures everything automatically:

```bash
# Full smoke test suite — all logs go to smoke-test-<timestamp>/:
./scripts/smoke-test.sh

# Run only the verbs provider test:
./scripts/smoke-test.sh -t verbs

# Bandwidth test (two machines, machine A first):
sudo ./scripts/smoke-test.sh -t bandwidth -m server   # Machine A
sudo ./scripts/smoke-test.sh -t bandwidth -m client   # Machine B

# Custom output directory:
./scripts/smoke-test.sh -o /tmp/odl-debug
```

Verbose logging is also available manually:

```bash
# Watch kernel driver logs (run in a separate terminal):
sudo dmesg -w | grep odl_tb5

# Trace all verbs calls — set level 1–5 (5 = most verbose):
export ODL_VERBS_DEBUG=5
```

### Manual smoke test steps

**1. Kernel module + device node**

```bash
sudo insmod driver/odl_tb5.ko
sudo chmod 666 /dev/odl_tb5_0    # allow non-root access
ls -l /dev/odl_tb5_0             # appears only when a TB5 peer is connected
```

**2. Full test suite** (3 suites: device, lib API, plugin)

```bash
build/tests/odl_tb5_test
```

**3. Verbs provider lifecycle test**

```bash
build/verbs/tests/test_verbs_basic
```

Exercises: device discovery, context open, PD/MR/CQ/QP lifecycle, post_send/post_recv.

**4. End-to-end bandwidth** (two machines)

```bash
# Machine A:
build/cli/odl_tb5_cli server -d 0

# Machine B (wait for server to be ready):
build/cli/odl_tb5_cli client -d 0 -t bandwidth
```

**5. ibv_devinfo discovery**

```bash
ibv_devinfo     # should list an odl_tb5 device
```

## Module Parameters

| Param | Default | What it does |
|-------|---------|--------------|
| `e2e=0` | 1 (on) | Disables end-to-end flow control handshake. **Only needed for old TB3 controllers** that choke on E2E. TB4/TB5 leave this alone. |
| `loopback=1` | 0 (off) | Creates fake devices with no cable — data loops back inside your own machine. For testing without a peer. |
| `protocol=1` | 0 (OdinLink) | Switches to Apple's protocol ID (0xFA57) so macOS peers can discover OdinLink. For Mac↔Linux only. |
| `odl_ring_size` | 4096 | Preferred DMA packet slots per ring. Each node halves this on `ENOMEM`, down to `odl_ring_fallback_min`; connected peers then negotiate the smaller working size. |
| `odl_ring_fallback_min` | 512 | Minimum automatic fallback depth. Set to 0 for strict testing that must fail instead of reducing ring depth. |

```bash
# Examples:
sudo insmod driver/odl_tb5.ko                  # TB4/TB5, default everything
sudo insmod driver/odl_tb5.ko e2e=0            # old TB3 controller
sudo insmod driver/odl_tb5.ko loopback=1        # no cable, just testing
sudo insmod driver/odl_tb5.ko protocol=1        # talk to macOS
sudo insmod driver/odl_tb5.ko odl_ring_size=1024 # deliberately prefer a smaller ring
```

## Debug

```bash
export ODL_VERBS_DEBUG=5     # Trace all verbs calls
sudo dmesg -w | grep odl_tb5 # Kernel driver logs
```

Troubleshooting → [`docs/TROUBLESHOOTING.md`](docs/TROUBLESHOOTING.md)

## Cross-Platform: macOS

Apple ships `libthunderboltrdma.dylib` + `libibverbs` on macOS 26.5, but
the kernel extension is a stub — `IORDMAFamily` is not shipped. Mac
Thunderbolt RDMA is not currently functional. OdinLink is the only
working implementation. See [`COMPAT.md`](COMPAT.md).

## Repository

| Resource | Link |
|----------|------|
| Install guide | [`docs/INSTALL.md`](docs/INSTALL.md) |
| GPU / NCCL / RCCL | [`docs/GPU.md`](docs/GPU.md) |
| Troubleshooting | [`docs/TROUBLESHOOTING.md`](docs/TROUBLESHOOTING.md) |
| Packaging / .deb | [`docs/PACKAGING.md`](docs/PACKAGING.md) |
| Agent instructions | [`AGENTS.md`](AGENTS.md) |
| Verbs provider manual | [`verbs/VERBS_PROVIDER.md`](verbs/VERBS_PROVIDER.md) |
| Cross-platform compat | [`COMPAT.md`](COMPAT.md) |

## License

- **Kernel driver** (`odl_tb5.ko`): GPL v2
- **All userspace**: MIT
