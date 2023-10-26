/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief DNS Message Builder
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "../buffer.h"

#include <variant>

namespace generator {

static constexpr int DNS_HEADER_SIZE = //
	sizeof(uint16_t) // Transaction ID
	+ sizeof(uint16_t) // Flags
	+ sizeof(uint16_t) // Questions
	+ sizeof(uint16_t) // Answer RRs
	+ sizeof(uint16_t) // Authority RRs
	+ sizeof(uint16_t); // Additional RRs

// Either as a pointer in case of a compressed domain name (a single uint16_t),
// or in uncompressed case one byte for length + one byte for ending
static constexpr int DNS_NAME_REQUIRED_BYTES = 2;

static constexpr int DNS_QUESTION_SIZE_EXCL_DOMAIN = //
	DNS_NAME_REQUIRED_BYTES // Domain name
	+ sizeof(uint16_t) // Type
	+ sizeof(uint16_t); // Class

static constexpr int DNS_ANSWER_REQUIRED_BYTES = //
	DNS_NAME_REQUIRED_BYTES // Domain name
	+ sizeof(uint16_t) // Type
	+ sizeof(uint16_t) // Class
	+ sizeof(uint32_t) // TTL
	+ sizeof(uint16_t); // RR Data length

static constexpr int DNS_CNAME_ANSWER_SIZE_EXCL_DOMAIN
	= DNS_ANSWER_REQUIRED_BYTES + DNS_NAME_REQUIRED_BYTES; // RR contains domain name
static constexpr int DNS_TXT_ANSWER_SIZE_EXCL_DATA
	= DNS_ANSWER_REQUIRED_BYTES + 1; // RR contains 1 byte for length + data
static constexpr int DNS_A_ANSWER_SIZE_EXCL_DOMAIN
	= DNS_ANSWER_REQUIRED_BYTES + 4; // RR contains IPv4 address
static constexpr int DNS_AAAA_ANSWER_SIZE_EXCL_DOMAIN
	= DNS_ANSWER_REQUIRED_BYTES + 16; // RR contains IPv6 address

static constexpr int DNS_MAX_TXT_DATA_LEN = 255;

static constexpr int DNS_MIN_DOMAIN_SIZE = 4;
static constexpr int DNS_MAX_DOMAIN_SIZE = 253;

static constexpr int DNS_MAX_LABEL_LEN = 63;

static constexpr uint16_t DNS_CLASS_INET = 1;

// Bits in domain name signifying that this value is an offset, not label length (see compression of
// domain names in DNS protocol)
static constexpr uint16_t DNS_NAME_POINTER_FLAG = 0xc000;

static constexpr uint16_t DNS_FLAGS_RESPONSE = 0x8000;
static constexpr uint16_t DNS_FLAGS_RECURSION_DESIRED = 0x0100;
static constexpr uint16_t DNS_FLAGS_RECURSION_AVAILABLE = 0x0080;

static constexpr int DNS_OPT_RR_REQUIRED_BYTES = //
	1 // root domain name
	+ sizeof(uint16_t) // Type
	+ sizeof(uint16_t) // Class
	+ sizeof(uint32_t) // TTL
	+ sizeof(uint16_t); // RR Data length

enum class DnsRRType : uint16_t {
	A = 1,
	CNAME = 5,
	MX = 15,
	TXT = 16,
	AAAA = 28,
	OPT = 41,
};

/**
 * @brief Helper class for building DNS messages
 */
class DnsBuilder : public Buffer {
public:
	/**
	 * @brief Append DNS header
	 *
	 * @param transactionId The transaction ID
	 * @param flags The flags
	 * @param numQuestions Number of questions
	 * @param numAnswers Number of answers
	 * @param numAuthorityRRs Number of authority RRs
	 * @param numAdditionalRRs Number of additional RRs
	 */
	void AppendDnsHeader(
		uint16_t transactionId,
		uint16_t flags,
		uint16_t numQuestions,
		uint16_t numAnswers,
		uint16_t numAuthorityRRs = 0,
		uint16_t numAdditionalRRs = 0);

	/**
	 * @brief Append a DNS question
	 *
	 * @param domainName The domain name
	 * @param type The RR type
	 */
	void AppendDnsQuestion(const std::string& domainName, DnsRRType type);

	/**
	 * @brief Append A record
	 *
	 * @param domainNameOrOffset The domain name or offset in case of compression
	 * @param ttl The TTL
	 * @param ip The IP address
	 */
	void AppendDnsARecord(
		std::variant<const std::string*, uint16_t> domainNameOrOffset,
		uint32_t ttl,
		const std::vector<uint8_t>& ip);

	/**
	 * @brief Append AAAA record
	 *
	 * @param domainNameOrOffset The domain name or offset in case of compression
	 * @param ttl The TTL
	 * @param ip The IP address
	 */
	void AppendDnsAAAARecord(
		std::variant<const std::string*, uint16_t> domainNameOrOffset,
		uint32_t ttl,
		const std::vector<uint8_t>& ip);

	/**
	 * @brief Append CNAME record
	 *
	 * @param domainNameOrOffset The domain name or offset in case of compression
	 * @param ttl The TTL
	 * @param cname The CNAME
	 */
	void AppendDnsCNAMERecord(
		std::variant<const std::string*, uint16_t> domainNameOrOffset,
		uint32_t ttl,
		const std::string& cname);

	/**
	 * @brief Append TXT record
	 *
	 * @param domainNameOrOffset The domain name or offset in case of compression
	 * @param ttl The TTL
	 * @param txtData The TXT data
	 */
	void AppendDnsTXTRecord(
		std::variant<const std::string*, uint16_t> domainNameOrOffset,
		uint32_t ttl,
		const std::string& txtData);

	/**
	 * @brief Append OPT record
	 *
	 * @param udpPayloadSize The maximum UDP payload size the server can send
	 * @param flags The flags
	 */
	void AppendDnsOPTRecord(uint16_t udpPayloadSize, uint32_t flags);

private:
	void AppendDnsName(const std::string& name);

	void AppendDnsRRHeader(
		std::variant<const std::string*, uint16_t> domainNameOrOffset,
		DnsRRType type,
		uint32_t ttl,
		uint16_t size);
};

} // namespace generator
