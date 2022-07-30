//
// server.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <LibInclude.h>
#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>
#include <asio/asio.hpp>

using asio::ip::tcp;

// Class to manage the memory to be used for handler-based custom allocation.
// It contains a single block of memory which may be returned for allocation
// requests. If the memory is in use when an allocation request is made, the
// allocator delegates allocation to the global heap.
class handler_memory
{
public:
    handler_memory()
        : in_use_(false)
    {
    }

    handler_memory(const handler_memory&) = delete;
    handler_memory& operator=(const handler_memory&) = delete;

    void* allocate(std::size_t size)
    {
        if (!in_use_ && size < sizeof(storage_))
        {
            LOG_DEBUG("handler_memory use allocate. size={}", size);
            in_use_ = true;
            return &storage_;
        }
        else
        {
            LOG_DEBUG("handler_memory new allocate. size={}", size);
            return ::operator new(size);
        }
    }

    void deallocate(void* pointer)
    {
        if (pointer == &storage_)
        {
            LOG_DEBUG("handler_memory use deallocate");
            in_use_ = false;
        }
        else
        {
            LOG_DEBUG("handler_memory new deallocate");
            ::operator delete(pointer);
        }
    }

private:
    // Storage space used for handler-based custom memory allocation.
    typename std::aligned_storage<1024>::type storage_;

    // Whether the handler-based custom allocation storage has been used.
    bool in_use_;
};

// The allocator to be associated with the handler objects. This allocator only
// needs to satisfy the C++11 minimal allocator requirements.
template <typename T>
class handler_allocator
{
public:
    using value_type = T;

    explicit handler_allocator(handler_memory& mem)
        : memory_(mem)
    {
    }

    template <typename U>
    handler_allocator(const handler_allocator<U>& other) noexcept
        : memory_(other.memory_)
    {
    }

    bool operator==(const handler_allocator& other) const noexcept
    {
        return &memory_ == &other.memory_;
    }

    bool operator!=(const handler_allocator& other) const noexcept
    {
        return &memory_ != &other.memory_;
    }

    T* allocate(std::size_t n) const
    {
        return static_cast<T*>(memory_.allocate(sizeof(T) * n));
    }

    void deallocate(T* p, std::size_t /*n*/) const
    {
        return memory_.deallocate(p);
    }

private:
    template <typename> friend class handler_allocator;

    // The underlying memory.
    handler_memory& memory_;
};

class session
    : public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket)
        : socket_(std::move(socket))
    {
    }

    void start()
    {
        auto end_point = socket_.local_endpoint();
        auto adress = end_point.address();
        auto port = end_point.port();
        LOG_INFO("session start : {}:{}", adress.to_string(), port);
        do_read();
    }

private:
    void do_read()
    {
        auto self(shared_from_this());
        socket_.async_read_some(asio::buffer(data_), 
            asio::bind_allocator(handler_allocator<int>(handler_memory_),
                [this, self](std::error_code ec, std::size_t length)
                {
                    if (!ec)
                    {
                        LOG_DEBUG("sessio do_read. data={}", std::string_view(data_.begin(), data_.begin() + length));
                        do_write(length);
                    }
                }));
    }

    void do_write(std::size_t length)
    {
        auto self(shared_from_this());
        asio::async_write(socket_, asio::buffer(data_, length),
            asio::bind_allocator(handler_allocator<int>(handler_memory_),
                [this, self](std::error_code ec, std::size_t length)
                {
                    if (!ec)
                    {
                        LOG_DEBUG("sessio do_write. data={}", std::string_view(data_.begin(), data_.begin() + length));
                        do_read();
                    }
                }));
    }

    // The socket used to communicate with the client.
    tcp::socket socket_;

    // Buffer used to store data received from the client.
    std::array<char, 1024> data_;

    // The memory to use for handler-based custom memory allocation.
    handler_memory handler_memory_;
};

class server
{
public:
    server(asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
    }

private:
    void do_accept()
    {
        acceptor_.async_accept(
            [this](std::error_code ec, tcp::socket socket)
            {
                if (!ec)
                {
                    std::make_shared<session>(std::move(socket))->start();
                }

                do_accept();
            });
    }

    tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
    spdlog_default_initialize();

    try
    {
        if (argc != 2)
        {
            LOG_ERROR("Usage: server <port>");
            return 1;
        }

        asio::io_context io_context;
        server s(io_context, std::atoi(argv[1]));
        io_context.run();
    }
    catch (std::exception& e)
    {
        LOG_CRITICAL("Exception: {}", e.what());
    }

    return 0;
}