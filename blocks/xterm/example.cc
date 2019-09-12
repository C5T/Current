#include <iostream>
#include <sstream>
#include <thread>

#include "progress.h"
#include "vt100.h"

inline void wait() { std::this_thread::sleep_for(std::chrono::seconds(1)); }

int main() {
  {
    // Basic VT100 demo.
    using namespace current::vt100;

    std::cout << "Default, " << bold << "bold" << normal << ", " << dim << "dim" << normal << ", done."
              << std::endl;
    std::cout << "Default, " << italic << "italic" << noitalic << '.' << std::endl;
    std::cout << "Default, " << underlined << "underlined" << nounderlined << '.' << std::endl;
    std::cout << "Default, " << doubleunderlined << "double underlined" << nounderlined << '.' << std::endl;
    std::cout << "Default, " << strikeout << "strikeout" << nostrikeout << '.' << std::endl;
    std::cout << "Default, " << bold << italic << underlined << "bold & italic & underlined" << reset << '.'
              << std::endl;

    std::ostringstream oss;
    oss << default_color << ", ";
    std::string const sep = oss.str();

    std::cout << "Default, " << black << "black" << sep << dark_gray << "dark gray" << sep << bright_gray
              << "bright gray" << sep << white << "white" << sep << "done." << std::endl;
    std::cout << "Default [ normal ], " << red << "red" << sep << green << "green" << sep << blue << "blue" << sep
              << yellow << "yellow" << sep << magenta << "magenta" << sep << cyan << "cyan" << sep << "done."
              << std::endl;
    std::cout << "Default [ bright ], " << bright_red << "red" << sep << bright_green << "green" << sep << bright_blue
              << "blue" << sep << bright_yellow << "yellow" << sep << bright_magenta << "magenta" << sep << bright_cyan
              << "cyan" << sep << "done." << std::endl;

    std::cout << "Default, " << background(red) << blue << bold << "blue on red " << reverse << " reversed "
              << noreverse << " back to blue on red" << reset << '.' << std::endl;

    std::cout << "Default, " << strikeout << "no " << red << "red" << bold << " bold" << italic << " italic"
              << noitalic << ' ' << underlined << "underlined" << nounderlined << ' ' << doubleunderlined
              << "doubleunderlined" << nounderlined << normal << default_color << " style" << nostrikeout
              << '.' << reset << std::endl;
  }

  {
    // Smoke test.
    std::cout << "Test1> ";
    {
      current::ProgressLine progress;
      progress << "Hello,";
      wait();
      progress << "Hello, "
               << "world.";
      wait();
      progress << "TTYL!";
      wait();
    }
    std::cout << "Done." << std::endl;
  }
  {
    // Cyrillic.
    // Does not work on Windows. -- D.K.
    std::cout << "Test2> ";
    {
      current::ProgressLine progress;
      progress << u8"Привет...";
      wait();
      progress << u8"Йоу" << '!';
      wait();
      progress << u8"Йоу" << ", world!";
      wait();
    }
    std::cout << "Done." << std::endl;
  }

  if (false) {
    // NOTE(dkorolev): This fails on my Linux. :/ Prints `Test3> tttesting ... OK`, then `Test3> ttDone.` at the end.
    // Variable length characters.
    std::cout << "Test*> ";
    {
      current::ProgressLine progress;
      progress << "testing ...";
      wait();
      progress << u8"testing ... 企 ...";
      wait();
      progress << u8"testing ... 企 ... фигасе ...";
      wait();
      progress << "testing ... OK";
      wait();
    }
    std::cout << "Done." << std::endl;
  }

  {
    // VT100 decorations.
    std::cout << "Test3> ";
    {
      using namespace current::vt100;
      current::ProgressLine progress;
      progress << "Here be ...";
      wait();
      progress << red << 'C' << green << 'O' << blue << 'L' << yellow << 'O' << magenta << 'R' << cyan << 'S';
      wait();
      progress << "And " << bold << "styles" << italic << " as " << underlined << "well" << reset << '!';
      wait();
    }
    std::cout << "Done." << std::endl;
  }
}
