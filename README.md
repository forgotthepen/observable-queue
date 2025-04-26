# observable-queue

A single header library implementing an observable queue.  

## Features:
* All subscribers to the queue will be notified once an element is pushed
* Thread-safe
* Supports both threaded and manual-polling modes
* Compatible with C++11

Example
```c++
#include "observable/queue.hpp"
#include <iostream>
#include <string>

int main(int argc, char* *argv) {
    // create an observable queue
    obs::queue<std::string> queue{};

    // subscribe to push events
    queue += [](std::string &str) {
        std::cout << "  >> You typed: " << str << '\n';
    };

    while (true) {
        std::cout << "Type anything and press Enter: ";

        std::string ss{};
        std::getline(std::cin, ss);

        if ("exit" == ss || "quit" == ss || "x" == ss || "q" == ss) {
            break;
        }

        // add it to the queue
        queue.emplace_back(ss);
    }

    std::cout << "\nDone... " << '\n';
    return 0;
}
```
