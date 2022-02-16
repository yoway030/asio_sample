//
// timer.cpp
// ~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <asio/asio.hpp>
#include <LibInclude.h>
#include <functional>

void print(const asio::error_code& /*e*/,
	asio::steady_timer* t, int* count)
{
	if (*count < 5)
	{
		LOG_INFO(*count);
		++(*count);

		t->expires_at(t->expiry() + asio::chrono::seconds(1));
		t->async_wait(std::bind(print,
			std::placeholders::_1, t, count));
	}
}

int main()
{
	asio::io_context io;

	int count = 0;
	asio::steady_timer t(io, asio::chrono::seconds(1));
	t.async_wait(std::bind(print,
		std::placeholders::_1, &t, &count));

	io.run();

	LOG_INFO("Final count is {}", count);

	return 0;
}