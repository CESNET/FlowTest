/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Packet iterator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "flow.h"
#include "packet.h"

#include <cassert>
#include <iostream>
#include <iterator>
#include <list>

namespace generator {

/**
 * @brief Custom iterator of class Flow
 */
class PacketFlowSpan {
public:
	class iterator {
	public:
		using packetIterator = std::list<Packet>::iterator;
		using iterator_category = std::input_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = Packet;
		using pointer = value_type*;
		using reference = value_type&;

		iterator(packetIterator wrapper, packetIterator wrapperEnd, bool getOnlyAvailable);

		bool operator==(const iterator& other) { return _wrapper == other._wrapper; }

		bool operator!=(const iterator& other) { return !(*this == other); }

		reference operator*() const { return *_wrapper; }

		pointer operator->() { return &(*_wrapper); }

		iterator& operator++()
		{
			GetElement();
			return *this;
		}

	private:
		void GetElement(bool incrementOperator = true);

		packetIterator _wrapper;
		packetIterator _wrapperEnd;
		bool _getOnlyAvailable;
	};

	/**
	 * @brief Construct a new Packet Flow Span object
	 *
	 * @param flow              The flow to be iterated over
	 * @param getOnlyAvailable  Whether finished packets should be skipped over
	 */
	PacketFlowSpan(Flow* flow, bool getOnlyAvailable)
		: _flow(flow)
		, _getOnlyAvailable(getOnlyAvailable)
	{
	}

	/**
	 * @brief Get the number of packets that are still assignable for forward/reverse directions
	 *
	 * @return Number of assignable packets in forward direction and number of assignable packets
	 *         in reverse direction
	 */
	std::pair<size_t, size_t> getAvailableDirections();

	iterator begin()
	{
		return iterator(std::begin(_flow->_packets), std::end(_flow->_packets), _getOnlyAvailable);
	}

	iterator end()
	{
		return iterator(std::end(_flow->_packets), std::end(_flow->_packets), _getOnlyAvailable);
	}

private:
	Flow* _flow;
	bool _getOnlyAvailable;
};

} // namespace generator
