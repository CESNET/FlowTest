/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Packet iterator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "packetflowspan.h"

namespace generator {

PacketFlowSpan::Iterator::Iterator(
	packetIterator wrapper,
	packetIterator wrapperEnd,
	bool getOnlyAvailable)
	: _wrapper(wrapper)
	, _wrapperEnd(wrapperEnd)
	, _getOnlyAvailable(getOnlyAvailable)
{
	GetElement(false);
}

void PacketFlowSpan::Iterator::GetElement(bool incrementOperator)
{
	if (incrementOperator == true) {
		++_wrapper;
	}

	while (_getOnlyAvailable && _wrapper != _wrapperEnd && (*_wrapper)._isFinished) {
		++_wrapper;
	}
}

std::pair<size_t, size_t> PacketFlowSpan::GetAvailableDirections()
{
	size_t fwd = _flow->_fwdPackets;
	size_t rev = _flow->_revPackets;

	for (auto& packet : _flow->_packets) {
		if (packet._direction == Direction::Forward) {
			assert(fwd > 0 && "value underflow");
			fwd--;
		}

		if (packet._direction == Direction::Reverse) {
			assert(rev > 0 && "value underflow");
			rev--;
		}
	}

	return std::make_pair(fwd, rev);
}

} // namespace generator
