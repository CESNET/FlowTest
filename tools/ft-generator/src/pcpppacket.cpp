/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief A wrapper class around pcpp::Packet
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pcpppacket.h"

namespace generator {

void PcppPacket::addLayer(pcpp::Layer* newLayer, bool ownInPacket)
{
	if (!pcpp::Packet::addLayer(newLayer, ownInPacket)) {
		throw PcppError();
	}
}

void PcppPacket::insertLayer(pcpp::Layer* prevLayer, pcpp::Layer* newLayer, bool ownInPacket)
{
	if (!pcpp::Packet::insertLayer(prevLayer, newLayer, ownInPacket)) {
		throw PcppError();
	}
}

void PcppPacket::removeLayer(pcpp::ProtocolType layerType, int index)
{
	if (!pcpp::Packet::removeLayer(layerType, index)) {
		throw PcppError();
	}
}

void PcppPacket::removeFirstLayer()
{
	if (!pcpp::Packet::removeFirstLayer()) {
		throw PcppError();
	}
}

void PcppPacket::removeLastLayer()
{
	if (!pcpp::Packet::removeLastLayer()) {
		throw PcppError();
	}
}

void PcppPacket::removeAllLayersAfter(pcpp::Layer* layer)
{
	if (!pcpp::Packet::removeAllLayersAfter(layer)) {
		throw PcppError();
	}
}

void PcppPacket::detachLayer(pcpp::Layer* layer)
{
	if (!pcpp::Packet::detachLayer(layer)) {
		throw PcppError();
	}
}

}
