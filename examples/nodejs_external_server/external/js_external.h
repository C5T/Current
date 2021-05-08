#pragma once

#include <functional>
#include <memory>

class ImplBase {
 public:
  virtual ~ImplBase() = default;
};

class RealImpl;  // `: public ImplBase`, for the pimpl idiom to work via a `std::unique_ptr<>`.

class ExternalServer final {
 private:
  std::unique_ptr<ImplBase> impl_;

 protected:
  RealImpl& Impl();
  const RealImpl& Impl() const;

 public:
  explicit ExternalServer(int port);
  void Register(const std::string& route, std::function<std::string(std::string)> f);
  void WaitUntilDELETEd() const;
};
