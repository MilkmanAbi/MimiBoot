# MimiBoot

**A minimal second-stage bootloader for ARM Cortex-M microcontrollers.**

```
┌────────────────────────────────────────────────────────────────────────────┐
│                                                                            │
│   Silicon ROM  ───►  MimiBoot  ───►  Payload                               │
│                          │                                                 │
│                          └── loads image, hands off, vanishes              │
│                                                                            │
└────────────────────────────────────────────────────────────────────────────┘
```

MimiBoot lives in internal flash. It loads ELF images from external storage into RAM, transfers execution to them, and gets out of the way. The payload runs with full ownership of system resources — it doesn't know or care that MimiBoot exists.

Think GRUB, but for microcontrollers.

---

## Why

Because I am retarded.

Established embedded workflows assume you have infrastructure: flash programmers, debug probes, CI pipelines, OTA backends. The tooling exists, but it's either vendor-locked, ecosystem-specific, or designed for production deployments.

For hobbyists and independent developers, this creates friction. You want to:

- Iterate without reflashing internal memory every time
- Store multiple firmware builds and swap between them
- Experiment with custom RTOSes, bare-metal projects, or weird one-off builds
- Have basic A/B update capability without enterprise infrastructure
- Develop on hardware without a programmer constantly attached

Existing solutions don't fit well. MCUboot assumes Zephyr or MyNewt. Vendor bootloaders are opaque and limited. U-Boot targets application processors with MMUs. Everything expects you to fit into an existing ecosystem.

MimiBoot is the missing piece: a simple, dumb loader that reads ELF files from storage and boots them. No signing, no encryption, no ecosystem. Just your binary on an SD card.

It's for hobbyists, students, tinkerers, and anyone whose development workflow is "build, copy to SD, reset, repeat."

---

## What It Does

---

## Why Not MCUboot?

MCUboot exists. It's mature, secure, well-tested, and backed by the Zephyr project. If you're deploying production firmware with signed images, secure boot chains, and established RTOSes — use MCUboot. Seriously.

MimiBoot is not that.

| MCUboot | MimiBoot |
|---------|----------|
| Secure boot, image signing, encryption | None of that |
| Designed for Zephyr, MyNewt, established ecosystems | Designed for whatever ELF you throw at it |
| Production-grade, audited | Hobbyist-grade, trust-me-bro |
| Opinionated image format | Standard ELF, nothing proprietary |
| Tight RTOS integration | Zero runtime integration |

MimiBoot exists for a different purpose: loading *arbitrary* code with *zero assumptions* about what that code is. Your custom kernel. Your weird experimental RTOS. Your bare-metal sensor logger. Your Picomimi build. Whatever.

It's a dumb loader. It reads an ELF, copies it to RAM, jumps to it. No verification, no signature checks, no rollback protection, no cryptographic anything. If you need security, this is not your tool.

But if you need a simple, transparent way to boot custom payloads from external storage on Cortex-M — without buying into an ecosystem, without conforming to someone else's image format, without Zephyr dependencies — MimiBoot does exactly that and nothing more.

It's not better than MCUboot. It's different. It's for tinkerers, not production.

---

## What It Does

1. Initializes minimal hardware (clocks, storage interface)
2. Mounts external storage
3. Reads boot configuration to determine which image to load
4. Parses ELF headers and copies loadable segments into RAM
5. Constructs a handoff structure with system context
6. Jumps to the payload entry point
7. Never executes again until the next reset

After handoff, MimiBoot is dormant code. The payload owns everything.

---

## Use Cases

**Development workflow**  
Build on host, copy ELF to SD, reset MCU. No flash programmer, no waiting for erase cycles.

**Multi-boot**  
Multiple complete firmware images on one storage device. Boot whichever one is configured.

**OTA updates**  
Download new image to storage, update config, reboot. Keep the old image for rollback.

**A/B partitioning**  
Two image slots. If the new one fails, fall back to the previous known-good.

**Field recovery**  
Ship a diagnostic image alongside production. Technicians can trigger it without special tools.

**Experimentation**  
Try out experimental firmware without touching your stable build. Just boot a different image.

---

## Supported Targets

MimiBoot is designed for ARM Cortex-M devices. Initial targets:

| Family | Specific Chips | RAM | Notes |
|--------|----------------|-----|-------|
| RP2040/RP2350 | Raspberry Pi Pico, Pico W, Pico 2 | 264-520 KB | Primary development target |
| STM32F4 | STM32F401, STM32F411, STM32F446 | 96-128 KB | Common hobbyist chips |
| STM32H7 | STM32H743, STM32H750 | 512 KB+ | High-RAM applications |
| nRF52 | nRF52832, nRF52840 | 64-256 KB | BLE-capable targets |

The architecture is portable. Adding new targets requires implementing a thin HAL layer.

---

## Constraints

**RAM execution only**  
Payloads run entirely from RAM. No XIP from external storage. This limits image size to available RAM.

**Statically linked ELFs**  
No dynamic linking, no shared libraries. Payloads must be complete, standalone executables.

**Boot latency**  
SD card initialization and image loading adds 100-400ms to boot time. Not suitable for latency-critical cold boot requirements.

**No runtime services**  
MimiBoot provides nothing after handoff. No callbacks, no shared drivers, no IPC. It's gone.

---

## Documentation (Coming Soon OwO)

| Document | Description |
|----------|-------------|
| [docs/architecture.md](docs/architecture.md) | System architecture and boot flow |
| [docs/elf_format.md](docs/elf_format.md) | ELF requirements for payloads |
| [docs/handoff_spec.md](docs/handoff_spec.md) | Handoff structure specification |
| [docs/payload_guide.md](docs/payload_guide.md) | How to build compatible payloads |
| [docs/storage.md](docs/storage.md) | Storage backends and filesystem support |
| [docs/porting.md](docs/porting.md) | Adding support for new targets |
| [docs/configuration.md](docs/configuration.md) | Boot configuration options |

---

## Project Structure

```
mimiboot/
├── src/                    # Core bootloader source
│   ├── elf/                # ELF parsing and loading
│   ├── handoff/            # Handoff construction and jump
│   ├── storage/            # Storage backend implementations
│   ├── fs/                 # Filesystem implementations
│   ├── config/             # Boot configuration parsing
│   └── hal/                # Hardware abstraction layers
│       ├── rp2040/
│       ├── stm32f4/
│       └── ...
├── include/                # Public headers
│   └── mimiboot/
│       └── handoff.h       # Handoff structure (for payloads)
├── linker/                 # Linker scripts per target
├── docs/                   # Documentation
├── examples/               # Example payloads
└── tools/                  # Host utilities
```

---

## Building

See [docs/building.md](docs/building.md) for full instructions.

Quick start:

```
mkdir build && cd build
cmake -DTARGET=rp2040 ..
make
```

Output: `mimiboot.uf2` (or `.bin`/`.hex` depending on target)

---

## Quick Start

1. Flash MimiBoot to your MCU's internal flash
2. Format an SD card as FAT32
3. Copy your payload ELF to the SD card as `boot.elf`
4. Insert SD card, reset MCU
5. Payload runs

For multi-image setups, see [docs/configuration.md](docs/configuration.md).

---

## Building Payloads

Payloads must be:

- ELF32 ARM executables
- Statically linked
- Linked to run from RAM (not flash)
- Self-contained (include their own startup code)

See [docs/payload_guide.md](docs/payload_guide.md) for linker script templates and examples.

---

## Handoff

MimiBoot passes a handoff structure to the payload containing:

- Memory map (RAM base, size, regions)
- Clock configuration
- Boot reason (cold, watchdog, etc.)
- Boot source (SD, SPI flash, etc.)
- Loader location (so payload can reclaim if needed)

The payload receives a pointer to this structure in `r0` at entry. It's optional — payloads can ignore it entirely.

Full specification: [docs/handoff_spec.md](docs/handoff_spec.md)

---

## Status

**Early development.** Core architecture is defined. Implementation in progress.

- [x] Architecture design
- [x] Handoff protocol specification
- [x] ELF format requirements
- [ ] Core ELF loader
- [ ] RP2040 HAL
- [ ] SD card driver
- [ ] FAT32 support
- [ ] STM32F4 HAL
- [ ] Example payloads
- [ ] Documentation

---

## Contributing

Contributions welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

Areas of interest:
- HAL implementations for new targets
- Storage backend implementations
- Testing infrastructure
- Documentation improvements

---

## License

MIT. See [LICENSE](LICENSE).

---

## Acknowledgments

Inspired by U-Boot, GRUB, and MCUboot — but smaller. Much smaller.

```
        ∧＿∧
      （｡･ω･｡)つ━☆・*。
      ⊂    ノ      ・゜+.
       しーＪ    °。+ *´¨)
                 .· ´¸.·*´¨) ¸.·*¨)
                  (¸.·´ (¸.·'* boot
```
