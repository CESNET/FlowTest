/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Domain name generator singleton
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "domainnamegenerator.h"
#include "data/words.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace generator {

static const std::vector<std::string_view> TLDS
	= {".com", ".cz", ".sk", ".net", ".org", ".us", ".eu"};

static constexpr int DOMAIN_NAME_MIN_SIZE = 4;
static constexpr int DOMAIN_NAME_MAX_SIZE = 255;

DomainNameGenerator& DomainNameGenerator::GetInstance()
{
	static DomainNameGenerator instance;
	return instance;
}

std::string DomainNameGenerator::Generate(std::size_t length)
{
	// Check minimal and maximal length
	if (length < DOMAIN_NAME_MIN_SIZE || length > DOMAIN_NAME_MAX_SIZE) {
		throw std::logic_error(
			"domain length must be between " + std::to_string(DOMAIN_NAME_MIN_SIZE) + " and "
			+ std::to_string(DOMAIN_NAME_MAX_SIZE) + " inclusive");
	}

	// Special case for the minimum size to simplify the process further
	if (length == DOMAIN_NAME_MIN_SIZE) {
		// Technically ineffective, but it shouldn't matter here
		std::vector<std::string_view> eligibleTlds;
		for (const auto& tld : TLDS) {
			if (tld.size() <= DOMAIN_NAME_MIN_SIZE - 1) {
				eligibleTlds.push_back(tld);
			}
		}
		return _rng.RandomChoice(LOWERCASE_LETTERS) + std::string(_rng.RandomChoice(eligibleTlds));
	}

	std::size_t lengthSoFar = 0;

	// Choose random TLD
	std::string_view tld = _rng.RandomChoice(TLDS);
	lengthSoFar += tld.size();

	// Choose random words to make up the domain name
	std::vector<std::string_view> words;

	while (lengthSoFar < length) {
		std::size_t maxNextWordLen = length - lengthSoFar;
		if (words.size() >= 1) {
			maxNextWordLen -= 1; // Dash
		}

		std::string_view word;
		if (maxNextWordLen >= _maxWordLen) {
			word = _rng.RandomChoice(WORDS);
		} else if (maxNextWordLen >= _minWordLen) {
			word = WORDS[_rng.RandomUInt(
				_startIndexForWordLen[maxNextWordLen],
				_endIndexForWordLen[maxNextWordLen] - 1)];
		} else {
			break;
		}

		if (words.size() >= 1) {
			lengthSoFar += 1; // Dash
		}
		words.push_back(word);
		lengthSoFar += word.size();
	}

	// Create the domain name from the chosen words
	std::string domain;
	domain.reserve(length);

	// Join the words and add a dash if needed
	for (std::size_t i = 0; i < words.size(); i++) {
		if (i > 0) {
			domain.push_back('-');
		}
		domain += words[i];
	}

	// If we still don't have enough, just add random letters
	while (lengthSoFar < length) {
		domain += _rng.RandomChoice(LOWERCASE_LETTERS);
		lengthSoFar += 1;
	}

	// Finally add the TLD
	domain += tld;

	return domain;
}

DomainNameGenerator::DomainNameGenerator()
	: _minWordLen(WORDS.front().size())
	, _maxWordLen(WORDS.back().size())
	, _startIndexForWordLen(_maxWordLen + 1)
	, _endIndexForWordLen(_maxWordLen + 1)
{
	auto isSorted = [](std::string_view a, std::string_view b) { return a.size() < b.size(); };
	if (!std::is_sorted(WORDS.begin(), WORDS.end(), isSorted)) {
		throw std::logic_error("wordlist is expected to be sorted by word length, but is not");
	}

	_startIndexForWordLen[_minWordLen] = 0;
	_endIndexForWordLen[_maxWordLen] = WORDS.size();

	auto prevWordLen = WORDS.front().size();
	for (std::size_t i = 1; i < WORDS.size(); i++) {
		auto thisWordLen = WORDS[i].size();
		if (thisWordLen != prevWordLen) {
			_startIndexForWordLen[thisWordLen] = i;
			_endIndexForWordLen[prevWordLen] = i;
		}
		prevWordLen = thisWordLen;
	}
}

} // namespace generator
