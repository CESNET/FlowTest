/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief HTTP message Builder
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "httpbuilder.h"
#include "../randomgenerator.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>

#include <pcapplusplus/PayloadLayer.h>

namespace generator {

static int NumberOfDigits(uint64_t value)
{
	return std::to_string(value).size();
}

HttpBuilder::HttpBuilder(MessageType type)
	: _type(type)
{
}

void HttpBuilder::SetUrl(const std::string& url)
{
	assert(_type == MessageType::Request);
	_url = url;
}

void HttpBuilder::SetVersion(const std::string& version)
{
	assert(version == "0.9" || version == "1.0" || version == "1.1");
	_version = version;
}

void HttpBuilder::SetMethod(const std::string& method)
{
	assert(
		method == "GET" || method == "POST" || method == "PUT" || method == "DELETE"
		|| method == "HEAD" || method == "OPTIONS");
	_method = method;
}

void HttpBuilder::SetField(const std::string& name, const std::string& value)
{
	for (auto& p : _fields) {
		if (p.first == name) {
			p.second = value;
			return;
		}
	}
	_fields.push_back({name, value});
}

void HttpBuilder::SetStatus(const std::string& status)
{
	_status = status;
}

void HttpBuilder::CalcAndSetContentLength(uint64_t contentLengthWithHeader)
{
	// Fill out the Content-Length with the minimum possible length
	SetField("Content-Length", "0");

	// Calculate new content length from the full length and the size of the header
	assert(contentLengthWithHeader >= GetHeaderLength());
	uint64_t contentLength = contentLengthWithHeader - GetHeaderLength();
	SetField("Content-Length", std::to_string(contentLength));

	// Calculate the content length once more in case we hit a weird edge-case,
	// in which case we compensate with spaces
	assert(contentLengthWithHeader >= GetHeaderLength());
	uint64_t correctedContentLength = contentLengthWithHeader - GetHeaderLength();

	assert(contentLength >= correctedContentLength);
	auto digitsDiff = NumberOfDigits(contentLength) - NumberOfDigits(correctedContentLength);

	std::string spaces(digitsDiff, ' ');
	SetField("Content-Length", std::to_string(correctedContentLength) + spaces);

	_calculatedContentLength = correctedContentLength;
}

void HttpBuilder::RemoveField(const std::string& name)
{
	_fields.erase(std::remove_if(_fields.begin(), _fields.end(), [&](const auto& p) {
		return p.first == name;
	}));
}

uint64_t HttpBuilder::GetCalculatedContentLength() const
{
	return _calculatedContentLength;
}

uint64_t HttpBuilder::GetHeaderLength() const
{
	return Build().size();
}

std::unique_ptr<pcpp::Layer> HttpBuilder::ToLayer() const
{
	auto data = Build();
	auto layer = std::make_unique<pcpp::PayloadLayer>(
		reinterpret_cast<uint8_t*>(data.data()),
		data.size(),
		true);
	return layer;
}

std::string HttpBuilder::Build() const
{
	std::stringstream ss;

	if (_type == MessageType::Request) {
		assert(!_method.empty());
		assert(!_url.empty());
		assert(!_version.empty());
		ss << _method << " " << _url << " HTTP/" << _version << "\r\n";

	} else if (_type == MessageType::Response) {
		assert(!_version.empty());
		assert(!_status.empty());
		ss << "HTTP/" << _version << " " << _status << "\r\n";
	}

	for (const auto& [name, value] : _fields) {
		ss << name << ": " << value << "\r\n";
	}
	ss << "\r\n";

	return ss.str();
}

void HttpBuilder::ShuffleFields()
{
	RandomGenerator::GetInstance().Shuffle(_fields);
}

void HttpBuilder::RandomizeFieldNamesCapitalization()
{
	auto p = RandomGenerator::GetInstance().RandomDouble();

	if (p <= 0.1) {
		// All lower case
		for (auto& p : _fields) {
			for (auto& c : p.first) {
				c = std::tolower(c);
			}
		}
	} else if (p <= 0.2) {
		// All upper case
		for (auto& p : _fields) {
			for (auto& c : p.first) {
				c = std::toupper(c);
			}
		}
	} else {
		// Camel-Case
		for (auto& p : _fields) {
			auto& s = p.first;
			for (std::size_t i = 0; i < s.size(); i++) {
				if (i == 0 || s[i - 1] == '-') {
					s[i] = std::toupper(s[i]);
				} else {
					s[i] = std::tolower(s[i]);
				}
			}
		}
	}
}

} // namespace generator
