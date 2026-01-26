/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0

#include <test/libyul/ssa/ShufflingTest.h>

#include "fmt/ranges.h"
#include "libyul/ControlFlowGraphTest.h"
#include "libyul/backends/evm/ssa/LivenessAnalysis.h"
#include "libyul/backends/evm/ssa/OperationForwardShuffler.h"
#include "libyul/backends/evm/ssa/Stack.h"
#include "range/v3/algorithm/find_if_not.hpp"
#include "range/v3/view/split.hpp"

#ifdef ISOLTEST
#include <boost/version.hpp>
#if (BOOST_VERSION < 108800)
#include <boost/process.hpp>
#else
#define BOOST_PROCESS_VERSION 1
#include <boost/process/v1/child.hpp>
#include <boost/process/v1/io.hpp>
#include <boost/process/v1/pipe.hpp>
#endif
#endif

using namespace solidity;
using namespace solidity::yul::ssa;
using namespace solidity::yul::ssa::test;

namespace
{
using Liveness = LivenessAnalysis::LivenessData;
using Slot = StackSlot;
using ValueId = SSACFG::ValueId;

/// Parse a value ID token like "v172", "phi109", "lit7"
/// Returns std::nullopt for "JUNK"
std::optional<ValueId> parseValueToken(std::string const& token)
{
	if (token == "JUNK")
		return std::nullopt;

	if (token.starts_with("v"))
	{
		size_t num = std::stoull(token.substr(1));
		return ValueId::makeVariable(num);
	}

	if (token.starts_with("phi"))
	{
		size_t num = std::stoull(token.substr(3));
		return ValueId::makePhi(num);
	}

	if (token.starts_with("lit"))
	{
		size_t num = std::stoull(token.substr(3));
		return ValueId::makeLiteral(num);
	}
	throw std::runtime_error("Unknown token: " + token);
}

/// Parse a string like "[v172, phi109, lit7, JUNK]" into Stack::Data
std::vector<Slot> parseStackData(std::string_view _input)
{
	std::vector<Slot> result;
	std::string input(_input);

	// Remove whitespace
	input.erase(std::ranges::remove_if(input, ::isspace).begin(), input.end());

	// Remove brackets
	if (!input.empty() && input.front() == '[')
		input.erase(input.begin());
	if (!input.empty() && input.back() == ']')
		input.pop_back();

	// Split by comma
	std::stringstream ss(input);
	std::string token;

	while (std::getline(ss, token, ','))
	{
		if (token.empty())
			continue;

		if (auto valueId = parseValueToken(token))
			result.push_back(Slot::makeValueID(*valueId));
		else
			result.push_back(Slot::makeJunk());
	}

	return result;
}

/// Parse liveness like "[phi109, phi150, v172]"
/// Returns Liveness with reference count 1 for each value
Liveness parseLiveness(std::string_view _input)
{
	std::vector<std::pair<ValueId, uint32_t>> liveCounts;
	std::string input(_input);

	// Remove whitespace
	input.erase(std::ranges::remove_if(input, ::isspace).begin(), input.end());

	// Remove brackets
	if (!input.empty() && input.front() == '[')
		input.erase(input.begin());
	if (!input.empty() && input.back() == ']')
		input.pop_back();

	// Split by comma
	std::stringstream ss(input);
	std::string token;

	while (std::getline(ss, token, ','))
	{
		if (token.empty())
			continue;

		auto valueId = parseValueToken(token);
		if (valueId)
			liveCounts.emplace_back(*valueId, 1);  // Default reference count of 1
	}

	return {liveCounts.begin(), liveCounts.end()};
}

struct StackManipulationCallbacks
{
	void swap(size_t _depth) const
	{
		if (hook)
			(*hook)(fmt::format("SWAP{}", _depth));
	}
	void dup(size_t const _depth) const
	{
		if (hook)
			(*hook)(fmt::format("DUP{}", _depth));
	}
	void push(Slot const& _slot) const
	{
		if (hook)
			(*hook)(fmt::format("PUSH {}", slotToString(_slot)));
	}
	void pop() const
	{
		if (hook)
			(*hook)("POP");
	}

	std::optional<std::function<void(std::string const&)>> hook = std::nullopt;
};

struct ShuffleTestInput
{
	std::optional<StackData> data;
	std::optional<StackData> args;
	std::optional<Liveness> liveness;
	std::optional<size_t> targetStackSize;

	bool valid() const
	{
		return data.has_value() && args.has_value() && liveness.has_value() && targetStackSize.has_value();
	}

	static std::string_view trim(std::string_view s)
	{
		s.remove_prefix(std::min(s.find_first_not_of(" \t\r\v\n"), s.size()));
		s.remove_suffix(std::min(s.size() - s.find_last_not_of(" \t\r\v\n") - 1, s.size()));
		return s;
	}

	static ShuffleTestInput parse(std::string_view _source)
	{
		ShuffleTestInput result;

		auto stripComment = [](std::string_view sv) -> std::string_view
		{
			auto const pos = sv.find("//");
			if (pos != std::string_view::npos)
				return sv.substr(0, pos);
			return sv;
		};

		for (auto&& lineRange: ranges::views::split(_source, '\n'))
		{
			auto lineBegin = ranges::begin(lineRange);
			auto lineEnd  = ranges::end(lineRange);
			if (lineBegin == lineEnd)
				continue;

			std::string_view line{&*lineBegin, static_cast<std::size_t>(ranges::distance(lineBegin, lineEnd))};
			line = trim(stripComment(line));
			if (line.empty())
				continue;

			auto const colonPos = line.find(':');
			if (colonPos == std::string_view::npos)
				continue;

			auto const key = trim(line.substr(0, colonPos));
			auto const value = trim(line.substr(colonPos + 1));

			if (key == "data")
				result.data = parseStackData(value);
			else if (key == "args")
				result.args = parseStackData(value);
			else if (key == "liveness")
				result.liveness = parseLiveness(value);
			else if (key == "targetStackSize")
				result.targetStackSize = std::stoull(std::string{value});
		}
		return result;
	}
};

using TestStack = Stack<StackManipulationCallbacks>;

class TraceRecorder {
	static constexpr size_t operationColumnWidth = 12;
	static constexpr size_t slotColumnWidth = 7;
	static constexpr char junkSymbol = '*';

public:
	TraceRecorder(std::ostream& _out, TestStack::Data _targetArgs, Liveness _targetTail, size_t _targetStackSize):
		m_out(_out),
		m_targetArgs(std::move(_targetArgs)),
		m_targetTail(std::move(_targetTail)),
		m_targetStackSize(_targetStackSize),
		m_targetTailSize(
			[&] {
				yulAssert(_targetStackSize >= m_targetArgs.size());
				return _targetStackSize - m_targetArgs.size();
			}()
		)
	{}

	void record(std::string const& _operation, TestStack::Data const& _stack)
	{
		m_entries.emplace_back(_operation, _stack);
	}

	~TraceRecorder()
	{
		if (m_entries.empty())
			return;

		size_t maxStackDepth = 0;
		for (const auto& [operation, stackAfter] : m_entries)
			maxStackDepth = std::max(maxStackDepth, stackAfter.size());

		if (maxStackDepth == 0)
			return;

		bool const hasExcess = maxStackDepth > m_targetStackSize;

		m_out << '\n';
		emitHeader(maxStackDepth, hasExcess);
		emitSeparatorLine(maxStackDepth, hasExcess);
		for (auto const& entry : m_entries)
			emitDataRow(entry, maxStackDepth, hasExcess);
		emitSeparatorLine(maxStackDepth, hasExcess);
		emitTargetRow(maxStackDepth, hasExcess);
	}

private:
	struct TraceEntry {
		std::string operation;
		TestStack::Data stackAfter;
	};

	std::ostream& m_out;
	std::vector<TraceEntry> m_entries;
	TestStack::Data const m_targetArgs;
	Liveness const m_targetTail;
	size_t const m_targetStackSize;
	size_t const m_targetTailSize;

	void emitSeparator(size_t const _index, bool const _hasExcess, char const _junction) const
	{
		if (_index == m_targetTailSize && !m_targetArgs.empty() && m_targetTailSize > 0)
			m_out << ' ' << _junction;
		else if (_hasExcess && _index == m_targetTailSize + m_targetArgs.size())
			m_out << ' ' << _junction;
	}

	void emitHeader(size_t const _maxStackDepth, bool const _hasExcess) const
	{
		m_out << fmt::format("{:>{}}", "", operationColumnWidth) << "|";
		for (size_t i = 0; i < _maxStackDepth; ++i)
		{
			emitSeparator(i, _hasExcess, '|');
			m_out << fmt::format("{:>{}}", i, slotColumnWidth);
		}
		m_out << "\n";
	}

	void emitSeparatorLine(size_t _maxStackDepth, bool const _hasExcess) const
	{
		m_out << fmt::format("{:>{}}", "", operationColumnWidth) << '+';
		for (size_t i = 0; i < _maxStackDepth; ++i)
		{
			emitSeparator(i, _hasExcess, '+');
			m_out << std::string(slotColumnWidth, '-');
		}
		m_out << '\n';
	}

	void emitDataRow(TraceEntry const& _entry, size_t _maxStackDepth, bool const _hasExcess) const
	{
		m_out << fmt::format("{:>{}}", _entry.operation, operationColumnWidth) << "|";
		for (size_t i = 0; i < _maxStackDepth; ++i)
		{
			emitSeparator(i, _hasExcess, '|');
			if (i < _entry.stackAfter.size())
			{
				auto const& slot = _entry.stackAfter[i];
				std::string slotStr = slot.isJunk()
					? std::string(1, junkSymbol)
					: solidity::yul::ssa::slotToString(slot);
				m_out << fmt::format("{:>{}}", slotStr, slotColumnWidth);
			}
			else
				m_out << std::string(slotColumnWidth, ' ');
		}
		m_out << '\n';
	}

	void emitTargetRow(size_t const _maxStackDepth, bool const _hasExcess) const
	{
		m_out << fmt::format("{:>{}}", "(target)", operationColumnWidth) << "|";

		// Print tail region with set notation
		if (m_targetTailSize > 0)
		{
			std::string tailSetStr;
			if (!m_targetTail.empty())
				tailSetStr = fmt::format(
				"{{{}}}",
					fmt::join(
						m_targetTail | ranges::views::keys | ranges::views::transform(
							[](auto const& id) { return solidity::yul::ssa::slotToString(Slot::makeValueID(id)); }
						),
					", ")
				);
			m_out << fmt::format("{:>{}}", tailSetStr, m_targetTailSize * slotColumnWidth);
		}

		// Args separator
		if (!m_targetArgs.empty() && m_targetTailSize > 0)
			m_out << " |";

		// Print args region
		for (auto const& slot : m_targetArgs)
		{
			std::string slotStr = slot.isJunk() ? std::string(1, junkSymbol) : solidity::yul::ssa::slotToString(slot);
			m_out << fmt::format("{:>{}}", slotStr, slotColumnWidth);
		}

		// Excess separator and region
		if (_hasExcess)
		{
			m_out << " |";
			size_t excessSize = _maxStackDepth - m_targetTailSize - m_targetArgs.size();
			m_out << std::string(excessSize * slotColumnWidth, ' ');
		}

		m_out << '\n';
	}
};
}

std::unique_ptr<frontend::test::TestCase> ShufflingTest::create(Config const& _config) {
	return std::make_unique<ShufflingTest>(_config.filename);
}

ShufflingTest::ShufflingTest(std::string const& _filename): TestCase(_filename)
{
	m_source = m_reader.source();
	auto dialectName = m_reader.stringSetting("dialect", "evm");
	soltestAssert(dialectName == "evm");
	m_expectation = m_reader.simpleExpectations();
}

ShufflingTest::TestResult ShufflingTest::run(std::ostream& _stream, std::string const& _linePrefix, bool const _formatted)
{
	auto const testConfig = ShuffleTestInput::parse(m_source);
	if (!testConfig.valid())
	{
		util::AnsiColorized(_stream, _formatted, {util::formatting::BOLD, util::formatting::RED})
			<< _linePrefix
			<< "Error parsing source."
			<< std::endl;
		return TestResult::FatalError;
	}

	std::ostringstream oss;
	{
		TraceRecorder trace(oss, *testConfig.args, *testConfig.liveness, *testConfig.targetStackSize);
		trace.record("(initial)", *testConfig.data);
		auto stackData = *testConfig.data;
		TestStack stack(stackData, {.hook = [&](std::string const& op)
		{
			trace.record(op, stackData);
		}});
		OperationForwardShuffler<StackManipulationCallbacks>::shuffle(
			stack,
			*testConfig.args,
			*testConfig.liveness,
			*testConfig.targetStackSize,
			false
		);
	}
	m_obtainedResult = oss.str();


	return checkResult(_stream, _linePrefix, _formatted);
}
