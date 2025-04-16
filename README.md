# C Thread Pool
A fully tested thread pool library implemented in C, featuring thread-safe components for robust multithreaded applications.

## Overview
This project consisting of tree main subcomponents:
1. **Thread-safe Queue** A concurrent two mutex queue implementation 
    that ensures safe access from multiple threads.
2. **Thread Pool** A flexible worker thread pool for efficient task execution.
3. **Simple Logger** A lightweight logging facility for debugging and monitoring.

## Key Features

## Installation
```shell
git clone https://github.com/Bright-Nabarro/c_thread_pool.git
cd c_thread_pool
cmake -B build -D DEBUG_MODE OFF
cmake --build build
make
```
