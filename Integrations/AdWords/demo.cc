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

#include "conversion_tracking.h"

#include "../../Bricks/dflags/dflags.h"

#ifdef ADWORDS_PARAM
#error "`ADWORDS_PARAM` should not be defined."
#endif

DEFINE_uint16(port,
              current::integrations::adwords::conversion_tracking::kDefaultAdWordsIntegrationPort,
              "The port from which the local nginx proxies request to AdWords.");

#define ADWORDS_PARAM(x) DEFINE_string(x, "", "The `#x` param for AdWords conversion tracking.");
ADWORDS_PARAM(conversion_id);
ADWORDS_PARAM(label);
ADWORDS_PARAM(bundleid);
ADWORDS_PARAM(idtype);
#undef ADWORDS_PARAM

DEFINE_string(rdid, "", "The `rdid` (\"advertising identifier\") to send the tracking info for.");

DEFINE_bool(dump, false, "Set to dump configuration parameters and exit.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  current::integrations::adwords::conversion_tracking::AdWordsConversionTrackingParams params;

#define ADWORDS_PARAM(x)                                                  \
  if (FLAGS_##x.empty()) {                                                \
    std::cerr << "The `--" << #x << "` flag should be set." << std::endl; \
    return 1;                                                             \
  } else {                                                                \
    params.x = FLAGS_##x;                                                 \
  }
  ADWORDS_PARAM(conversion_id);
  ADWORDS_PARAM(label);
  ADWORDS_PARAM(bundleid);
  ADWORDS_PARAM(idtype);
#undef ADWORDS_PARAM

  if (FLAGS_dump) {
    std::cout << "Local port :   " << FLAGS_port << std::endl;
    std::cout << "Configuration: " << JSON(params) << std::endl;
    return 0;
  }

  try {
    const current::integrations::adwords::conversion_tracking::AdWordsMobileConversionEventsSender sender(
        params, FLAGS_port);
    if (!FLAGS_rdid.empty()) {
      if (sender.SendConversionEvent(FLAGS_rdid)) {
        return 0;
      } else {
        std::cerr << "An error occurred.\n";
        return 1;
      }
    } else {
      std::cerr << "Set `--rdid` to send the conversion event." << std::endl;
      return 1;
    }
  } catch (const current::Exception& e) {
    std::cerr << "Failed: " << e.What() << std::endl;
    return -1;
  }
}
