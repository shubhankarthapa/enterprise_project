# Parallel vs Sequential C Program

This repository contains two C programs to demonstrate the difference between:

- Sequential execution
- Parallel execution

## Files

- `sequential.c` → Runs the task in a single thread (normal execution)
- `parallel.c` → Runs the task using parallel processing

## Purpose

The goal of this project is to compare:

- Execution time
- Performance improvement (speedup)
- Efficiency of parallel processing

## How to Compile

### Sequential

```bash
gcc -O2 -c sequential.c -o sequential.o
gcc -O2 -pthread parallel.c sequential.o -o sorter2
