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

#ifndef KARL_KARL_H
#define KARL_KARL_H

#include "../port.h"

#include "schema.h"

#include "../Storage/storage.h"
#include "../Storage/persister/sherlock.h"

#include "../Blocks/HTTP/api.h"

namespace current {
namespace karl {

// Karl's persisted storage.
CURRENT_STRUCT(Server) {
  CURRENT_FIELD(location, std::string);
  CURRENT_FIELD(service, std::string);
  CURRENT_FIELD(codename, std::string);

  const std::string& key() const { return location; }
  void set_key(const std::string& key) const { location = key; }
};

CURRENT_STRUCT(Service) {
  CURRENT_FIELD(service, std::string);
  CURRENT_FIELD(codename, std::string);
  CURRENT_FIELD(location, std::string);

  const std::string& key() const { return service; }
  void set_key(const std::string& key) const { service = key; }
};

CURRENT_STORAGE_FIELD_ENTRY(UnorderedDictionary, Server, ServerDictionary);
CURRENT_STORAGE_FIELD_ENTRY(OrderedDictionary, Service, ServiceDictionary);

CURRENT_STORAGE(ServiceStorage) {
  CURRENT_STORAGE_FIELD(servers, ServerDictionary);
  CURRENT_STORAGE_FIELD(services, ServiceDictionary);
};

// Karl's HTTP responses.
CURRENT_STRUCT(KarlStatus) {
  CURRENT_FIELD(services, std::vector<Service>);
  CURRENT_FIELD(servers, std::vector<Server>);
};

class Karl final {
 public:
  using logger_t = std::function<void(const Request&)>;
  using storage_t = ServiceStorage<SherlockInMemoryStreamPersister>;

  explicit Karl(int port, const std::string& url = "/karl", logger_t logger = nullptr)
      : logger_(logger),
        http_scope_(HTTP(port).Register(
            url,
            [this](Request r) {
              if (logger_) {
                logger_(r);
              }
              // Convention: state mutations are POSTs, user-level getters are GETs.
              if (r.method == "POST" && r.url.query.has("codename") && r.url.query.has("port") &&
                  r.url.query.has("url")) {
                // The key for the newly registering service.
                const std::string location =
                    r.connection.RemoteIPAndPort().ip + ':' + r.url.query["port"] + r.url.query["url"];

                // TODO(dkorolev): Error checking.
                const auto loopback = ParseJSON<ClaireToKarlBase>(HTTP(POST(location, "")).body);

                const std::string service = loopback.service;
                const std::string codename = r.url.query["codename"];

                storage_.ReadWriteTransaction(
                             [location, service, codename](MutableFields<storage_t> fields) -> Response {
                               Service service_record;
                               service_record.location = location;
                               service_record.service = service;
                               service_record.codename = codename;
                               fields.services.Add(service_record);

                               Server server_record;
                               server_record.location = location;
                               server_record.service = service;
                               server_record.codename = codename;
                               fields.servers.Add(server_record);

                               return Response("OK\n");
                             },
                             std::move(r)).Detach();
              } else {
                storage_.ReadOnlyTransaction([](ImmutableFields<storage_t> fields) -> Response {
                  KarlStatus status;
                  for (const auto& service : fields.services) {
                    status.services.push_back(service);
                  }
                  for (const auto& server : fields.servers) {
                    status.servers.push_back(server);
                  }
                  return status;
                }, std::move(r)).Detach();
              }
            })) {}

 private:
  const logger_t logger_;
  storage_t storage_;
  const HTTPRoutesScope http_scope_;
};

}  // namespace current::karl
}  // namespace current

#endif  // KARL_KARL_H
