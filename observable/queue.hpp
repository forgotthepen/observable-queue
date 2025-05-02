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

#include <type_traits> // std::remove_cv, std::remove_reference, ...
#include <typeinfo> // std::type_info
#include <functional>
#include <memory>
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
    private:
        template<typename Tany>
        struct fn_traits {
            template<typename Tignore>
            inline constexpr static const std::type_info& type(Tignore &&) {
                return typeid(Tany);
            }
        };

        template<typename Tany>
        struct fn_traits<std::function<Tany>> {
            inline static const std::type_info& type(const std::function<Tany> &fn) noexcept {
                return fn.target_type();
            }
        };

    public:
        class t_consumer {
        private:
            struct ICallableConsumer {
                virtual ~ICallableConsumer() { } // in case the child is a type with a dtor
                virtual void operator ()(Ty &item) = 0;
                virtual const std::type_info& type() const noexcept = 0;
            };

            template<typename Tfn>
            class CallableConsumerImpl : public ICallableConsumer {
            private:
                using TfnObj = typename std::remove_cv<typename std::remove_reference<Tfn>::type>::type;

                TfnObj fn_;

            public:
                CallableConsumerImpl(TfnObj fn):
                    fn_( std::move(fn) )
                { }

                void operator ()(Ty &item) override {
                    fn_( item );
                }

                const std::type_info& type() const noexcept override {
                    return fn_traits<TfnObj>::type(fn_);
                }
            };

            std::unique_ptr<ICallableConsumer> callable_consumer_;

        public:
            template<typename Tfn>
            t_consumer(Tfn &&fn):
                callable_consumer_(new CallableConsumerImpl<Tfn>(
                    std::forward<Tfn>(fn)
                ))
            { }

            inline void operator ()(Ty &item) const {
                (*callable_consumer_)(item);
            }
            
            inline const std::type_info& type() const {
                return callable_consumer_->type();
            }
        };

    private:
        std::thread thread_;
        std::condition_variable cv_{};
        bool kill_ = false;

        std::mutex mtx_{};
        std::list<Ty> queue_{};
        
        std::mutex mtx_consumers_{};
        std::list<t_consumer> consumers_{};

        void kill() {
            {
                std::lock_guard<std::mutex> lock(mtx_);

                if (kill_) {
                    return;
                }

                kill_ = true;
            }

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
        
        bool try_pop_front(Ty *obj = nullptr) {
            std::lock_guard<std::mutex> lock(mtx_);

            if (queue_.empty()) {
                return false;
            }

            if (obj) {
                *obj = std::move( queue_.front() );
            }

            queue_.pop_front();
            return true;
        }

        template<typename Tfn>
        queue<Ty>& operator +=(Tfn &&consumer) {
            std::lock_guard<std::mutex> lock(mtx_consumers_);

            const auto &consumer_type = fn_traits<Tfn>::type(std::forward<Tfn>(consumer));
            bool exists = std::any_of(consumers_.begin(), consumers_.end(), [&consumer_type](const t_consumer &item){
                return item.type() == consumer_type;
            });
            if (!exists) {
                consumers_.emplace_back(std::forward<Tfn>(consumer));
            }
            return *this;
        }

        template<typename Tfn>
        queue<Ty>& operator -=(Tfn &&consumer) {
            std::lock_guard<std::mutex> lock(mtx_consumers_);

            const auto &consumer_type = fn_traits<Tfn>::type(std::forward<Tfn>(consumer));
            consumers_.remove_if([&consumer_type](const t_consumer &item){
                return item.type() == consumer_type;
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

        inline void poll() {
            if (!IsThreaded) {
                worker_fn();                
            }
        }

    };
}
