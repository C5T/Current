/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2019 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

// NOTE(dkorolev): This implementation is quick and dirty, no error checking whatsoever, just to measure the throughput.

#ifndef EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_WORKERS_SAVER_H
#define EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_WORKERS_SAVER_H

#include <cstdio>
#include <queue>

#include "../blob.h"

#include "../../../../bricks/file/file.h"
#include "../../../../bricks/net/tcp/tcp.h"
#include "../../../../bricks/strings/printf.h"
#include "../../../../bricks/time/chrono.h"

namespace current::examples::streamed_sockets {

struct SavingWorker {
  const std::string dirname;
  const std::string filebase;
  const size_t blobs_per_file;
  const size_t max_total_files;
  uint64_t next_anticipated_index = 0u;
  std::queue<uint64_t> written_files_indexes;  // Unique `uint64_t`, `file_index` == `begin->index / blobs_per_file`.
  FILE* current_file = nullptr;                // The handle of the file for block `written_file_indexes.back()`.
  SavingWorker(std::string input_dirname,
               std::string input_filebase,
               size_t blobs_per_file,
               size_t max_total_files,
               bool wipe_files_at_startup)
      : dirname(std::move(input_dirname)),
        filebase(std::move(input_filebase)),
        blobs_per_file(blobs_per_file),
        max_total_files(max_total_files) {
    if (wipe_files_at_startup && !dirname.empty() && !filebase.empty()) {
      std::vector<std::string> files_to_remove;
      FileSystem::ScanDir(dirname, [this, &files_to_remove](const FileSystem::ScanDirItemInfo& info) {
        const std::string& fn = info.basename;
        if (fn.length() >= filebase.length() && fn.substr(0u, filebase.length()) == filebase) {
          files_to_remove.push_back(fn);
        }
      });
      for (const std::string& filename_to_remove : files_to_remove) {
        FileSystem::RmFile(FileSystem::JoinPath(dirname, filename_to_remove));
      }
    }
  }

  std::string FullFilenameForBlobIndex(uint64_t blob_index) const {
    return FileSystem::JoinPath(
        dirname,
        filebase + strings::Printf("0x%016llx-0x%016llx.bin",
                                   static_cast<long long>(blob_index * blobs_per_file),
                                   static_cast<long long>((blob_index + 1u) * blobs_per_file) - 1u));
  }

  const Blob* DoWork(const Blob* begin, const Blob* end) {
    const uint64_t begin_blob_index = begin->index / blobs_per_file;
    if (written_files_indexes.empty() || !(written_files_indexes.back() == begin_blob_index)) {
      if (current_file) {
        fclose(current_file);
      }
      current_file = nullptr;
      written_files_indexes.push(begin_blob_index);
      if (max_total_files) {
        while (written_files_indexes.size() > max_total_files) {
          FileSystem::RmFile(FullFilenameForBlobIndex(written_files_indexes.front()));
          written_files_indexes.pop();
        }
      }
      current_file = fopen(FullFilenameForBlobIndex(begin_blob_index).c_str(), "wb");
    }
    const uint64_t blobs_left_in_block = blobs_per_file - (begin->index - begin_blob_index * blobs_per_file);
    const Blob* end_blob = begin + blobs_left_in_block;
    if (end_blob < end) {
      // Write the currently open file (to close it and repen a new one, on the very next call to `DoWork()`).
      fwrite(begin, blobs_left_in_block, sizeof(Blob), current_file);
      return end_blob;
    } else {
      // The whole [begin, end) range is part of the blob that is currenyly being written.
      fwrite(begin, end - begin, sizeof(Blob), current_file);
      return end;
    }
    return end;
  }
};

}  // namespace current::examples::streamed_sockets

#endif  // EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_WORKERS_SAVER_H
