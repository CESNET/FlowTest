
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

// Append given string to given buffer with size info
static void AppendString(std::string& str, Buffer& buf)
{
	buf.Append<UInt32Be>(str.length());
	buf.Append(str);
}

// Calculate and append size header and padding to given buffer
// Expects the buffer to contain reserved space for the header
static void AppendPacketPadding(Buffer& buf)
{
	uint8_t paddingSize = SSH_KEX_BLOCK_SIZE - (buf.Length() % SSH_KEX_BLOCK_SIZE);
	if (paddingSize < SSH_MIN_PADDING_SIZE) {
		paddingSize += SSH_KEX_BLOCK_SIZE;
	}
	for (uint8_t i = 0; i < paddingSize; i++) {
		buf.Append<uint8_t>(0);
	}

	uint32_t packetSize = buf.Length() - SSH_DEFAULT_SIZE_BYTES;
	buf.WriteAt<UInt32Be>(0, packetSize);
	buf.WriteAt<uint8_t>(4, paddingSize);
}

Buffer SshBuilder::BuildIdString()
{
	auto& rnd = RandomGenerator::GetInstance();
	Buffer buf;
	buf.Append(rnd.RandomChoice(SSH_SOFTWARE_STRINGS));
	return buf;
}

// Build a string from given strings
static void BuildAlgorithmString(
	std::vector<std::string_view>& algs,
	const std::string_view& selected,
	std::string& out)
{
	for (const auto& alg : algs) {
		if (alg != selected) {
			out += ",";
			out += alg;
		}
	}
}

// Generate algortihm without the need to save them into settings
static void GenerateGenericAlgorithms(Buffer& buf, const std::vector<std::string>& database)
{
	auto& rnd = RandomGenerator::GetInstance();
	std::vector<std::string_view> chosenAlgorithms;
	std::string outString;

	// Insert algorithms
	chosenAlgorithms.assign(database.begin(), database.end());

	// Remove random amount of algorithms
	rnd.Shuffle(chosenAlgorithms);
	chosenAlgorithms.resize(
		chosenAlgorithms.size() - rnd.RandomUInt(0, chosenAlgorithms.size() - 1));

	outString += chosenAlgorithms.front();
	BuildAlgorithmString(chosenAlgorithms, chosenAlgorithms.front(), outString);

	// These algorithms are stored in two directions in BOTH the server and client message
	// The first one is client -> server, the second one is server -> client
	// For simplicity both directions contain the same algorithms
	AppendString(outString, buf);
	AppendString(outString, buf);
}

static void GenerateKexAlgs(Buffer& buf, SshSettings& set, const Direction& dir)
{
	auto& rnd = RandomGenerator::GetInstance();
	std::string outString;
	std::vector<std::string_view> chosenAlgorithms;
	// reserve space in the vector for all possible algorithms, to suppress push_back in loop err
	chosenAlgorithms.reserve(
		SSH_KEX_ALGORITHM_DATABASE.must.size() + SSH_KEX_ALGORITHM_DATABASE.should.size()
		+ SSH_KEX_ALGORITHM_DATABASE.may.size());

	// Insert all "should" and "may" algorithms
	for (const auto& alg : SSH_KEX_ALGORITHM_DATABASE.should) {
		chosenAlgorithms.emplace_back(alg.first);
	}
	for (const auto& alg : SSH_KEX_ALGORITHM_DATABASE.may) {
		chosenAlgorithms.emplace_back(alg.first);
	}
	// remove random number of algorithms (only the "must" algorithms are required)
	rnd.Shuffle(chosenAlgorithms);
	chosenAlgorithms.resize(chosenAlgorithms.size() - rnd.RandomUInt(0, chosenAlgorithms.size()));

	// Insert "must" algorithms
	for (const auto& alg : SSH_KEX_ALGORITHM_DATABASE.must) {
		chosenAlgorithms.emplace_back(alg.first);
	}

	// Build a string message from selected algorithms
	rnd.Shuffle(chosenAlgorithms);
	if (dir == Direction::Forward) {
		// client chooses the algorithm that will be used for generating the keys
		auto& selectedAlg = rnd.RandomChoice(SSH_KEX_ALGORITHM_DATABASE.available);
		set.kexAlgorithm = selectedAlg;
		outString += selectedAlg.first;
		BuildAlgorithmString(chosenAlgorithms, selectedAlg.first, outString);
	} else {
		outString += set.kexAlgorithm.first;
		BuildAlgorithmString(chosenAlgorithms, set.kexAlgorithm.first, outString);
	}

	AppendString(outString, buf);
}

static void GenerateServerHostKeyAlgs(Buffer& buf, SshSettings& set, const Direction& dir)
{
	auto& rnd = RandomGenerator::GetInstance();
	std::vector<std::string_view> algs;
	std::string algString;
	algs.reserve(SSH_PUB_KEY_ALGORITHM_DATABASE.size());

	// Insert all algorithms
	for (const auto& alg : SSH_PUB_KEY_ALGORITHM_DATABASE) {
		algs.emplace_back(alg.first);
	}

	// remove random amount of algorithms
	rnd.Shuffle(algs);
	algs.resize(algs.size() - rnd.RandomUInt(0, algs.size() - 1));

	// the key set is chosen by the size not the actual name of the algorithm so random one can be
	// chosen
	if (dir == Direction::Forward) {
		auto& selectedAlg = rnd.RandomChoice(SSH_PUB_KEY_ALGORITHM_DATABASE);
		set.pubKeyAlgorithm.first = selectedAlg.first;
		set.pubKeyAlgorithm.second = selectedAlg.second / 8;
		algString += selectedAlg.first;
		BuildAlgorithmString(algs, selectedAlg.first, algString);
	} else {
		algString += set.pubKeyAlgorithm.first;
		BuildAlgorithmString(algs, set.pubKeyAlgorithm.first, algString);
	}

	AppendString(algString, buf);
}

static void GenerateMacAlgs(Buffer& buf, SshSettings& set, const Direction& dir)
{
	auto& rnd = RandomGenerator::GetInstance();
	std::vector<std::string_view> algs;
	std::string algString;
	algs.reserve(SSH_MAC_ALGORITHM_DATABASE.size());

	// Insert all algorithms
	for (const auto& alg : SSH_MAC_ALGORITHM_DATABASE) {
		algs.emplace_back(alg.first);
	}

	// remove random amount of algorithms
	rnd.Shuffle(algs);
	algs.resize(algs.size() - rnd.RandomUInt(0, algs.size() - 1));

	if (dir == Direction::Forward) {
		auto& selectedAlg = rnd.RandomChoice(SSH_MAC_ALGORITHM_DATABASE);
		set.macAlgorithm.first = selectedAlg.first;
		set.macAlgorithm.second = selectedAlg.second;
		algString += selectedAlg.first;
		BuildAlgorithmString(algs, selectedAlg.first, algString);
	} else {
		algString += set.macAlgorithm.first;
		BuildAlgorithmString(algs, set.macAlgorithm.first, algString);
	}

	// These algorithms are stored in two directions in BOTH the server and client message
	// The first one is client -> server, the second one is server -> client
	// For simplicity both directions contain the same algorithms
	AppendString(algString, buf);
	AppendString(algString, buf);
}

Buffer SshBuilder::BuildKexInit(SshSettings& set, const Direction& dir)
{
	auto& rnd = RandomGenerator::GetInstance();
	Buffer buf;

	buf.Append<UInt32Be>(0); // reserve four bytes for packet size header
	buf.Append<uint8_t>(0); // reserve byte for padding size header

	buf.Append<uint8_t>(SSH_KEX_INIT_MSG_CODE);
	buf.Append(rnd.RandomBytes(SSH_COOKIE_BYTES_CNT));

	GenerateKexAlgs(buf, set, dir);
	GenerateServerHostKeyAlgs(buf, set, dir);
	GenerateGenericAlgorithms(buf, SSH_ENCRYPTION_ALGORITHM_DATABASE); // encryption
	GenerateMacAlgs(buf, set, dir);
	GenerateGenericAlgorithms(buf, SSH_COMPRESSION_ALGORITHM_DATABASE); // compression

	// Setting languages to zero as that seems to be common behavior
	buf.Append<uint32_t>(SSH_DEFAULT_LANGUAGE_CNT);
	buf.Append<uint32_t>(SSH_DEFAULT_LANGUAGE_CNT);

	// The number of packets sent before the exchange finishes, defaults to zero.
	// There is no other supported option than zero.
	buf.Append<uint8_t>(SSH_DEFAULT_FIRST_PACKET_CNT);

	// Reserved four bytes of zeros
	buf.Append<uint32_t>(0);

	AppendPacketPadding(buf);

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
