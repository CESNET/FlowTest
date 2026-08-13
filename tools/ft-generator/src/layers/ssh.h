/**
 * @file
 * @author Jaroslav Renda <jaroslav.renda@cesnet.cz>
 * @brief SSH application layer
 *
 * SPDX-Licence-Identifier: BSD-3-Clause
 */

#pragma once

#include "../buffer.h"
#include "../flow.h"
#include "../layer.h"
#include "../packet.h"
#include "sshbuilder.h"
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace generator {

/**
 * @brief Structure of active flow KEX parameters
 */
struct SshSettings {
	std::pair<std::string_view, uint8_t> macAlgorithm
		= {"", 0}; ///< Selected MAC algorithm and size
	std::pair<std::string_view, uint16_t> pubKeyAlgorithm
		= {"", 0}; ///< Selected Public Key algorithm and size
	std::pair<std::string_view, uint16_t> kexAlgorithm = {"", 0}; ///< Selected KEX algorithm
};

/**
 * @brief Class implementing the SSH layer
 */
class Ssh : public Layer {
public:
	/**
	 * @brief Plan SSH layer
	 *
	 * @param flow Flow to plan.
	 */
	void PlanFlow(Flow& flow) override;

	/**
	 * @brief Build SSH layer in packet
	 *
	 * @param packet Packet to build.
	 * @params Layer parameters.
	 * @param plan Packet plan.
	 */
	void Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan) override;

private:
	std::vector<Buffer> _messageStore;
	SshBuilder _builder;
	size_t _kexPacketCntr = 0;
	SshSettings _settings;
};

} // namespace generator
