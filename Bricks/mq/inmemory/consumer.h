/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef BRICKS_MQ_INMEMORY_CONSUMER_H
#define BRICKS_MQ_INMEMORY_CONSUMER_H

#include <cstddef>
#include <utility>

namespace bricks {
namespace mq {
namespace mmq {

template <typename T_MESSAGE>
constexpr bool HasProcessMethodByPtr(char) {
  return false;
}

template <typename T_MESSAGE>
constexpr auto HasProcessMethodByPtr(int) -> decltype(std::declval<T_MESSAGE>() -> Process(), bool()) {
  return true;
}

template <typename T_MESSAGE>
constexpr bool HasProcessMethodByRef(char) {
  return false;
}

template <typename T_MESSAGE>
constexpr auto HasProcessMethodByRef(int) -> decltype(std::declval<T_MESSAGE&>().Process(), bool()) {
  return true;
}

template <typename T_MESSAGE, bool HAS_PROCESS_BY_PTR>
struct ProcessMessageImpl {};

template <typename T_MESSAGE>
struct ProcessMessageImpl<T_MESSAGE, true> {
  static void Process(T_MESSAGE message) { message->Process(); }
};

template <typename T_MESSAGE>
struct ProcessMessageImpl<T_MESSAGE, false> {
  static void Process(T_MESSAGE&& message) { message.Process(); }
};

template <typename T_MESSAGE>
void ProcessMessage(T_MESSAGE&& message) {
  static_assert(HasProcessMethodByPtr<T_MESSAGE>(0) || HasProcessMethodByRef<T_MESSAGE>(0),
                "`ProcessMessageConsumer` requires `Process()` method defined in the message class.");
  ProcessMessageImpl<T_MESSAGE, HasProcessMethodByPtr<T_MESSAGE>(0)>::Process(std::forward<T_MESSAGE>(message));
}

// Default MMQ consumer. Calls `Process()` method for each message.
template <typename T_MESSAGE>
struct ProcessMessageConsumer {
  void OnMessage(T_MESSAGE&& message, size_t) { ProcessMessage(std::move(message)); }
};

}  // namespace mmq
}  // namespace mq
}  // namespace bricks

#endif  // BRICKS_MQ_INMEMORY_CONSUMER_H
