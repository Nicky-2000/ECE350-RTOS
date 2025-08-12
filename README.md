# TinyRTOS

A custom **real-time operating system kernel** written in **C** for the Intel **DE1-SoC** FPGA board, built by a team of four for an embedded systems course. Supports **dynamic task management**, **priority scheduling**, **dynamic memory allocation**, and **mailbox-based IPC**.

## Features
- **Dynamic Task Management** – Preemptive, priority-based scheduler with:
  - **Two-level design**: Red-black tree of active priorities → FIFO ready queue per priority.
  - O(1) highest-priority pick, O(log n) scheduler updates.
- **Dynamic Memory Management** – First-fit allocation with:
  - Linked-list free list, 8-byte alignment, block splitting/coalescing, fragmentation tracking.
- **Inter-Task Communication** – Mailboxes using a **ring buffer** for messages with sender ID.
- **Keyboard Command Dispatcher (KCD)** – System task that parses `%x...` commands and routes them to registered tasks.
- **Preemptive Scheduling** – Timer-based task switching with support for yield and priority changes.

## Data Structures
- **Red-black tree** – Tracks active priorities.
- **Linked lists** – Per-priority ready queues & free memory list.
- **Ring buffer** – For mailbox messages.

## Repository Structure
/src
k_task.c # Task creation, scheduling, context switching
k_rtx_init.c # Kernel initialization
mailbox.c # Mailbox IPC
tcb_storage.c # Task Control Block storage
main_svc_cw.c # System call handling & KCD

## Building & Running
1. Clone repo and open in **Intel SoC EDS** environment.
2. Compile kernel with `make`.
3. Load to **Intel DE1-SoC** board via USB-Blaster.
4. Run and interact via UART terminal.

## Authors
Built by **Nicky Khorasani** and three teammates.