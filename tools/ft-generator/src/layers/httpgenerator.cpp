/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief HTTP message generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "httpgenerator.h"
#include "../domainnamegenerator.h"
#include "../randomgenerator.h"
#include "../utils.h"
#include "httpbuilder.h"

#include <pcapplusplus/PayloadLayer.h>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <random>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

namespace generator {

static constexpr int MAX_HOST_LEN = 64;

static const std::vector<std::string> ACCEPT_HEADER_VALUES = {
	"*/*",
	"text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8",
	"text/plain;q=0.5, text/html",
	"application/xml, application/json;q=0.9",
	"text/html; charset=UTF-8",
	"text/html; level=1",
	"text/css",
	"application/pdf;q=0.8, application/json;q=0.5",
	"image/jpeg, image/png, image/gif",
	"audio/mpeg, audio/wav; q=0.7",
};

static const std::vector<std::string> CACHE_CONTROL_HEADER_VALUES = {
	"no-cache",
	"no-store",
	"public, max-age=3600",
	"private",
	"max-age=86400, must-revalidate",
	"no-transform",
	"max-age=0",
	"s-maxage=1800",
	"stale-if-error=3600",
	"no-cache=\"set-cookie\"",
};

static const std::vector<std::string> ACCEPT_ENCODING_HEADER_VALUES = {
	"*",
	"identity",
	"gzip, deflate",
	"br",
	"gzip;q=1.0, identity;q=0.5, *;q=0",
	"compress",
	"gzip, deflate, sdch",
	"br, gzip;q=0.6, deflate;q=0.5",
	"br;q=0.9, gzip;q=0.8",
	"gzip; q=1.0, identity; q=0.5, *; q=0",
};

static const std::vector<std::string> CONTENT_TYPE_HEADER_VALUES = {
	"text/html; charset=UTF-8",
	"application/json",
	"application/xml",
	"text/plain",
	"image/jpeg",
	"application/pdf",
	"application/javascript",
	"text/css",
	"application/octet-stream",
	"application/x-www-form-urlencoded",
	"text/csv",
	"application/x-www-form-urlencoded; charset=UTF-8",
	"image/png",
	"application/rss+xml",
};

static const std::vector<std::string> SERVER_HEADER_VALUES = {
	"Apache/2.4.41 (Unix)",
	"nginx/1.18.0",
	"Microsoft-IIS/10.0",
	"LiteSpeed",
	"cloudflare",
	"Jetty/9.4.40.v20210413",
	"Apache-Coyote/1.1",
	"gunicorn/20.1.0",
	"Caddy",
	"lighttpd/1.4.59",
};

// clang-format off
static const std::vector<std::string> USER_AGENT_HEADER_VALUES = {
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/115.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_2) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/79.0.3945.88 Safari/537.36",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/111.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/15.6.1 Safari/605.1.15",
    "Mozilla/5.0 (X11; CrOS x86_64 14541.0.0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/112.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/115.0.0.0 Safari/537.36 OPR/101.0.0.0",
    "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/114.0",
    "Mozilla/5.0 (compatible; MSIE 11.0; Windows NT 6.3; Trident/7.0)",
    "Mozilla/4.0 (compatible; MSIE 5.5; Windows NT 5.0)",
    "curl/7.77.0",
    "Wget/1.20.3 (linux-gnu)",
    "ELinks/0.11.3-5 (textmode; Linux; 131x40-2)",
    "w3m/0.5.3+git20180125 (Linux; i686; EMI)",
    "libwww-perl/5.825",
};
// clang-format on

static void TryAddField(
	HttpBuilder& builder,
	uint64_t maxHeaderSize,
	const std::string& name,
	const std::vector<std::string>& values,
	double probability = 1.0)
{
	auto& rng = RandomGenerator::GetInstance();
	if (rng.RandomDouble() > probability) {
		return;
	}

	const auto& value = rng.RandomChoice(values);

	builder.SetField(name, value);
	if (builder.GetHeaderLength() > maxHeaderSize) {
		builder.RemoveField(name);
	}
}

void HttpGeneratePayload(PcppPacket& packet, uint64_t size)
{
	std::vector<uint8_t> payload(size);
	std::independent_bits_engine<std::default_random_engine, 8, uint8_t> rbe;
	std::generate(payload.begin(), payload.end(), std::ref(rbe));

	pcpp::PayloadLayer* payloadLayer = new pcpp::PayloadLayer(payload.data(), payload.size(), true);
	packet.addLayer(payloadLayer, true);
}

static std::string GenerateUrl(uint64_t length)
{
	auto& rng = RandomGenerator::GetInstance();

	std::string s = "/";
	s.reserve(length);

	double p = rng.RandomDouble();
	uint64_t pathLength = std::max<uint64_t>(length * p, 1);
	uint64_t queryLength = length - pathLength;

	for (uint64_t i = 1; i < pathLength; i++) {
		if (rng.RandomDouble() < 0.1 && s.back() != '/') {
			s.push_back('/');
		} else {
			s.push_back(rng.RandomChoice(LOWERCASE_LETTERS));
		}
	}

	if (queryLength > 0) {
		s.push_back('?');
		bool placedEq = false;
		for (uint64_t i = 1; i < queryLength; i++) {
			if (rng.RandomDouble() < 0.1 && s.back() != '?' && s.back() != '&'
				&& (placedEq || rng.RandomDouble() < 0.2)) {
				s.push_back('&');
				placedEq = false;
			} else if (
				rng.RandomDouble() < 0.2 && s.back() != '?' && s.back() != '&' && !placedEq) {
				s.push_back('=');
				placedEq = true;
			} else {
				s.push_back(rng.RandomChoice(LOWERCASE_LETTERS));
			}
		}
	}

	return s;
}

static std::string GenerateCookie(int length)
{
	auto& rng = RandomGenerator::GetInstance();
	std::string s;
	s.reserve(length);
	for (int i = 0; i < length; i++) {
		s.push_back(rng.RandomChoice(ALPHANUMERIC_CHARS));
	}
	return s;
}

static void GenerateFillerValues(
	HttpBuilder& builder,
	uint64_t requiredSize,
	bool noHost = false,
	bool noCookie = false)
{
	std::string url = "/";
	std::string host = "x.xx";
	std::string cookie = "x";

	builder.SetUrl(url);
	if (!noHost) {
		builder.SetField("Host", host);
	}
	if (!noCookie) {
		builder.SetField("Cookie", cookie);
	}

	assert(builder.GetHeaderLength() <= requiredSize);
	uint64_t remainder = requiredSize - builder.GetHeaderLength();

	auto& rng = RandomGenerator::GetInstance();

	double urlPortion = rng.RandomDouble();
	double hostPortion = noHost ? 0 : rng.RandomDouble() * 0.3;
	double cookiePortion = noCookie ? 0 : rng.RandomDouble() * 0.6;
	double sum = urlPortion + hostPortion + cookiePortion;
	urlPortion /= sum;
	hostPortion /= sum;
	cookiePortion /= sum;

	uint64_t urlLen = url.size() + urlPortion * remainder;
	uint64_t hostLen = noHost ? 0 : host.size() + hostPortion * remainder;
	uint64_t cookieLen = noCookie ? 0 : cookie.size() + cookiePortion * remainder;

	if (hostLen > MAX_HOST_LEN) {
		uint64_t remainingToDistribute = hostLen - MAX_HOST_LEN;
		hostLen = MAX_HOST_LEN;
		double urlPortion = noCookie ? 1.0 : rng.RandomDouble();

		uint64_t urlExtraLen = urlPortion * remainingToDistribute;
		urlLen += urlExtraLen;

		cookieLen += remainingToDistribute - urlExtraLen;
	}

	remainder -= urlLen - url.size();
	remainder -= noHost ? 0 : hostLen - host.size();
	remainder -= noCookie ? 0 : cookieLen - cookie.size();
	urlLen += remainder;

	url = GenerateUrl(urlLen);
	assert(url.size() == urlLen);
	if (!noHost) {
		host = DomainNameGenerator::GetInstance().Generate(hostLen);
		assert(host.size() == hostLen);
	}
	if (!noCookie) {
		cookie = GenerateCookie(cookieLen);
		assert(cookie.size() == cookieLen);
	}

	builder.SetUrl(url);
	if (!noHost) {
		builder.SetField("Host", host);
	}
	if (!noCookie) {
		builder.SetField("Cookie", cookie);
	}

	assert(builder.GetHeaderLength() == requiredSize);
}

void HttpGenerateGetRequest(PcppPacket& packet, uint64_t size)
{
	HttpBuilder builder(HttpBuilder::Request);
	builder.SetVersion("1.1");
	builder.SetUrl("/");
	builder.SetMethod("GET");

	// This is only a placeholder to reserve space. Proper domain name will be generated later.
	builder.SetField("Host", "x.xx");

	if (builder.GetHeaderLength() > size) {
		throw std::logic_error("provided packet size is too small to generate a HTTP message");
	}

	TryAddField(builder, size, "Connection", {"Keep-Alive"});
	TryAddField(builder, size, "User-Agent", USER_AGENT_HEADER_VALUES, 0.9);
	TryAddField(builder, size, "Accept", ACCEPT_HEADER_VALUES, 0.5);
	TryAddField(builder, size, "Accept-Encoding", ACCEPT_ENCODING_HEADER_VALUES, 0.5);
	TryAddField(builder, size, "Content-Type", CONTENT_TYPE_HEADER_VALUES, 0.5);
	TryAddField(builder, size, "Cache-Control", CACHE_CONTROL_HEADER_VALUES, 0.5);

	bool noHost = false;
	bool noCookie = false;
	if (builder.GetHeaderLength() + LengthOf("Cookie: x\r\n") > size) {
		noCookie = true;
	}
	GenerateFillerValues(builder, size, noHost, noCookie);

	builder.RandomizeFieldNamesCapitalization();
	builder.ShuffleFields();

	auto layer = builder.ToLayer();
	assert(layer->getDataLen() == size);
	packet.addLayer(layer.release(), true);
}

void HttpGeneratePostRequest(PcppPacket& packet, uint64_t size, uint64_t contentLenInclHeader)
{
	auto& rng = RandomGenerator::GetInstance();
	uint64_t maxHeaderSize = rng.RandomDouble() * size;

	assert(maxHeaderSize <= size);

	HttpBuilder builder(HttpBuilder::Request);
	builder.SetVersion("1.1");
	builder.SetUrl("/");
	builder.SetMethod("POST");

	// This is called here only to add the "Content-Length" field to the message
	// a reserve the necessary space before optional headers are added, the
	// actual calculation is done later.
	builder.CalcAndSetContentLength(contentLenInclHeader);

	// This is only a placeholder to reserve space. Proper domain name will be generated later.
	builder.SetField("Host", "x.xx");

	if (builder.GetHeaderLength() > size) {
		throw std::logic_error("provided packet size is too small to generate a HTTP message");
	}

	TryAddField(builder, maxHeaderSize, "Connection", {"Keep-Alive"});
	TryAddField(builder, maxHeaderSize, "User-Agent", USER_AGENT_HEADER_VALUES, 0.9);
	TryAddField(builder, maxHeaderSize, "Accept", ACCEPT_HEADER_VALUES, 0.5);
	TryAddField(builder, maxHeaderSize, "Accept-Encoding", ACCEPT_ENCODING_HEADER_VALUES, 0.5);
	TryAddField(builder, maxHeaderSize, "Content-Type", CONTENT_TYPE_HEADER_VALUES, 0.5);
	TryAddField(builder, maxHeaderSize, "Cache-Control", CACHE_CONTROL_HEADER_VALUES, 0.5);

	// We are allowed to pass the randomly generated maxHeaderSize as long as it
	// doesn't exceed the actual packet size. In such case, adjust the value
	// accordingly.
	uint64_t actualHeaderSize = builder.GetHeaderLength();
	if (actualHeaderSize > maxHeaderSize) {
		maxHeaderSize = actualHeaderSize;
	}

	bool noHost = false;
	bool noCookie = false;
	if (maxHeaderSize < builder.GetHeaderLength() + LengthOf("Cookie: x\r\n")) {
		noCookie = true;
	}
	GenerateFillerValues(builder, maxHeaderSize, noHost, noCookie);

	builder.CalcAndSetContentLength(contentLenInclHeader);

	builder.RandomizeFieldNamesCapitalization();
	builder.ShuffleFields();

	auto layer = builder.ToLayer();
	auto payloadSize = SafeSub(size, layer->getDataLen());
	packet.addLayer(layer.release(), true);
	HttpGeneratePayload(packet, payloadSize);
}

void HttpGenerateResponse(PcppPacket& packet, uint64_t size, uint64_t contentLenInclHeader)
{
	HttpBuilder builder(HttpBuilder::Response);

	builder.SetVersion("1.1");
	builder.SetStatus("200 OK");

	// This is called here only to add the "Content-Length" field to the message
	// a reserve the necessary space before optional headers are added, the
	// actual calculation is done later.
	builder.CalcAndSetContentLength(contentLenInclHeader);

	TryAddField(builder, size, "Server", SERVER_HEADER_VALUES, 0.9);
	TryAddField(builder, size, "Content-Type", CONTENT_TYPE_HEADER_VALUES, 0.5);
	builder.CalcAndSetContentLength(contentLenInclHeader);

	builder.RandomizeFieldNamesCapitalization();
	builder.ShuffleFields();

	auto layer = builder.ToLayer();
	auto payloadSize = SafeSub(size, layer->getDataLen());
	packet.addLayer(layer.release(), true);
	HttpGeneratePayload(packet, payloadSize);
}

} // namespace generator
