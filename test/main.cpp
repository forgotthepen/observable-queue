#include "observable/queue.hpp"
#include <thread>
#include <chrono>
#include <iostream>
#include <string>


struct MyStr {
    std::string str{};
    MyStr() {
        std::cout << "** MyStr()" << '\n';
    }
    MyStr(const MyStr &other) {
        std::cout << "** MyStr(copy)" << '\n';
        str = other.str;
    }
    MyStr(MyStr &&other) {
        std::cout << "** MyStr(move)" << '\n';
        str = std::move(other.str);
    }
    MyStr(const std::string &str_other) {
        std::cout << "** MyStr(str)" << '\n';
        str = str_other;
    }
    ~MyStr() {
        std::cout << "** ~MyStr()" << '\n';
    }
    MyStr& operator =(const MyStr &other) {
        std::cout << "** MyStr +(copy)" << '\n';
        str = other.str;
        return *this;
    }
    MyStr& operator =(MyStr &&other) {
        std::cout << "** MyStr +(move)" << '\n';
        str = std::move(other.str);
        return *this;
    }
};


int main(int argc, char* *argv) {
    obs::queue<MyStr> queue{};

    auto magic = [&](MyStr &){
        std::cout << "  ## magic! -- " << '\n';
    };

    queue += [&](MyStr &str) {
        std::cout << "  >> You typed: " << str.str << '\n';
        if ("magic" == str.str || "secret" == str.str) {
            queue += magic;
        } else if ("regular" == str.str || "normal" == str.str) {
            queue -= magic;
        }
    };

    while (true) {
        std::cout << "Type anything and press Enter: ";
        std::string ss{};
        std::getline(std::cin, ss);
        if ("exit" == ss || "quit" == ss || "x" == ss || "q" == ss) {
            break;
        }

        queue.emplace_back(ss);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    std::cout << "\nDone... " << '\n';
    return 0;
}
