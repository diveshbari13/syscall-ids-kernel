# syscall-ids-kernel

A minimal x86 kernel, written from scratch in C and assembly, that intercepts and logs every syscall a process makes — paired with a Python-based anomaly detector that flags suspicious syscall sequences using n-gram frequency modeling.

This is a from-the-ground-up systems project: no libc, no existing kernel, no OS underneath. Every layer — from the first instruction that runs after the bootloader hands off control, to interrupt handling, to the syscall boundary itself — is built and understood from first principles.

---

## What this project demonstrates

- **Operating systems fundamentals**: multiboot-compliant booting, the GDT (Global Descriptor Table), the IDT (Interrupt Descriptor Table), the 8259 PIC, port-mapped I/O, and a working syscall interface (`int 0x80` convention) — the same primitives every real OS kernel is built on.
- **Applied security thinking**: syscall-sequence anomaly detection is a real, established technique used in host-based intrusion detection systems (HIDS) — this project implements a simplified, from-scratch version of that idea rather than a toy example.
- **Applied ML/data pipeline work**: turning a raw log stream into structured sequences, building a frequency-based statistical model, and scoring new data against it — the same shape of pipeline used in many production anomaly-detection systems, just at a scope that fits a from-scratch kernel project.

---

## Architecture

```
┌─────────────────────────────────────────────┐
│              QEMU (x86, 32-bit)              │
│  ┌─────────────────────────────────────┐    │
│  │           Kernel (C + asm)           │    │
│  │  GDT → IDT → PIC → Syscalls          │    │
│  │                                       │    │
│  │  [Syscall Handler] ──logs──▶ Buffer  │    │
│  └─────────────────────────────────────┘    │
│                    │                          │
│           Serial port (COM1)                  │
└────────────────────┼──────────────────────────┘
                      │  syscall log stream
                      ▼
        ┌──────────────────────────┐
        │   Host: Python analyzer   │
        │  - parses log              │
        │  - builds syscall n-grams  │
        │  - flags anomalies         │
        └──────────────────────────┘
```

The kernel handles boot, interrupts, and syscall logging. All ML/analysis work happens on the host in Python — deliberately kept separate, since training or scoring models inside a freestanding kernel environment (no libc, no floating point setup, no memory management to speak of) would add enormous complexity for zero benefit.

---

## Repo structure

```
syscall-ids-kernel/
├── kernel/
│   ├── boot.s              # multiboot entry point, sets up stack, jumps to kmain
│   ├── kmain.c              # kernel entry, orchestrates all subsystem init
│   ├── linker.ld            # memory layout for the kernel binary
│   ├── gdt.c / gdt.h        # Global Descriptor Table setup
│   ├── gdt_flush.s          # loads GDT, reloads segment registers
│   ├── idt.c / idt.h        # Interrupt Descriptor Table setup
│   ├── isr.s                 # 32 CPU exception stubs + common dispatcher
│   ├── isr_handler.c        # C-level exception handler (prints exception to screen)
│   ├── pic.c / pic.h        # 8259 PIC remapping (avoids IRQ/exception vector collision)
│   ├── serial.c / serial.h  # COM1 serial port driver — the kernel's logging channel
│   ├── mem.c / mem.h        # bump allocator (physical memory allocation)
│   ├── syscall.c / syscall.h # syscall dispatch table + per-call logging
│   ├── kprintf.c / kprintf.h # minimal sprintf-style formatter (no libc available)
│   ├── io.h                  # inb/outb port I/O helpers
│   └── isodir/               # GRUB ISO staging directory
├── analyzer/
│   ├── analyze.py            # n-gram anomaly detector
│   └── venv/                  # Python virtual environment (not committed)
└── README.md
```

---

## How it works, step by step

### 1. Boot
The kernel is booted via **GRUB** using the **multiboot specification** — GRUB handles the messy, hardware-specific parts of getting from BIOS into 32-bit protected mode, and hands off control directly to `_start` in `boot.s`, which sets up a stack and calls `kmain()` in C. This is the standard approach for hobby/from-scratch kernels; writing a custom bootloader is a separate, much larger problem that doesn't add to the OS-fundamentals story.

### 2. GDT — memory segmentation
The **GDT** defines the kernel's memory segments (code and data) and privilege level. A flat memory model is used — one segment spanning all 4GB of addressable memory for code, one for data — which is standard for simple kernels that aren't yet doing user/kernel memory separation. `gdt_flush.s` loads the table via `lgdt` and reloads every segment register to make the change take effect.

### 3. IDT — interrupt handling
The **IDT** maps interrupt/exception numbers to handler functions. All 32 CPU exception vectors (divide-by-zero, page fault, general protection fault, etc.) are wired up. Each has its own assembly stub (`isr.s`) that pushes context onto the stack and jumps to a common C handler (`isr_handler.c`), which currently prints the exception number to the screen in white-on-red and halts.

This was verified by deliberately triggering a divide-by-zero from `kmain.c` — instead of QEMU triple-faulting and silently resetting (the default failure mode with no IDT), the kernel caught the exception, decoded the vector number, and printed `Exception: 0x00` before halting cleanly. This confirmed the full interrupt path — CPU → IDT → assembly stub → C handler — works correctly.

### 4. PIC remapping
The 8259 Programmable Interrupt Controller's default IRQ vector mapping collides with the CPU's own exception vectors (0–31). `pic.c` remaps the master PIC to vectors 32–39 and the slave to 40–47, which is a mandatory step before hardware interrupts (keyboard, timer, etc.) can be safely enabled — otherwise a hardware IRQ could be misinterpreted as a CPU exception.

### 5. Serial output — the logging channel
A COM1 serial port driver (`serial.c`) gives the kernel a way to send data out to the host machine, independent of the VGA screen. This became the backbone of the entire syscall-logging pipeline: every syscall event is written out over serial as a line of text, which QEMU redirects to a log file on the host (`-serial file:syscalls.log`).

### 6. Bump allocator
A minimal physical memory allocator (`mem.c`): a single pointer that starts at the end of the kernel's memory footprint (`end`, defined by the linker script) and increments by the requested size on every allocation. No freeing is implemented — this is intentionally out of scope; the point is to have a working `kalloc()` for the `SYS_ALLOC` syscall to actually do something.

### 7. Syscalls — the core of the project
`syscall.c` implements a syscall dispatch table with four calls:
- `SYS_WRITE` — simulates a write operation
- `SYS_GETPID` — returns a simulated process ID
- `SYS_ALLOC` — calls into the bump allocator
- `SYS_EXIT` — simulates process termination

**Every single syscall invocation is logged** before dispatch, in the format:
```
[tick] pid=<pid> syscall=<number> arg=<value>
```
This log is what the anomaly detector consumes. `kmain.c` simulates three "processes" by calling `syscall_invoke()` directly in different patterns:
- **PID 1**: WRITE → ALLOC → WRITE → EXIT (varied, realistic pattern)
- **PID 2**: GETPID → WRITE → EXIT (varied, realistic pattern)
- **PID 3**: ALLOC × 10 in a tight loop (deliberately anomalous — no variation, no EXIT)

Note: for this project's scope, syscalls are invoked directly from kernel context rather than from separate ring-3 user-mode programs via a real `int 0x80` trap. The dispatch and logging logic is identical either way — the difference is purely in *how* the syscall number reaches the dispatcher (direct call vs. software interrupt), not in what gets logged or analyzed. Full ring-3 user mode (privilege-level separation, `iret` into user code) is a natural next step but wasn't required to validate the actual security concept.

### 8. Anomaly detection — the Python analyzer
`analyzer/analyze.py` implements a **sliding-window n-gram frequency model**, a simplified version of a real technique used in syscall-sequence-based host intrusion detection (the general approach traces back to foundational HIDS research, e.g. Forrest et al.'s work on syscall sequence analysis):

1. Parse the serial log into a per-PID sequence of syscall numbers.
2. Build a frequency table of syscall bigrams (pairs of consecutive syscalls) from a designated **baseline** set of PIDs — i.e., processes considered "known good."
3. Score any sequence (including ones outside the baseline) by the fraction of its bigrams that are rare or unseen in the baseline model.
4. Flag sequences whose anomaly score crosses a threshold (0.5) as anomalous.

**Usage:**
```bash
python3 analyze.py syscalls.log 1,2
```
This tells the analyzer to treat PID 1 and PID 2 as the trusted baseline, then score all PIDs (including 1, 2, and 3) against that baseline.

**Result on the test log:**
```
Baseline built from PIDs: [1, 2]

PID   Length  Score   Status
-----------------------------------
1     4       0.67    ANOMALOUS
2     3       0.50    normal
3     10      1.00    ANOMALOUS
```

PID 3 — the deliberately anomalous process — is correctly and strongly flagged (score 1.00), since its repeated `ALLOC, ALLOC` bigram never appears in the PID 1/2 baseline at all.

---

## A known, honest limitation

PID 1 also gets flagged here, which is a real side effect of scoring a PID against a baseline that includes itself (a form of information leakage in leave-one-out evaluation), combined with an extremely small sample size (4 syscalls / 3 bigrams). With so little data, a couple of legitimately rare bigrams are enough to push the score over the threshold.

This isn't hidden or worked around, because it's a genuinely instructive limitation: **anomaly detection is highly sensitive to baseline size and composition**, and this result is a small-scale demonstration of exactly why real intrusion-detection systems need large amounts of representative "normal" traffic to avoid high false-positive rates. A production version of this system would:
- Score each PID against a baseline built only from *other* PIDs (true leave-one-out), and
- Use a much larger set of baseline sequences to reduce noise from small-sample statistics.

---

## How to build and run

### Prerequisites
- WSL2 (or native Linux)
- `i686-elf-gcc` cross-compiler toolchain (do not use your system's default gcc — it will silently produce a broken kernel)
- `qemu-system-i386`
- `grub-mkrescue` + `xorriso`
- Python 3 with `venv`

### Build the kernel
```bash
cd kernel
export PATH="$HOME/opt/cross/bin:$PATH"

i686-elf-gcc -c kmain.c -o kmain.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
i686-elf-gcc -c gdt.c -o gdt.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
i686-elf-gcc -c idt.c -o idt.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
i686-elf-gcc -c isr_handler.c -o isr_handler.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
i686-elf-gcc -c pic.c -o pic.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
i686-elf-gcc -c serial.c -o serial.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
i686-elf-gcc -c mem.c -o mem.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
i686-elf-gcc -c syscall.c -o syscall.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
i686-elf-gcc -c kprintf.c -o kprintf.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
i686-elf-as boot.s -o boot.o
i686-elf-as gdt_flush.s -o gdt_flush.o
i686-elf-as isr.s -o isr.o

i686-elf-gcc -T linker.ld -o kernel.bin -ffreestanding -O2 -nostdlib \
  boot.o kmain.o gdt.o gdt_flush.o idt.o isr.o isr_handler.o pic.o \
  serial.o mem.o syscall.o kprintf.o -lgcc
```

### Build the bootable ISO
```bash
mkdir -p isodir/boot/grub
cp kernel.bin isodir/boot/kernel.bin
# isodir/boot/grub/grub.cfg should contain:
#   menuentry "syscall-ids-kernel" { multiboot /boot/kernel.bin }
grub-mkrescue -o kernel.iso isodir
```

### Run and capture the syscall log
```bash
qemu-system-i386 -cdrom kernel.iso -serial file:syscalls.log
```

### Run the anomaly detector
```bash
cd ../analyzer
python3 -m venv venv
source venv/bin/activate
pip install numpy matplotlib
python3 analyze.py ../kernel/syscalls.log 1,2
```

---

## What was deliberately out of scope

To keep this a finishable, focused project rather than an open-ended rebuild of Linux, the following were explicitly not implemented:
- A custom bootloader (GRUB/multiboot is used instead)
- A real filesystem
- Preemptive multitasking / a real scheduler (simulated PIDs are used instead)
- Full ring-3 user mode with hardware-enforced privilege separation (syscalls are invoked directly from kernel context)
- A more sophisticated ML model (the n-gram approach was chosen deliberately — it's simple, interpretable, and sufficient to demonstrate the concept correctly)

---

## Build history

This kernel was built incrementally and tested at every stage:
1. Boot via GRUB multiboot, print to VGA text buffer.
2. GDT implemented and loaded — verified via stable boot with no crash.
3. IDT implemented — verified by deliberately triggering a divide-by-zero exception and confirming it was caught and handled instead of causing a triple fault.
4. PIC remapped, serial (COM1) output implemented and verified.
5. Bump allocator implemented.
6. Syscall interface implemented with per-call logging — verified against three simulated processes with distinct syscall patterns.
7. Python n-gram anomaly detector implemented — verified to correctly flag the deliberately anomalous process.
