#include <libyul/backends/evm/ssa/OperationForwardShuffler.h>

#include <boost/test/unit_test.hpp>

#include <sstream>
#include <string>
#include <vector>

namespace
{
using Liveness = solidity::yul::ssa::LivenessAnalysis::LivenessData;
using Slot = solidity::yul::ssa::StackSlot;
using ValueId = solidity::yul::ssa::SSACFG::ValueId;

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

	return Liveness(liveCounts.begin(), liveCounts.end());
}

struct StackManipulationCallbacks
{
	size_t numOps = 0;
	void swap(size_t _depth)
	{
		++numOps;
		auto op = fmt::format("SWAP{}", _depth);
		if (hook) (*hook)(op);
	}
	void dup(size_t _depth)
	{
		++numOps;
		auto op = fmt::format("DUP{}", _depth);
		if (hook) (*hook)(op);
	}
	void push(Slot const& _slot)
	{
		++numOps;
		auto op = fmt::format("PUSH {}", slotToString(_slot));
		if (hook) (*hook)(op);
	}
	void pop()
	{
		++numOps;
		std::string op = "POP";
		if (hook) (*hook)(op);
	}

	std::optional<std::function<void(std::string const&)>> hook = std::nullopt;
};
using Stack = solidity::yul::ssa::Stack<StackManipulationCallbacks>;

struct TraceEntry {
	std::string operation;
	Stack::Data stackAfter;
};
struct TraceRecorder {
	std::vector<TraceEntry> entries;
	~TraceRecorder()
	{
		size_t maxSlots = 0;
		for (auto const& entry : entries)
			maxSlots = std::max(maxSlots, entry.stackAfter.size());

		std::cout << "\n";
		// Header row with slot indices
		std::cout << fmt::format("{:>12}|", "");
		for (size_t i = 0; i < maxSlots; ++i)
			std::cout << fmt::format("{:>6}", i);
		std::cout << "\n";

		// Data rows
		for (auto const& entry : entries)
		{
			std::cout << fmt::format("{:>12}|", entry.operation);
			for (size_t i = 0; i < maxSlots; ++i)
			{
				if (i < entry.stackAfter.size())
				{
					auto const& slot = entry.stackAfter[i];
					std::string s = slot.isJunk() ? "*" : slotToString(slot);
					std::cout << fmt::format("{:>6}", s);
				}
				else
					std::cout << "      ";
			}
			std::cout << "\n";
		}
	}
};
}

namespace solidity::yul::test
{
BOOST_AUTO_TEST_SUITE(OperationForwardShufflerTest)

BOOST_AUTO_TEST_CASE(TestCycle)
{
	Stack::Data data = parseStackData("[v64, JUNK, v64, JUNK, v60, v74, JUNK, v60]");
	Stack::Data args = parseStackData("[v74, lit15]");
	Liveness liveness = parseLiveness("[v60, v64]");

	Stack stack(data, {});
	ssa::OperationForwardShuffler<StackManipulationCallbacks>::shuffle(stack, args, liveness, 7, false);
}

BOOST_AUTO_TEST_CASE(TestJunk)
{
	{
		/*// todo extremely inefficient atm
		Stack::Data data = parseStackData("[JUNK, JUNK, v56, v57, JUNK, JUNK]");
		Stack::Data args = parseStackData("[JUNK, JUNK, v56, v57, lit0, v56, lit11]");
		Liveness liveness = parseLiveness("");

		Stack stack(data, {.hook = [&]{ std::cout << " -> " << ssa::stackToString(data) << std::endl; }});
		ssa::OperationForwardShuffler<StackManipulationCallbacks>::shuffle(stack, args, liveness, args.size(), false);*/
	}
	/*{
		Stack::Data data = parseStackData("[JUNK, v44, v129, v43, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, v101, JUNK, v130, v131]");
		Stack::Data args = parseStackData("[JUNK, v44, JUNK, v43, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, v101, JUNK, v130, v131, lit0, v129]");
		Liveness liveness = parseLiveness("");

		Stack stack(data, {.hook = [&]{ std::cout << " -> " << ssa::stackToString(data) << std::endl; }});
		ssa::OperationForwardShuffler<StackManipulationCallbacks>::shuffle(stack, args, liveness, args.size(), false);
	}*/
	/*{
		Stack::Data data = parseStackData("[JUNK, v189, phi112, JUNK, v204, JUNK, JUNK, JUNK, JUNK, JUNK, v185, JUNK, v188, v190, v191, JUNK, v199, phi112, phi112, v205, v206]");
		Stack::Data args = parseStackData("[JUNK, v199, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, v185, JUNK, v188, v190, v191, JUNK, v189, JUNK, v206, v205, v204]");
		Liveness liveness;

		Stack stack(data, {});
		ssa::OperationForwardShuffler<StackManipulationCallbacks>::shuffle(stack, args, liveness, args.size(), false);
	}*/
	/*{
		Stack::Data data = parseStackData("[JUNK, v189, phi112, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, v185, phi115, v188, v190, v191, phi113, v199, phi112, v199, v202]");
		Stack::Data args = parseStackData("[v202]");
		Liveness liveness = parseLiveness("[phi112, phi113, phi115, v185, v188, v189, v190, v191, v199]");

		Stack stack(data, {.hook = [&]{ std::cout << " -> " << ssa::stackToString(data) << std::endl; }});
		ssa::OperationForwardShuffler<StackManipulationCallbacks>::shuffle(stack, args, liveness, 19, false);
	}*/
	{
		TraceRecorder trace;
		Stack::Data data = parseStackData("[JUNK, v12, phi9, phi13, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, v65, v67]");
		Stack::Data args = parseStackData("[lit27, phi13, phi9, v12, v67]");
		Liveness liveness = parseLiveness("[phi9, v12, phi13, v65]");

		trace.entries.push_back({"(initial)", data});
		Stack stack(data, {.hook = [&](std::string const& op){ trace.entries.push_back({op, data}); }});
		ssa::OperationForwardShuffler<StackManipulationCallbacks>::shuffle(stack, args, liveness, 21, false);
	}
	/*{
		Stack::Data data = parseStackData("[JUNK, v12, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, JUNK, v68, JUNK, phi111, v84, v86]");
		Stack::Data args = parseStackData("[v12, v86]");
		Liveness liveness = parseLiveness("[v68, v84, phi111]");

		Stack stack(data, {.hook = [&]{ std::cout << " -> " << ssa::stackToString(data) << std::endl; }});
		ssa::OperationForwardShuffler<StackManipulationCallbacks>::shuffle(stack, args, liveness, 20, false);
	}*/
	/*{
		// this ends in a cycle but also it does this swap15 which i dont think is great
		// instead we should swap up closer to the top if possible so the top region stays intact for the most part
		// (reduces entropy)
		Stack::Data data = parseStackData("[v6, v5, v4, v3, v2, JUNK, JUNK, v59, v60, JUNK, JUNK, JUNK, JUNK, v98, JUNK, JUNK, JUNK, v113, v114, v115, v116]");
		Stack::Data args = parseStackData("[v116, v5]");
		Liveness liveness = parseLiveness("[v2, v3, v4, v5, v6, v59, v60, v98, v113, v114, v115]");

		Stack stack(data, {.hook = [&]{ std::cout << " -> " << ssa::stackToString(data) << std::endl; }});
		ssa::OperationForwardShuffler<StackManipulationCallbacks>::shuffle(stack, args, liveness, 20, false);
	}*/
}

BOOST_AUTO_TEST_SUITE_END()
}
