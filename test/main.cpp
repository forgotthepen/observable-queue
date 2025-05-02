#include "observable/queue.hpp"
#include <thread>
#include <chrono>
#include <iostream>
#include <string>
#include <functional>


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
        std::cout << "** MyStr =(copy)" << '\n';
        str = other.str;
        return *this;
    }
    MyStr& operator =(MyStr &&other) {
        std::cout << "** MyStr =(move)" << '\n';
        str = std::move(other.str);
        return *this;
    }
};

struct MyCallable {
    MyCallable() {
        std::cout << "** MyCallable()" << '\n';
    }
    MyCallable(const MyCallable &other) {
        std::cout << "** MyCallable(copy)" << '\n';
    }
    MyCallable(MyCallable &&other) {
        std::cout << "** MyCallable(move)" << '\n';
    }
    MyCallable(const std::string &str_other) {
        std::cout << "** MyCallable(str)" << '\n';
    }
    ~MyCallable() {
        std::cout << "** ~MyCallable()" << '\n';
    }
    MyCallable& operator =(const MyCallable &other) {
        std::cout << "** MyCallable +(copy)" << '\n';
        return *this;
    }
    MyCallable& operator =(MyCallable &&other) {
        std::cout << "** MyCallable +(move)" << '\n';
        return *this;
    }

    void operator () (MyStr &str) const {
        std::cout << "  (( MyStr ()(call)" << '\n';
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

    auto bound_lval = std::bind([](MyStr &str){
        std::cout << "  >> bound l-val: " << str.str << '\n';
    }, std::placeholders::_1);

    queue += bound_lval;
    queue += bound_lval;

    {
        queue += std::bind([](MyStr &str){
            std::cout << "  >> bound r-val: " << str.str << '\n';
        }, std::placeholders::_1);
    }

    auto func_lval = std::function<void(MyStr &)>([](MyStr &str){
        std::cout << "  >> func l-val: " << str.str << '\n';
    });

    queue += func_lval;
    queue += func_lval;

    {
        queue += std::function<void(MyStr &)>([](MyStr &str){
            std::cout << "  >> func r-val: " << str.str << '\n';
        });
    }

    {
        queue += MyCallable{};
    }

    auto mycallable_lval = MyCallable{};
    queue += mycallable_lval;
    queue += mycallable_lval;

    while (true) {
        std::cout << "Type anything and press Enter: ";
        std::string ss{};
        std::getline(std::cin, ss);
        if ("exit" == ss || "quit" == ss || "x" == ss || "q" == ss) {
            break;
        } else if ("remove bound l-val" == ss) {
            queue -= bound_lval;
            queue -= bound_lval;
        } else if ("add bound l-val" == ss) {
            queue += bound_lval;
            queue += bound_lval;
        } else if ("remove func l-val" == ss) {
            queue -= func_lval;
            queue -= func_lval;
        } else if ("add func l-val" == ss) {
            queue += func_lval;
            queue += func_lval;
        } else if ("try pop" == ss) {
            queue.try_pop_front();
        }

        queue.emplace_back(ss);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    std::cout << "\nDone... " << '\n';
    return 0;
}
