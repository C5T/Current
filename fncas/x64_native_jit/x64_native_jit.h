/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2019 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

// NOTE(dkorolev): This is really an alpha version. "It works on my machines", and then I don't care for now. -- D.K.
// TODO(dkorolev): Indirect calls to external functions (to avoid dealing with `@plt`) may be suboptimal.
// TODO(dkorolev): Offsets to offsets, to make sure the instructions have the same opcode length, may be suboptimal.
// TODO(dkorolev): Look into endianness.

#ifndef FNCAS_X64_NATIVE_JIT_X64_NATIVE_JIT_H
#define FNCAS_X64_NATIVE_JIT_X64_NATIVE_JIT_H

#if defined(__x86_64__)

#define FNCAS_X64_NATIVE_JIT_ENABLED

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include <sys/mman.h>

#ifdef NDEBUG
#include <cassert>
#define X64_JIT_ASSERT(x) assert(x)
#else
#define X64_JIT_ASSERT(x)
#endif

static_assert(sizeof(double) == 8, "FnCAS X64 native JIT compiler requires `double` to be 8 bytes.");

namespace current {
namespace fncas {
namespace x64_native_jit {

// The signature is:
// * Returns a `double`:              The value of the function.
// * Uses the `double const* x`:      Input, the parameters vector `x[i]`.
// * Uses the `double* o`:            Output, first the gradient, then the temporary memory buffer.
// * Uses the `double (*f[])(double): External functions (`sin`, `exp`, etc.) to be called, to avoid dealing with PLT.
typedef double (*pf_t)(double const* x, double* o, double (*f[])(double));

constexpr static size_t const kX64NativeJITExecutablePageSize = 4096;

struct CallableVectorUInt8 final {
  size_t const allocated_size_;
  void* buffer_ = nullptr;

  explicit CallableVectorUInt8(std::vector<uint8_t> const& data)
      : allocated_size_(kX64NativeJITExecutablePageSize *
                        ((data.size() + kX64NativeJITExecutablePageSize - 1) / kX64NativeJITExecutablePageSize)) {
    buffer_ = ::mmap(NULL, allocated_size_, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (!buffer_) {
      std::cerr << "`mmap()` failed.\n";
      std::exit(-1);
    }
    if (::mprotect(buffer_, allocated_size_, PROT_READ | PROT_WRITE | PROT_EXEC)) {
      std::cerr << "`mprotect()` failed.\n";
      std::exit(-1);
    }
    ::memcpy(buffer_, &data[0], data.size());
  }

  CallableVectorUInt8(CallableVectorUInt8 const&) = delete;
  CallableVectorUInt8(CallableVectorUInt8&) = delete;
  CallableVectorUInt8(CallableVectorUInt8&&) = delete;
  CallableVectorUInt8& operator=(CallableVectorUInt8 const&) = delete;
  CallableVectorUInt8& operator=(CallableVectorUInt8&) = delete;
  CallableVectorUInt8& operator=(CallableVectorUInt8&&) = delete;

  double operator()(double const* x, double* o, double (*f[])(double)) const {
    // HACK(dkorolev): Shift the buffers by 16 doubles (16 * 8 bytes) to have the load/save/etc. opcodes of same length.
    // HACK(dkorolev): Shift the functions buffer by one function (8 bytes) to even up the indirect call opcodes.
    return reinterpret_cast<pf_t>(buffer_)(x - 16, o - 16, f - 1);
  }

  ~CallableVectorUInt8() {
    if (buffer_) {
      ::munmap(buffer_, allocated_size_);
    }
  }
};

namespace opcodes {

template <typename C>
void push_rbx(C& c) {
  c.push_back(0x53);
}

template <typename C>
void push_rsi(C& c) {
  c.push_back(0x56);
}

template <typename C>
void push_rdi(C& c) {
  c.push_back(0x57);
}

template <typename C>
void push_rdx(C& c) {
  c.push_back(0x52);
}

template <typename C>
void mov_rsi_rbx(C& c) {
  c.push_back(0x48);
  c.push_back(0x89);
  c.push_back(0xf3);
}

template <typename C>
void pop_rbx(C& c) {
  c.push_back(0x5b);
}

template <typename C>
void pop_rsi(C& c) {
  c.push_back(0x5e);
}

template <typename C>
void pop_rdi(C& c) {
  c.push_back(0x5f);
}

template <typename C>
void pop_rdx(C& c) {
  c.push_back(0x5a);
}

template <typename C>
void ret(C& c) {
  c.push_back(0xc3);
}

template <typename C, typename O>
void internal_load_immediate_to_memory_by_someregister_offset(C& c, uint8_t reg, O offset, double v) {
  uint64_t x = *reinterpret_cast<uint64_t const*>(&v);
  c.push_back(0x48);
  c.push_back(0xb8);
  for (size_t i = 0; i < 8; ++i) {
    c.push_back(x & 0xff);
    x >>= 8;
  }
  c.push_back(0x48);
  c.push_back(0x89);
  c.push_back(reg);

  auto o = static_cast<int64_t>(offset);
  o += 16;  // HACK(dkorolev): Shift by 16 doubles to have the opcodes have the same length.
  o *= 8;   // Double is eight bytes, signed multiplication by design.
  X64_JIT_ASSERT(o >= 0x80);
  X64_JIT_ASSERT(o <= 0x7fffffff);
  for (size_t i = 0; i < 4; ++i) {
    c.push_back(o & 0xff);
    o >>= 8;
  }
}

// NOTE(dkorolev): The `unsafe` prefix is becase the usecase is unit test only; FnCAS should not overwrite that memory.
template <typename C, typename O>
void unsafe_load_immediate_to_memory_by_rdi_offset(C& c, O offset, double v) {
  internal_load_immediate_to_memory_by_someregister_offset(c, 0x87, offset, v);
}

template <typename C, typename O>
void load_immediate_to_memory_by_rsi_offset(C& c, O offset, double v) {
  internal_load_immediate_to_memory_by_someregister_offset(c, 0x86, offset, v);
}

template <typename C, typename O>
void load_immediate_to_memory_by_rbx_offset(C& c, O offset, double v) {
  internal_load_immediate_to_memory_by_someregister_offset(c, 0x83, offset, v);
}

template <typename C, typename O>
void internal_load_from_memory_by_offset_to_xmm0(C& c, uint8_t reg, O offset) {
  auto o = static_cast<int64_t>(offset);
  o += 16;  // HACK(dkorolev): Shift by 16 doubles to have the opcodes have the same length.
  o *= 8;   // Double is eight bytes, signed multiplication by design.
  X64_JIT_ASSERT(o >= 0x80);
  X64_JIT_ASSERT(o <= 0x7fffffff);
  c.push_back(0xf2);
  c.push_back(0x0f);
  c.push_back(0x10);
  c.push_back(reg);
  for (size_t i = 0; i < 4; ++i) {
    c.push_back(o & 0xff);
    o >>= 8;
  }
}

template <typename C, typename O>
void load_from_memory_by_rdi_offset_to_xmm0(C& c, O offset) {
  internal_load_from_memory_by_offset_to_xmm0(c, 0x87, offset);
}

template <typename C, typename O>
void load_from_memory_by_rsi_offset_to_xmm0(C& c, O offset) {
  internal_load_from_memory_by_offset_to_xmm0(c, 0x86, offset);
}

template <typename C, typename O>
void load_from_memory_by_rbx_offset_to_xmm0(C& c, O offset) {
  internal_load_from_memory_by_offset_to_xmm0(c, 0x83, offset);
}

template <typename C, typename O>
void internal_op_from_memory_by_offset_to_xmm0(uint8_t add_sub_mul_div_code, C& c, uint8_t reg, O offset) {
  auto o = static_cast<int64_t>(offset);
  o += 16;  // HACK(dkorolev): Shift by 16 doubles to have the opcodes have the same length.
  o *= 8;   // Double is eight bytes, signed multiplication by design.
  X64_JIT_ASSERT(o >= 0x80);
  X64_JIT_ASSERT(o <= 0x7fffffff);
  c.push_back(0xf2);
  c.push_back(0x0f);
  c.push_back(add_sub_mul_div_code);
  c.push_back(reg);
  for (size_t i = 0; i < 4; ++i) {
    c.push_back(o & 0xff);
    o >>= 8;
  }
}

template <typename C, typename O>
void add_from_memory_by_rdi_offset_to_xmm0(C& c, O offset) {
  internal_op_from_memory_by_offset_to_xmm0(0x58, c, 0x87, offset);
}

template <typename C, typename O>
void sub_from_memory_by_rdi_offset_to_xmm0(C& c, O offset) {
  internal_op_from_memory_by_offset_to_xmm0(0x5c, c, 0x87, offset);
}

template <typename C, typename O>
void mul_from_memory_by_rdi_offset_to_xmm0(C& c, O offset) {
  internal_op_from_memory_by_offset_to_xmm0(0x59, c, 0x87, offset);
}

template <typename C, typename O>
void div_from_memory_by_rdi_offset_to_xmm0(C& c, O offset) {
  internal_op_from_memory_by_offset_to_xmm0(0x5e, c, 0x87, offset);
}

template <typename C, typename O>
void add_from_memory_by_rsi_offset_to_xmm0(C& c, O offset) {
  internal_op_from_memory_by_offset_to_xmm0(0x58, c, 0x86, offset);
}

template <typename C, typename O>
void sub_from_memory_by_rsi_offset_to_xmm0(C& c, O offset) {
  internal_op_from_memory_by_offset_to_xmm0(0x5c, c, 0x86, offset);
}

template <typename C, typename O>
void mul_from_memory_by_rsi_offset_to_xmm0(C& c, O offset) {
  internal_op_from_memory_by_offset_to_xmm0(0x59, c, 0x86, offset);
}

template <typename C, typename O>
void div_from_memory_by_rsi_offset_to_xmm0(C& c, O offset) {
  internal_op_from_memory_by_offset_to_xmm0(0x5e, c, 0x86, offset);
}

template <typename C, typename O>
void add_from_memory_by_rbx_offset_to_xmm0(C& c, O offset) {
  internal_op_from_memory_by_offset_to_xmm0(0x58, c, 0x83, offset);
}

template <typename C, typename O>
void sub_from_memory_by_rbx_offset_to_xmm0(C& c, O offset) {
  internal_op_from_memory_by_offset_to_xmm0(0x5c, c, 0x83, offset);
}

template <typename C, typename O>
void mul_from_memory_by_rbx_offset_to_xmm0(C& c, O offset) {
  internal_op_from_memory_by_offset_to_xmm0(0x59, c, 0x83, offset);
}

template <typename C, typename O>
void div_from_memory_by_rbx_offset_to_xmm0(C& c, O offset) {
  internal_op_from_memory_by_offset_to_xmm0(0x5e, c, 0x83, offset);
}

template <typename C, typename O>
void internal_store_xmm0_to_memory_by_reg_offset(C& c, uint8_t reg, O offset) {
  auto o = static_cast<int64_t>(offset);
  o += 16;  // HACK(dkorolev): Shift by 16 doubles to have the opcodes have the same length.
  o *= 8;   // Double is eight bytes, signed multiplication by design.
  X64_JIT_ASSERT(o >= 0x80);
  X64_JIT_ASSERT(o <= 0x7fffffff);
  c.push_back(0xf2);
  c.push_back(0x0f);
  c.push_back(0x11);
  c.push_back(reg);
  for (size_t i = 0; i < 4; ++i) {
    c.push_back(o & 0xff);
    o >>= 8;
  }
}

template <typename C, typename O>
void store_xmm0_to_memory_by_rsi_offset(C& c, O offset) {
  internal_store_xmm0_to_memory_by_reg_offset(c, 0x86, offset);
}

template <typename C, typename O>
void store_xmm0_to_memory_by_rbx_offset(C& c, O offset) {
  internal_store_xmm0_to_memory_by_reg_offset(c, 0x83, offset);
}

template <typename C>
void call_function_from_rdx_pointers_array_by_index(C& c, uint8_t index) {
  X64_JIT_ASSERT(index < 31);  // Should fit one byte after adding one and multiplying by 8. -- D.K.
  c.push_back(0xff);
  c.push_back(0x52);
  c.push_back((index + 1) * 0x08);
}

}  // namespace opcodes

}  // namespace x64_native_jit
}  // namespace fncas
}  // namespace current

#endif  // defined(__x86_64__)

#endif  // FNCAS_X64_NATIVE_JIT_X64_NATIVE_JIT_H
