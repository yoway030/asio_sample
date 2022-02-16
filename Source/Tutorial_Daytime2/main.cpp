//
// server.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#define _CRT_SECURE_NO_WARNINGS

#include <LibInclude.h>
#include <ctime>
#include <string>
#include <asio/asio.hpp>

using asio::ip::tcp;

std::string make_daytime_string()
{
    using namespace std; // For time_t, time and ctime;
    time_t now = time(0);
    return ctime(&now);
}

int main()
{
    try
    {
        asio::io_context io_context;

        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 13));

        for (;;)
        {
            tcp::socket socket(io_context);
            acceptor.accept(socket);

            std::string message = make_daytime_string();

            asio::error_code ignored_error;
            asio::write(socket, asio::buffer(message), ignored_error);
        }
    }
    catch (std::exception& e)
    {
        LOG_ERROR(e.what());
    }

    return 0;
}