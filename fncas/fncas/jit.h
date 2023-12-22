/*******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * *******************************************************************************/

// FnCAS on-the-fly compilation logic.

#ifndef FNCAS_FNCAS_JIT_H
#define FNCAS_FNCAS_JIT_H

#ifndef CURRENT_WINDOWS
#if defined(__x86_64__)

#ifndef FNCAS_USE_LONG_DOUBLE

#define FNCAS_JIT_COMPILED

#include <iostream>
#include <sstream>
#include <stack>
#include <string>
#include <type_traits>
#include <vector>

#include <dlfcn.h>

#include "../../bricks/strings/printf.h"
#include "../../bricks/file/file.h"
#include "../../bricks/system/syscalls.h"

#include "base.h"
#include "node.h"
#include "differentiate.h"

namespace fncas {
namespace impl {

static_assert(std::is_same_v<double, double_t>, "FnCAS JIT assumes `double_t` is the native double.");

// Linux- and Mac-friendly code to compile into .so and link against it at runtime.
// Not portable.

inline const char* operation_as_assembler_opcode(MathOperation operation) {
  static const char* representation[static_cast<size_t>(MathOperation::end)] = {
      "addpd",
      "subpd",
      "mulpd",
      "divpd",
  };
  return operation < MathOperation::end ? representation[static_cast<size_t>(operation)] : "?";
}

struct compiled_expression final : noncopyable {
  typedef long long (*DIM)();
  typedef long long (*HEAP_SIZE)();
  typedef double (*FUNCTION)(const double* x, double* a);
  typedef void (*GRADIENT)(const double* x, double* a);

  void* lib_;
  DIM dim_;
  HEAP_SIZE heap_size_;
  FUNCTION function_;
  GRADIENT gradient_;

  const std::string lib_filename_;

  using gradient_indexes_t = std::vector<node_index_t>;
  gradient_indexes_t gradient_indexes_;

  explicit compiled_expression(const std::string& lib_filename,
                               const gradient_indexes_t& gradient_indexes = gradient_indexes_t())
      : lib_filename_(lib_filename), gradient_indexes_(gradient_indexes) {
    lib_ = dlopen(lib_filename.c_str(), RTLD_LAZY);
    CURRENT_ASSERT(lib_);
    dim_ = reinterpret_cast<HEAP_SIZE>(dlsym(lib_, "dim"));
    heap_size_ = reinterpret_cast<HEAP_SIZE>(dlsym(lib_, "heap_size"));
    function_ = reinterpret_cast<FUNCTION>(dlsym(lib_, "eval_f"));
    gradient_ = reinterpret_cast<GRADIENT>(dlsym(lib_, "eval_g"));
    CURRENT_ASSERT(dim_);
    CURRENT_ASSERT(heap_size_);
  }

  ~compiled_expression() {
    if (lib_) {
      dlclose(lib_);
    }
  }

  bool HasFunction() const { return function_; }
  bool HasGradient() const { return gradient_; }

  compiled_expression(const compiled_expression&) = delete;
  void operator=(const compiled_expression&) = delete;

  compiled_expression(compiled_expression&& rhs)
      : lib_(std::move(rhs.lib_)),
        dim_(std::move(rhs.dim_)),
        heap_size_(std::move(rhs.heap_size_)),
        function_(std::move(rhs.function_)),
        gradient_(std::move(rhs.gradient_)),
        lib_filename_(std::move(rhs.lib_filename_)) {
    rhs.lib_ = nullptr;
  }

  double compute_compiled_f(const double* x) const {
    CURRENT_ASSERT(dim_);
    CURRENT_ASSERT(heap_size_);
    CURRENT_ASSERT(function_);

    std::vector<double>& heap = internals_singleton().heap_for_compiled_evaluations_;
    const size_t desired_heap_size = heap_size_();
    if (heap.size() < desired_heap_size) {
      heap.resize(desired_heap_size);
    }

    return function_(x, &heap[0]);
  }

  double compute_compiled_f(const std::vector<double>& x) const { return compute_compiled_f(&x[0]); }

  std::vector<double> compute_compiled_g(const double* x) const {
    CURRENT_ASSERT(dim_);
    CURRENT_ASSERT(heap_size_);
    CURRENT_ASSERT(gradient_);
    const auto dim = static_cast<size_t>(dim_());
    CURRENT_ASSERT(gradient_indexes_.size() == dim);

    std::vector<double>& heap = internals_singleton().heap_for_compiled_evaluations_;
    const size_t desired_heap_size = heap_size_();
    if (heap.size() < desired_heap_size) {
      heap.resize(desired_heap_size);
    }

    gradient_(x, &heap[0]);

    CURRENT_ASSERT(gradient_indexes_.size() == dim);
    std::vector<double> result(dim);
    for (size_t i = 0; i < gradient_indexes_.size(); ++i) {
      CURRENT_ASSERT(static_cast<size_t>(gradient_indexes_[i]) < heap.size());
      result[i] = heap[gradient_indexes_[i]];
    }

    return result;
  }

  std::vector<double> compute_compiled_g(const std::vector<double>& x) const { return compute_compiled_g(&x[0]); }

  node_index_t dim() const { return dim_ ? static_cast<size_t>(dim_()) : 0; }
  size_t heap_size() const { return heap_size_ ? static_cast<size_t>(heap_size_()) : 0; }

  static void syscall(const std::string& command) {
    const int retval = current::bricks::system::SystemCall(command.c_str());
    if (retval) {
      std::cerr << command << std::endl << retval << std::endl;
      exit(-1);
    }
  }

  const std::string& lib_filename() const { return lib_filename_; }
};

template <>
class JITImplementation<JIT::NASM> final {
 public:
  JITImplementation(const std::string& filebase, bool has_g)
      : filebase(filebase), f(fopen((filebase + ".asm").c_str(), "w")) {
    CURRENT_ASSERT(f);

    fprintf(f, "[bits 64]\n");
    fprintf(f, "\n");
#ifdef CURRENT_APPLE
    fprintf(f, "global %s, _dim, _heap_size\n", !has_g ? "_eval_f" : "_eval_g");
    fprintf(f, "extern _sqrt, _exp, _log, _sin, _cos, _tan, _asin, _acos, _atan\n");
#else
    fprintf(f, "global %s, dim, heap_size\n", !has_g ? "eval_f" : "eval_g");
    fprintf(f, "extern sqrt, exp, log, sin, cos, tan, asin, acos, atan\n");
#endif
    fprintf(f, "\n");
    fprintf(f, "section .text\n");
    fprintf(f, "\n");
  }

  void compile_eval_f(node_index_t index) {
#ifdef CURRENT_APPLE
    fprintf(f, "_eval_f:\n");
#else
    fprintf(f, "eval_f:\n");
#endif
    fprintf(f, "  push rbp\n");
    fprintf(f, "  mov rbp, rsp\n");
    generate_nasm_code_for_node(index);
    fprintf(f, "  ; return a[%lld]\n", static_cast<long long>(index));
    fprintf(f, "  movq xmm0, [rsi+%lld]\n", static_cast<long long>(index) * 8);
    fprintf(f, "  mov rsp, rbp\n");
    fprintf(f, "  pop rbp\n");
    fprintf(f, "  ret\n");
  }

  void compile_eval_g(node_index_t f_index, const std::vector<node_index_t>& g_indexes) {
    CURRENT_ASSERT(g_indexes.size() == internals_singleton().dim_);
#ifdef CURRENT_APPLE
    fprintf(f, "_eval_g:\n");
#else
    fprintf(f, "eval_g:\n");
#endif
    fprintf(f, "  push rbp\n");
    fprintf(f, "  mov rbp, rsp\n");
    generate_nasm_code_for_node(f_index);
    for (size_t i = 0; i < g_indexes.size(); ++i) {
      generate_nasm_code_for_node(g_indexes[i]);
      fprintf(f, "  ; g[%lld] is a[%lld]\n", static_cast<long long>(i), static_cast<long long>(g_indexes[i]));
    }
    fprintf(f, "  ; return a[%lld]\n", static_cast<long long>(f_index));
    fprintf(f, "  movq xmm0, [rsi+%lld]\n", static_cast<long long>(f_index) * 8);
    fprintf(f, "  mov rsp, rbp\n");
    fprintf(f, "  pop rbp\n");
    fprintf(f, "  ret\n");
  }

  ~JITImplementation() {
    fprintf(f, "\n");
#ifdef CURRENT_APPLE
    fprintf(f, "_dim:\n");
#else
    fprintf(f, "dim:\n");
#endif
    fprintf(f, "  push rbp\n");
    fprintf(f, "  mov rbp, rsp\n");
    fprintf(f, "  mov rax, %lld\n", static_cast<long long>(internals_singleton().dim_));
    fprintf(f, "  mov rsp, rbp\n");
    fprintf(f, "  pop rbp\n");
    fprintf(f, "  ret\n");
    fprintf(f, "\n");
#ifdef CURRENT_APPLE
    fprintf(f, "_heap_size:\n");
#else
    fprintf(f, "heap_size:\n");
#endif
    fprintf(f, "  push rbp\n");
    fprintf(f, "  mov rbp, rsp\n");
    fprintf(f, "  mov rax, %lld\n", static_cast<long long>(max_dim + 1));
    fprintf(f, "  mov rsp, rbp\n");
    fprintf(f, "  pop rbp\n");
    fprintf(f, "  ret\n");
    fclose(f);

#ifdef CURRENT_APPLE
    const char* compile_cmdline = "nasm -O0 -f macho64 %s.asm -o %s.o";
    // `g++` is the best proxy for `ld` on OS X that passes proper command line args. -- M.Z.
    const char* link_cmdline = "g++ -shared -o %s.so %s.o";
#else
    const char* compile_cmdline = "nasm -O0 -f elf64 %s.asm -o %s.o";
    const char* link_cmdline = "ld -lm -shared -o %s.so %s.o";
#endif

    compiled_expression::syscall(current::strings::Printf(compile_cmdline, filebase.c_str(), filebase.c_str()));
    compiled_expression::syscall(current::strings::Printf(link_cmdline, filebase.c_str(), filebase.c_str()));
  }

 private:
  const std::string& filebase;
  FILE* f;
  std::vector<bool> computed;
  node_index_t max_dim = 0;

  // generate_nasm_code_for_node() writes NASM code to evaluate the expression to the file.
  void generate_nasm_code_for_node(node_index_t index) {
    const double d_0 = 0;
    const double d_1 = 1;
    std::stack<node_index_t> stack;
    stack.push(index);
    while (!stack.empty()) {
      const node_index_t i = stack.top();
      stack.pop();
      const node_index_t dependent_i = ~i;
      if (i > dependent_i) {
        max_dim = std::max(max_dim, static_cast<node_index_t>(i));
        if (computed.size() <= static_cast<size_t>(i)) {
          computed.resize(static_cast<size_t>(i) + 1);
        }
        if (!computed[i]) {
          computed[i] = true;
          node_impl& node = node_vector_singleton()[i];
          if (node.type() == NodeType::variable) {
            int32_t v = node.variable();
            fprintf(f, "  ; a[%lld] = x[%d];\n", static_cast<long long>(i), v);
            fprintf(f, "  mov rax, [rdi+%d]\n", v * 8);
            fprintf(f, "  mov [rsi+%lld], rax\n", static_cast<long long>(i) * 8);
          } else if (node.type() == NodeType::value) {
            fprintf(f, "  ; a[%lld] = %lf\n", static_cast<long long>(i), node.value());
            fprintf(f, "  mov rax, %s\n", std::to_string(*reinterpret_cast<int64_t*>(&node.value())).c_str());
            fprintf(f, "  mov [rsi+%lld], rax\n", static_cast<long long>(i) * 8);
          } else if (node.type() == NodeType::operation) {
            stack.push(~i);
            stack.push(node.lhs_index());
            stack.push(node.rhs_index());
          } else if (node.type() == NodeType::function) {
            stack.push(~i);
            stack.push(node.argument_index());
          } else {
            CURRENT_ASSERT(false);
          }
        }
      } else {
        node_impl& node = node_vector_singleton()[dependent_i];
        if (node.type() == NodeType::operation) {
          fprintf(f,
                  "  ; a[%lld] = a[%lld] %s a[%lld];\n",
                  static_cast<long long>(dependent_i),
                  static_cast<long long>(node.lhs_index()),
                  operation_as_string(node.operation()),
                  static_cast<long long>(node.rhs_index()));
          fprintf(f, "  movq xmm0, [rsi+%lld]\n", static_cast<long long>(node.lhs_index()) * 8);
          fprintf(f, "  movq xmm1, [rsi+%lld]\n", static_cast<long long>(node.rhs_index()) * 8);
          fprintf(f, "  %s xmm0, xmm1\n", operation_as_assembler_opcode(node.operation()));
          fprintf(f, "  movq [rsi+%lld], xmm0\n", static_cast<long long>(dependent_i) * 8);
        } else if (node.type() == NodeType::function) {
          if (node.function() == MathFunction::sqr) {
            fprintf(f,
                    "  ; a[%lld] = sqr(a[%lld]);  # `sqr` is a special case.\n",
                    static_cast<long long>(dependent_i),
                    static_cast<long long>(node.argument_index()));
            fprintf(f, "  movq xmm0, [rsi+%lld]\n", static_cast<long long>(node.argument_index()) * 8);
            fprintf(f, "  mulpd xmm0, xmm0\n");
            fprintf(f, "  movq [rsi+%lld], xmm0\n", static_cast<long long>(dependent_i) * 8);
          } else if (node.function() == MathFunction::unit_step) {
            fprintf(f,
                    "  ; a[%lld] = unit_step(a[%lld]);  # `unit_step` is a special case.\n",
                    static_cast<long long>(dependent_i),
                    static_cast<long long>(node.argument_index()));
            fprintf(f, "  movq xmm0, [rsi+%lld]\n", static_cast<long long>(node.argument_index()) * 8);
            fprintf(f, "  mov rax, %s  ; 0\n", std::to_string(*reinterpret_cast<const int64_t*>(&d_0)).c_str());
            fprintf(f, "  movq xmm1, rax\n");
            fprintf(f, "  ucomisd xmm0, xmm1\n");
            fprintf(f, "  jb unit_step_%lld\n", static_cast<long long>(dependent_i));
            fprintf(f, "  mov rax, %s  ; 1\n", std::to_string(*reinterpret_cast<const int64_t*>(&d_1)).c_str());
            fprintf(f, "unit_step_%lld:\n", static_cast<long long>(dependent_i));
            fprintf(f, "  mov [rsi+%lld], rax\n", static_cast<long long>(dependent_i) * 8);
          } else if (node.function() == MathFunction::ramp) {
            fprintf(f,
                    "  ; a[%lld] = ramp(a[%lld]);  # `ramp` is a special case.\n",
                    static_cast<long long>(dependent_i),
                    static_cast<long long>(node.argument_index()));
            fprintf(f, "  movq xmm0, [rsi+%lld]\n", static_cast<long long>(node.argument_index()) * 8);
            fprintf(f, "  mov rax, %s  ; 0\n", std::to_string(*reinterpret_cast<const int64_t*>(&d_0)).c_str());
            fprintf(f, "  movq xmm1, rax\n");
            fprintf(f, "  ucomisd xmm0, xmm1\n");
            fprintf(f, "  ja ramp_%lld\n", static_cast<long long>(dependent_i));
            fprintf(f, "  movq xmm0, rax\n");
            fprintf(f, "ramp_%lld:\n", static_cast<long long>(dependent_i));
            fprintf(f, "  movq [rsi+%lld], xmm0\n", static_cast<long long>(dependent_i) * 8);
          } else {
            fprintf(f,
                    "  ; a[%lld] = %s(a[%lld]);\n",
                    static_cast<long long>(dependent_i),
                    function_as_string(node.function()),
                    static_cast<long long>(node.argument_index()));
            fprintf(f, "  movq xmm0, [rsi+%lld]\n", static_cast<long long>(node.argument_index()) * 8);
            fprintf(f, "  push rdi\n");
            fprintf(f, "  push rsi\n");
#ifdef CURRENT_APPLE
            fprintf(f, "  call _%s\n", function_as_string(node.function()));
#else
            fprintf(f, "  call %s wrt ..plt\n", function_as_string(node.function()));
#endif
            fprintf(f, "  pop rsi\n");
            fprintf(f, "  pop rdi\n");
            fprintf(f, "  movq [rsi+%lld], xmm0\n", static_cast<long long>(dependent_i) * 8);
          }
        } else {
          CURRENT_ASSERT(false);
        }
      }
    }
  }
};

template <>
class JITImplementation<JIT::AS> final {
 public:
  JITImplementation(const std::string& filebase, bool has_g)
      : filebase(filebase), f(fopen((filebase + ".s").c_str(), "w")) {
    CURRENT_ASSERT(f);

    // `.section .text' is equivalent to the `.text` directive.
    fprintf(f, ".text\n");
#ifdef CURRENT_APPLE
    fprintf(f, ".globl %s, _dim, _heap_size\n", !has_g ? "_eval_f" : "_eval_g");
    fprintf(f, ".extern _sqrt, _exp, _log, _sin, _cos, _tan, _asin, _acos, _atan\n");
#else
    fprintf(f, ".globl %s, dim, heap_size\n", !has_g ? "eval_f" : "eval_g");
    fprintf(f, ".extern sqrt, exp, log, sin, cos, tan, asin, acos, atan\n");
#endif
    fprintf(f, "\n");
  }

  void compile_eval_f(node_index_t index) {
#ifdef CURRENT_APPLE
    fprintf(f, "_eval_f:\n");
#else
    fprintf(f, "eval_f:\n");
#endif
    fprintf(f, "  push %%rbp\n");
    fprintf(f, "  mov %%rsp, %%rbp\n");
    generate_as_code_for_node(index);
    fprintf(f, "  # return a[%lld]\n", static_cast<long long>(index));
    fprintf(f, "  movq %lld(%%rsi), %%xmm0\n", static_cast<long long>(index) * 8);
    fprintf(f, "  mov %%rbp, %%rsp\n");
    fprintf(f, "  pop %%rbp\n");
    fprintf(f, "  ret\n");
  }

  void compile_eval_g(node_index_t f_index, const std::vector<node_index_t>& g_indexes) {
    CURRENT_ASSERT(g_indexes.size() == internals_singleton().dim_);
#ifdef CURRENT_APPLE
    fprintf(f, "_eval_g:\n");
#else
    fprintf(f, "eval_g:\n");
#endif
    fprintf(f, "  push %%rbp\n");
    fprintf(f, "  mov %%rsp, %%rbp\n");
    generate_as_code_for_node(f_index);
    for (size_t i = 0; i < g_indexes.size(); ++i) {
      generate_as_code_for_node(g_indexes[i]);
      fprintf(f, "  # g[%lld] is a[%lld]\n", static_cast<long long>(i), static_cast<long long>(g_indexes[i]));
    }
    fprintf(f, "  # return a[%lld]\n", static_cast<long long>(f_index));
    fprintf(f, "  movq %lld(%%rsi), %%xmm0\n", static_cast<long long>(f_index) * 8);
    fprintf(f, "  mov %%rbp, %%rsp\n");
    fprintf(f, "  pop %%rbp\n");
    fprintf(f, "  ret\n");
  }

  ~JITImplementation() {
    fprintf(f, "\n");
#ifdef CURRENT_APPLE
    fprintf(f, "_dim:\n");
#else
    fprintf(f, "dim:\n");
#endif
    fprintf(f, "  push %%rbp\n");
    fprintf(f, "  mov %%rsp, %%rbp\n");
    fprintf(f, "  movabs $%lld, %%rax\n", static_cast<long long>(internals_singleton().dim_));
    fprintf(f, "  mov %%rbp, %%rsp\n");
    fprintf(f, "  pop %%rbp\n");
    fprintf(f, "  ret\n");
    fprintf(f, "\n");
#ifdef CURRENT_APPLE
    fprintf(f, "_heap_size:\n");
#else
    fprintf(f, "heap_size:\n");
#endif
    fprintf(f, "  push %%rbp\n");
    fprintf(f, "  mov %%rsp, %%rbp\n");
    fprintf(f, "  movabs $%lld, %%rax\n", static_cast<long long>(max_dim + 1));
    fprintf(f, "  mov %%rbp, %%rsp\n");
    fprintf(f, "  pop %%rbp\n");
    fprintf(f, "  ret\n");
    fclose(f);

    const char* cmdline = "gcc -O0 -shared %s.s -o %s.so -lm";
    compiled_expression::syscall(current::strings::Printf(cmdline, filebase.c_str(), filebase.c_str()));
  }

 private:
  const std::string& filebase;
  FILE* f;
  std::vector<bool> computed;
  node_index_t max_dim = 0;

  // generate_as_code_for_node() writes AS code to evaluate the expression to the file.
  void generate_as_code_for_node(node_index_t index) {
    const double d_0 = 0;
    const double d_1 = 1;
    std::stack<node_index_t> stack;
    stack.push(index);
    while (!stack.empty()) {
      const node_index_t i = stack.top();
      stack.pop();
      const node_index_t dependent_i = ~i;
      if (i > dependent_i) {
        max_dim = std::max(max_dim, static_cast<node_index_t>(i));
        if (computed.size() <= static_cast<size_t>(i)) {
          computed.resize(static_cast<size_t>(i) + 1);
        }
        if (!computed[i]) {
          computed[i] = true;
          node_impl& node = node_vector_singleton()[i];
          if (node.type() == NodeType::variable) {
            int32_t v = node.variable();
            fprintf(f, "  # a[%lld] = x[%d];\n", static_cast<long long>(i), v);
            fprintf(f, "  mov %d(%%rdi), %%rax\n", v * 8);
            fprintf(f, "  mov %%rax, %lld(%%rsi)\n", static_cast<long long>(i) * 8);
          } else if (node.type() == NodeType::value) {
            fprintf(f, "  # a[%lld] = %lf\n", static_cast<long long>(i), node.value());
            fprintf(f, "  movabs $%s, %%rax\n", std::to_string(*reinterpret_cast<int64_t*>(&node.value())).c_str());
            fprintf(f, "  mov %%rax, %lld(%%rsi)\n", static_cast<long long>(i) * 8);
          } else if (node.type() == NodeType::operation) {
            stack.push(~i);
            stack.push(node.lhs_index());
            stack.push(node.rhs_index());
          } else if (node.type() == NodeType::function) {
            stack.push(~i);
            stack.push(node.argument_index());
          } else {
            CURRENT_ASSERT(false);
          }
        }
      } else {
        node_impl& node = node_vector_singleton()[dependent_i];
        if (node.type() == NodeType::operation) {
          fprintf(f,
                  "  # a[%lld] = a[%lld] %s a[%lld];\n",
                  static_cast<long long>(dependent_i),
                  static_cast<long long>(node.lhs_index()),
                  operation_as_string(node.operation()),
                  static_cast<long long>(node.rhs_index()));
          fprintf(f, "  movq %lld(%%rsi), %%xmm0\n", static_cast<long long>(node.lhs_index()) * 8);
          fprintf(f, "  movq %lld(%%rsi), %%xmm1\n", static_cast<long long>(node.rhs_index()) * 8);
          fprintf(f, "  %s %%xmm1, %%xmm0\n", operation_as_assembler_opcode(node.operation()));
          fprintf(f, "  movq %%xmm0, %lld(%%rsi)\n", static_cast<long long>(dependent_i) * 8);
        } else if (node.type() == NodeType::function) {
          if (node.function() == MathFunction::sqr) {
            fprintf(f,
                    "  # a[%lld] = sqr(a[%lld]);  # `sqr` is a special case.\n",
                    static_cast<long long>(dependent_i),
                    static_cast<long long>(node.argument_index()));
            fprintf(f, "  movq %lld(%%rsi), %%xmm0\n", static_cast<long long>(node.argument_index()) * 8);
            fprintf(f, "  mulpd %%xmm0, %%xmm0\n");
            fprintf(f, "  movq %%xmm0, %lld(%%rsi)\n", static_cast<long long>(dependent_i) * 8);
          } else if (node.function() == MathFunction::unit_step) {
            fprintf(f,
                    "  # a[%lld] = unit_step(a[%lld]);  # `unit_step` is a special case.\n",
                    static_cast<long long>(dependent_i),
                    static_cast<long long>(node.argument_index()));
            fprintf(f, "  movq %lld(%%rsi), %%xmm0\n", static_cast<long long>(node.argument_index()) * 8);
            fprintf(f, "  movabs $%s, %%rax  # 0\n", std::to_string(*reinterpret_cast<const int64_t*>(&d_0)).c_str());
            fprintf(f, "  movq %%rax, %%xmm1\n");
            fprintf(f, "  ucomisd %%xmm1, %%xmm0\n");
            fprintf(f, "  jb . +12\n");  // NOTE(dkorolev): `. +12` skips the next `movabs`.
            fprintf(f, "  movabs $%s, %%rax #; 1\n", std::to_string(*reinterpret_cast<const int64_t*>(&d_1)).c_str());
            fprintf(f, "  mov %%rax, %lld(%%rsi)\n", static_cast<long long>(dependent_i) * 8);
          } else if (node.function() == MathFunction::ramp) {
            fprintf(f,
                    "  # a[%lld] = ramp(a[%lld]);  # `ramp` is a special case.\n",
                    static_cast<long long>(dependent_i),
                    static_cast<long long>(node.argument_index()));
            fprintf(f, "  movq %lld(%%rsi), %%xmm0\n", static_cast<long long>(node.argument_index()) * 8);
            fprintf(f, "  movabs $%s, %%rax #; 0\n", std::to_string(*reinterpret_cast<const int64_t*>(&d_0)).c_str());
            fprintf(f, "  movq %%rax, %%xmm1\n");
            fprintf(f, "  ucomisd %%xmm1, %%xmm0\n");
            fprintf(f, "  ja . +7\n");  // NOTE(dkorolev): `. +7` skips the next `movq`.
            fprintf(f, "  movq %%rax, %%xmm0\n");
            fprintf(f, "  movq %%xmm0, %lld(%%rsi)\n", static_cast<long long>(dependent_i) * 8);
          } else {
            fprintf(f,
                    "  # a[%lld] = %s(a[%lld]);\n",
                    static_cast<long long>(dependent_i),
                    function_as_string(node.function()),
                    static_cast<long long>(node.argument_index()));
            fprintf(f, "  movq %lld(%%rsi), %%xmm0\n", static_cast<long long>(node.argument_index()) * 8);
            fprintf(f, "  push %%rdi\n");
            fprintf(f, "  push %%rsi\n");
#ifdef CURRENT_APPLE
            fprintf(f, "  call _%s\n", function_as_string(node.function()));
#else
            fprintf(f, "  call %s@plt\n", function_as_string(node.function()));
#endif
            fprintf(f, "  pop %%rsi\n");
            fprintf(f, "  pop %%rdi\n");
            fprintf(f, "  movq %%xmm0, %lld(%%rsi)\n", static_cast<long long>(dependent_i) * 8);
          }
        } else {
          CURRENT_ASSERT(false);
        }
      }
    }
  }
};

template <>
class JITImplementation<JIT::CLANG> final {
 public:
  JITImplementation(const std::string& filebase, bool has_g)
      : filebase(filebase), f(fopen((filebase + ".c").c_str(), "w")) {
    static_cast<void>(has_g);
    CURRENT_ASSERT(f);
    fprintf(f, "#include <math.h>\n");
    fprintf(f, "#define sqr(x) ((x) * (x))\n");
    fprintf(f, "#define unit_step(x) ((x) >= 0 ? 1 : 0)\n");
    fprintf(f, "#define ramp(x) ((x) >= 0 ? (x) : 0)\n");
  }

  void compile_eval_f(node_index_t index) {
    fprintf(f, "double eval_f(const double* x, double* a) {\n");
    generate_c_code_for_node(index);
    fprintf(f, "  return a[%lld];\n", static_cast<long long>(index));
    fprintf(f, "}\n");
  }

  void compile_eval_g(node_index_t f_index, const std::vector<node_index_t>& g_indexes) {
    CURRENT_ASSERT(g_indexes.size() == internals_singleton().dim_);
    fprintf(f, "double eval_g(const double* x, double* a) {\n");
    generate_c_code_for_node(f_index);
    for (size_t i = 0; i < g_indexes.size(); ++i) {
      generate_c_code_for_node(g_indexes[i]);
    }
    for (size_t i = 0; i < g_indexes.size(); ++i) {
      fprintf(f, "  // g[%lld] is a[%lld]\n", static_cast<long long>(i), static_cast<long long>(g_indexes[i]));
    }
    fprintf(f, "  return a[%lld];\n", static_cast<long long>(f_index));
    fprintf(f, "}\n");
  }

  ~JITImplementation() {
    fprintf(f, "long long dim() { return %lld; }\n", static_cast<long long>(internals_singleton().dim_));
    fprintf(f, "long long heap_size() { return %lld; }\n", static_cast<long long>(max_dim + 1));

    fclose(f);

    const char* compile_cmdline = "clang -fPIC -shared -nostartfiles %s.c -o %s.so";  // `-O3` is too slow.
    std::string cmdline = current::strings::Printf(compile_cmdline, filebase.c_str(), filebase.c_str());
    compiled_expression::syscall(cmdline);
  }

 private:
  const std::string& filebase;
  FILE* f;
  std::vector<bool> computed;
  node_index_t max_dim = 0;

  // generate_c_code_for_node() writes C code to evaluate the expression to the file.
  void generate_c_code_for_node(node_index_t index) {
    std::stack<node_index_t> stack;
    stack.push(index);
    while (!stack.empty()) {
      const node_index_t i = stack.top();
      stack.pop();
      const node_index_t dependent_i = ~i;
      if (i > dependent_i) {
        max_dim = std::max(max_dim, static_cast<node_index_t>(i));
        if (computed.size() <= static_cast<size_t>(i)) {
          computed.resize(static_cast<size_t>(i) + 1);
        }
        if (!computed[i]) {
          computed[i] = true;
          node_impl& node = node_vector_singleton()[i];
          if (node.type() == NodeType::variable) {
            int32_t v = node.variable();
            fprintf(f, "  a[%lld] = x[%d];\n", static_cast<long long>(i), v);
          } else if (node.type() == NodeType::value) {
            fprintf(f,
                    "  a[%lld] = %a;  // %lf\n",  // "%a" is hexadecimal full precision.
                    static_cast<long long>(i),
                    node.value(),
                    node.value());  // "%a" is hexadecimal full precision.
          } else if (node.type() == NodeType::operation) {
            stack.push(~i);
            stack.push(node.lhs_index());
            stack.push(node.rhs_index());
          } else if (node.type() == NodeType::function) {
            stack.push(~i);
            stack.push(node.argument_index());
          } else {
            CURRENT_ASSERT(false);
          }
        }
      } else {
        node_impl& node = node_vector_singleton()[dependent_i];
        if (node.type() == NodeType::operation) {
          fprintf(f,
                  "  a[%lld] = a[%lld] %s a[%lld];\n",
                  static_cast<long long>(dependent_i),
                  static_cast<long long>(node.lhs_index()),
                  operation_as_string(node.operation()),
                  static_cast<long long>(node.rhs_index()));
        } else if (node.type() == NodeType::function) {
          fprintf(f,
                  "  a[%lld] = %s(a[%lld]);\n",
                  static_cast<long long>(dependent_i),
                  function_as_string(node.function()),
                  static_cast<long long>(node.argument_index()));
        } else {
          CURRENT_ASSERT(false);
        }
      }
    }
  }
};

template <JIT JIT_IMPLEMENTATION>
compiled_expression compile_eval_f(node_index_t index) {
  const std::string filebase(current::FileSystem::GenTmpFileName());
  const std::string filename_so = filebase + ".so";
  current::FileSystem::RmFile(filename_so, current::FileSystem::RmFileParameters::Silent);
  {
    JITImplementation<JIT_IMPLEMENTATION> code_generator(filebase, false);
    code_generator.compile_eval_f(index);
  }
  return compiled_expression(filename_so);
}

template <JIT JIT_IMPLEMENTATION>
inline compiled_expression compile_eval_f(const V& node) {
  return compile_eval_f<JIT_IMPLEMENTATION>(node.index_);
}

template <JIT JIT_IMPLEMENTATION>
compiled_expression compile_eval_g(node_index_t f_index, const std::vector<node_index_t>& g_indexes) {
  const std::string filebase(current::FileSystem::GenTmpFileName());
  const std::string filename_so = filebase + ".so";
  current::FileSystem::RmFile(filename_so, current::FileSystem::RmFileParameters::Silent);
  {
    JITImplementation<JIT_IMPLEMENTATION> code_generator(filebase, true);
    code_generator.compile_eval_g(f_index, g_indexes);
  }
  return compiled_expression(filename_so, g_indexes);
}

template <JIT JIT_IMPLEMENTATION>
inline compiled_expression compile_eval_g(const V& f_node, const std::vector<V>& g_nodes) {
  std::vector<node_index_t> g_node_indexes;
  g_node_indexes.reserve(g_nodes.size());
  for (const auto& gi : g_nodes) {
    g_node_indexes.push_back(gi.index_);
  }
  return compile_eval_g<JIT_IMPLEMENTATION>(f_node.index_, g_node_indexes);
}

struct f_compiled_super : f_super {};

template <JIT JIT_IMPLEMENTATION>
struct f_compiled final : f_compiled_super {
  fncas::impl::compiled_expression c_;

  explicit f_compiled(const V& node) : c_(compile_eval_f<JIT_IMPLEMENTATION>(node)) {
    CURRENT_ASSERT(c_.HasFunction());
  }
  explicit f_compiled(const f_impl<JIT::Blueprint>& f) : c_(compile_eval_f<JIT_IMPLEMENTATION>(f.f_)) {
    CURRENT_ASSERT(c_.HasFunction());
  }

  f_compiled(const f_compiled&) = delete;
  void operator=(const f_compiled&) = delete;
  f_compiled(f_compiled&& rhs) : c_(std::move(rhs.c_)) { CURRENT_ASSERT(c_.HasFunction()); }

  double operator()(const std::vector<double>& x) const override { return c_.compute_compiled_f(x); }

  size_t dim() const override { return c_.dim(); }
  size_t heap_size() const override { return c_.heap_size(); }

  const std::string& lib_filename() const { return c_.lib_filename(); }
};

#ifdef FNCAS_X64_NATIVE_JIT_ENABLED

// #define FNCAS_DEBUG_NATIVE_JIT  // NOTE(dkorolev): This line should be commented out.

#if defined(FNCAS_DEBUG_NATIVE_JIT) && defined(NDEBUG)
#error "Someone forgot to un-#define `FNCAS_DEBUG_NATIVE_JIT`."
#endif

struct JITCodeGenerator final {
  std::vector<uint8_t>& code;
  size_t const dim;  // "Pre-allocated" in the output vector, 0 for function computation, dim. of `x` for gradients.

  std::vector<bool> computed;
  node_index_t max_dim = 0;

  JITCodeGenerator(std::vector<uint8_t>& code, size_t dim) : code(code), dim(dim) {
    using namespace current::fncas::x64_native_jit;

    opcodes::push_rbx(code);
    opcodes::mov_rsi_rbx(code);

#ifdef FNCAS_DEBUG_NATIVE_JIT
    std::cerr << "begin();\n";
#endif
  }

  ~JITCodeGenerator() {
    using namespace current::fncas::x64_native_jit;

    opcodes::pop_rbx(code);
    opcodes::ret(code);

#ifdef FNCAS_DEBUG_NATIVE_JIT
    std::cerr << "end();\n";
#endif
  }

  void jit_compile_node(node_index_t index) {
    using namespace current::fncas::x64_native_jit;

    std::stack<node_index_t> stack;
    stack.push(index);

    while (!stack.empty()) {
      const node_index_t i = stack.top();
      stack.pop();
      const node_index_t dependent_i = ~i;
      if (i > dependent_i) {
        max_dim = std::max(max_dim, static_cast<node_index_t>(i));
        if (computed.size() <= static_cast<size_t>(i)) {
          computed.resize(static_cast<size_t>(i) + 1);
        }
        if (!computed[i]) {
          computed[i] = true;
          node_impl& node = node_vector_singleton()[i];
          if (node.type() == NodeType::variable) {
            int32_t v = node.variable();
            // Load input variable `v` by address `i + dim`, because the first `dim` of the output are the gradient.
            // NOTE(dkorolev) / HACK(dkorolev): Perhaps use `rax`, not `xmm0`, for mere value transfer?
            opcodes::load_from_memory_by_rdi_offset_to_xmm0(code, v);
            opcodes::store_xmm0_to_memory_by_rbx_offset(code, i + dim);
#ifdef FNCAS_DEBUG_NATIVE_JIT
            std::cerr << "# Z[" << i << " + " << dim << "] = X[" << v << "];\n";
            std::cerr << "load_from_memory_by_rdi_offset_to_xmm0(" << v << ");\n";
            std::cerr << "store_xmm0_to_memory_by_rbx_offset(" << i + dim << ");\n";
#endif
          } else if (node.type() == NodeType::value) {
            // Load value `node.value()` by address `i + dim`, because the first `dim` of the output are the gradient.
            opcodes::load_immediate_to_memory_by_rbx_offset(code, i + dim, node.value());
#ifdef FNCAS_DEBUG_NATIVE_JIT
            std::cerr << "# Z[" << i << " + " << dim << "] = " << node.value() << ";\n";
            std::cerr << "load_immediate_to_memory_by_rbx_offset(" << i + dim << ", " << node.value() << ");\n";
#endif
          } else if (node.type() == NodeType::operation) {
            stack.push(~i);
            stack.push(node.lhs_index());
            stack.push(node.rhs_index());
          } else if (node.type() == NodeType::function) {
            stack.push(~i);
            stack.push(node.argument_index());
          } else {
            CURRENT_ASSERT(false);
          }
        }
      } else {
        node_impl& node = node_vector_singleton()[dependent_i];
        if (node.type() == NodeType::operation) {
          // Perform add/sub/mul/div, all in the operating memory, shifted by `dim`.
          auto const op = node.operation();
          opcodes::load_from_memory_by_rbx_offset_to_xmm0(code, node.lhs_index() + dim);
#ifdef FNCAS_DEBUG_NATIVE_JIT
          std::cerr << "# Z[" << dependent_i << " + " << dim << "] = Z[" << node.lhs_index() << " + " << dim << "] "
                    << operation_as_string(op) << " Z[" << node.rhs_index() << " + " << dim << "];\n";

          std::cerr << "load_from_memory_by_rbx_offset_to_xmm0(" << node.lhs_index() + dim << ");\n";
#endif
          if (op == MathOperation::add) {
            opcodes::add_from_memory_by_rbx_offset_to_xmm0(code, node.rhs_index() + dim);
          } else if (op == MathOperation::subtract) {
            opcodes::sub_from_memory_by_rbx_offset_to_xmm0(code, node.rhs_index() + dim);
          } else if (op == MathOperation::multiply) {
            opcodes::mul_from_memory_by_rbx_offset_to_xmm0(code, node.rhs_index() + dim);
          } else if (op == MathOperation::divide) {
            opcodes::div_from_memory_by_rbx_offset_to_xmm0(code, node.rhs_index() + dim);
          } else {
            CURRENT_ASSERT(false);
          }
          opcodes::store_xmm0_to_memory_by_rbx_offset(code, dependent_i + dim);
#ifdef FNCAS_DEBUG_NATIVE_JIT
          std::cerr << std::string(operation_as_assembler_opcode(op)).substr(0, 3)
                    << "_from_memory_by_rbx_offset_to_xmm0(" << node.rhs_index() + dim << ");\n";
          std::cerr << "store_xmm0_to_memory_by_rbx_offset(" << dependent_i + dim << ");\n";
#endif
        } else if (node.type() == NodeType::function) {
          opcodes::load_from_memory_by_rbx_offset_to_xmm0(code, node.argument_index() + dim);
          opcodes::push_rdi(code);
          opcodes::push_rdx(code);
          opcodes::call_function_from_rdx_pointers_array_by_index(code, static_cast<uint8_t>(node.function()));
          opcodes::pop_rdx(code);
          opcodes::pop_rdi(code);
          opcodes::store_xmm0_to_memory_by_rbx_offset(code, dependent_i + dim);
#ifdef FNCAS_DEBUG_NATIVE_JIT
          std::cerr << "# Z[" << dependent_i << " + " << dim << "] = " << function_as_string(node.function()) << "(Z["
                    << node.argument_index() << " + " << dim << "]);\n";
          std::cerr << "load_from_memory_by_rbx_offset_to_xmm0(" << node.argument_index() + dim << ");\n";
          std::cerr << "push_rdi();\n";
          std::cerr << "push_rdx();\n";
          std::cerr << "call_function_from_rdx_pointers_array_by_index(" << static_cast<int>(node.function()) << ");\n";
          std::cerr << "pop_rdx();\n";
          std::cerr << "pop_rdi();\n";
          std::cerr << "store_xmm0_to_memory_by_rbx_offset(" << dependent_i + dim << ");\n";
#endif
        } else {
          CURRENT_ASSERT(false);
        }
      }
    }
  }
};

struct x64_native_jit_function_pointers {
  std::vector<double (*)(double x)> p;
  x64_native_jit_function_pointers() {
#define FNCAS_FUNCTION(f) p.push_back(fncas::f);
#include "fncas_functions.dsl.h"
#undef FNCAS_FUNCTION
  }
  static x64_native_jit_function_pointers& tls() {
    return current::ThreadLocalSingleton<x64_native_jit_function_pointers>();
  }
};

struct f_compiled_x64_native_jit final {
  std::unique_ptr<current::fncas::x64_native_jit::CallableVectorUInt8> jit_compiled_code;
  mutable std::vector<double> actual_heap;

  void generate_code_for_f(V const& v) {
    std::vector<uint8_t> code;
    size_t required_heap_size;
    {
      using namespace current::fncas::x64_native_jit;
      JITCodeGenerator code_generator(code, 0);
      code_generator.jit_compile_node(v.index());
      opcodes::load_from_memory_by_rbx_offset_to_xmm0(code, v.index());
#ifdef FNCAS_DEBUG_NATIVE_JIT
      std::cerr << "# return Z[" << v.index() << "];\n";
      std::cerr << "load_from_memory_by_rbx_offset_to_xmm0(" << v.index() << ");\n";
#endif
      required_heap_size = code_generator.max_dim + 1;
    }
#ifdef FNCAS_DEBUG_NATIVE_JIT
    std::cerr << "Code:";
    for (uint8_t c : code) {
      fprintf(stderr, " %02x", int(c));
    }
    std::cerr << "\nHeap size: " << required_heap_size << '\n';
    std::cerr << "Desired index: " << v.index() << '\n';
#endif
    jit_compiled_code = std::make_unique<current::fncas::x64_native_jit::CallableVectorUInt8>(code);
    actual_heap.resize(required_heap_size);
  }

  explicit f_compiled_x64_native_jit(V const& node) { generate_code_for_f(node); }

  explicit f_compiled_x64_native_jit(const f_impl<JIT::Blueprint>& f) { generate_code_for_f(f.f_); }

  double operator()(const std::vector<double>& x) const {
    return (*jit_compiled_code)(&x[0], &actual_heap[0], &x64_native_jit_function_pointers::tls().p[0]);
  }

  // For backwards "compatibility" with the unit tests. -- D.K.
  static const char* lib_filename() { return ""; }
};

struct g_compiled_x64_native_jit final {
  size_t const dim;
  std::unique_ptr<current::fncas::x64_native_jit::CallableVectorUInt8> jit_compiled_code;
  mutable std::vector<double> actual_heap;

  void generate_code_for_g(JITCodeGenerator& code_generator, V const& v, size_t output_index) {
    using namespace current::fncas::x64_native_jit;
    code_generator.jit_compile_node(v.index());
    opcodes::load_from_memory_by_rbx_offset_to_xmm0(code_generator.code, v.index() + dim);
    opcodes::store_xmm0_to_memory_by_rbx_offset(code_generator.code, output_index);
#ifdef FNCAS_DEBUG_NATIVE_JIT
    std::cerr << "# G[" << output_index << "] = Z[" << v.index() << " + " << dim << "];\n";
    std::cerr << "load_from_memory_by_rbx_offset_to_xmm0(" << v.index() + dim << ");\n";
    std::cerr << "store_xmm0_to_memory_by_rbx_offset(" << output_index << ");\n";
#endif
  }

  g_compiled_x64_native_jit(const f_impl<JIT::Blueprint>& unused_f, const g_impl<JIT::Blueprint>& g)
      : dim(g.g_.size()) {
    CURRENT_ASSERT(dim == internals_singleton().dim_);
    std::vector<uint8_t> code;
    {
      JITCodeGenerator code_generator(code, dim);
      static_cast<void>(unused_f);
      for (size_t i = 0; i < dim; ++i) {
        generate_code_for_g(code_generator, g.g_[i], i);
      }
      CURRENT_ASSERT(static_cast<size_t>(code_generator.max_dim + 1) >= dim);
      actual_heap.resize(dim + code_generator.max_dim + 1);
    }
#ifdef FNCAS_DEBUG_NATIVE_JIT
    std::cerr << "Code:";
    for (uint8_t c : code) {
      fprintf(stderr, " %02x", int(c));
    }
    std::cerr << "\nHeap size: " << actual_heap.size() << '\n';
#endif
    jit_compiled_code = std::make_unique<current::fncas::x64_native_jit::CallableVectorUInt8>(code);
  }

  // NOTE(dkorolev): Perhaps just return a pointer to `&actual_heap[0]` to avoid a copy?
  // NOTE(dkorolev): This would require looking into the optimizer(s) code, I'll do it some time later.
  std::vector<double> operator()(const std::vector<double>& x) const {
    (*jit_compiled_code)(&x[0], &actual_heap[0], &x64_native_jit_function_pointers::tls().p[0]);
    return std::vector<double>(&actual_heap[0], &actual_heap[0] + dim);
  }

  // For backwards "compatibility" with the unit tests. -- D.K.
  static const char* lib_filename() { return ""; }
};

#endif  // FNCAS_X64_NATIVE_JIT_ENABLED

struct g_compiled_super : g_super {};

template <JIT JIT_IMPLEMENTATION>
struct g_compiled final : g_compiled_super {
  fncas::impl::compiled_expression c_;

  explicit g_compiled(const f_impl<JIT::Blueprint>& f, const g_impl<JIT::Blueprint>& g)
      : c_(compile_eval_g<JIT_IMPLEMENTATION>(f.f_, g.g_)) {
    CURRENT_ASSERT(c_.HasGradient());
  }

  g_compiled(const g_compiled&) = delete;
  void operator=(const g_compiled&) = delete;
  g_compiled(g_compiled&& rhs) : c_(std::move(rhs.c_)) { CURRENT_ASSERT(c_.HasGradient()); }

  std::vector<double> operator()(const std::vector<double>& x) const override { return c_.compute_compiled_g(x); }

  size_t dim() const override { return c_.dim(); }
  size_t heap_size() const override { return c_.heap_size(); }

  const std::string& lib_filename() const { return c_.lib_filename(); }
};

// Expose JIT-compiled functions as `fncas::function_t<JIT::*>`.
template <>
struct f_impl_selector<JIT::AS> {
  using type = f_compiled<JIT::AS>;
};

template <>
struct f_impl_selector<JIT::CLANG> {
  using type = f_compiled<JIT::CLANG>;
};

template <>
struct f_impl_selector<JIT::NASM> {
  using type = f_compiled<JIT::NASM>;
};

#ifdef FNCAS_X64_NATIVE_JIT_ENABLED
template <>
struct f_impl_selector<JIT::X64NativeJIT> {
  using type = f_compiled_x64_native_jit;
};
#endif  // FNCAS_X64_NATIVE_JIT_ENABLED

// Expose JIT-compiled gradients as `fncas::gradient_t<JIT::*>`.
template <>
struct g_impl_selector<JIT::AS> {
  using type = g_compiled<JIT::AS>;
};

template <>
struct g_impl_selector<JIT::CLANG> {
  using type = g_compiled<JIT::CLANG>;
};

template <>
struct g_impl_selector<JIT::NASM> {
  using type = g_compiled<JIT::NASM>;
};

#ifdef FNCAS_X64_NATIVE_JIT_ENABLED
template <>
struct g_impl_selector<JIT::X64NativeJIT> {
  using type = g_compiled_x64_native_jit;
};
#endif  // FNCAS_X64_NATIVE_JIT_ENABLED

}  // namespace impl
}  // namespace fncas

#endif  // FNCAS_USE_LONG_DOUBLE

#endif  // defined(__x86_64__)
#endif  // !CURRENT_WINDOWS

#endif  // #ifndef FNCAS_FNCAS_JIT_H
