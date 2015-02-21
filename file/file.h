/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Alexander Zolotarev <me@alex.bio> from Minsk, Belarus
          (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2015 John Babak <babak.john@gmail.com>

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

#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include <cstring>

#include "../port.h"

#ifndef BRICKS_WINDOWS
#include <dirent.h>
#include <unistd.h>
#else
#include <direct.h>
#endif

#include <sys/stat.h>

#include "exceptions.h"

#include "../util/make_scope_guard.h"

namespace bricks {

// Platform-indepenent, injection-friendly filesystem wrapper.
// TODO(dkorolev): Move the above methods under FileSystem.
struct FileSystem {
  static inline std::string GetFileExtension(const std::string& file_name) {
    const size_t i = file_name.find_last_of("/\\.");
    if (i == std::string::npos || file_name[i] != '.') {
      return "";
    } else {
      return file_name.substr(i + 1);
    }
  }

  static inline std::string ReadFileAsString(const std::string& file_name) {
    try {
      std::ifstream fi;
      fi.exceptions(std::ifstream::failbit | std::ifstream::badbit);
      fi.open(file_name, std::ifstream::binary);
      fi.seekg(0, std::ios::end);
      const size_t size = static_cast<size_t>(fi.tellg());
      std::string buffer(size, '\0');
      fi.seekg(0);
      if (fi.read(&buffer[0], size).good()) {
        return buffer;
      } else {
        BRICKS_THROW(FileException());  // LCOV_EXCL_LINE: This line not unit tested.
      }
    } catch (const std::ifstream::failure&) {
      BRICKS_THROW(FileException());
    }
  }

  // `file_name` is `const char*` to require users do `.c_str()` on it.
  // This reduces the risk of accidentally passing `file_name` and `contents` in the wrong order,
  // since `contents` should naturally be a C++ string supporting '\0'-s, while `file_name` does not have to.
  static inline void WriteStringToFile(const std::string& contents,
                                       const char* file_name,
                                       bool append = false) {
    try {
      std::ofstream fo;
      fo.exceptions(std::ofstream::failbit | std::ofstream::badbit);
      fo.open(file_name, (append ? std::ofstream::app : std::ofstream::trunc) | std::ofstream::binary);
      fo << contents;
    } catch (const std::ofstream::failure&) {
      BRICKS_THROW(FileException());
    }
  }

  static inline std::string GenTmpFileName() {
    char buffer[L_tmpnam];
#ifndef BRICKS_WINDOWS
    assert(buffer == ::tmpnam(buffer));
#else
    assert(!(::tmpnam_s(buffer)));
#endif
    return buffer;
  }

  static inline std::string WriteStringToTmpFile(const std::string& contents) {
    const std::string file_name = std::move(GenTmpFileName());
    WriteStringToFile(contents, file_name.c_str());
    return file_name;
  }

  static inline std::string JoinPath(const std::string& path_name, const std::string& base_name) {
    if (base_name.empty()) {
      BRICKS_THROW(FileException());
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
    if (::stat(file_name.c_str(), &info)) {
      BRICKS_THROW(FileException());
    } else {
      if (
#ifndef BRICKS_WINDOWS
          S_ISDIR(info.st_mode)
#else
          info.st_mode & _S_IFDIR
#endif
          ) {
        BRICKS_THROW(FileException());
      } else {
        return static_cast<uint64_t>(info.st_size);
      }
    }
  }

  // Please keep the short `MkDir` name instead of the long `CreateDirectory`,
  // since the latter is `#define`-d into `CreateDirectoryA` on Windows.
  enum class MkDirParameters { ThrowExceptionOnError, Silent };
  static inline void MkDir(const std::string& directory,
                           MkDirParameters parameters = MkDirParameters::ThrowExceptionOnError) {
    // Hard-code default permissions to avoid cross-platform compatibility issues.
    if (
#ifndef BRICKS_WINDOWS
        ::mkdir(directory.c_str(), 0755)
#else
        ::_mkdir(directory.c_str())
#endif
        ) {
      if (parameters == MkDirParameters::ThrowExceptionOnError) {
        // TODO(dkorolev): Analyze errno.
        BRICKS_THROW(FileException());
      }
    }
  }

  static inline void RenameFile(const std::string& old_name, const std::string& new_name) {
    if (::rename(old_name.c_str(), new_name.c_str())) {
      // TODO(dkorolev): Analyze errno.
      BRICKS_THROW(FileException());
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
      BRICKS_THROW(FileException());
    }
  }

  template <typename F>
  static inline void ScanDir(const std::string& directory, F&& f) {
    ScanDirUntil(directory, [&f](const std::string& filename) {
      f(filename);
      return true;
    });
  }

  enum class RmFileParameters { ThrowExceptionOnError, Silent };
  static inline void RmFile(const std::string& file_name,
                            RmFileParameters parameters = RmFileParameters::ThrowExceptionOnError) {
    if (::remove(file_name.c_str())) {
      if (parameters == RmFileParameters::ThrowExceptionOnError) {
        BRICKS_THROW(FileException());
      }
    }
  }

  class ScopedRmFile final {
   public:
    explicit ScopedRmFile(const std::string& file_name, bool remove_now_as_well = true)
        : file_name_(file_name) {
      if (remove_now_as_well) {
        RmFile(file_name_, RmFileParameters::Silent);
      }
    }
    ~ScopedRmFile() { RmFile(file_name_, RmFileParameters::Silent); }

   private:
    std::string file_name_;
  };

  enum class RmDirParameters { ThrowExceptionOnError, Silent };
  static inline void RmDir(const std::string& directory,
                           RmDirParameters parameters = RmDirParameters::ThrowExceptionOnError) {
    if (
#ifndef BRICKS_WINDOWS
        ::rmdir(directory.c_str())
#else
        ::_rmdir(directory.c_str())
#endif
        ) {
      if (parameters == RmDirParameters::ThrowExceptionOnError) {
        // TODO(dkorolev): Analyze errno.
        BRICKS_THROW(FileException());
      }
    }
  }
};

}  // namespace bricks

#endif  // BRICKS_FILE_FILE_H
