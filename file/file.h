/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Alexander Zolotarev <me@alex.bio> from Minsk, Belarus
          (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_FILE_FILE_H
#define BRICKS_FILE_FILE_H

#include <cstdio>
#include <fstream>
#include <string>
#include <cstring>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "exceptions.h"

#include "../util/make_scope_guard.h"

namespace bricks {

// Platform-indepenent, injection-friendly filesystem wrapper.
// TODO(dkorolev): Move the above methods under FileSystem.
struct FileSystem {
  static inline std::string ReadFileAsString(std::string const& file_name) {
    try {
      std::ifstream fi;
      fi.exceptions(std::ifstream::failbit | std::ifstream::badbit);
      fi.open(file_name, std::ifstream::binary);
      fi.seekg(0, std::ios::end);
      const size_t size = fi.tellg();
      std::string buffer(size, '\0');
      fi.seekg(0);
      if (fi.read(&buffer[0], size).good()) {
        return buffer;
      } else {
        throw FileException();  // LCOV_EXCL_LINE: This line not unit tested.
      }
    } catch (const std::ifstream::failure&) {
      throw FileException();
    }
  }

  // `file_name` is `const char*` to require users do `.c_str()` on it.
  // This reduces the risk of accidentally passing `file_name` and `contents` in the wrong order,
  // since `contents` should naturally be a C++ string supporting '\0'-s, while `file_name` does not have to.
  static inline void WriteStringToFile(const char* file_name,
                                       const std::string& contents,
                                       bool append = false) {
    try {
      std::ofstream fo;
      fo.exceptions(std::ofstream::failbit | std::ofstream::badbit);
      fo.open(file_name, (append ? std::ofstream::app : std::ofstream::trunc) | std::ofstream::binary);
      fo << contents;
    } catch (const std::ofstream::failure&) {
      throw FileException();
    }
  }

  static inline std::string GenTmpFileName() {
    char buffer[L_tmpnam];
    return std::string(::tmpnam(buffer));
  }

  static inline std::string WriteStringToTmpFile(const std::string& contents) {
    const std::string file_name = std::move(GenTmpFileName());
    WriteStringToFile(file_name.c_str(), contents);
    return file_name;
  }

  static inline std::string JoinPath(const std::string& path_name, const std::string& base_name) {
    if (base_name.empty()) {
      throw FileException();
    } else if (path_name.empty() || base_name.front() == '/') {
      return base_name;
    } else if (path_name.back() == '/') {
      return path_name + base_name;
    } else {
      return path_name + '/' + base_name;
    }
  }

  static inline uint64_t GetFileSize(const std::string& file_name) {
    struct stat info;
    if (stat(file_name.c_str(), &info)) {
      throw FileException();
    } else {
      if (S_ISDIR(info.st_mode)) {
        throw FileException();
      } else {
        return static_cast<uint64_t>(info.st_size);
      }
    }
  }

  enum class CreateDirectoryParameters { ThrowExceptionOnError, Silent };
  static inline void CreateDirectory(
      const std::string& directory,
      CreateDirectoryParameters parameters = CreateDirectoryParameters::ThrowExceptionOnError) {
    // Hard-code default permissions to avoid cross-platform compatibility issues.
    if (::mkdir(directory.c_str(), 0755)) {
      if (parameters == CreateDirectoryParameters::ThrowExceptionOnError) {
        // TODO(dkorolev): Analyze errno.
        throw FileException();
      }
    }
  }

  static inline void RenameFile(const std::string& old_name, const std::string& new_name) {
    if (::rename(old_name.c_str(), new_name.c_str())) {
      // TODO(dkorolev): Analyze errno.
      throw FileException();
    }
  }

  // TODO(dkorolev): Make OutputFile not as tightly coupled with std::ofstream as it is now.
  typedef std::ofstream OutputFile;

  template <typename F>
  static inline void ScanDirUntil(const std::string& directory, F&& f) {
    DIR* dir = ::opendir(directory.c_str());
    const auto closedir_guard = MakeScopeGuard([dir]() { ::closedir(dir); });
    if (dir) {
      while (struct dirent* entry = ::readdir(dir)) {
        if (*entry->d_name && ::strcmp(entry->d_name, ".") && ::strcmp(entry->d_name, "..")) {
          if (!f(entry->d_name)) {
            return;
          }
        }
      }
    } else {
      // TODO(dkorolev): Analyze errno.
      throw FileException();
    }
  }

  template <typename F>
  static inline void ScanDir(const std::string& directory, F&& f) {
    ScanDirUntil(directory, [&f](const std::string& filename) {
      f(filename);
      return true;
    });
  }

  enum class RemoveFileParameters { ThrowExceptionOnError, Silent };
  static inline void RemoveFile(const std::string& file_name,
                                RemoveFileParameters parameters = RemoveFileParameters::ThrowExceptionOnError) {
    if (::remove(file_name.c_str())) {
      if (parameters == RemoveFileParameters::ThrowExceptionOnError) {
        throw FileException();
      }
    }
  }

  class ScopedRemoveFile final {
   public:
    explicit ScopedRemoveFile(const std::string& file_name, bool remove_now_as_well = true)
        : file_name_(file_name) {
      if (remove_now_as_well) {
        RemoveFile(file_name_, RemoveFileParameters::Silent);
      }
    }
    ~ScopedRemoveFile() { RemoveFile(file_name_, RemoveFileParameters::Silent); }

   private:
    std::string file_name_;
  };

  enum class RemoveDirectoryParameters { ThrowExceptionOnError, Silent };
  static inline void RemoveDirectory(
      const std::string& directory,
      RemoveDirectoryParameters parameters = RemoveDirectoryParameters::ThrowExceptionOnError) {
    if (::rmdir(directory.c_str())) {
      if (parameters == RemoveDirectoryParameters::ThrowExceptionOnError) {
        // TODO(dkorolev): Analyze errno.
        throw FileException();
      }
    }
  }
};

}  // namespace bricks

#endif  // BRICKS_FILE_FILE_H
