# LunaOS v2.0 Minimal Prototype

This directory contains a bare-metal LunaOS v2.0 prototype with two spaces:

- `BOOT`: dawn loader, constellation manifest reader, space gate caller
- `SECURITY`: capability root, fixed-token verifier, roster listing endpoint

The current build keeps the original memory layout and serial flow, but the runtime payloads are now compiled from maintainable sources instead of being emitted directly as Python-crafted machine code:

- `boot/bootsector.asm`: real-mode loader, page tables, long-mode transition
- `boot/entry.asm`: BOOT stage entry stub
- `boot/main.c`: BOOT serial setup, manifest wiring, SECURITY calls
- `security/src/main.rs`: `no_std` SECURITY implementation
- `build/build.py`: compiler driver and image packer

The image format, loader records, IPC shape, and capability token layout remain project-defined.

## Layout

- `boot/`: BOOT bootsector, entry stub, C runtime, linker script
- `security/`: Rust SECURITY implementation and linker script
- `include/`: protocol and binary record definitions
- `build/`: linker layout, build script, generated image

## Terms

- `space`: an isolated execution realm with its own memory span and pulse stack
- `space gate`: a shared pulse slab used for explicit cross-space exchange
- `CID`: a 128-bit numeric capability seal
- `constellation manifest`: BOOT-readable embedded image map
- `reserve horizon`: preclaimed physical/virtual span for future AI space growth

## Toolchains

The repository expects local Windows toolchains under `toolchains/`:

- `toolchains/winlibs/mingw64/bin`: `gcc`, `ld`, `objcopy`, `nm`, `nasm`
- `toolchains/rustup-home/toolchains/stable-x86_64-pc-windows-gnu/bin`: `rustc`

`build/build.py` will also honor explicit overrides such as `LUNA_GCC`, `LUNA_LD`, `LUNA_OBJCOPY`, `LUNA_NASM`, `LUNA_RUSTC`, and `LUNA_NM`.

## Build

```powershell
python .\build\build.py
```

The build compiles:

- `boot/bootsector.asm`
- `boot/entry.asm`
- `boot/main.c`
- `security/src/main.rs`

and emits:

- `build/lunaos.img`
- `build/constellation.map`

## Run

If `qemu-system-x86_64` is available, use:

```powershell
.\build\run_qemu.ps1
```

Expected serial flow:

```text
[BOOT] dawn online
[SECURITY] ready
[BOOT] capability request ok
[BOOT] capability roster ok
```
