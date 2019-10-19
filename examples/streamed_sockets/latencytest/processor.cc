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

#include "workers/processor.h"
#include "workers/receiver.h"

DEFINE_double(buffer_mb, 512.0, "The size of the circular buffer to use, in megabytes.");

DEFINE_uint16(listen_port, 8004, "The local port to listen on.");
DEFINE_string(host, "127.0.0.1", "The destination address to send the confirmations to.");
DEFINE_uint16(port, 8005, "The destination port to send the confirmations to.");

PIPELINE_MAIN(BlockSource<ReceivingWorker>(FLAGS_listen_port), BlockWorker<ProcessingWorker>(FLAGS_host, FLAGS_port));
