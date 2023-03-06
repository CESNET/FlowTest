/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Replicator strategies interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "ipAddress.hpp"
#include "macAddress.hpp"

#include <memory>

namespace replay {

/**
 * @brief Unit strategy virtual class
 *
 * @tparam T type of strategy
 */
template <typename T>
struct UnitStrategy {
	/**
	 * @brief Apply strategy to given type
	 */
	virtual void ApplyStrategy(T type) = 0;

	/**
	 * @brief Virtual destructor
	 */
	virtual ~UnitStrategy() {}
};

/**
 * @brief Loop strategy virtual class
 */
struct LoopStrategy {
	/**
	 * @brief Apply strategy to IP layer
	 *
	 * @param ipAddress Packet Ip address structure to modify
	 * @param loopId Current ID of replication loop
	 */
	virtual void ApplyStrategy(IpAddressView ipAddress, size_t loopId) = 0;

	/**
	 * @brief Virtual destructor
	 */
	virtual ~LoopStrategy() {}
};

/**
 * @brief Default unit strategy - Do nothing
 */
template <typename T>
class UnitNoneDefault : public UnitStrategy<T> {
public:
	UnitNoneDefault() = default;

	/**
	 * @brief Apply default strategy - No operation
	 */
	void ApplyStrategy([[maybe_unused]] T type) override {}
};

/**
 * @brief Default loop strategy - Do nothing
 */
class LoopNoneDefault : public LoopStrategy {
public:
	LoopNoneDefault() = default;
	/**
	 * @brief Apply default strategy - No operation
	 */
	void ApplyStrategy([[maybe_unused]] IpAddressView ip, [[maybe_unused]] size_t loopId) override
	{
	}
};

/**
 * @brief Add constant value to the current value of IP address
 */
class UnitIpAddConstant : public UnitStrategy<IpAddressView> {
public:
	/**
	 * @brief Construct a new Unit Ip Add Constant object
	 *
	 * @param constant A constant that is added to the IP value
	 */
	UnitIpAddConstant(uint32_t constant);

	/**
	 * @brief Add constant to the IP
	 *
	 * @param ip IP address to nodify
	 */
	void ApplyStrategy(IpAddressView ip) override;

private:
	uint32_t _constant;
};

/**
 * @brief Add counter value to the current value of IP address
 */
class UnitIpAddCounter : public UnitStrategy<IpAddressView> {
public:
	/**
	 * @brief Construct a new Unit Ip Add Counter
	 *
	 * IpAddress += start + (step * iteration)
	 *
	 * iteration is incremented with every call of ApplyStrategy function
	 *
	 * @param start Initialization counter value
	 * @param step Value added to the previous iteration
	 */
	UnitIpAddCounter(uint32_t start, uint32_t step);

	/**
	 * @brief Add counter value to the Ip address
	 *
	 * IpAddress += start + (step * iteration)
	 *
	 * @param ip IP address to modify
	 */
	void ApplyStrategy(IpAddressView ip) override;

private:
	uint32_t _counter;
	uint32_t _step;
};

/**
 * @brief Set MAC address
 */
class UnitMacSetAddress : public UnitStrategy<MacAddress> {
public:
	/**
	 * @brief Construct a new Unit Mac Set Address object
	 *
	 * @param mac Mac address to set
	 */
	UnitMacSetAddress(const MacAddress& mac);

	/**
	 * @brief Set MAC address
	 *
	 * @param mac packet MAC address to modify
	 */
	void ApplyStrategy(MacAddress mac) override;

private:
	MacAddress _macAddress;
};

/**
 * @brief Add offset (value) to the current value of IP address
 *
 * Same loop strategy is applicated to every given packet until the loopId is changed.
 */
class LoopIpAddOffset : public LoopStrategy {
public:
	LoopIpAddOffset(uint32_t offset);
	void ApplyStrategy(IpAddressView ip, size_t loopId) override;

private:
	uint32_t _offset;
};

/**
 * @brief Holds modifier strategies for the supported packet fields
 */
struct ModifierStrategies {
	using UnitIpStrategy = std::unique_ptr<UnitStrategy<IpAddressView>>;
	using UnitMacStrategy = std::unique_ptr<UnitStrategy<MacAddress>>;
	using LoopIpStrategy = std::unique_ptr<LoopStrategy>;

	UnitIpStrategy unitSrcIp = std::make_unique<UnitNoneDefault<IpAddressView>>();
	UnitIpStrategy unitDstIp = std::make_unique<UnitNoneDefault<IpAddressView>>();
	LoopIpStrategy loopSrcIp = std::make_unique<LoopNoneDefault>();
	LoopIpStrategy loopDstIp = std::make_unique<LoopNoneDefault>();
	UnitMacStrategy unitSrcMac = std::make_unique<UnitNoneDefault<MacAddress>>();
	UnitMacStrategy unitDstMac = std::make_unique<UnitNoneDefault<MacAddress>>();
};

} // namespace replay
