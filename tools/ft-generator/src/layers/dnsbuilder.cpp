/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief DNS Message Builder
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "dnsbuilder.h"

#include <cassert>

namespace generator {

void DnsBuilder::AppendDnsName(const std::string& name)
{
	std::size_t pos = 0;
	while (pos < name.size()) {
		std::size_t dotPos = name.find('.', pos);
		if (dotPos == std::string::npos) {
			dotPos = name.size();
		}
		std::size_t labelLen = dotPos - pos;
		assert(labelLen <= DNS_MAX_LABEL_LEN);
		Append<uint8_t>(labelLen);
		Append(name.data() + pos, labelLen);
		pos = dotPos + 1;
	}
	Append<uint8_t>(0);
}

void DnsBuilder::AppendDnsHeader(
	uint16_t transactionId,
	uint16_t flags,
	uint16_t numQuestions,
	uint16_t numAnswers,
	uint16_t numAuthorityRRs,
	uint16_t numAdditionalRRs)
{
	// Transaction ID
	Append<uint16_t>(htobe16(transactionId));
	// Flags
	Append<uint16_t>(htobe16(flags));
	// Questions
	Append<uint16_t>(htobe16(numQuestions));
	// Answer RRs
	Append<uint16_t>(htobe16(numAnswers));
	// Authority RRs
	Append<uint16_t>(htobe16(numAuthorityRRs));
	// Additional RRs
	Append<uint16_t>(htobe16(numAdditionalRRs));
}

void DnsBuilder::AppendDnsQuestion(const std::string& domainName, DnsRRType type)
{
	// Name
	AppendDnsName(domainName);
	// Type
	Append<uint16_t>(htobe16(uint16_t(type)));
	// Class
	Append<uint16_t>(htobe16(DNS_CLASS_INET));
}

void DnsBuilder::AppendDnsRRHeader(
	std::variant<const std::string*, uint16_t> domainNameOrOffset,
	DnsRRType type,
	uint32_t ttl,
	uint16_t size)
{
	// Name
	if (auto* offset = std::get_if<uint16_t>(&domainNameOrOffset)) {
		Append<uint16_t>(htobe16(*offset | DNS_NAME_POINTER_FLAG));
	} else {
		AppendDnsName(*std::get<const std::string*>(domainNameOrOffset));
	}
	// Type
	Append<uint16_t>(htobe16(uint16_t(type)));
	// Class
	Append<uint16_t>(htobe16(DNS_CLASS_INET));
	// TTL
	Append<uint32_t>(htobe32(ttl));
	// Data length
	Append<uint16_t>(htobe16(size));
}

void DnsBuilder::AppendDnsOPTRecord(uint16_t udpPayloadSize, uint32_t flags)
{
	// MUST be 0 (root domain)
	Append<uint8_t>(0);
	// Type
	Append<uint16_t>(htobe16(uint16_t(DnsRRType::OPT)));
	// UDP payload size
	Append<uint16_t>(htobe16(udpPayloadSize));
	// RCODE/Flags
	Append<uint32_t>(htobe32(flags));
	// Data length
	Append<uint16_t>(htobe16(0));
}

void DnsBuilder::AppendDnsARecord(
	std::variant<const std::string*, uint16_t> domainNameOrOffset,
	uint32_t ttl,
	const std::vector<uint8_t>& ip)
{
	assert(ip.size() == 4);
	AppendDnsRRHeader(domainNameOrOffset, DnsRRType::A, ttl, 4);
	Append(reinterpret_cast<const char*>(ip.data()), 4);
}

void DnsBuilder::AppendDnsAAAARecord(
	std::variant<const std::string*, uint16_t> domainNameOrOffset,
	uint32_t ttl,
	const std::vector<uint8_t>& ip)
{
	assert(ip.size() == 16);
	AppendDnsRRHeader(domainNameOrOffset, DnsRRType::AAAA, ttl, 16);
	Append(reinterpret_cast<const char*>(ip.data()), 16);
}

void DnsBuilder::AppendDnsCNAMERecord(
	std::variant<const std::string*, uint16_t> domainNameOrOffset,
	uint32_t ttl,
	const std::string& cname)
{
	assert(cname.size() >= DNS_MIN_DOMAIN_SIZE && cname.size() <= DNS_MAX_DOMAIN_SIZE);

	// AppendDnsName will transform the domain name to a DNS format which will result in a payload
	// 2 bytes longer than the original string, which is why we add DNS_NAME_REQUIRED_BYTES to the
	// size
	AppendDnsRRHeader(
		domainNameOrOffset,
		DnsRRType::CNAME,
		ttl,
		cname.size() + DNS_NAME_REQUIRED_BYTES);
	AppendDnsName(cname);
}

void DnsBuilder::AppendDnsTXTRecord(
	std::variant<const std::string*, uint16_t> domainNameOrOffset,
	uint32_t ttl,
	const std::string& txtData)
{
	assert(txtData.size() >= 1 && txtData.size() <= DNS_MAX_TXT_DATA_LEN);
	AppendDnsRRHeader(
		domainNameOrOffset,
		DnsRRType::TXT,
		ttl,
		txtData.size() + 1); // 1 byte for TXT length
	Append<uint8_t>(txtData.size());
	Append(txtData.data(), txtData.size());
}

} // namespace generator
