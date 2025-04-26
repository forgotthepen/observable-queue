/*
MIT License

Copyright (c) 2025 forgotthepen (https://github.com/forgotthepen/observable-queue)

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
*/

#pragma once

#include <functional>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <list>
#include <cstdlib> // std::size_t
#include <utility>
#include <algorithm>

namespace obs {
    template<typename Ty, bool IsThreaded = true>
    class queue {
    public:
        using t_consumer = std::function<void(Ty &)>;

    private:
        std::thread thread_;
        std::condition_variable cv_{};
        bool kill_ = false;

        std::mutex mtx_{};
        std::list<Ty> queue_{};
        
        std::mutex mtx_consumers_{};
        std::list<t_consumer> consumers_{};

        void kill() {
            if (kill_) {
                return;
            }

            kill_ = true;

            if (IsThreaded) {
                cv_.notify_one();
                thread_.join();
            }
        }

        void worker_fn() {
            Ty *item = nullptr;
            decltype(consumers_.cbegin()) it_consumer;
            decltype(consumers_.cend()) it_end_consumer;

            while (true) {
                {
                    std::unique_lock<std::mutex> lock(mtx_);

                    if (IsThreaded) {
                        cv_.wait(lock, [this]{
                            return kill_ || !queue_.empty();
                        });
                    }

                    item = queue_.empty() ? nullptr : &queue_.front();
                }

                while (!kill_ && item != nullptr) {
                    {
                        std::lock_guard<std::mutex> lock(mtx_consumers_);

                        it_consumer = consumers_.cbegin();
                        it_end_consumer = consumers_.cend();
                    }
    
                    while (it_consumer != it_end_consumer) {
                        if (kill_) {
                            break;
                        }

                        bool bad_consumer;
                        try {
                            (*it_consumer)( *item );
                            bad_consumer = false;
                        } catch (...) {
                            bad_consumer = true;
                        }

                        {
                            std::lock_guard<std::mutex> lock(mtx_consumers_);
    
                            if (bad_consumer) {
                                it_consumer = consumers_.erase(it_consumer);
                            } else {
                                ++it_consumer;
                            }
                        }
                    }

                    {
                        std::lock_guard<std::mutex> lock(mtx_);

                        queue_.pop_front();
                        item = queue_.empty() ? nullptr : &queue_.front();
                    }
                }

                if (kill_ || !IsThreaded) {
                    break;
                }
            }
        }

    public:
        queue():
            thread_( IsThreaded ? std::thread([this] { worker_fn(); }) : std::thread{} )
        { }

        ~queue() {
            kill();
        }

        template<typename ...Args>
        Ty& emplace_back(Args ...args) {
            Ty *item;
            {
                std::lock_guard<std::mutex> lock(mtx_);
    
                queue_.emplace_back( std::forward<Args>(args) ... );
                item = &queue_.back();
            }

            if (IsThreaded) {
                cv_.notify_one();
            }
            return *item;
        }
        
        bool try_pop_front() {
            std::lock_guard<std::mutex> lock(mtx_);

            if (queue_.empty()) {
                return false;
            }

            queue_.pop_front();
            return true;
        }

        queue<Ty>& operator +=(const t_consumer &consumer) {
            std::lock_guard<std::mutex> lock(mtx_consumers_);

            bool exists = std::any_of(consumers_.begin(), consumers_.end(), [&consumer](const t_consumer &item){
                return item.target_type() == consumer.target_type();
            });
            if (!exists) {
                consumers_.emplace_back(consumer);
            }
            return *this;
        }

        queue<Ty>& operator -=(const t_consumer &consumer) {
            std::lock_guard<std::mutex> lock(mtx_consumers_);

            consumers_.remove_if([&consumer](const t_consumer &item){
                return item.target_type() == consumer.target_type();
            });
            return *this;
        }

        std::size_t size() {
            std::lock_guard<std::mutex> lock(mtx_);
            
            return queue_.size();
        }

        std::size_t size_consumers() {
            std::lock_guard<std::mutex> lock(mtx_consumers_);
            
            return consumers_.size();
        }

        void poll() {
            if (!IsThreaded) {
                worker_fn();                
            }
        }

    };
}
