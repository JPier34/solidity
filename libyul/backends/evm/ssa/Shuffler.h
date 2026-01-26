#pragma once

#include "range/v3/view/iota.hpp"


#include <libyul/backends/evm/ssa/LivenessAnalysis.h>
#include <libyul/backends/evm/ssa/Stack.h>

#include <boost/container/flat_map.hpp>
#include <cstddef>

namespace solidity::yul::ssa
{

namespace detail
{
struct Target
{
	Target(StackData const& _args, LivenessAnalysis::LivenessData const& _liveOut, std::size_t _targetSize);

	StackData const& args;
	LivenessAnalysis::LivenessData const& liveOut;
	std::size_t const size;
	std::size_t const tailSize;
	boost::container::flat_map<StackSlot, size_t> minCount;
};
class State
{
public:
	State(StackData const& _stackData, Target const& _target, std::size_t _reachableStackDepth);

	std::size_t size() const;
	std::size_t count(StackSlot const& _slot) const;
	std::size_t countInArgs(StackSlot const& _slot) const;
	std::size_t countInTail(StackSlot const& _slot) const;
	std::size_t countReachable(StackSlot const& _slot) const;

	std::size_t targetMinCount(StackSlot const& _slot) const;
	std::size_t targetArgsCount(StackSlot const& _slot) const;

	bool argsRegionIsCorrect() const;
	bool distributionIsCorrect() const;
	bool admissible() const;

	bool requiredInArgs(StackSlot const& _slot) const;
	bool requiredInTail(StackSlot const& _slot) const;

	bool canBePopped(StackSlot const& _slot) const;

	bool offsetInTargetArgsRegion(StackOffset _offset) const;
	StackSlot const& targetArg(StackOffset _targetOffset) const;
	bool isArgsCompatible(StackOffset _sourceOffset, StackOffset _targetOffset) const;
	bool targetArbitrary(StackOffset _targetOffset) const;
	bool isSourceCompatible(StackOffset const& _sourceOffset1, StackOffset const& _sourceOffset2) const;

	Target const& target() const;

	auto stackArgsRange() const
	{
		return ranges::views::iota(std::min(m_target.tailSize, m_stackData.size()), m_stackData.size()) | ranges::views::transform([](auto _i) { return StackOffset{_i}; });
	}

	auto stackTailRange() const
	{
		return ranges::views::iota(0u, std::min(m_target.tailSize, m_stackData.size())) | ranges::views::transform([](auto _i) { return StackOffset{_i}; });
	}

	auto stackRange() const
	{
		return ranges::views::iota(0u, m_stackData.size()) | ranges::views::transform([&](auto _i) { return StackOffset{_i}; });
	}

	auto stackSwapReachableRange() const
	{
		return ranges::views::iota(0u, std::min(m_stackData.size(), m_reachableStackDepth + 1)) | ranges::views::transform([&](auto _i) { return StackOffset{m_stackData.size() - _i - 1}; }) | ranges::views::reverse;
	}

private:
	StackData const& m_stackData;
	Target const& m_target;
	std::size_t const m_reachableStackDepth;
	boost::container::flat_map<StackSlot, size_t> m_histogramTail;
	boost::container::flat_map<StackSlot, size_t> m_histogramArgs;
	boost::container::flat_map<StackSlot, size_t> m_histogramReachable;
	boost::container::flat_map<StackSlot, size_t> m_histogram;
};
}

template<StackManipulationCallbackConcept Callback, std::size_t ReachableStackDepth=16>
class Shuffler
{
	using Slot = StackSlot;

public:
	static void shuffle(
		Stack<Callback>& _stack,
		StackData const& _args,
		LivenessAnalysis::LivenessData const& _liveOut,
		std::size_t _targetStackSize
	)
	{
		detail::Target const target(_args, _liveOut, _targetStackSize);
		yulAssert(_liveOut.size() <= target.size, "not enough tail space");
		{
			// check that all required values are on stack
			detail::State const state(_stack.data(), target, ReachableStackDepth);
			for (const auto& liveVariable: _liveOut | ranges::views::keys | ranges::views::transform(Slot::makeValueID))
				yulAssert(_stack.canBeFreelyGenerated(liveVariable) || state.count(liveVariable) > 0);
			for (const auto& arg: _args)
				yulAssert(_stack.canBeFreelyGenerated(arg) || state.count(arg) > 0);
		}

		constexpr std::size_t maxIterations = 1000;
		std::size_t i = 0;
		while (i < maxIterations)
		{
			detail::State const state(_stack.data(), target, ReachableStackDepth);
			if (!shuffleStep(_stack, state))
			{
				yulAssert(state.admissible());
				break;
			}
			++i;
		}

		yulAssert(i < maxIterations, fmt::format("Maximum iterations reached on {}", stackToString(_stack.data())));
	}

private:
	static bool shuffleStep(Stack<Callback>& _stack, detail::State const& _state)
	{
		if (_stack.size() > _state.target().size)
		{
			if(shrinkStack(_stack, _state))
				return true;
			yulAssert(false, "stack too deep");
		}
		yulAssert(_stack.size() <= _state.target().size, "I1 violated: Stack size too large");

		if (!allNecessarySlotsReachableOrFinal(_stack, _state))
		{
			// !allNecessarySlotsReachableOrFinal(ops) ≡ ¬(∀s: reachable(s) ∨ final(s)) ≡ ∃s: ¬reachable(s) ∧ ¬final(s)
			if (shrinkStack(_stack, _state))
				return true;

			// todo: in the future we'll want stack too deep handling here and
			//		 dup up the args if possible or mload them by explicitly calling _stack.reportStackTooDeep(arg)
			yulAssert(false, "stack too deep");
		}

		// if we need something in the tail, try swapping it down there, there must be a spot
		// that can be swapped out (although it might be unreachable in which case we'll try to fix args
		// and/or compress)
		if (fixTailSlot(_stack, _state))
			return true;

		// if the stack reaches into the args region try fixing a slot in there
		if (_stack.size() >= _state.target().tailSize && fixArgsSlot(_stack, _state))
			return true;

		// todo i don't really need this do i
		/*if (auto missingSlot = findMissingFreelyGeneratableLiveOutSlot(ops))
		{
			// Push missing freely-generatable liveOut slot (e.g., literal)
			if (!dupDeepSlotIfRequired(ops))
				_stack.push(*missingSlot);
		}*/

		{
			// todo
			// if there's something at the top of the stack that has to be popped anyways:
			//     - its often enough on stack to be popped (more than required)
			//	   - its not in the right position
			//     - we don't need it to fill the stack to the target size, ie, the num of required elements plus
			//       the stack deficit (what is still missing) overshoot target size
			//     - all below slots are also something that has to be popped or the tail end is finished
		}

		// todo i think this should be handled in fix tail slot
		if (_stack.size() < _state.target().tailSize)
		{
			// if something is on the verge of going out of scope by duping something, dup that first
			if (dupDeepSlotIfRequired(_stack, _state))
				return true;

			// dup up the deepest slot that needs to go into args so we avoid having to fish it back up later
			if (dupDeepestRelevantTailSlot(ops))
				return true;

			// Try to dup the optimal slot based on liveness analysis
			if (auto slotToDup = selectOptimalSlotToDup(ops))
			{
				if (!dupDeepSlotIfRequired(_stack, _state))
					_stack.dup(*slotToDup);
			}
			else
			{
				// If no suitable slot found, push junk
				if (!dupDeepSlotIfRequired(_stack, _state))
					_stack.push(Slot::makeJunk());
			}
			return true;
		}

		// we are now in a position that we only have to potentially dup up args and/or fix the existing args slots
		yulAssert(_state.target().tailSize <= _stack.size() && _stack.size() <= _state.target().size);

		// if there are no args, we should be done now
		if (_state.target().args.empty())
			return false;

		// of the existing args, can we improve the situation?
		if (fixArgsSlot(_stack, _state))
			return true;

		if (fixTailSlot(_stack, _state))
			return true;

		// dup up whatever is missing
		if (_stack.size() < _state.target().size)
		{
			if (dupDeepSlotIfRequired(_stack, _state))
				return true;

			{
				StackOffset const targetOffset{_stack.size()};
				if (_state.count(_state.targetArg(targetOffset)) < _state.targetMinCount(_state.targetArg(targetOffset)))
				{
					auto const sourceDepth = _stack.findSlotDepth(_state.targetArg(targetOffset));
					if (!sourceDepth)
					{
						_stack.push(_state.targetArg(targetOffset));
						return true;
					}

					if (!_stack.dupReachable(*sourceDepth))
						yulAssert(false, fmt::format("todo: stack too deep handling, couldn't dup up arg {}", slotToString(ops.targetArg(_stack.depthToOffset(*sourceDepth)))));
					_stack.dup(*sourceDepth);
					return true;
				}
			}

			// if we can't directly produce targetOffset, take the deepest arg that we don't have enough of and dup/push that
			// First, prioritize duping args that are on the stack over pushing freely-generatable ones
			for (StackOffset offset{_state.target().tailSize}; offset < _state.target().size; ++offset.value)
			{
				Slot const& arg = _state.targetArg(offset);
				if (!arg.isJunk() && (_state.count(arg) < _state.targetMinCount(arg) || _state.countInArgs(arg) < _state.targetArgsCount(arg)))
				{
					if (auto sourceDepth = _stack.findSlotDepth(arg))
					{
						if (_stack.dupReachable(*sourceDepth))
						{
							_stack.dup(*sourceDepth);
							return true;
						}
						yulAssert(false, "stack too deep handling");
					}
					yulAssert(_stack.canBeFreelyGenerated(arg));
					_stack.push(arg);
					return true;
				}
			}

			if (!dupDeepSlotIfRequired(_stack, _state))
			{
				// Try to dup the optimal slot based on liveness analysis
				if (auto slotToDup = selectOptimalSlotToDup(ops))
				{
					if (!dupDeepSlotIfRequired(_stack, _state))
						_stack.dup(*slotToDup);
				}
				else
				{
					// If no suitable slot found, push junk
					if (!dupDeepSlotIfRequired(_stack, _state))
						_stack.push(Slot::makeJunk());
				}
			}
			return true;
		}

		yulAssert(_stack.size() == _state.target().size);

		StackOffset stackTopOffset{_stack.size() - 1};

		if (fixArgsSlot(_stack, _state))
			return true;

		// If we find a lower slot that is out of position, but also compatible with the top, swap that up.
		for (StackOffset const offset: stackSwapReachableRange(_stack))
			if (
				!_state.isArgsCompatible(offset, offset) &&
				!_state.isSourceCompatible(offset, stackTopOffset) &&
				_state.isArgsCompatible(offset, stackTopOffset) &&
				_state.isArgsCompatible(stackTopOffset, offset) // &&
			)
			{
				_stack.swap(offset);
				return true;
			}

		// Swap up any reachable slot that is still out of position.
		for (StackOffset const offset: stackSwapReachableRange(_stack))
			if (_stack.offsetToDepth(offset) < _state.target().args.size())
			{
				if (
					_state.offsetInTargetArgsRegion(offset) &&
					!_state.isArgsCompatible(offset, offset) &&
					!_state.isSourceCompatible(offset, stackTopOffset) &&
					_state.requiredInArgs(_stack[offset]) &&
					_state.countInArgs(_stack[offset]) <= _state.targetArgsCount(_stack[offset])
				)
				{
					_stack.swap(offset);
					return true;
				}
			}
			else
			{
				if (
					_state.requiredInArgs(_stack[offset]) &&
					_state.countInArgs(_stack[offset]) < _state.targetArgsCount(_stack[offset]) &&
					!_state.isSourceCompatible(offset, stackTopOffset)
				)
				{
					_stack.swap(offset);
					return true;
				}
			}

		if (_state.admissible())
			return false;

		// We are in a stack-too-deep situation and try to reduce the stack size.
		if (shrinkStack(_stack, _state))
			return true;

		yulAssert(false, "reached final and forbidden state");
	}

	static bool dupDeepestRelevantTailSlot(Stack<Callback>& _stack, detail::State const& _state)
	{
		auto& stack = _ops.stack;

		// dup up the deepest slot that is required in args (or compress if unreachable)
		for (StackOffset offset: stackRange(_ops.stack))
		{
			// if we need the slot in args and there's no slot of the same kind further up
			if (
				_ops.requiredInArgs(_ops.stack[offset]) &&
				std::find(_ops.stack.begin() + offset.value + 1, _ops.stack.end(), _ops.stack[offset]) == _ops.stack.end()
			)
			{
				// dup if we can
				if (_ops.stack.dupReachable(offset))
				{
					_ops.stack.dup(offset);
					return true;
				}

				// try to compress
				if (shrinkStack(_ops.stack, _ops))
					return true;

				// todo stack too deep handling, the slot at offset is required in args but we can't reach it
				yulAssert(false);
			}
		}
		return false;
	}

	// If dupping an ideal slot causes a slot that will still be required to become unreachable, then dup
	// the latter slot first.
	// @returns true, if it performed a dup.
	static bool dupDeepSlotIfRequired(Stack<Callback>& _stack, detail::State const& _state)
	{
		// Check if the stack is large enough for anything to potentially become unreachable.
		if (_stack.size() < ReachableStackDepth - 1)
			return false;
		// Check whether any deep slot might still be needed later (i.e. we still need to reach it with a DUP or SWAP).
		for (StackOffset sourceOffset{0u}; sourceOffset < _stack.size() - (ReachableStackDepth - 1); ++sourceOffset.value)
		{
			// This slot needs to be moved into args and there is no tail slot of the same kind further up in the stack.
			auto const& slot = _stack[sourceOffset];
			// no need top dup deep junk
			if (slot.isJunk())
				continue;
			// check if we have more of the same slot further up in the stack
			bool const neededInArgs = _state.targetArgsCount(slot) > _state.countInArgs(slot);
			bool const needMore = _state.targetMinCount(slot) > _state.count(slot);
			if (neededInArgs || needMore)
			{
				// if we ever need more of a slot then this can only happen if it is something we require
				// in the arguments
				yulAssert(_state.requiredInArgs(slot));

				// todo why without args!? if it's there, it's there, that's fine
				auto const [haveMoreAboveWithoutArgs, haveMoreAbove] = [&]
				{
					for (StackOffset offset{sourceOffset.value + 1}; offset < _stack.size(); ++offset.value)
					{
						if (_stack[offset] == slot)
							return std::make_tuple(_stack.size() - offset.value - 1 >= _state.target().args.size(), true);
					}
					return std::make_tuple(false, false);
				}();

				// if we have more of the same further above, just unconditionally skip this one
				if (haveMoreAboveWithoutArgs)
					continue;

				// if we need this in args and we have the same above but outside args, or we can introduce junk and
				// there is more of the same further up in the stack, skip it
				if ((neededInArgs && haveMoreAboveWithoutArgs) || (haveMoreAbove))
					continue;

				if (_stack.dupReachable(sourceOffset))
				{
					// todo i don't think i need this honestly
					// If sourceOffset has the same value as top, skip - no point swapping (no-op) or duping (already at top)
					if (_stack[sourceOffset] == _stack.top())
						continue;

					if (
						!_state.isArgsCompatible(sourceOffset, sourceOffset) &&  // the offset isn't already in the right position wrt args
						(
							!_state.requiredInArgs(_stack.top()) || // current top can go into tail, ie it's not required as arg or
							_state.countReachable(_stack.top()) > 1 // there's more of it in reachable stack depth
						)
					)
					{
						// top can go into the tail bit, swap it down
						_stack.swap(sourceOffset);
						return true;
					}
					else
					{
						// we need more of the slot that is about to go out of reach, dup it
						_stack.dup(sourceOffset);
						return true;
					}
				}
				else
				{
					std::optional<StackDepth> depth = _stack.findSlotDepth(_stack[sourceOffset]);
					yulAssert(depth);
					// if there's a shallower slot with the same info that is reachable, skip this one
					if (*depth < _stack.offsetToDepth(sourceOffset))
						continue;

					// the slot we need something in the args region of is unreachable, try compressing the stack,
					// first looking at the top
					if (shrinkStack(_stack, _state))
						return true;

					yulAssert(false, "Stack too deep");
				}
			}
		}
		return false;
	}

	static bool fixArgsSlot(Stack<Callback>& _stack, detail::State const& _state)
	{
		yulAssert(_stack.size() <= _state.target().size, "this method assumes that the stack isn't too large");
		// todo the _stack.empty() check here is wrong
		if (_stack.size() <= _state.target().tailSize || _stack.empty())
			return false;

		StackOffset const stackTop{_stack.size() - 1};
		// if the stack top isn't where it likes to be right now, try to put it somewhere more sensible
		if (!_state.isArgsCompatible(stackTop, stackTop))
		{
			// if the stack top should go into the tail but isn't there yet and we have enough of it in args
			if (
				_state.requiredInTail(_stack[stackTop]) &&
				_state.countInTail(_stack[stackTop]) == 0 &&
				_state.countInArgs(_stack[stackTop]) > _state.targetArgsCount(_stack[stackTop])
			)
			{
				// try swapping it with something in the tail that also fixes the top
				for (StackOffset offset: _state.stackTailRange())
					if (_stack.swapReachable(offset) && _state.isArgsCompatible(offset, stackTop))
					{
						_stack.swap(offset);
						return true;
					}
				// otherwise try swapping it with something that needs to go into args
				for (StackOffset offset: _state.stackTailRange())
					if (_stack.swapReachable(offset) && _state.countInArgs(_stack[offset]) < _stack.targetArgsCount(_stack[offset]))
					{
						_stack.swap(offset);
						return true;
					}
				// otherwise try swapping it with something that can be popped
				for (StackOffset offset: _state.stackTailRange())
					if (_stack.swapReachable(offset) && _stack.canBeFreelyGenerated(_stack[offset]) && !_stack[offset].isLiteralValueID())
					{
						_stack.swap(offset);
						return true;
					}
				// otherwise try swapping it with a literal
				for (StackOffset offset: _state.stackTailRange())
					if (_stack.swapReachable(offset) && _stack[offset].isLiteralValueID())
					{
						_stack.swap(offset);
						return true;
					}
			}
			// try finding a slot that is compatible with the top and also admits the current top:
			//		- could be that the top slot is used elsewhere in the args (exclude junk)
			//		- could be that the top slot is something that is only required in the tail
			for (StackOffset offset: _state.stackArgsRange())
				if (
					offset != stackTop &&
					_stack[offset] != _stack[stackTop] &&  // don't swap identical values (no-op)
					_stack.swapReachable(offset) &&
					_state.isArgsCompatible(offset, stackTop) &&
					_state.isArgsCompatible(stackTop, offset) &&
					!_state.targetArbitrary(offset)
				)
				{
					_stack.swap(offset);
					return true;
				}

			// try finding a slot in args that wants to have the top, swap that
			for (StackOffset offset: _state.stackArgsRange())
				if (
					offset != stackTop &&
					_stack[offset] != _stack[stackTop] &&  // don't swap identical values (no-op)
					_stack.swapReachable(offset) &&
					!_state.isArgsCompatible(offset, offset) &&
					_state.isArgsCompatible(stackTop, offset)
				)
				{
					_stack.swap(offset);
					return true;
				}
		}

		// swap up any slot in args that is out of position and has a slot available in args that it can occupy
		for (StackOffset offset: _state.stackArgsRange())
		{
			bool const reachable = _stack.swapReachable(offset);
			bool const identical = _state.isArgsCompatible(offset, stackTop) && !_state.targetArbitrary(stackTop);
			if (
				reachable &&
				!identical && // we wouldn't just be swapping identical things
				(
					!_state.isArgsCompatible(offset, offset) || // the slot at offset isn't final
					(_state.targetArbitrary(offset) && !_stack.slot(offset).isJunk()) // or the target is arbitrary and the current slot isn't already junk
				)
			)
			{
				// for each `targetOffset` in target args, see if we can't swap the out of position `offset` to `targetOffset`
				for (StackOffset targetOffset: _state.stackArgsRange())
					if (
						targetOffset != offset &&  // we shouldn't be looking at the very same offset
						_stack.swapReachable(targetOffset) &&  // the target offset should be within reach
						_state.isArgsCompatible(offset, targetOffset) &&  // we can put offset -> targetOffset
						!_state.isArgsCompatible(targetOffset, targetOffset)  // targetOffset doesn't like where it is
					)
					{
						if (offset != stackTop)
							// swap up slot at offset
							_stack.swap(offset);
						// bring slot at offset into fixed position
						_stack.swap(targetOffset);
						return true;
					}
			}
		}

		// if we're at size and would have to push or dup something to satisfy args, try shrinking
		if (_stack.size() == _state.target().size)
		{
			for (auto const& arg: _state.target().args)
				if (_state.count(arg) < _state.targetMinCount(arg))
				{
					if (shrinkStack(_stack, _state))
						return true;
					else
						yulAssert(false, "stack too deep");
				}
		}
		return false;
	}

	static bool fixTailSlot(Stack<Callback>& _stack, detail::State const& _state)
	{
		yulAssert(_stack.size() <= _state.target().size, "this method assumes that the stack isn't exceeding target size");
		for (StackOffset offset: _state.stackArgsRange() | ranges::views::reverse)
		{
			Slot const& slotAtOffset = _stack[offset];
			if (
				_state.requiredInTail(slotAtOffset) &&  // if we need the slot in tail
				_state.countInTail(slotAtOffset) == 0  // if we don't have the slot in tail right now
			)
			{
				// If we don't have enough copies of this slot, dup first instead of swapping.
				if (_state.count(slotAtOffset) < _state.targetMinCount(slotAtOffset))
				{
					if (_stack.dupReachable(offset))
					{
						_stack.dup(offset);
						return true;
					}
				}

				if (
					!_state.isArgsCompatible(offset, offset) && // don't swap away a slot already in correct args position)
					_stack.swapReachable(offset) // if we can swap it up
				) {
					// find the lowest swappable slot in tail that needs to go to args, swap
					for (StackOffset tailOffset: _state.stackTailRange())
					{
						auto const& slotAtTailOffset = _stack[tailOffset];
						if (
							_stack.swapReachable(tailOffset) &&  // we can swap that deep
							(!_state.requiredInTail(slotAtTailOffset) || _state.countInTail(slotAtTailOffset) > 1) &&  // dont need it in tail or it's available more than once
							_state.requiredInArgs(slotAtTailOffset) &&  // we need the tail offset slot in args
							_state.targetArgsCount(slotAtTailOffset) > _state.countInArgs(slotAtTailOffset)  // we don't already have enough of it in args
						)
						{
							// bring up offset slot if necessary
							if (offset != StackOffset{_stack.size() - 1})
								_stack.swap(offset);
							// swap offset slot down into tail
							_stack.swap(tailOffset);
							return true;
						}
					}
					// find the lowest swappable slot in tail that can be popped but is no literal, swap
					for (StackOffset tailOffset: _state.stackTailRange())
						if (
							_stack.swapReachable(tailOffset) &&
							_stack.canBeFreelyGenerated(_stack[tailOffset]) &&
							!_stack[tailOffset].isLiteralValueID()
						)
						{
							// bring up offset slot if necessary
							if (offset != StackOffset{_stack.size() - 1})
								_stack.swap(offset);
							// swap offset slot down into tail
							_stack.swap(tailOffset);
							return true;
						}
					// find the lowest swappable slot in tail that is a literal, swap
					for (StackOffset tailOffset: _state.stackTailRange())
						if (
							_stack.swapReachable(tailOffset) &&
							_stack[tailOffset].isLiteralValueID()
						)
						{
							// bring up offset slot if necessary
							if (offset != StackOffset{_stack.size() - 1})
								_stack.swap(offset);
							// swap offset slot down into tail
							_stack.swap(tailOffset);
							return true;
						}
				}
			}
		}

		// todo dup/push something that isn't yet in tail but required or push0 if we need to fill it up
		return false;
	}

	static bool shrinkStack(Stack<Callback>& _stack, detail::State const& _state)
	{
		yulAssert(!_stack.empty(), "Stack is empty, can't shrink");

		StackOffset const stackTop{_stack.size() - 1};
		// pop top if it is junk (ie actual junk, not in args, not in live out)
		if (
			_stack[stackTop].isJunk() ||
			(!_state.requiredInArgs(_stack[stackTop]) && !_state.requiredInTail(_stack[stackTop]))
		)
		{
			_stack.pop();
			return true;
		}

		// swap top to suitable position, prioritizing args region
		{
			if (_state.requiredInArgs(_stack[stackTop]))
			{
				for (StackOffset argsOffset: _state.stackArgsRange())
					if (
						_stack[argsOffset] != _stack[stackTop] &&  // don't swap identical values (no-op)
						_stack.swapReachable(argsOffset) &&
						_state.isArgsCompatible(stackTop, argsOffset) &&
						!_state.isArgsCompatible(argsOffset, argsOffset)
					)
					{
						_stack.swap(argsOffset);
						return true;
					}
			}
			// we don't need it in args but in tail
			if (!_state.requiredInArgs(_stack[stackTop]) && _state.requiredInTail(_stack[stackTop]))
			{
				// pop when at least one of the two conditions is fulfilled
				//	- the top slot is contained in tail, and we're in args or excess region
				//	- there's more than one in tail
				if (
					(
						_state.countInTail(_stack[stackTop]) >= 1 &&
						(_state.offsetInTargetArgsRegion(stackTop) || _stack.size() > _state.target().size)
					) || _state.countInTail(_stack[stackTop]) > 1
				)
				{
					_stack.pop();
					return true;
				}

				// if we need it down there, try to swap down
				for (StackOffset tailOffset: _state.stackTailRange() | ranges::views::reverse)
					if (
						_stack[tailOffset] != _stack[stackTop] &&  // don't swap identical values (no-op)
						_stack.swapReachable(tailOffset) &&  // we can reach the offset
						!(_state.requiredInTail(_stack[tailOffset]) && _state.countInTail(_stack[tailOffset]) <= 1)  // it's okay to swap the tail offset out
					)
					{
						_stack.swap(tailOffset);
						return true;
					}
			}
		}
		// pop junk (but not if JUNK is exactly what's needed at that position)
		for (StackOffset offset: _state.stackSwapReachableRange())
			if (_stack[offset].isJunk() && !_state.isArgsCompatible(offset, offset))
			{
				if (offset != stackTop && _stack[offset] != _stack[stackTop])
					_stack.swap(offset);
				_stack.pop();
				return true;
			}

		// pop something that can be freely generated except for literals
		// (but not if it's already in a compatible position)
		for (StackOffset offset: _state.stackSwapReachableRange())
			if (
				_stack.canBeFreelyGenerated(_stack[offset]) &&
				!_stack[offset].isLiteralValueID() &&
				!_state.isArgsCompatible(offset, offset)
			)
			{
				if (offset != stackTop && _stack[offset] != _stack[stackTop])
					_stack.swap(offset);
				_stack.pop();
				return true;
			}

		// pop anything that isn't in position and we have more than one of
		for (StackOffset offset: _state.stackSwapReachableRange())
			if (_state.count(_stack[offset]) > _state.targetMinCount(_stack[offset]))
			{
				if (offset != stackTop && _stack[offset] != _stack[stackTop])
					_stack.swap(offset);
				_stack.pop();
				return true;
			}
		// pop any literals we can find
		for (StackOffset offset: _state.stackSwapReachableRange())
			if (_stack[offset].isLiteralValueID())
			{
				if (offset != stackTop && _stack[offset] != _stack[stackTop])
					_stack.swap(offset);
				_stack.pop();
				return true;
			}
		return false;
	}

	static bool allNecessarySlotsReachableOrFinal(Stack<Callback> const& _stack, detail::State const& _state)
	{
		// check that args are either in position or reachable
		for (StackOffset offset{_state.target().size}; offset < _state.target().size; ++offset.value)
			if (
				offset < _state.size() &&
				!_state.isArgsCompatible(offset, offset)
			)
			{
				// find first occurrence of the slot
				std::optional<StackDepth> depth = _stack.findSlotDepth(_state.targetArg(offset));

				if (!depth)
				{
					// if there is no occurrence of the slot anywhere, we must be able to freely generate it
					yulAssert(_stack.canBeFreelyGenerated(_state.targetArg(offset)));
				}
				else
				{
					if (!_stack.swapReachable(*depth))
						return false;
				}
			}
		// distribution check: all we have to dup can be duped
		for (StackOffset const offset: _state.stackRange())
		{
			auto const& slotAtOffset = _stack[offset];
			// we don't have enough of the slot
			if (
				_state.count(slotAtOffset) < _state.targetMinCount(slotAtOffset) &&
				!_stack.dupReachable(offset)
			)
			{
				// find first occurrence of the slot
				std::optional<StackDepth> depth = _stack.findSlotDepth(slotAtOffset);
				// it must exist
				yulAssert(depth);
				if (!_stack.swapReachable(*depth))
					return false;
			}
		}

		return true;
	}
};

}
