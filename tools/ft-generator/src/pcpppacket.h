/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief A wrapper class around pcpp::Packet
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <pcapplusplus/Packet.h>

#include <stdexcept>

namespace generator {

/**
 * @brief An exception indicating failure of a pcpp operation
 */
class PcppError : public std::runtime_error {
public:
	PcppError()
		: std::runtime_error("Pcpp operation failed, see error log message")
	{
	}
};

/**
 * @brief A wrapper around pcpp::Packet that indicates failure by throwing an exception instead of
 * returning false
 */
class PcppPacket : public pcpp::Packet {
public:
	/**
	 * Add a new layer as the last layer in the packet. This method gets a pointer to the new layer
	 * as a parameter and attaches it to the packet. Notice after calling this method the input
	 * layer is attached to the packet so every change you make in it affect the packet; Also it
	 * cannot be attached to other packets
	 * @param[in] newLayer A pointer to the new layer to be added to the packet
	 * @param[in] ownInPacket If true, Packet fully owns newLayer, including memory deletion upon
	 * destruct.  Default is false.
	 * @throw PcppError if an error occured (an appropriate error log message will be printed in
	 * such cases)
	 */
	void addLayer(pcpp::Layer* newLayer, bool ownInPacket = false);

	/**
	 * Insert a new layer after an existing layer in the packet. This method gets a pointer to the
	 * new layer as a parameter and attaches it to the packet. Notice after calling this method the
	 * input layer is attached to the packet so every change you make in it affect the packet; Also
	 * it cannot be attached to other packets
	 * @param[in] prevLayer A pointer to an existing layer in the packet which the new layer should
	 * followed by. If this layer isn't attached to a packet and error will be printed to log and
	 * false will be returned
	 * @param[in] newLayer A pointer to the new layer to be added to the packet
	 * @param[in] ownInPacket If true, Packet fully owns newLayer, including memory deletion upon
	 * destruct.  Default is false.
	 * @throw PcppError if an error occured (an appropriate error log message will be printed in
	 * such cases)
	 */
	void insertLayer(pcpp::Layer* prevLayer, pcpp::Layer* newLayer, bool ownInPacket = false);

	/**
	 * Remove an existing layer from the packet. The layer to removed is identified by its type
	 * (protocol). If the packet has multiple layers of the same type in the packet the user may
	 * specify the index of the layer to remove (the default index is 0 - remove the first layer of
	 * this type). If the layer was allocated during packet creation it will be deleted and any
	 * pointer to it will get invalid. However if the layer was allocated by the user and manually
	 * added to the packet it will simply get detached from the packet, meaning the pointer to it
	 * will stay valid and its data (that was removed from the packet) will be copied back to the
	 * layer. In that case it's the user's responsibility to delete the layer instance
	 * @param[in] layerType The layer type (protocol) to remove
	 * @param[in] index If there are multiple layers of the same type, indicate which instance to
	 * remove. The default value is 0, meaning remove the first layer of this type
	 * @throw PcppError if an error occured (an appropriate error log message will be printed in
	 * such cases)
	 */
	void removeLayer(pcpp::ProtocolType layerType, int index = 0);

	/**
	 * Remove the first layer in the packet. The layer will be deleted if it was allocated during
	 * packet creation, or detached if was allocated outside of the packet. Please refer to
	 * removeLayer() to get more info
	 * @throw PcppError if an error occured (an appropriate error log message will be printed in
	 * such cases)
	 */
	void removeFirstLayer();

	/**
	 * Remove the last layer in the packet. The layer will be deleted if it was allocated during
	 * packet creation, or detached if was allocated outside of the packet. Please refer to
	 * removeLayer() to get more info
	 * @throw PcppError if an error occured (an appropriate error log message will be printed in
	 * such cases)
	 */
	void removeLastLayer();

	/**
	 * Remove all layers that come after a certain layer. All layers removed will be deleted if they
	 * were allocated during packet creation or detached if were allocated outside of the packet,
	 * please refer to removeLayer() to get more info
	 * @param[in] layer A pointer to the layer to begin removing from. Please note this layer will
	 * not be removed, only the layers that come after it will be removed. Also, if removal of one
	 * layer failed, the method will return immediately and the following layers won't be deleted
	 * @throw PcppError if an error occured (an appropriate error log message will be printed in
	 * such cases)
	 */
	void removeAllLayersAfter(pcpp::Layer* layer);

	/**
	 * Detach a layer from the packet. Detaching means the layer instance will not be deleted, but
	 * rather separated from the packet - e.g it will be removed from the layer chain of the packet
	 * and its data will be copied from the packet buffer into an internal layer buffer. After a
	 * layer is detached, it can be added into another packet (but it's impossible to attach a layer
	 * to multiple packets at the same time). After layer is detached, it's the user's
	 * responsibility to delete it when it's not needed anymore
	 * @param[in] layer A pointer to the layer to detach
	 * @throw PcppError if an error occured (an appropriate error log message will be printed in
	 * such cases)
	 */
	void detachLayer(pcpp::Layer* layer);
};

} // namespace generator
