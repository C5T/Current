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

#include "pipeline_main.h"

#include "workers/indexer.h"
#include "workers/receiver.h"
#include "workers/saver.h"
#include "workers/sender.h"

DEFINE_double(buffer_mb, 512.0, "The size of the circular buffer to use, in megabytes.");

DEFINE_uint16(listen_port, 8001, "The local port to listen on.");
DEFINE_string(host1, "127.0.0.1", "The destination address to send data to.");
DEFINE_uint16(port1, 8002, "The destination port to send data to.");
DEFINE_string(host2, "127.0.0.1", "The destination address to send data to.");
DEFINE_uint16(port2, 8003, "The destination port to send data to.");
DEFINE_string(dirname, ".current", "The dir name for the stored data files.");
DEFINE_string(filebase, "idx.", "The filename prefix for the stored data files.");
DEFINE_uint64(blobs_per_file,
              (1 << 28) / sizeof(current::examples::streamed_sockets::Blob),
              "The number of blobs per file saved, defaults to 256MB files.");
DEFINE_uint32(max_total_files, 4u, "The maximum number of data files to keep, defaults to four files, for 1GB total.");
DEFINE_bool(wipe_files_at_startup, true, "Unset to not wipe the files from the previous run.");
DEFINE_bool(skip_fwrite, false, "Set to not `fwrite()` into the files; for network perftesting only.");

PIPELINE_MAIN(BlockSource<ReceivingWorker>(FLAGS_listen_port),
              BlockWorker<IndexingWorker>(),
              BlockWorker<SavingWorker>(FLAGS_dirname,
                                        FLAGS_filebase,
                                        FLAGS_blobs_per_file,
                                        FLAGS_max_total_files,
                                        FLAGS_wipe_files_at_startup,
                                        FLAGS_skip_fwrite),
              BlockWorker<SendingWorker>(FLAGS_host1, FLAGS_port1),
              BlockWorker<SendingWorker>(FLAGS_host2, FLAGS_port2));
