/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Flow plan helper
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "flowplanhelper.h"
#include "randomgenerator.h"

#include <algorithm>
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

		if (!packet._isFinished) {
			_nUnfinished++;
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
	auto next = [&]() -> Packet* {
		if (_nTraversed == _flow->_packets.size()) {
			return nullptr;
		}

		if (_nTraversed == 0) {
			_it = _flow->_packets.begin();
		} else if (_it != _flow->_packets.end()) {
			++_it;
		}
		_nTraversed++;

		return &(*_it);
	};

	Packet* pkt = nullptr;
	while (true) {
		pkt = next();
		// Break if there are no more packets (nullptr) or we have found a unfinished packet
		if (!pkt || !pkt->_isFinished) {
			break;
		}
	}
	_packet = pkt;
	if (pkt && !pkt->_isFinished) {
		_nUnfinishedTraversed++;
	}
	return pkt;
}

uint64_t FlowPlanHelper::PktsFromStart() const
{
	return _nUnfinishedTraversed == 0 ? 0 : _nUnfinishedTraversed - 1;
}

uint64_t FlowPlanHelper::PktsTillEnd() const
{
	assert(_nUnfinished >= _nUnfinishedTraversed);
	return _nUnfinished - _nUnfinishedTraversed;
}

void FlowPlanHelper::Reset()
{
	_nTraversed = 0;
	_nUnfinishedTraversed = 0;
	_packet = nullptr;

	_nUnfinished = 0;
	for (auto& packet : _flow->_packets) {
		if (!packet._isFinished) {
			_nUnfinished++;
		}
	}
}

uint64_t FlowPlanHelper::PktsRemaining() const
{
	return FwdPktsRemaining() + RevPktsRemaining();
}

uint64_t FlowPlanHelper::PktsRemaining(Direction dir) const
{
	assert(dir != Direction::Unknown);
	return dir == Direction::Reverse ? RevPktsRemaining() : FwdPktsRemaining();
}

uint64_t FlowPlanHelper::FwdPktsRemaining() const
{
	return _flow->_fwdPackets - _assignedFwdPkts;
}

uint64_t FlowPlanHelper::RevPktsRemaining() const
{
	return _flow->_revPackets - _assignedRevPkts;
}

uint64_t FlowPlanHelper::BytesRemaining(Direction dir) const
{
	assert(dir != Direction::Unknown);
	return dir == Direction::Reverse ? RevBytesRemaining() : FwdBytesRemaining();
}

Direction FlowPlanHelper::GetRandomDir() const
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

uint64_t FlowPlanHelper::FwdBytesRemaining() const
{
	return _flow->_fwdBytes - _assignedFwdBytes;
}

uint64_t FlowPlanHelper::RevBytesRemaining() const
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
