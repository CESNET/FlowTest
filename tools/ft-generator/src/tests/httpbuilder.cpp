/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Tests for HTTP builder
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "../layers/httpbuilder.h"

#include <doctest.h>

#include <string>

using namespace generator;

TEST_SUITE_BEGIN("HttpBuilder");

TEST_CASE("Content-Length is calculated correctly")
{
	HttpBuilder builder(HttpBuilder::Request);
	builder.SetUrl("/");
	builder.SetMethod("GET");
	builder.SetVersion("1.0");
	builder.CalcAndSetContentLength(100);

	CHECK(builder.GetHeaderLength() + builder.GetCalculatedContentLength() == 100);
}

TEST_CASE("reported length matches actual length of output")
{
	HttpBuilder builder(HttpBuilder::Request);
	builder.SetUrl("/");
	builder.SetMethod("GET");
	builder.SetVersion("1.0");
	builder.SetField("User-Agent", "ft-generator");
	builder.SetField("Host", "test.com");
	builder.CalcAndSetContentLength(100);

	CHECK(builder.GetHeaderLength() == builder.ToLayer()->getDataLen());
}

TEST_CASE("simple request is generated validly")
{
	HttpBuilder builder(HttpBuilder::Request);
	builder.SetUrl("/test");
	builder.SetMethod("GET");
	builder.SetVersion("1.0");
	builder.SetField("User-Agent", "ft-generator");
	builder.SetField("Host", "test.com");

	const std::string expected
		= "GET /test HTTP/1.0\r\n"
		  "User-Agent: ft-generator\r\n"
		  "Host: test.com\r\n"
		  "\r\n";

	const auto layer = builder.ToLayer();
	const auto actual = std::string(reinterpret_cast<char*>(layer->getData()), layer->getDataLen());

	CHECK(actual == expected);
}

TEST_CASE("simple response is generated validly")
{
	HttpBuilder builder(HttpBuilder::Response);
	builder.SetStatus("200 OK");
	builder.SetMethod("GET");
	builder.SetVersion("1.0");
	builder.SetField("Server", "ft-generator");

	const std::string expected
		= "HTTP/1.0 200 OK\r\n"
		  "Server: ft-generator\r\n"
		  "\r\n";

	const auto layer = builder.ToLayer();
	const auto actual = std::string(reinterpret_cast<char*>(layer->getData()), layer->getDataLen());

	CHECK(actual == expected);
}

TEST_SUITE_END();
