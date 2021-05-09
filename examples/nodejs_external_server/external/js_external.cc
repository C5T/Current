#include <iostream>

#include "js_external.h"

#include "../../../blocks/http/api.h"
#include "../../../bricks/sync/waitable_atomic.h"

class RealImpl final : public ImplBase {
 private:
  const int port_;
  HTTPRoutesScope routes_;
  current::WaitableAtomic<bool> done_{false};

 public:
  explicit RealImpl(int port) : port_(port) {}

  int Port() const { return port_; }

  void Register(const std::string& route, std::function<std::string(std::string)> f) {
    routes_ += HTTP(current::net::BarePort(port_)).Register(route, [f, this](Request r) {
      if (r.method == "DELETE") {
        std::cout << "[ inner c++ ] Received the 'DELETE' request on port " << port_ << "." << std::endl;
        r("[ inner c++ ] The DELETE request is received by the server on port " + std::to_string(port_) + ".\n");
        *done_.MutableScopedAccessor() = true;
      } else {
        std::cout << "[ inner c++ ] Received the '" << r.method << "' request on port " << port_ << "." << std::endl;
        r(f(r.body));
        std::cout << "[ inner c++ ] Served the '" << r.method << "' request on port " << port_ << "." << std::endl;
      }
    });
  }

  void WaitUntilDELETEd() const {
    done_.Wait([](bool done) { return done; });
  }
};

ExternalServer::ExternalServer(int port) : impl_(std::make_unique<RealImpl>(port)) {}

RealImpl& ExternalServer::Impl() { return *(dynamic_cast<RealImpl*>(impl_.get())); }
const RealImpl& ExternalServer::Impl() const { return *(dynamic_cast<const RealImpl*>(impl_.get())); }

void ExternalServer::Register(const std::string& route, std::function<std::string(std::string)> f) {
  Impl().Register(route, f);
  std::cout << "[ inner c++ ] Registered the '" << route << "' route on port " << Impl().Port() << "." << std::endl;
}

void ExternalServer::WaitUntilDELETEd() const { Impl().WaitUntilDELETEd(); }
