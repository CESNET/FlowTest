/**
 * @file
 * @author Samuel Luptak <xluptas00@stud.fit.vutbr.cz>
 * @brief Thread manager source file
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "threadManager.hpp"

#include "utils.hpp"
#include <algorithm>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <numa.h>
#include <numaif.h>
#include <pthread.h>
#include <sched.h>
#include <sys/sysinfo.h>

namespace replay {

void BindToCores(const std::vector<size_t>& coreIds)
{
	cpu_set_t cpuSet;
	CPU_ZERO(&cpuSet);
	for (size_t core : coreIds) {
		if (core >= CPU_SETSIZE) {
			throw std::runtime_error("Core index is out of range (CPU_SETSIZE)");
		}

		CPU_SET(core, &cpuSet);
	}

	pthread_t thisThread = pthread_self();
	if (pthread_setaffinity_np(thisThread, sizeof(cpuSet), &cpuSet) != 0) {
		throw std::runtime_error("pthread_setaffinity_np() has failed");
	}
}

void SetInitialAffinity(const std::set<size_t>& cores)
{
	if (cores.empty()) {
		return;
	}

	size_t numOfCores = get_nprocs();
	if (*cores.rbegin() >= numOfCores) {
		throw std::runtime_error("One or more cores specified are invalid!");
	}

	std::vector<size_t> vectorCores(cores.begin(), cores.end());
	BindToCores(vectorCores);
}

ThreadManager::ThreadManager(const std::set<size_t>& cpuSet, NumaNode numaNode, size_t queueCount)
	: _specifiedCores(cpuSet.begin(), cpuSet.end())
	, _numaNode(numaNode)
{
	if (_numaNode == std::nullopt) {
		_logger->info("OutputPlugin does not provide NUMA node.");
	} else {
		_logger->info("OutputPlugin does provide NUMA node.");
	}

	if (numa_available() < 0) {
		_logger->warn("numa_available() has failed. CPU affinity will not be configured.");
		_specifiedCores.clear();
		_numaNode = std::nullopt;
	}

	if (!_specifiedCores.empty() && queueCount > _specifiedCores.size()) {
		_logger->error(
			"The number of specified cores must be higher or equal than the number of queues.");
		throw std::runtime_error("ThreadManager::ThreadManager has failed");
	}

	if (!_specifiedCores.empty()) {
		VerifyCores();
	}

	if (_numaNode != std::nullopt) {
		_numaCores = GetNumaInfo(_numaNode.value());
	} else {
		_numaCores = GetAllCoresInfo();
	}
}

void ThreadManager::SetThreadAffinity(size_t queueId)
{
	if (!_specifiedCores.empty()) {
		if (queueId >= _specifiedCores.size()) {
			_logger->error(
				"Specified thread in SetThreadAffinity() is out of range of specified cores.");
			throw std::runtime_error("ThreadManager::SetThreadAffinity has failed");
		}

		std::vector<size_t> argVector {_specifiedCores[queueId]};
		BindToCores(argVector);
		return;
	}

	BindToCores(_numaCores);
}

void ThreadManager::ResetThreadAffinity()
{
	BindToCores(_numaCores);
}

std::vector<size_t> ThreadManager::GetAllCoresInfo()
{
	size_t numOfCores = get_nprocs();

	std::vector<size_t> result;
	result.reserve(numOfCores);
	for (size_t core = 0; core < numOfCores; core++) {
		result.push_back(core);
	}

	return result;
}

std::vector<size_t> ThreadManager::GetNumaInfo(size_t nodeId)
{
	std::vector<size_t> result;

	std::unique_ptr<struct bitmask, decltype(&numa_free_cpumask)> allOnNode(
		numa_allocate_cpumask(),
		&numa_free_cpumask);
	if (!allOnNode) {
		_logger->error("Cannot allocate the CPU mask. numa_allocate_cpumask() has failed.");
		throw std::runtime_error("ThreadManager::GetNumaInfo has failed");
	}
	if (numa_node_to_cpus(nodeId, allOnNode.get()) != 0) {
		_logger->error("NUMA node cannot be converted to CPU set. numa_node_to_cpus() has failed.");
		throw std::runtime_error("ThreadManager::GetNumaInfo has failed");
	}

	for (size_t core = 0; core < allOnNode.get()->size; core++) {
		if (numa_bitmask_isbitset(allOnNode.get(), core)) {
			result.push_back(core);
		}
	}

	return result;
}

void ThreadManager::VerifyCores()
{
	std::set<size_t> nodesToVerify;
	for (size_t core : _specifiedCores) {
		int node = numa_node_of_cpu(core);
		if (node < 0) {
			_logger->error("Core {} is non-existent.", core);
			throw std::runtime_error("ThreadManager::VerifyCores has failed");
		}

		nodesToVerify.insert(node);
	}

	if (nodesToVerify.size() != 1) {
		_logger->warn("Cores specified on different NUMA nodes. Performance might be affected.");
		return;
	}

	if (_numaNode != std::nullopt) {
		if (nodesToVerify.find(_numaNode.value()) == nodesToVerify.end()) {
			_logger->warn("Specified cores are on a different NUMA node than the NIC.");
			return;
		}
	}
}

} // namespace replay
