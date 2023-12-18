/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Definition of the NfbQueue class
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "nfbQueue.hpp"

namespace replay {

NfbQueue::NfbQueue(const NfbQueueConfig& queueConfig, nfb_device* nfbDevice, unsigned queueId)
	: _queueConfig(queueConfig)
	, _nfbDevice(nfbDevice)
{
	_lastBurstTotalPacketLen = 0;
	_lastBurstSize = 0;
	_isBufferInUse = false;
	_offloads = {};

	SetupNdpQueue(queueId);
}

void NfbQueue::SetupNdpQueue(unsigned queueId)
{
	_txPacket = std::make_unique<ndp_packet[]>(_queueConfig.maxBurstSize);
	_txQueue.reset(ndp_open_tx_queue(_nfbDevice, queueId));
	if (!_txQueue) {
		_logger->error("Unable to open NDP TX queue: {}", queueId);
		throw std::runtime_error("NfbQueue::SetupNdpQueue() has failed");
	}
	if (ndp_queue_start(_txQueue.get()) != 0) {
		_logger->error("Unable to start NDP TX queue: {}", queueId);
		throw std::runtime_error("NfbQueue::SetupNdpQueue() has failed");
	}
}

NfbQueue::~NfbQueue()
{
	Flush();
}

void NfbQueue::Flush()
{
	ndp_tx_burst_flush(_txQueue.get());
	_isBufferInUse = false;
}

size_t NfbQueue::GetMaxBurstSize() const noexcept
{
	return _queueConfig.maxBurstSize;
}

void NfbQueue::GetBurst(PacketBuffer* burst, size_t burstSize)
{
	if (_isBufferInUse) {
		_logger->error(
			"GetBurst() called before the previous request was "
			"processed by SendBurst()");
		throw std::runtime_error("NfbQueue::GetBurst() has failed");
	}
	if (burstSize > _queueConfig.maxBurstSize) {
		_logger->error(
			"Requested burstSize {} is bigger than predefined {}",
			burstSize,
			_queueConfig.maxBurstSize);
		throw std::runtime_error("NfbQueue::GetBurst() has failed");
	}

	_lastBurstTotalPacketLen = 0;

	if (_queueConfig.superPacketsEnabled) {
		GetSuperBurst(burst, burstSize);
	} else {
		GetRegularBurst(burst, burstSize);
	}

	_lastBurstSize = burstSize;
	_isBufferInUse = true;
}

void NfbQueue::GetSuperBurst(PacketBuffer* burst, size_t burstSize)
{
	unsigned usedSuperPackets = 0;
	size_t packetsInSuperPacket = 0;
	_txPacket[0].data_length = 0;

	for (size_t idx = 0; idx < burstSize; idx++) {
		size_t packetTotalLength = HEADER_LEN + std::max(burst[idx]._len, MIN_PACKET_SIZE);
		size_t alignedLength = AlignBlockSize(packetTotalLength);

		if (_txPacket[usedSuperPackets].data_length + alignedLength <= _queueConfig.superPacketSize
			&& packetsInSuperPacket < _queueConfig.superPacketLimit) {
			_txPacket[usedSuperPackets].data_length += alignedLength;
			packetsInSuperPacket++;
		} else {
			if (packetsInSuperPacket) {
				usedSuperPackets++;
			}
			_txPacket[usedSuperPackets].data_length = alignedLength;
			packetsInSuperPacket = 1;
		}
	}

	usedSuperPackets++;
	GetBuffers(usedSuperPackets);

	usedSuperPackets = 0;
	unsigned offset = 0;
	for (size_t idx = 0; idx < burstSize; idx++) {
		size_t packetLength = std::max(burst[idx]._len, MIN_PACKET_SIZE);
		_lastBurstTotalPacketLen += packetLength;
		size_t packetTotalLength = HEADER_LEN + packetLength;
		size_t alignedLength = AlignBlockSize(packetTotalLength);

		// next packet/break condition
		if (alignedLength + offset > _txPacket[usedSuperPackets].data_length) {
			usedSuperPackets++;
			offset = 0;
		}

		FillReplicatorHeader(
			burst[idx],
			*reinterpret_cast<NfbReplicatorHeader*>(_txPacket[usedSuperPackets].data + offset));

		offset += HEADER_LEN; // move offset to packet data

		// assign buffer
		burst[idx]._data = reinterpret_cast<std::byte*>(_txPacket[usedSuperPackets].data + offset);
		// fill padding with zeros
		if (packetLength != burst[idx]._len) {
			std::fill_n(
				_txPacket[usedSuperPackets].data + offset + burst[idx]._len,
				packetLength - burst[idx]._len,
				0);
		}
		offset += alignedLength - HEADER_LEN; // move offset to next packet
	}
}

void NfbQueue::GetRegularBurst(PacketBuffer* burst, size_t burstSize)
{
	for (size_t idx = 0; idx < burstSize; idx++) {
		size_t packetLength = std::max(burst[idx]._len, MIN_PACKET_SIZE);
		_txPacket[idx].data_length = packetLength;
		if (burst[idx]._len < MIN_PACKET_SIZE) {
			_outputQueueStats.upscaledPackets++;
		}

		_lastBurstTotalPacketLen += _txPacket[idx].data_length;

		if (_queueConfig.replicatorHeaderEnabled) {
			_txPacket[idx].data_length += HEADER_LEN;
		}
	}

	GetBuffers(burstSize);

	for (unsigned idx = 0; idx < burstSize; idx++) {
		size_t packetLength = std::max(burst[idx]._len, MIN_PACKET_SIZE);
		unsigned packetDataOffset = 0;
		if (_queueConfig.replicatorHeaderEnabled) {
			FillReplicatorHeader(
				burst[idx],
				*reinterpret_cast<NfbReplicatorHeader*>(_txPacket[idx].data));
			packetDataOffset = HEADER_LEN;
		}

		burst[idx]._data = reinterpret_cast<std::byte*>(_txPacket[idx].data + packetDataOffset);
		if (packetLength != burst[idx]._len) {
			std::fill_n(
				_txPacket[idx].data + packetDataOffset + burst[idx]._len,
				packetLength - burst[idx]._len,
				0);
		}
	}
}

void NfbQueue::FillReplicatorHeader(const PacketBuffer& packetBuffer, NfbReplicatorHeader& header)
{
	size_t packetLength = std::max(packetBuffer._len, MIN_PACKET_SIZE);

	std::memset(&header, 0, sizeof(header));

	header.length = htole16(packetLength);
	header.l2Length = packetBuffer._info->l3Offset;

	if (_offloads & Offload::RATE_LIMIT_TIME) {
		// NOLINTBEGIN
		struct uint48_t {
			uint64_t value : 48;
		} __attribute__((packed));
		// NOLINTEND
		uint48_t* timestamp = (uint48_t*) header.timestamp.data();
		timestamp->value = packetBuffer._timestamp;
	}

	if (packetBuffer._info->l3Type == L3Type::IPv4) {
		header.l3Type = NfbReplicatorHeader::L3Type::IPv4;
		header.calculateL3Checksum = (_offloads & Offload::CHECKSUM_IPV4) != 0;
	} else if (packetBuffer._info->l3Type == L3Type::IPv6) {
		header.l3Type = NfbReplicatorHeader::L3Type::IPv6;
	}

	if (packetBuffer._info->l4Type == L4Type::NotFound) {
		return;
	}

	header.l3Length = packetBuffer._info->l4Offset - packetBuffer._info->l3Offset;

	if (packetBuffer._info->l4Type == L4Type::TCP && _offloads & Offload::CHECKSUM_TCP) {
		header.l4Type = NfbReplicatorHeader::L4Type::TCP;
		header.calculateL4Checksum = true;
	} else if (packetBuffer._info->l4Type == L4Type::UDP && _offloads & Offload::CHECKSUM_UDP) {
		header.l4Type = NfbReplicatorHeader::L4Type::UDP;
		header.calculateL4Checksum = true;
	} else if (
		packetBuffer._info->l4Type == L4Type::ICMPv6 && _offloads & Offload::CHECKSUM_ICMPV6) {
		header.l4Type = NfbReplicatorHeader::L4Type::ICMPv6;
		header.calculateL4Checksum = true;
	}
}

void NfbQueue::SendBurst(const PacketBuffer* burst)
{
	(void) burst;

	ndp_tx_burst_put(_txQueue.get());
	_isBufferInUse = false;

	_outputQueueStats.transmittedPackets += _lastBurstSize;
	_outputQueueStats.transmittedBytes += _lastBurstTotalPacketLen;
	_outputQueueStats.UpdateTime();
}

void NfbQueue::GetBuffers(size_t burstSize)
{
	while (ndp_tx_burst_get(_txQueue.get(), _txPacket.get(), burstSize) != burstSize)
		;
}

size_t NfbQueue::AlignBlockSize(size_t size)
{
	static constexpr size_t BlockAlignment = 8;

	if (size % BlockAlignment == 0) {
		return size;
	}

	const size_t offset = BlockAlignment - (size % BlockAlignment);
	return size + offset;
}

void NfbQueue::SetOffloads(Offloads offloads) noexcept
{
	_offloads = offloads;
}

} // namespace replay
