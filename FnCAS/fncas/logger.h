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

#ifndef FNCAS_LOGGER_H
#define FNCAS_LOGGER_H

#include <string>
#include <ostream>

namespace fncas {

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

  void Log(const std::string& message) const { impl_->Log(message); }

  void DisableLogging() { impl_ = std::make_unique<Impl>(); }
  void LogToStderr() { impl_ = std::make_unique<OStreamLogger>(std::cerr); }

 private:
  std::unique_ptr<Impl> impl_{std::make_unique<Impl>()};
};

inline OptimizerLoggerImpl& OptimizerLogger() { return current::ThreadLocalSingleton<OptimizerLoggerImpl>(); }

struct ScopedLogToStderr final {
  ScopedLogToStderr() { fncas::OptimizerLogger().LogToStderr(); }
  ~ScopedLogToStderr() { fncas::OptimizerLogger().DisableLogging(); }
};

}  // namespace fncas

#endif  // #ifndef FNCAS_LOGGER_H
