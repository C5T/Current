/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef FNCAS_FNCAS_LOGGER_H
#define FNCAS_FNCAS_LOGGER_H

#include "../../port.h"

#include <iostream>
#include <sstream>

#include "base.h"

#include "../../bricks/time/chrono.h"
#include "../../bricks/util/singleton.h"

namespace fncas {
namespace impl {

class OptimizerLoggerImpl final {
 public:
  struct Impl {
    virtual ~Impl() = default;
    virtual void Log(const std::string&) const {}
  };

  struct OStreamLogger : Impl {
    std::ostream& os_;
    explicit OStreamLogger(std::ostream& os) : os_(os) {}
    void Log(const std::string& message) const override { os_ << message << std::endl; }
  };

  operator bool() const { return impl_ != nullptr; }

  void Log(const std::string& message) const { impl_->Log(message); }

  void DisableLogging() { impl_ = std::make_unique<Impl>(); }
  void LogToStderr() { impl_ = std::make_unique<OStreamLogger>(std::cerr); }

 private:
  std::unique_ptr<Impl> impl_{std::make_unique<Impl>()};
};

inline OptimizerLoggerImpl& OptimizerLogger() { return current::ThreadLocalSingleton<OptimizerLoggerImpl>(); }

struct ScopedLogToStderr final {
  ScopedLogToStderr() { fncas::impl::OptimizerLogger().LogToStderr(); }
  ~ScopedLogToStderr() { fncas::impl::OptimizerLogger().DisableLogging(); }
};

}  // namespace impl

namespace optimize {

class OptimizerStats final {
 public:
  explicit OptimizerStats(const std::string& name) : name_(name), begin_timestamp_(current::time::Now()) {}
  OptimizerStats() = delete;

  ~OptimizerStats() {
    const auto k = static_cast<double_t>(1e6) / (current::time::Now() - begin_timestamp_).count();
    std::ostringstream os;
    os << n_iterations_ << " iterations, " << n_iterations_ * k << " per second, " << n_f_ << " f() calls, " << n_g_
       << " g() calls";
    if (n_backtracking_calls_) {
      os << ", " << n_backtracking_calls_ << " backtracking calls of " << n_backtracking_steps_ << " total steps";
    }
    impl::OptimizerLogger().Log(name_ + ": " + os.str() + '.');
  }

  void JournalFunction() { ++n_f_; }
  void JournalGradient() { ++n_g_; }
  void JournalIteration() { ++n_iterations_; }
  void JournalBacktrackingCall() { ++n_backtracking_calls_; }
  void JournalBacktrackingStep() { ++n_backtracking_steps_; }

 private:
  const std::string name_;
  const std::chrono::microseconds begin_timestamp_;

  size_t n_f_ = 0;
  size_t n_g_ = 0;
  size_t n_iterations_ = 0;
  size_t n_backtracking_calls_ = 0;
  size_t n_backtracking_steps_ = 0;
};

}  // namespace optimize
}  // namespace fncas

#endif  // #ifndef FNCAS_FNCAS_LOGGER_H
