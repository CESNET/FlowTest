/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief TLS application layer
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../buffer.h"
#include "../flow.h"
#include "../layer.h"
#include "../packet.h"
#include "../pcpppacket.h"
#include "tlsbuilder.h"

namespace generator {

/**
 * @brief Class implementing the TLS layer
 */
class Tls : public Layer {
public:
	/**
	 * @brief Construct a TLS layer object
	 *
	 * @param maxPayloadSizeHint  The maximum size the TLS layer _should_ occupy in a packet.
	 *
	 * @note maxPayloadSizeHint only involes handshake packets (application data packet sizes are
	 *       determined by the packet size planner, not this layer). The value is also only a hint
	 *       and can be ignored if too low.
	 */
	Tls(uint64_t maxPayloadSizeHint);

	/**
	 * @brief Plan TLS layer
	 *
	 * @param flow Flow to plan.
	 */
	void PlanFlow(Flow& flow) override;

	/**
	 * @brief Build TLS layer in packet
	 *
	 * @param packet Packet to build.
	 * @param params Layers parameters
	 * @param plan   Packet plan.
	 */
	void Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan) override;

private:
	std::vector<Buffer> _messageStore;
	TlsBuilder _builder;
	std::size_t _maxPayloadSizeHint;
};

} // namespace generator
