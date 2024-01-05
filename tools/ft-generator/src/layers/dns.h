/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief DNS layer
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../layer.h"
#include "dnsbuilder.h"
#include "logger.h"

namespace generator {

/**
 * @brief A representation of a Dns layer.
 */
class Dns : public Layer {
public:
	/**
	 * @brief Plan Dns layer
	 *
	 * @param flow Flow to plan.
	 */
	void PlanFlow(Flow& flow) override;

	/**
	 * @brief Post plan callback
	 *
	 * @param flow The flow
	 */
	void PostPlanFlow(Flow& flow) override;

	/**
	 * @brief Build Dns layer in packet
	 *
	 * @param packet Packet to build.
	 * @param params Layers parameters
	 * @param plan   Packet plan.
	 */
	void Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan) override;

private:
	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("Dns");

	uint16_t _transactionId;
	std::string _domainName;

	int _numAnswers;
	DnsRRType _type;
	bool _useCompression;
	bool _useEdns;

	bool _generateRandomPayloadInsteadOfDns = false;

	void ReplanSizesToUniformDistribution(Flow& flow);
	void RevertSizesReplan(Flow& flow);
	void ProcessPlannedFlow(Flow& flow);
	void PlanCommunication(int64_t querySize, int64_t responseSize);
	void BuildForwardPacket(PcppPacket& packet, Packet::layerParams& params, Packet& plan);
	void BuildReversePacket(PcppPacket& packet, Packet::layerParams& params, Packet& plan);
	size_t GetDnsPayloadSize(Packet& plan) const;
};

} // namespace generator
