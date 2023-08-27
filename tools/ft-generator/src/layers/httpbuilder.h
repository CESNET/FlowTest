/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief HTTP message Builder
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <pcapplusplus/Layer.h>

namespace generator {

/**
 * @brief A builder for HTTP messages
 */
class HttpBuilder {
public:
	/**
	 * @brief Type of the HTTP message
	 */
	enum MessageType { Request, Response };

	/**
	 * @brief Create a HttpBuilder
	 *
	 * @param type  Request or Response
	 */
	HttpBuilder(MessageType type);

	/**
	 * @brief Set the URL of the HTTP message (Request only)
	 *
	 * @param url  The URL
	 */
	void SetUrl(const std::string& url);

	/**
	 * @brief Set the HTTP version
	 *
	 * @param version  The version (0.9, 1.0 or 1.1)
	 */
	void SetVersion(const std::string& version);

	/**
	 * @brief Set the HTTP method
	 *
	 * @param method  The method (GET, POST, ...)
	 */
	void SetMethod(const std::string& method);

	/**
	 * @brief Set the HTTP status (response only)
	 *
	 * @param status  The status, HTTP code + optional message, e.g. "200 OK" or just "200"
	 */
	void SetStatus(const std::string& status);

	/**
	 * @brief Set a header field. If the field is not present in the header yet, add it, otherwise
	 *        update its value.
	 *
	 * @param name  The name of the header field
	 * @param value  The value to set the field to
	 */
	void SetField(const std::string& name, const std::string& value);

	/**
	 * @brief Get the HTTP method
	 */
	const std::string& GetMethod() const { return _method; }

	/**
	 * @brief Get the HTTP status
	 */
	const std::string& GetStatus() const { return _status; }

	/**
	 * @brief Get the HTTP version
	 */
	const std::string& GetVersion() const { return _version; }

	/**
	 * @brief Get the URL
	 */
	const std::string& GetUrl() const { return _url; }

	/**
	 * @brief Calculate the Content-Length field value and assign it to the header.
	 *
	 * The Content-Length field specifies the length of only the body portion of
	 * the HTTP message, EXCLUDING the header. This method allows specifying
	 * the length of the full HTTP message INCLUDING the header, from which the
	 * correct Content-Length will be calculated and set.
	 *
	 * This is way more convenient for our use case.
	 */
	void CalcAndSetContentLength(uint64_t contentLengthWithHeader);

	/**
	 * @brief Get the length of the HTTP header built so far (including the end of header part)
	 */
	uint64_t GetHeaderLength() const;

	/**
	 * @brief Build a pcpp::Layer object that can be added to a pcpp::Packet
	 */
	std::unique_ptr<pcpp::Layer> ToLayer() const;

	/**
	 * @brief Remove a field from the header that was previously set
	 *
	 * If the field with the specified name is not found, nothing happens
	 *
	 * @param name  The name of the field to remove
	 */
	void RemoveField(const std::string& name);

	/**
	 * @brief Get the actual Content-Length calculated by the CalcAndSetContentLength method
	 *
	 * If the method wasn't called and the Content-Length was set by SetField instead,
	 * the return value of this function is undefined.
	 */
	uint64_t GetCalculatedContentLength() const;

	/**
	 * @brief Shuffle order of fields
	 *
	 * This is to allow us to generate fields in different orders as the order of fields in a HTTP
	 * message should not matter.
	 */
	void ShuffleFields();

	/**
	 * @brief Randomize capitalization of field names
	 *
	 * This is to allow us to generate fields with different field name capitalizations as field
	 * names should be considered case-insensitive.
	 */
	void RandomizeFieldNamesCapitalization();

private:
	MessageType _type;

	std::string _url;
	std::string _method;
	std::string _status;
	std::string _version;

	uint64_t _calculatedContentLength = 0;

	std::vector<std::pair<std::string, std::string>> _fields;

	std::string Build() const;
};

} // namespace generator
