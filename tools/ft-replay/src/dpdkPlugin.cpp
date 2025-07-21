/**
 * @file
 * @author Samuel Luptak <xluptas00@stud.fit.vutbr.cz>
 * @brief Dpdk plugin implementation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "dpdkPlugin.hpp"

#include "CStringArray/cStringArray.hpp"
#include "dpdkQueue.hpp"
#include "outputPluginFactoryRegistrator.hpp"
#include "utils.hpp"
#include <cinttypes>
#include <cstdint>
#include <iostream>
#include <memory>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <stdexcept>
#include <string>

namespace replay {

// register the dpdk plugin to the factory
OutputPluginFactoryRegistrator<DpdkPlugin> dpdkPluginRegistration("dpdk");

DpdkPlugin::DpdkPlugin(const std::string& params)
{
	DpdkQueueParams queueParams;

	int ret;
	ParseArguments(params);

	CStringArray array;
	FillDpdkArgs(array);

	ret = rte_eal_init(array.GetSize(), array.GetData());
	if (ret < 0) {
		_logger->error("rte_eal_init() has failed: {}", ret);
		throw std::runtime_error("DpdkPlugin::DpdkPlugin() has failed");
	}

	for (std::string& pciAddress : _PCIAddresses) {
		ret = ConfigureDpdkPort(pciAddress);
		_ports.push_back(ret);
		queueParams.ports.push_back(ret);
	}

	for (uint16_t portId : _ports) {
		if (rte_eth_dev_start(portId) != 0) {
			_logger->error("rte_eth_dev_start() has failed");
			throw std::runtime_error("DpdkPlugin::DpdkPlugin() has failed");
		}
	}

	queueParams.MTUSize = _MTUSize;
	queueParams.poolSize = _memPoolSize;
	queueParams.burstSize = _burstSize;

	for (size_t queueId = 0; queueId < _queueCount; queueId++) {
		queueParams.queueId = queueId;
		_queues.emplace_back(std::make_unique<DpdkQueue>(queueParams));
	}
}

size_t DpdkPlugin::GetQueueCount() const noexcept
{
	return _queueCount;
}

size_t DpdkPlugin::GetMTU() const noexcept
{
	return _MTUSize;
}

OutputQueue* DpdkPlugin::GetQueue(uint16_t queueId)
{
	return _queues.at(queueId).get();
}

DpdkPlugin::~DpdkPlugin()
{
	for (uint16_t port : _ports) {
		if (rte_eth_dev_stop(port) != 0)
			_logger->error("rte_eth_dev_stop() has failed");

		if (rte_eth_dev_close(port) != 0)
			_logger->error("rte_eth_dev_close() has failed");
	}

	_queues.clear();

	if (rte_eal_cleanup() != 0)
		_logger->error("rte_eal_cleanup() has failed");
}

int DpdkPlugin::ConfigureDpdkPort(const std::string& PCIAddress)
{
	auto throwErr = []() { throw std::runtime_error("DpdkPlugin::ConfigurePort() has failed"); };
	int ret;

	uint16_t portId;
	ret = rte_eth_dev_get_port_by_name(PCIAddress.c_str(), &portId);
	if (ret < 0) {
		_logger->error(
			"rte_eth_dev_get_port_by_name() has failed with code {} on PCIe {}",
			ret,
			PCIAddress.c_str());
		throwErr();
	}

	if (rte_eth_dev_is_valid_port(portId) != 1) {
		_logger->error("rte_eth_dev_is_valid_port() has failed");
		throwErr();
	}

	struct rte_eth_dev_info devInfo;
	ret = rte_eth_dev_info_get(portId, &devInfo);
	if (ret < 0) {
		_logger->error("rte_eth_dev_info() has failed with code {}", ret);
		throwErr();
	}

	if (_MTUSize < devInfo.min_mtu || _MTUSize > devInfo.max_mtu) {
		_logger->error(
			"MTU size of out NIC supported range {}-{}",
			devInfo.min_mtu,
			devInfo.max_mtu);
		throwErr();
	}

	if (_queueCount > devInfo.max_tx_queues) {
		_logger->error("Queue count of out range. max: {}", devInfo.max_tx_queues);
		throwErr();
	}

	struct rte_eth_conf portConfig;
	std::memset(&portConfig, 0, sizeof(portConfig));
	portConfig.txmode.offloads |= RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
	ret = rte_eth_dev_configure(portId, 0, _queueCount, &portConfig);
	if (ret < 0) {
		_logger->error("rte_eth_dev_configure() has failed with code {}", ret);
		throwErr();
	}

	for (size_t queueId = 0; queueId < _queueCount; queueId++) {
		if (rte_eth_tx_queue_setup(portId, queueId, _queueSize, rte_eth_dev_socket_id(portId), NULL)
			< 0) {
			_logger->error("rte_eth_tx_queue_setup() for queue {} has failed", queueId);
			throwErr();
		}
	}

	ret = rte_eth_dev_set_mtu(portId, _MTUSize);
	if (ret < 0) {
		_logger->error("rte_eth_dev_set_mtu() has failed with code {}", ret);
		throwErr();
	}

	return portId;
}

void DpdkPlugin::FillDpdkArgs(CStringArray& array)
{
	array.Push("dummyArg");
	for (std::string& pciAddress : _PCIAddresses) {
		array.Push("-a");
		array.Push(pciAddress);
	}
	array.Push("--lcores");
	array.Push("0@(0-" + std::to_string(_queueCount) + ")");
}

void DpdkPlugin::ParseAddr(const std::string& value)
{
	auto ret = value.find('#');
	if (ret == std::string::npos) {
		_PCIAddresses.push_back(value);
	} else {
		_PCIAddresses.push_back(value.substr(0, ret));
		_PCIAddresses.push_back(value.substr(ret + 1));
	}
}

void DpdkPlugin::ParseMap(const std::map<std::string, std::string>& argMap)
{
	for (const auto& [key, value] : argMap) {
		if (key == "addr") {
			ParseAddr(value);
		} else if (key == "poolSize") {
			utils::FromString<size_t>(value, key, _memPoolSize);
		} else if (key == "queueCount") {
			utils::FromString<size_t>(value, key, _queueCount);
		} else if (key == "queueSize") {
			utils::FromString<size_t>(value, key, _queueSize);
		} else if (key == "burstSize") {
			utils::FromString<size_t>(value, key, _burstSize);
		} else if (key == "MTU") {
			utils::FromString<size_t>(value, key, _MTUSize);
		} else {
			_logger->error("Unknown parameter {}", key);
			throw std::runtime_error("DpdkPlugin::ParseMap() has failed");
		}
	}
}

int DpdkPlugin::ParseArguments(const std::string& args)
{
	std::map<std::string, std::string> argMap = SplitArguments(args);

	ParseMap(argMap);

	if (_PCIAddresses.size() == 0) {
		_logger->error("Required parameter \"addr\" missing/empty");
		throw std::invalid_argument("DpdkPlugin::ParseArguments() has failed");
	}

	return argMap.size();
}

} // namespace replay
