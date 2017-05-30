/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Alexander Zolotarev <me@alex.bio> from Minsk, Belarus
          (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2015 Ivan Babak <babak.john@gmail.com> https://github.com/sompylasar

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

#include "../port.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <cstring>

#include <errno.h>

#ifndef CURRENT_WINDOWS
#include <dirent.h>
#include <unistd.h>
#else
#include <direct.h>
#endif

#include <sys/stat.h>

#include "exceptions.h"

#include "../util/make_scope_guard.h"

namespace current {

// Platform-indepenent, injection-friendly filesystem wrapper.
struct FileSystem {
#ifndef CURRENT_WINDOWS
  static constexpr char PathSeparatingSlash = '/';
  static inline std::string NullDeviceName() { return "/dev/null"; }
#else
  static constexpr char PathSeparatingSlash = '\\';
  static inline std::string NullDeviceName() { return "NUL"; }
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
      if (!fi.read(&buffer[0], size).bad()) {
        return buffer;
      } else {
        CURRENT_THROW(FileException(file_name));  // LCOV_EXCL_LINE: This line not unit tested.
      }
    } catch (const std::ifstream::failure&) {
      CURRENT_THROW(CannotReadFileException(file_name));
    }
  }

  template <typename F>
  static inline void ReadFileByLines(const std::string& file_name, F&& f) {
    try {
      std::ifstream fi(file_name);
      if (fi.bad()) {
        CURRENT_THROW(CannotReadFileException(file_name));
      }
      std::string line;
      while (std::getline(fi, line)) {
        f(std::move(line));
      }
    } catch (const std::ifstream::failure&) {
      CURRENT_THROW(CannotReadFileException(file_name));
    }
  }

  // `file_name` is `const char*` to require users do `.c_str()` on it.
  // This reduces the risk of accidentally passing `file_name` and `contents` in the wrong order,
  // since `contents` should naturally be a C++ string supporting '\0'-s, while `file_name` does not have to.
  static inline void WriteStringToFile(const std::string& contents, const char* file_name, bool append = false) {
    try {
      std::ofstream fo;
      fo.exceptions(std::ofstream::failbit | std::ofstream::badbit);
      fo.open(file_name, (append ? std::ofstream::app : std::ofstream::trunc) | std::ofstream::binary);
      fo << contents;
    } catch (const std::ofstream::failure&) {
      CURRENT_THROW(FileException(file_name));
    }
  }

  static inline std::string GenTmpFileName() {
#ifndef CURRENT_WINDOWS
    // TODO(dkorolev): Fix temporary file names generation.
    return strings::Printf("/tmp/.current-tmp-%08x", rand());
#else
    char buffer[L_tmpnam_s];  // NOTE(dkorolev): Changed `[L_tmpnam]` into `[L_tmpnam_s]`, as per
                              // https://msdn.microsoft.com/en-us/library/18x8h1bh.aspx
    CURRENT_ASSERT(!(::tmpnam_s(buffer)));
    return buffer;
#endif
  }

  static inline std::string WriteStringToTmpFile(const std::string& contents) {
    const std::string file_name = GenTmpFileName();
    WriteStringToFile(contents, file_name.c_str());
    return file_name;
  }

  static inline std::string JoinPath(const std::string& path_name, const std::string& base_name) {
    if (base_name.empty()) {
      CURRENT_THROW(FileException(base_name));
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
      CURRENT_THROW(FileException(file_or_directory_name));
    } else {
#ifndef CURRENT_WINDOWS
      return !!(S_ISDIR(info.st_mode));
#else
      return !!(info.st_mode & _S_IFDIR);
#endif
    }
  }

  static inline uint64_t GetFileSize(const std::string& file_name) {
    struct stat info;
    if (::stat(file_name.c_str(), &info)) {
      CURRENT_THROW(FileException(file_name));
    } else {
      if (
// clang-format off
#ifndef CURRENT_WINDOWS
          S_ISDIR(info.st_mode)
#else
          info.st_mode & _S_IFDIR
#endif
    ) {
        // clang-format on
        CURRENT_THROW(FileException(file_name));
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
#ifndef CURRENT_WINDOWS
        ::mkdir(directory.c_str(), 0755)
#else
        ::_mkdir(directory.c_str())
#endif
    ) {
      // clang-format on
      if (parameters == MkDirParameters::ThrowExceptionOnError) {
        // TODO(dkorolev): Analyze errno.
        CURRENT_THROW(MkDirException(directory));
      }
    }
  }

  static inline void RenameFile(const std::string& old_name, const std::string& new_name) {
    if (::rename(old_name.c_str(), new_name.c_str())) {
      // TODO(dkorolev): Analyze errno.
      CURRENT_THROW(FileException(old_name + " -> " + new_name));
    }
  }

  // TODO(dkorolev): Make OutputFile not as tightly coupled with std::ofstream as it is now.
  typedef std::ofstream OutputFile;

  struct ScanDirItemInfo {
    std::string basename;
    std::string pathname;
    bool is_directory;

    ScanDirItemInfo() = delete;
    ScanDirItemInfo(std::string basename, std::string pathname, bool is_directory)
        : basename(std::move(basename)), pathname(std::move(pathname)), is_directory(is_directory) {}
  };

  enum class ScanDirParameters : int { ListFilesOnly = 1, ListDirsOnly = 2, ListFilesAndDirs = 3 };
  enum class ScanDirRecursive : bool { No = false, Yes = true };

  static inline bool ScanDirCanHandleName(const char* const name) {
    return (*name && ::strcmp(name, ".") && ::strcmp(name, ".."));
  }

  template <typename ITEM_HANDLER>
  static inline void ScanDirUntil(const std::string& directory,
                                  ITEM_HANDLER&& item_handler,
                                  ScanDirParameters parameters = ScanDirParameters::ListFilesOnly,
                                  ScanDirRecursive recursive = ScanDirRecursive::No) {
    if (recursive == ScanDirRecursive::No) {
#ifdef CURRENT_WINDOWS
      WIN32_FIND_DATAA find_data;
      HANDLE handle = ::FindFirstFileA((directory + "\\*.*").c_str(), &find_data);
      if (handle == INVALID_HANDLE_VALUE) {
        CURRENT_THROW(DirDoesNotExistException(directory));
      } else {
        struct ScopedCloseFindFileHandle {
          HANDLE handle_;
          ScopedCloseFindFileHandle(HANDLE handle) : handle_(handle) {}
          ~ScopedCloseFindFileHandle() { ::FindClose(handle_); }
        };
        const ScopedCloseFindFileHandle closer(handle);
        do {
          const char* const name = find_data.cFileName;
          if (ScanDirCanHandleName(name)) {
            const bool is_directory = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
            const ScanDirParameters mask = is_directory
                                             ? ScanDirParameters::ListDirsOnly
                                             : ScanDirParameters::ListFilesOnly;
            if (static_cast<int>(parameters) & static_cast<int>(mask)) {
              if (!item_handler(ScanDirItemInfo(name, JoinPath(directory, name), is_directory))) {
                return;
              }
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
          const char* const name = entry->d_name;
          if (ScanDirCanHandleName(name)) {
            // `IsDir` is proved to be required on Ubuntu running in Parallels on a Mac,
            // with Bricks' directory mounted from Mac's filesystem.
            // `entry->d_type` is always zero there, see http://comments.gmane.org/gmane.comp.lib.libcg.devel/4236
            std::string path = JoinPath(directory, name);
            const bool is_directory = IsDir(path);
            const ScanDirParameters mask =
                is_directory ? ScanDirParameters::ListDirsOnly : ScanDirParameters::ListFilesOnly;
            if (static_cast<int>(parameters) & static_cast<int>(mask)) {
              if (!item_handler(ScanDirItemInfo(name, std::move(path), is_directory))) {
                return;
              }
            }
          }
        }
      } else {
        if (errno == ENOENT) {
          CURRENT_THROW(DirDoesNotExistException(directory));
        } else if (errno == ENOTDIR) {
          CURRENT_THROW(PathNotDirException(directory));
        } else {
          CURRENT_THROW(FileException(directory));  // LCOV_EXCL_LINE
        }
      }
#endif
    } else {
      const std::function<bool(const ScanDirItemInfo&)> item_handler_recursive = [&item_handler, parameters](const ScanDirItemInfo& item_info) {
        const ScanDirParameters mask = item_info.is_directory
                                         ? ScanDirParameters::ListDirsOnly
                                         : ScanDirParameters::ListFilesOnly;
        if (static_cast<int>(parameters) & static_cast<int>(mask)) {
          if (!item_handler(item_info)) {
            return false;
          }
        }
        if (item_info.is_directory) {
          ScanDirUntil<ITEM_HANDLER>(item_info.pathname, std::forward<ITEM_HANDLER>(item_handler), parameters, ScanDirRecursive::Yes);
        }
        return true;
      };
      ScanDirUntil(directory,
                   item_handler_recursive,
                   ScanDirParameters::ListFilesAndDirs,
                   ScanDirRecursive::No);
    }
  }

  template <typename ITEM_HANDLER>
  static inline void ScanDir(const std::string& directory,
                             ITEM_HANDLER&& item_handler,
                             ScanDirParameters parameters = ScanDirParameters::ListFilesOnly,
                             ScanDirRecursive recursive = ScanDirRecursive::No) {
    ScanDirUntil(directory,
                 [&item_handler](const ScanDirItemInfo& item_info) {
                   item_handler(item_info);
                   return true;
                 },
                 parameters,
                 recursive);
  }

  enum class RmFileParameters { ThrowExceptionOnError, Silent };
  static inline void RmFile(const std::string& file_name,
                            RmFileParameters parameters = RmFileParameters::ThrowExceptionOnError) {
    if (::remove(file_name.c_str())) {
      if (parameters == RmFileParameters::ThrowExceptionOnError) {
        CURRENT_THROW(FileException(file_name));
      }
    }
  }

  class ScopedRmFile final {
   public:
    explicit ScopedRmFile(const std::string& file_name, bool remove_now_as_well = true) : file_name_(file_name) {
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
#ifndef CURRENT_WINDOWS
          ::rmdir(directory.c_str())
#else
          ::_rmdir(directory.c_str())
#endif
      ) {
        // clang-format on
        if (parameters == RmDirParameters::ThrowExceptionOnError) {
          if (errno == ENOENT) {
            CURRENT_THROW(DirDoesNotExistException(directory));
          } else if (errno == ENOTEMPTY) {
            CURRENT_THROW(DirNotEmptyException(directory));
          } else {
            CURRENT_THROW(FileException(directory));  // LCOV_EXCL_LINE
          }
        }
      }
    } else {
      try {
        ScanDir(directory,
                [parameters](const ScanDirItemInfo& item_info) {
                  if (item_info.is_directory) {
                    RmDir(item_info.pathname, parameters, RmDirRecursive::Yes);
                  } else {
                    RmFile(item_info.pathname,
                           (parameters == RmDirParameters::ThrowExceptionOnError)
                               ? RmFileParameters::ThrowExceptionOnError
                               : RmFileParameters::Silent);
                  }
                },
                ScanDirParameters::ListFilesAndDirs);
        RmDir(directory, parameters, RmDirRecursive::No);
      } catch (const current::Exception&) {
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

}  // namespace current

#endif  // BRICKS_FILE_FILE_H
