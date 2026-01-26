#include <libyul/backends/evm/ssa/Shuffler.h>

#include "range/v3/algorithm/count.hpp"
#include "range/v3/view/enumerate.hpp"

using namespace solidity::yul::ssa;
using namespace solidity::yul::ssa::detail;

Target::Target(StackData const& _args, LivenessAnalysis::LivenessData const& _liveOut, std::size_t const _targetSize):
	args(_args),
	liveOut(_liveOut),
	size(_targetSize),
	tailSize(_targetSize - _args.size())
{
	minCount.reserve(_args.size() + _liveOut.size());
	for (auto const& arg: _args)
		if (!arg.isJunk())
			++minCount[arg];
	for (auto const& _liveValueId: _liveOut | ranges::views::keys)
		++minCount[StackSlot::makeValueID(_liveValueId)];
}

State::State(StackData const& _stackData, Target const& _target, std::size_t const _reachableStackDepth):
	m_stackData(_stackData),
	m_target(_target),
	m_reachableStackDepth(_reachableStackDepth)
{
	m_histogram.reserve(_stackData.size());
	m_histogramReachable.reserve(_stackData.size());
	m_histogramTail.reserve(_stackData.size());
	m_histogramArgs.reserve(_target.args.size());
	for (auto const& [i, slot]: _stackData | ranges::views::enumerate)
	{
		++m_histogram[slot];
		if (i < _target.tailSize)
			++m_histogramTail[slot];
		else
			// if the slot points to a junk slot in the target, it is already 'used up' in this iteration so we don't mark it as such
			// targetSize = argsSize + tailSize
			if (i >= _target.tailSize || !_target.args[i - _target.tailSize].isJunk())
				++m_histogramArgs[slot];
		if (_stackData.size() - i - 1 < _reachableStackDepth)
			++m_histogramReachable[slot];
	}
}

std::size_t State::size() const
{
	return m_stackData.size();
}

std::size_t State::count(StackSlot const& _slot) const
{
	return util::valueOrDefault(m_histogram, _slot, static_cast<size_t>(0));
}

std::size_t State::countInArgs(StackSlot const& _slot) const
{
	return util::valueOrDefault(m_histogramArgs, _slot, static_cast<size_t>(0));
}

std::size_t State::countInTail(StackSlot const& _slot) const
{
	return util::valueOrDefault(m_histogramTail, _slot, static_cast<size_t>(0));
}

std::size_t State::countReachable(StackSlot const& _slot) const
{
	return util::valueOrDefault(m_histogramReachable, _slot, static_cast<size_t>(0));
}

std::size_t State::targetMinCount(StackSlot const& _slot) const
{
	return util::valueOrDefault(m_target.minCount, _slot, size_t{0});
}

std::size_t State::targetArgsCount(StackSlot const& _slot) const
{
	return static_cast<size_t>(ranges::count(m_target.args, _slot));
}

bool State::argsRegionIsCorrect() const
{
	if (m_target.size != m_stackData.size())
		return false;

	for (size_t i = 0; i < m_target.args.size(); ++i)
		if (!isArgsCompatible(StackOffset{m_stackData.size() - i - 1}, StackOffset{m_stackData.size() - i - 1}))
			return false;

	return true;
}

bool State::distributionIsCorrect() const
{
	for (auto const& [targetSlot, targetMinCount]: m_target.minCount)
		if (count(targetSlot) < targetMinCount)
			return false;
	return true;
}

bool State::admissible() const
{
	return argsRegionIsCorrect() && distributionIsCorrect();
}

bool State::requiredInArgs(StackSlot const& _slot) const
{
	return ranges::find(m_target.args, _slot) != ranges::end(m_target.args);
}

bool State::requiredInTail(StackSlot const& _slot) const
{
	return _slot.isValueID() && m_target.liveOut.contains(_slot.valueID());
}

bool State::canBePopped(StackSlot const& _slot) const
{
	bool enoughQuantity = count(_slot) > targetMinCount(_slot);
	return (!requiredInArgs(_slot) && enoughQuantity) || (requiredInArgs(_slot) && countReachable(_slot) > 0); // todo  || stack.canBeFreelyGenerated(_slot)?
}

bool State::offsetInTargetArgsRegion(StackOffset _offset) const
{
	return _offset.value >= m_target.size - m_target.args.size() && _offset.value < m_target.size;
}

StackSlot const& State::targetArg(StackOffset _targetOffset) const
{
	return m_target.args[_targetOffset.value - m_target.tailSize];
}

bool State::isArgsCompatible(StackOffset _sourceOffset, StackOffset _targetOffset) const
{
	if (_sourceOffset >= m_stackData.size() || !offsetInTargetArgsRegion(_targetOffset))
		return false;
	auto const& arg = targetArg(_targetOffset);
	return arg.isJunk() || m_stackData[_sourceOffset.value] == arg;
}

bool State::targetArbitrary(StackOffset _targetOffset) const
{
	return targetArg(_targetOffset).isJunk();
}

bool State::isSourceCompatible(StackOffset const& _sourceOffset1, StackOffset const& _sourceOffset2) const
{
	return _sourceOffset1 < m_stackData.size() &&
		_sourceOffset2 < m_stackData.size() &&
		m_stackData[_sourceOffset1.value] == m_stackData[_sourceOffset2.value];
}

Target const& State::target() const
{
	return m_target;
}
