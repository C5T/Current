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

#include <errno.h>

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
struct FileSystem {
#ifndef BRICKS_WINDOWS
  static constexpr char PathSeparatingSlash = '/';
#else
  static constexpr char PathSeparatingSlash = '\\';
#endif
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
      BRICKS_THROW(CannotReadFileException(file_name));
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
#if defined(BRICKS_WINDOWS)
    assert(!(::tmpnam_s(buffer)));
#elif defined(BRICKS_APPLE)
    // TODO(dkorolev): Fix temporary file names generation.
    return strings::Printf("/tmp/.current-tmp-%08x", rand());
#else
    assert(buffer == ::tmpnam(buffer));
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
    } else if (path_name.empty() || base_name.front() == PathSeparatingSlash) {
      return base_name;
    } else if (path_name.back() == PathSeparatingSlash) {
      return path_name + base_name;
    } else {
      return path_name + PathSeparatingSlash + base_name;
    }
  }

  static inline bool IsDir(const std::string& file_or_directory_name) {
    struct stat info;
    if (::stat(file_or_directory_name.c_str(), &info)) {
      BRICKS_THROW(FileException());
    } else {
#ifndef BRICKS_WINDOWS
      return !!(S_ISDIR(info.st_mode));
#else
      return !!(info.st_mode & _S_IFDIR);
#endif
    }
  }

  static inline uint64_t GetFileSize(const std::string& file_name) {
    struct stat info;
    if (::stat(file_name.c_str(), &info)) {
      BRICKS_THROW(FileException());
    } else {
      if (
// clang-format off
#ifndef BRICKS_WINDOWS
          S_ISDIR(info.st_mode)
#else
          info.st_mode & _S_IFDIR
#endif
    ) {
        // clang-format on
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
// clang-format off
#ifndef BRICKS_WINDOWS
        ::mkdir(directory.c_str(), 0755)
#else
        ::_mkdir(directory.c_str())
#endif
    ) {
      // clang-format on
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

  enum class ScanDirParameters { ListFilesOnly, ListFilesAndDirs };
  template <typename F>
  static inline void ScanDirUntil(const std::string& directory,
                                  F&& f,
                                  ScanDirParameters parameters = ScanDirParameters::ListFilesOnly) {
#ifdef BRICKS_WINDOWS
    WIN32_FIND_DATAA find_data;
    HANDLE handle = ::FindFirstFileA((directory + "\\*.*").c_str(), &find_data);
    if (handle == INVALID_HANDLE_VALUE) {
      BRICKS_THROW(DirDoesNotExistException());
    } else {
      struct ScopedCloseFindFileHandle {
        HANDLE handle_;
        ScopedCloseFindFileHandle(HANDLE handle) : handle_(handle) {}
        ~ScopedCloseFindFileHandle() { ::FindClose(handle_); }
      };
      const ScopedCloseFindFileHandle closer(handle);
      do {
        if (parameters == ScanDirParameters::ListFilesAndDirs ||
            !(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
          if (!f(find_data.cFileName)) {
            return;
          }
        }
      } while (::FindNextFileA(handle, &find_data) != 0);
    }
#else
    DIR* dir = ::opendir(directory.c_str());
    const auto closedir_guard = MakeScopeGuard([dir]() {
      if (dir) {
        ::closedir(dir);
      }
    });
    if (dir) {
      while (struct dirent* entry = ::readdir(dir)) {
        if (*entry->d_name && ::strcmp(entry->d_name, ".") && ::strcmp(entry->d_name, "..")) {
          const char* const filename = entry->d_name;
          // Proved to be required on Ubuntu running in Parallels on a Mac,
          // with Bricks' directory mounted from Mac's filesystem.
          // `entry->d_type` is always zero there, see http://comments.gmane.org/gmane.comp.lib.libcg.devel/4236
          if (parameters == ScanDirParameters::ListFilesAndDirs || !IsDir(JoinPath(directory, filename))) {
            if (!f(filename)) {
              return;
            }
          }
        }
      }
    } else {
      if (errno == ENOENT) {
        BRICKS_THROW(DirDoesNotExistException());
      } else if (errno == ENOTDIR) {
        BRICKS_THROW(PathIsNotADirectoryException());
      } else {
        BRICKS_THROW(FileException());  // LCOV_EXCL_LINE
      }
    }
#endif
  }

  template <typename F>
  static inline void ScanDir(const std::string& directory,
                             F&& f,
                             ScanDirParameters parameters = ScanDirParameters::ListFilesOnly) {
    ScanDirUntil(directory,
                 [&f](const std::string& filename) {
                   f(filename);
                   return true;
                 },
                 parameters);
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
  enum class RmDirRecursive { No, Yes };
  static inline void RmDir(const std::string& directory,
                           RmDirParameters parameters = RmDirParameters::ThrowExceptionOnError,
                           RmDirRecursive recursive = RmDirRecursive::No) {
    if (recursive == RmDirRecursive::No) {
      if (
// clang-format off
#ifndef BRICKS_WINDOWS
          ::rmdir(directory.c_str())
#else
          ::_rmdir(directory.c_str())
#endif
      ) {
        // clang-format on
        if (parameters == RmDirParameters::ThrowExceptionOnError) {
          if (errno == ENOENT) {
            BRICKS_THROW(DirDoesNotExistException());
          } else if (errno == ENOTEMPTY) {
            BRICKS_THROW(DirIsNotEmptyException());
          } else {
            BRICKS_THROW(FileException());  // LCOV_EXCL_LINE
          }
        }
      }
    } else {
      try {
        ScanDir(directory,
                [&directory, parameters](const std::string& name) {
                  const std::string full_name = JoinPath(directory, name);
                  if (IsDir(full_name)) {
                    RmDir(full_name, parameters, RmDirRecursive::Yes);
                  } else {
                    RmFile(full_name,
                           (parameters == RmDirParameters::ThrowExceptionOnError)
                               ? RmFileParameters::ThrowExceptionOnError
                               : RmFileParameters::Silent);
                  }
                },
                ScanDirParameters::ListFilesAndDirs);
        RmDir(directory, parameters, RmDirRecursive::No);
      } catch (const bricks::Exception&) {
        if (parameters == RmDirParameters::ThrowExceptionOnError) {
          throw;
        }
      }
    }
  }

  class ScopedRmDir final {
   public:
    explicit ScopedRmDir(const std::string& directory,
                         bool remove_now_as_well = true,
                         RmDirRecursive recursive = RmDirRecursive::Yes)
        : directory_(directory), recursive_(recursive) {
      if (remove_now_as_well) {
        RmDir(directory_, RmDirParameters::Silent, recursive_);
      }
    }
    ~ScopedRmDir() { RmDir(directory_, RmDirParameters::Silent, recursive_); }

   private:
    std::string directory_;
    RmDirRecursive recursive_;
  };
};

}  // namespace bricks

#endif  // BRICKS_FILE_FILE_H
