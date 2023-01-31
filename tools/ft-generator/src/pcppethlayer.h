/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief A wrapper class around pcpp::EthLayer
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <pcapplusplus/EthLayer.h>

namespace generator {

/**
 * @brief A customized pcpp::EthLayer
 *
 * This is needed because the original pcpp::EthLayer's computeCalculateFields
 * overwrites the etherType, yet only supports a very limited set of layers, so
 * we set the correct etherType ourselves instead of relying on the
 * computeCalculateFields method to do so automatically.
 */
class PcppEthLayer : public pcpp::EthLayer {
public:
	using pcpp::EthLayer::EthLayer;

	/**
	 * @brief Override computeCalculateFields of pcpp::EthLayer to do nothing
	 */
	void computeCalculateFields() override;
};

} // namespace generator
