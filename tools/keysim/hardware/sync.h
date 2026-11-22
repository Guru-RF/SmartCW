#pragma once
#define __compiler_memory_barrier() asm volatile("" ::: "memory")
