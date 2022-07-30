//
// chat_client.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <LibInclude.h>

#include <cstdlib>
#include <deque>
#include <iostream>
#include <thread>
#include <asio/asio.hpp>

using asio::ip::tcp;

#include <cstdio>
#include <cstdlib>
#include <cstring>

class chat_message
{
public:
    enum { header_length = 4 };
    enum { max_body_length = 512 };

    chat_message()
        : body_length_(0)
    {
    }

    const char* data() const
    {
        return data_;
    }

    char* data()
    {
        return data_;
    }

    std::size_t length() const
    {
        return header_length + body_length_;
    }

    const char* body() const
    {
        return data_ + header_length;
    }

    char* body()
    {
        return data_ + header_length;
    }

    std::size_t body_length() const
    {
        return body_length_;
    }

    void body_length(std::size_t new_length)
    {
        body_length_ = new_length;
        if (body_length_ > max_body_length)
            body_length_ = max_body_length;
    }

    bool decode_header()
    {
        char header[header_length + 1] = "";
        std::strncat(header, data_, header_length);
        body_length_ = std::atoi(header);
        if (body_length_ > max_body_length)
        {
            body_length_ = 0;
            return false;
        }
        return true;
    }

    void encode_header()
    {
        char header[header_length + 1] = "";
        std::sprintf(header, "%4d", static_cast<int>(body_length_));
        std::memcpy(data_, header, header_length);
    }

private:
    char data_[header_length + max_body_length];
    std::size_t body_length_;
};


typedef std::deque<chat_message> chat_message_queue;

class chat_client
{
public:
    chat_client(asio::io_context& io_context,
        const tcp::resolver::results_type& endpoints)
        : io_context_(io_context),
        socket_(io_context)
    {
        do_connect(endpoints);
    }

    void write(const chat_message& msg)
    {
        asio::post(io_context_,
            [this, msg]()
            {
                bool write_in_progress = !write_msgs_.empty();
                write_msgs_.push_back(msg);
                if (!write_in_progress)
                {
                    do_write();
                }
            });
    }

    void close()
    {
        asio::post(io_context_, [this]() { socket_.close(); });
    }

private:
    void do_connect(const tcp::resolver::results_type& endpoints)
    {
        asio::async_connect(socket_, endpoints,
            [this](std::error_code ec, tcp::endpoint)
            {
                if (!ec)
                {
                    do_read_header();
                }
            });
    }

    void do_read_header()
    {
        asio::async_read(socket_,
            asio::buffer(read_msg_.data(), chat_message::header_length),
            [this](std::error_code ec, std::size_t /*length*/)
            {
                if (!ec && read_msg_.decode_header())
                {
                    do_read_body();
                }
                else
                {
                    socket_.close();
                }
            });
    }

    void do_read_body()
    {
        asio::async_read(socket_,
            asio::buffer(read_msg_.body(), read_msg_.body_length()),
            [this](std::error_code ec, std::size_t /*length*/)
            {
                if (!ec)
                {
                    LOG_DEBUG("read body. msg={}", std::string_view(read_msg_.body(), read_msg_.body_length()));
                    do_read_header();
                }
                else
                {
                    socket_.close();
                }
            });
    }

    void do_write()
    {
        asio::async_write(socket_,
            asio::buffer(write_msgs_.front().data(),
                write_msgs_.front().length()),
            [this](std::error_code ec, std::size_t /*length*/)
            {
                if (!ec)
                {
                    auto& msg = write_msgs_.front();
                    LOG_DEBUG("write body. msg={}", std::string_view(msg.body(), msg.body_length()));

                    write_msgs_.pop_front();
                    if (!write_msgs_.empty())
                    {
                        do_write();
                    }
                }
                else
                {
                    socket_.close();
                }
            });
    }

private:
    asio::io_context& io_context_;
    tcp::socket socket_;
    chat_message read_msg_;
    chat_message_queue write_msgs_;
};

int main(int argc, char* argv[])
{
    spdlog_default_initialize();

    try
    {
        LOG_INFO("input port : ");
        std::string port;
        std::cin >> port;

        asio::io_context io_context;

        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve("127.0.0.1", port);
        chat_client c(io_context, endpoints);

        std::thread t([&io_context]() { io_context.run(); });

        char line[chat_message::max_body_length + 1];
        while (std::cin.getline(line, chat_message::max_body_length + 1))
        {
            chat_message msg;
            msg.body_length(std::strlen(line));
            std::memcpy(msg.body(), line, msg.body_length());
            msg.encode_header();
            c.write(msg);
        }

        c.close();
        t.join();
    }
    catch (std::exception& e)
    {
        LOG_INFO("Exception: {}", e.what());
    }

    return 0;
}