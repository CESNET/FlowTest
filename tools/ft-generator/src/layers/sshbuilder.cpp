
/**
 * @file
 * @author Jaroslav Renda <jaroslav.renda@cesnet.cz>
 * @brief SSH message builder
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "sshbuilder.h"
#include "../data/sshdata.h"
#include "../randomgenerator.h"
#include "ssh.h"
#include "sshconstants.h"

namespace generator {

Buffer SshBuilder::BuildIdString()
{
	auto& rnd = RandomGenerator::GetInstance();
	Buffer buf;
	buf.Append(rnd.RandomChoice(SSH_SOFTWARE_STRINGS));
	return buf;
}

Buffer SshBuilder::BuildEncryptedPackage(std::size_t payloadSize, SshSettings& set)
{
	auto& rnd = RandomGenerator::GetInstance();
	uint8_t macSize = set.macAlgorithm.second;

	uint32_t overhead = SSH_DEFAULT_SIZE_BYTES + macSize;
	if (payloadSize <= overhead) {
		payloadSize = overhead + SSH_KEX_BLOCK_SIZE;
	}

	uint32_t packetSize = payloadSize - overhead;

	Buffer outBuf;
	outBuf.Append<UInt32Be>(packetSize);
	outBuf.Append(rnd.RandomBytes(packetSize));
	outBuf.Append(rnd.RandomBytes(macSize));

	return outBuf;
}

} // namespace generator
