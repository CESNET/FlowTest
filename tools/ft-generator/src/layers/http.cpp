/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief HTTP application layer
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "http.h"
#include "../domainnamegenerator.h"
#include "../flowplanhelper.h"
#include "../randomgenerator.h"
#include "../utils.h"
#include "httpbuilder.h"
#include "httpgenerator.h"

#include <pcapplusplus/HttpLayer.h>
#include <pcapplusplus/PayloadLayer.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <string_view>
#include <utility>
#include <vector>

namespace generator {

static constexpr int HTTP_GET_THRESHOLD = 500; // Number of bytes when to choose GET over POST

static constexpr int MIN_FWD_GET_PKT_LEN = LengthOf(
	"GET / HTTP/1.1\r\n"
	"Host: x.xx\r\n"
	"\r\n");

// We do not know the maximum content length in advance, so let's just reserve the maximum number
// of digits an uint64_t can have so this can never fail
static constexpr int MIN_FWD_POST_PKT_LEN = LengthOf(
	"POST / HTTP/1.1\r\n"
	"Host: x.xx\r\n"
	"Content-Length: xxxxxxxxxxxxxxxxxxx\r\n"
	"\r\n");

static constexpr int MIN_REV_PKT_LEN = LengthOf(
	"HTTP/1.1 200 OK\r\n"
	"Content-Length: xxxxxxxxxxxxxxxxxxx\r\n"
	"\r\n");

namespace {

enum HttpPacketKind : uint64_t {
	ReqInitial, //< First packet of a HTTP request
	ResInitial, //< First packet of a HTTP response
	ReqContinuation, //< Continuation packet of a HTTP request
	ResContinuation //< Continuation packet of a HTTP response
};

enum HttpParams : int {
	Kind, //< The HttpPacketKind of the packet
	MessageSize, //< The size of the whole HTTP message
	LayerStartOffset, //< The offset of the HTTP layer from the start of the packet
	NumParts //< Number of packets the HTTP message is split to
};
} // namespace

void Http::PlanFlow(Flow& flow)
{
	FlowPlanHelper planner(flow);

	Packet* prevPkt = nullptr;
	bool prevPktFirstInDir = false;

	while (planner.PktsTillEnd() > 0) {
		Packet* pkt = planner.NextPacket();

		Packet::layerParams params;
		params[HttpParams::LayerStartOffset] = pkt->_size;
		pkt->_layers.emplace_back(this, params);

		/**
		 * First packet must be forward,
		 * last packet must be reverse,
		 * direction of other packets are random
		 */
		if (planner.PktsFromStart() == 0 && planner.FwdPktsRemaining() > 0) {
			pkt->_direction = Direction::Forward;
			planner.IncludePkt(pkt);
		} else if (planner.PktsTillEnd() == 0 && planner.RevPktsRemaining() > 0) {
			pkt->_direction = Direction::Reverse;
			planner.IncludePkt(pkt);
		} else {
			pkt->_direction = planner.GetRandomDir();
			planner.IncludePkt(pkt);
		}

		// Specify the minimum length of the packet for the planner
		if (pkt->_direction == Direction::Forward) {
			pkt->_size += MIN_FWD_GET_PKT_LEN;

			// If this is a request message that consists of multiple parts, the first packet has to
			// be larger than the absolute minimum
			if (prevPkt && prevPktFirstInDir && prevPkt->_direction == Direction::Forward) {
				prevPkt->_size -= MIN_FWD_GET_PKT_LEN;
				prevPkt->_size += MIN_FWD_POST_PKT_LEN;
			}
		} else {
			pkt->_size += MIN_REV_PKT_LEN;
		}

		prevPktFirstInDir = !prevPkt || prevPkt->_direction != pkt->_direction;
		prevPkt = pkt;
	}
}

void Http::PostPlanFlow(Flow& flow)
{
	Direction lastDir = Direction::Unknown;
	Packet* initPkt = nullptr;

	for (auto& pkt : flow._packets) {
		if (!PacketHasLayer(pkt)) {
			continue;
		}

		auto& params = GetPacketParams(pkt);
		auto sizeOffset = std::get<uint64_t>(params[HttpParams::LayerStartOffset]);

		assert(sizeOffset <= pkt._size);
		assert(pkt._direction == Direction::Forward || pkt._direction == Direction::Reverse);

		if (pkt._direction != lastDir) {
			initPkt = &pkt;
			lastDir = pkt._direction;

			params[HttpParams::NumParts] = 1u;
			params[HttpParams::MessageSize] = pkt._size - sizeOffset;
			params[HttpParams::Kind]
				= (pkt._direction == Direction::Forward ? HttpPacketKind::ReqInitial
														: HttpPacketKind::ResInitial);

		} else {
			auto& initParams = GetPacketParams(*initPkt);
			std::get<uint64_t>(initParams[HttpParams::MessageSize]) += pkt._size - sizeOffset;
			std::get<uint64_t>(initParams[HttpParams::NumParts]) += 1;

			params[HttpParams::Kind]
				= (pkt._direction == Direction::Forward ? HttpPacketKind::ReqContinuation
														: HttpPacketKind::ResContinuation);
		}
	}
}

void Http::Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	auto kind = HttpPacketKind {std::get<uint64_t>(params[HttpParams::Kind])};

	switch (kind) {
	case HttpPacketKind::ReqInitial: {
		auto fullRequestLen = std::get<uint64_t>(params[HttpParams::MessageSize]);
		auto numParts = std::get<uint64_t>(params[HttpParams::NumParts]);
		if (numParts == 1 && plan._size < HTTP_GET_THRESHOLD) {
			assert(fullRequestLen == plan._size);
			HttpGenerateGetRequest(packet, plan._size);
		} else {
			HttpGeneratePostRequest(packet, plan._size, fullRequestLen);
		}

		plan._size = 0;

	} break;

	case HttpPacketKind::ResInitial: {
		auto fullRequestLen = std::get<uint64_t>(params[HttpParams::MessageSize]);
		HttpGenerateResponse(packet, plan._size, fullRequestLen);

		plan._size = 0;
	} break;

	case HttpPacketKind::ReqContinuation: {
		HttpGeneratePayload(packet, plan._size);
	} break;

	case HttpPacketKind::ResContinuation: {
		HttpGeneratePayload(packet, plan._size);
	} break;
	}
}

} // namespace generator
