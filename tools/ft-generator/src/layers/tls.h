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
	 */
	Tls() = default;

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
};

} // namespace generator
