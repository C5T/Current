// https://github.com/dkorolev/fncas

// FNCAS on-the-fly compilation logic.
// FNCAS_JIT must be defined to enable, supported values are 'NASM' and 'CLANG'.

#ifndef FNCAS_JIT_H
#define FNCAS_JIT_H

#ifdef FNCAS_JIT

#include <sstream>
#include <stack>
#include <string>
#include <vector>

#include <dlfcn.h>

#include <boost/assert.hpp>
#include <boost/format.hpp>

#include "fncas_base.h"
#include "fncas_node.h"

namespace fncas {

// Linux-friendly code to compile into .so and link against it at runtime.
// Not portable.

struct compiled_expression : boost::noncopyable {
  typedef int (*DIM)();
  typedef double (*EVAL)(const double* x, double* a);
  void* lib_;
  DIM dim_;
  EVAL eval_;
  explicit compiled_expression(const std::string& lib_filename) {
    lib_ = dlopen(lib_filename.c_str(), RTLD_LAZY);
    BOOST_ASSERT(lib_);
    dim_ = reinterpret_cast<DIM>(dlsym(lib_, "dim"));
    eval_ = reinterpret_cast<EVAL>(dlsym(lib_, "eval"));
    BOOST_ASSERT(dim_);
    BOOST_ASSERT(eval_);
  }
  ~compiled_expression() {
    dlclose(lib_);
  }
  double eval(const double* x) const {
    std::vector<double>& tmp = internals().ram_for_evaluations;
    size_t dim = static_cast<size_t>(dim_());
    if (tmp.size() < dim) {
      tmp.resize(dim);
    }
    return eval_(x, &tmp[0]);
  }
  double eval(const std::vector<double>& x) const {
    return eval(&x[0]);
  }
  static void syscall(const std::string& command) {
    int retval = system(command.c_str());
    if (retval) {
      std::cerr << command << std::endl << retval << std::endl;
      exit(-1);
    }
  }
};

// generate_c_code_for_node() writes C code to evaluate the expression to the file.
void generate_c_code_for_node(uint32_t index, FILE* f) {
  fprintf(f, "#include <math.h>\n");
  fprintf(f, "double eval(const double* x, double* a) {\n");
  size_t max_dim = index;
  std::stack<uint32_t> stack;
  stack.push(index);
  while (!stack.empty()) {
    const uint32_t i = stack.top();
    stack.pop();
    const uint32_t dependent_i = ~i;
    if (i < dependent_i) {
      max_dim = std::max(max_dim, static_cast<size_t>(i));
      node_impl& node = node_vector_singleton()[i];
      if (node.type() == type_t::variable) {
        uint32_t v = node.variable();
        fprintf(f, "  a[%d] = x[%d];\n", i, v);
      } else if (node.type() == type_t::value) {
        fprintf(f, "  a[%d] = %a;\n", i, node.value());  // "%a" is hexadecial full precision.
      } else if (node.type() == type_t::operation) {
        stack.push(~i);
        stack.push(node.lhs_index());
        stack.push(node.rhs_index());
      } else if (node.type() == type_t::function) {
        stack.push(~i);
        stack.push(node.argument_index());
      } else {
        BOOST_ASSERT(false);
      }
    } else {
      node_impl& node = node_vector_singleton()[dependent_i];
      if (node.type() == type_t::operation) {
        fprintf(
          f,
          "  a[%d] = a[%d] %s a[%d];\n",
          dependent_i,
          node.lhs_index(),
          operation_as_string(node.operation()),
          node.rhs_index());
      } else if (node.type() == type_t::function) {
        fprintf(
          f,
          "  a[%d] = %s(a[%d]);\n",
          dependent_i,
          function_as_string(node.function()),
          node.argument_index());
      } else {
        BOOST_ASSERT(false);
      }
    }
  }
  fprintf(f, "  return a[%d];\n", index);
  fprintf(f, "}\n");
  fprintf(f, "int dim() { return %d; }\n", static_cast<int>(max_dim + 1));
}

// generate_asm_code_for_node() writes NASM code to evaluate the expression to the file.
const char* const operation_as_nasm_instruction(operation_t operation) {
  static const char* representation[static_cast<size_t>(operation_t::end)] = {
    "addpd", "subpd", "mulpd", "divpd",
  };
  return operation < operation_t::end ? representation[static_cast<size_t>(operation)] : "?";
}
void generate_asm_code_for_node(uint32_t index, FILE* f) {
  fprintf(f, "[bits 64]\n");
  fprintf(f, "\n");
  fprintf(f, "global eval, dim\n");
  fprintf(f, "extern exp, log, sin, cos, tan, asin, acos, atan\n");
  fprintf(f, "\n");
  fprintf(f, "section .text\n");
  fprintf(f, "\n");
  fprintf(f, "eval:\n");
  fprintf(f, "  push rbp\n");
  fprintf(f, "  mov rbp, rsp\n");
  size_t max_dim = index;
  std::stack<uint32_t> stack;
  stack.push(index);
  while (!stack.empty()) {
    const uint32_t i = stack.top();
    stack.pop();
    const uint32_t dependent_i = ~i;
    if (i < dependent_i) {
      max_dim = std::max(max_dim, static_cast<size_t>(i));
      node_impl& node = node_vector_singleton()[i];
      if (node.type() == type_t::variable) {
        uint32_t v = node.variable();
        fprintf(f, "  ; a[%d] = x[%d];\n", i, v);
        fprintf(f, "  mov rax, [rdi+%d]\n", v * 8);
        fprintf(f, "  mov [rsi+%d], rax\n", i * 8);
      } else if (node.type() == type_t::value) {
        fprintf(f, "  ; a[%d] = %a;\n", i, node.value());  // "%a" is hexadecial full precision.
        fprintf(f, "  mov rax, %s\n", std::to_string(*reinterpret_cast<uint64_t*>(&node.value())).c_str());
        fprintf(f, "  mov [rsi+%d], rax\n", i * 8);
      } else if (node.type() == type_t::operation) {
        stack.push(~i);
        stack.push(node.lhs_index());
        stack.push(node.rhs_index());
      } else if (node.type() == type_t::function) {
        stack.push(~i);
        stack.push(node.argument_index());
      } else {
        BOOST_ASSERT(false);
      }
    } else {
      node_impl& node = node_vector_singleton()[dependent_i];
      if (node.type() == type_t::operation) {
        fprintf(
          f,
          "  ; a[%d] = a[%d] %s a[%d];\n",
          dependent_i,
          node.lhs_index(),
          operation_as_string(node.operation()),
          node.rhs_index());
        fprintf(f, "  movq xmm0, [rsi+%d]\n", node.lhs_index() * 8);
        fprintf(f, "  movq xmm1, [rsi+%d]\n", node.rhs_index() * 8);
        fprintf(f, "  %s xmm0, xmm1\n", operation_as_nasm_instruction(node.operation()));
        fprintf(f, "  movq [rsi+%d], xmm0\n", dependent_i * 8);
      } else if (node.type() == type_t::function) {
        fprintf(
          f,
          "  ; a[%d] = %s(a[%d]);\n",
          dependent_i,
          function_as_string(node.function()),
          node.argument_index());
        fprintf(f, "  movq xmm0, [rsi+%d]\n", node.argument_index() * 8);
        fprintf(f, "  push rdi\n");
        fprintf(f, "  push rsi\n");
        fprintf(f, "  call %s wrt ..plt\n", function_as_string(node.function()));
        fprintf(f, "  pop rsi\n");
        fprintf(f, "  pop rdi\n");
        fprintf(f, "  movq [rsi+%d], xmm0\n", dependent_i * 8);
      } else {
        BOOST_ASSERT(false);
      }
    }
  }
  fprintf(f, "  ; return a[%d]\n", index);
  fprintf(f, "  movq xmm0, [rsi+%d]\n", index * 8);
  fprintf(f, "  mov rsp, rbp\n");
  fprintf(f, "  pop rbp\n");
  fprintf(f, "  ret\n");
  fprintf(f, "\n");
  fprintf(f, "dim:\n");
  fprintf(f, "  push rbp\n");
  fprintf(f, "  mov rbp, rsp\n");
  fprintf(f, "  mov rax, %d\n", static_cast<int>(max_dim + 1));
  fprintf(f, "  mov rsp, rbp\n");
  fprintf(f, "  pop rbp\n");
  fprintf(f, "  ret\n");
}

struct compile_impl {
  struct NASM {
    static void compile(const std::string& filebase, uint32_t index) {
      FILE* f = fopen((filebase + ".asm").c_str(), "w");
      BOOST_ASSERT(f);
      generate_asm_code_for_node(index, f);
      fclose(f);

      const char* compile_cmdline = "nasm -f elf64 %1%.asm -o %1%.o";
      const char* link_cmdline = "ld -lm -shared -o %1%.so %1%.o";

      compiled_expression::syscall((boost::format(compile_cmdline) % filebase).str());
      compiled_expression::syscall((boost::format(link_cmdline) % filebase).str());
    }
  };
  struct CLANG {
    static void compile(const std::string& filebase, uint32_t index) {
      FILE* f = fopen((filebase + ".c").c_str(), "w");
      BOOST_ASSERT(f);
      generate_c_code_for_node(index, f);
      fclose(f);

      const char* compile_cmdline = "clang -fPIC -shared -nostartfiles %1%.c -o %1%.so";
      std::string cmdline = (boost::format(compile_cmdline) % filebase).str();
      compiled_expression::syscall(cmdline);
    }
  };
  struct _TMP { struct FNCAS_JIT {}; };  // Confirm FNCAS_JIT is a valid identifier.
  typedef FNCAS_JIT selected;
};

std::unique_ptr<compiled_expression> compile(uint32_t index) {
  std::random_device random;
  std::uniform_int_distribution<int> distribution(1000000, 9999999);
  std::ostringstream os;
  os << "/tmp/" << distribution(random);
  const std::string filebase = os.str();
  const std::string filename_so = filebase + ".so";
  unlink(filename_so.c_str());
  compile_impl::selected::compile(filebase, index);
  return std::unique_ptr<compiled_expression>(new compiled_expression(filename_so));
}

std::unique_ptr<compiled_expression> compile(const node& node) {
  return compile(node.index_);
}

}  // namespace fncas

#endif  // #ifdef FNCAS_JIT

#endif  // #ifndef FNCAS_JIT_H
