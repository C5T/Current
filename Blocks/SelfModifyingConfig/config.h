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

#ifndef BLOCKS_SELFMODIFYINGCONFIG_CONFIG_H
#define BLOCKS_SELFMODIFYINGCONFIG_CONFIG_H

// `SelfModifyingConfig<T>` reads the config of type `T`, and can update it dynamically, preserving the history.
//
// `SelfModifyingConfig<T>`, takes one required constructor parameter: path to the file from where the config
// should be read. The underlying config can be accessed via `.Config()`, or a cast to `const T&`.
//
// The user can reload the config via `Reload()`, or update the config via `Update(const T& new_config)`.
//
// All updates happen by atomically renaming the old config into a temporary name ('*.yyyymmdd-HHMMSS').
// During startup, the binary also makes an atomic copy of the config file, making sure it is possible to make.
//
// On any error, an exception is thrown.

#include "../port.h"

#include "exceptions.h"

#include "../../TypeSystem/struct.h"
#include "../../TypeSystem/Serialization/json.h"

#include "../../bricks/file/file.h"
#include "../../bricks/time/chrono.h"

namespace current {

struct SelfModifyingConfigHelper {
  static std::string HistoricalFilename(const std::string& filename,
                                        std::chrono::microseconds now = current::time::Now()) {
    return filename + current::FormatDateTime<current::time::TimeRepresentation::UTC>(now, ".%Y%m%d-%H%M%S");
  }
};

template <typename CONFIG>
class SelfModifyingConfig final : public SelfModifyingConfigHelper {
 private:
  static_assert(IS_CURRENT_STRUCT(CONFIG), "");

 public:
  using config_t = CONFIG;

  // The path to the config file can be absolute or relative.
  explicit SelfModifyingConfig(const std::string& filename) : filename_(filename) {
    Reload();
    Update(config_);  // Confirm the historical config file is writable.
  }

  // Not thread-safe.
  void Reload() {
    // First, read the file.
    const std::string contents = [this]() -> std::string {
      try {
        return current::FileSystem::ReadFileAsString(filename_);
      } catch (const current::FileException&) {
        CURRENT_THROW(SelfModifyingConfigReadFileException(filename_));
      }
    }();
    // Then, parse the JSON from it.
    try {
      config_ = ParseJSON<config_t>(contents);
    } catch (const current::serialization::json::InvalidJSONException&) {
      const std::string what = "File doesn't contain a valid JSON: '" + contents + "'";
      CURRENT_THROW(SelfModifyingConfigParseJSONException(what));
    } catch (const current::serialization::json::TypeSystemParseJSONException& json_exception) {
      const std::string what = "Unable to parse JSON: " + json_exception.DetailedDescription();
      CURRENT_THROW(SelfModifyingConfigParseJSONException(what));
    }
  }

  // Not thread-safe.
  void Update(const config_t& new_config, std::chrono::microseconds now = current::time::Now()) {
    // First, save the current config under a new name.
    // This could be atomic rename, but meh. -- D.K.
    const std::string new_filename = HistoricalFilename(filename_, now);
    try {
      current::FileSystem::WriteStringToFile(JSON(config_), new_filename.c_str());
    } catch (const current::FileException&) {
      CURRENT_THROW(SelfModifyingConfigWriteFileException(new_filename));
    }
    // Then, update the config.
    config_ = new_config;
    // Finally, save the current config under the original config file name.
    try {
      current::FileSystem::WriteStringToFile(JSON(config_), filename_.c_str());
    } catch (const current::FileException&) {
      CURRENT_THROW(SelfModifyingConfigWriteFileException(filename_));
    }
  }

  // Not thread-safe.
  const config_t& Config() const { return config_; }
  operator const config_t&() const { return Config(); }

 private:
  const std::string filename_;
  config_t config_;
};

}  // namespace current

#endif  // BLOCKS_SELFMODIFYINGCONFIG_CONFIG_H
