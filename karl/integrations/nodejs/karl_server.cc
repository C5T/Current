/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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

#define EXTRA_KARL_LOGGING
#include "current_build.h"

#include "../../karl.h"
#include "../../../bricks/dflags/dflags.h"

DEFINE_uint16(karl_keepalives_port, 30001, "Local test port for the `Karl` keepalive listener.");
DEFINE_uint16(karl_fleet_view_port, 30999, "Local test port for the `Karl` fleet view listener.");
DEFINE_string(karl_stream_persistence_file, ".current/stream", "Local file to store Karl's keepalives.");
DEFINE_string(karl_storage_persistence_file, ".current/storage", "Local file to store Karl's status.");

CURRENT_STRUCT(custom_status) {
  CURRENT_FIELD(custom_field, std::string);
  void Render(std::ostream & os, std::chrono::microseconds) const {
    os << "<TR><TD COLSPAN='2'>"
       << "Custom JS Claire field value: " << custom_field << "</TD></TR>";
  }
};

using karl_t =
    current::karl::GenericKarl<current::karl::UseOwnStorage, current::karl::default_user_status::status, custom_status>;

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  current::WaitableAtomic<bool> shutdown(false);
  const auto http_routes_scope = HTTP(FLAGS_karl_keepalives_port).Register("/shutdown", [&shutdown](Request r) {
    shutdown.SetValue(true);
    r("OK\n");
  }) + HTTP(FLAGS_karl_keepalives_port).Register("/up", [](Request r) { r("OK\n"); });

  const auto stream_file_remover = current::FileSystem::ScopedRmFile(FLAGS_karl_stream_persistence_file);
  const auto storage_file_remover = current::FileSystem::ScopedRmFile(FLAGS_karl_storage_persistence_file);

  current::karl::KarlParameters params;
  params.keepalives_port = FLAGS_karl_keepalives_port;
  params.fleet_view_port = FLAGS_karl_fleet_view_port;
  params.stream_persistence_file = FLAGS_karl_stream_persistence_file;
  params.storage_persistence_file = FLAGS_karl_storage_persistence_file;

  karl_t karl(params);

  shutdown.Wait([](bool done) { return done; });
  return 0;
}
