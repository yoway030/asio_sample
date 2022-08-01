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


// A reference-counted non-modifiable buffer class.
class shared_const_buffer
{
public:
    // Construct from a std::string.
    explicit shared_const_buffer(const std::string& data)
        : data_(new std::vector<char>(data.begin(), data.end())),
        buffer_(asio::buffer(*data_))
    {
        LOG_DEBUG("shared_const_buffer constructor. ref_count={}", data_.use_count());
    }

    // Implement the ConstBufferSequence requirements.
    typedef asio::const_buffer value_type;
    typedef const asio::const_buffer* const_iterator;
    const asio::const_buffer* begin() const 
    {
        LOG_DEBUG("shared_const_buffer begin call");
        return &buffer_; 
    }
    const asio::const_buffer* end() const 
    { 
        LOG_DEBUG("shared_const_buffer end call");
        return &buffer_ + 1; 
    }

private:
    std::shared_ptr<std::vector<char> > data_;
    asio::const_buffer buffer_;
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
        do_write();
    }

private:
    void do_write()
    {
        LOG_DEBUG("session do_write");

#pragma warning(disable:4996)
        std::time_t now = std::time(0);
        shared_const_buffer buffer(std::ctime(&now));
#pragma warning(default:4996)

        auto self(shared_from_this());

        LOG_DEBUG("session do_write async_write");

        asio::async_write(socket_, buffer,
            [self](std::error_code /*ec*/, std::size_t /*length*/)
            {
                LOG_DEBUG("session do_write callback");
            });
    }

    // The socket used to communicate with the client.
    tcp::socket socket_;
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
        auto end_point = acceptor_.local_endpoint();
        auto adress = end_point.address();
        auto port = end_point.port();
        LOG_INFO("server do_accept. listener={}:{}", adress.to_string(), port);

        acceptor_.async_accept(
            [this](std::error_code ec, tcp::socket socket)
            {
                if (!ec)
                {
                    LOG_DEBUG("server do_accept callback");
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
            LOG_ERROR("Usage: reference_counted <port>");
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