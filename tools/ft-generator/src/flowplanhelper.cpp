/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Flow plan helper
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "flowplanhelper.h"
#include "randomgenerator.h"

#include <cassert>

namespace generator {

FlowPlanHelper::FlowPlanHelper(Flow& flow)
	: _flow(&flow)
{
	uint64_t availFwdPkts = _flow->_fwdPackets;
	uint64_t availRevPkts = _flow->_revPackets;
	uint64_t availFwdBytes = _flow->_fwdBytes;
	uint64_t availRevBytes = _flow->_revBytes;

	for (auto& packet : _flow->_packets) {
		if (packet._direction == Direction::Forward) {
			assert(availFwdPkts > 0 && "value underflow");
			availFwdPkts--;

			assert(availFwdBytes >= packet._size && "value underflow");
			availFwdBytes -= packet._size;

		} else if (packet._direction == Direction::Reverse) {
			assert(availRevPkts > 0 && "value underflow");
			availRevPkts--;

			assert(availRevBytes >= packet._size && "value underflow");
			availRevBytes -= packet._size;
		}
	}

	_assignedFwdPkts = _flow->_fwdPackets - availFwdPkts;
	_assignedRevPkts = _flow->_revPackets - availRevPkts;
	_assignedFwdBytes = _flow->_fwdBytes - availFwdBytes;
	_assignedRevBytes = _flow->_revBytes - availRevBytes;
	_fwdPktChance = double(FwdPktsRemaining()) / double(PktsRemaining());
}

Packet* FlowPlanHelper::NextPacket()
{
	if (_first) {
		_it = _flow->_packets.begin();
		_first = false;
	} else if (_it != _flow->_packets.end()) {
		++_it;
	}

	while (_it != _flow->_packets.end() && _it->_isFinished) {
		++_it;
	}

	if (_it != _flow->_packets.end()) {
		_packet = &(*_it);
	} else {
		_packet = nullptr;
	}

	return _packet;
}

void FlowPlanHelper::Reset()
{
	_first = true;
	_packet = nullptr;
}

uint64_t FlowPlanHelper::PktsRemaining()
{
	return FwdPktsRemaining() + RevPktsRemaining();
}

uint64_t FlowPlanHelper::FwdPktsRemaining()
{
	return _flow->_fwdPackets - _assignedFwdPkts;
}

uint64_t FlowPlanHelper::RevPktsRemaining()
{
	return _flow->_revPackets - _assignedRevPkts;
}

Direction FlowPlanHelper::GetRandomDir()
{
	bool hasFwd = FwdPktsRemaining() > 0;
	bool hasRev = RevPktsRemaining() > 0;
	if (!hasFwd && !hasRev) {
		throw std::logic_error("No more available packets");
	}

	if (hasFwd && hasRev) {
		return RandomGenerator::GetInstance().RandomDouble() <= _fwdPktChance ? Direction::Forward
																			  : Direction::Reverse;
	}

	return hasFwd ? Direction::Forward : Direction::Reverse;
}

uint64_t FlowPlanHelper::FwdBytesRemaining()
{
	return _flow->_fwdBytes - _assignedFwdBytes;
}

uint64_t FlowPlanHelper::RevBytesRemaining()
{
	return _flow->_revBytes - _assignedRevBytes;
}

void FlowPlanHelper::IncludePkt(Packet* pkt)
{
	if (pkt->_direction == Direction::Forward) {
		_assignedFwdPkts++;
		_assignedFwdBytes += pkt->_size;
		assert(_assignedFwdPkts <= _flow->_fwdPackets);
	} else if (pkt->_direction == Direction::Reverse) {
		_assignedRevPkts++;
		_assignedRevBytes += pkt->_size;
		assert(_assignedRevPkts <= _flow->_revPackets);
	} else {
		assert(0 && "Cannot include packet with unknown direction");
	}
}

} // namespace generator
