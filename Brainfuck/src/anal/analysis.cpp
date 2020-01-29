#include "anal/analysis.h"

#include <numeric>
#include <algorithm>
#include <queue>
#include <iterator>

namespace bf::analysis {

	same_offset_iterator& same_offset_iterator::operator++() {
		switch (state_) {
		case state::too_far:
			break;
		case state::too_low:
			current_range_ = bounds_.begin_;
			current_instruction_ = current_range_->begin_;
			state_ = state::valid;
			break;
		case state::valid:
			++current_instruction_;
			if (current_instruction_ == current_range_->end_) { //if it reached the end of this range, find the next range
				++current_range_;
				if (current_range_ != bounds_.end_)
					current_instruction_ = current_range_->begin_;
				else
					state_ = state::too_far;
			}
			break;
		case state::no_range:
			break;
			ASSERT_NO_OTHER_OPTION;
		}
		return *this;
	}

	same_offset_iterator& same_offset_iterator::operator--() {
		switch (state_) {
		case state::too_far:
			current_range_ = std::prev(bounds_.end_);
			current_instruction_ = std::prev(current_range_->end_);
			break;
		case state::too_low:
			break;
		case state::valid:
			if (current_instruction_ != current_range_->begin_)
				--current_instruction_;	//decrement the instruction iterator
			else if (current_range_ != bounds_.begin_) {
				--current_range_;
				current_instruction_ = std::prev(current_range_->end_);
			}
			else
				state_ = state::too_low;
			break;

		case state::no_range:
			break;
			ASSERT_NO_OTHER_OPTION;
		}
		return *this;
	}

	instruction& same_offset_iterator::operator*() {
		assert(state_ == state::valid);
		return *current_instruction_;
	}

	instruction* same_offset_iterator::operator->() {
		assert(state_ == state::valid);
		return &*current_instruction_;
	}

	same_offset_iterator::operator bool() const {
		return state_ == state::valid;
	}

	pointer_movement::pointer_movement(basic_block* const block)
		: subject_{ block } {
		assert(block);
		do_analyze();
	}

	void pointer_movement::do_analyze() {
		assert(subject_ && ptr_delta_ == 0 && !ptr_moves_);
		assert(stationary_ranges_.empty());

		auto first_shift_op = subject_->ops_.begin();

		for (auto const [begin, end] : utils::iterate_ranges_if(subject_->ops_.begin(), subject_->ops_.end(), std::not_fn(std::mem_fn(&instruction::is_shift)))) {
			ptr_delta_ += std::transform_reduce(first_shift_op, begin, std::ptrdiff_t{ 0 }, std::plus{}, std::mem_fn(&instruction::argument));
			if (ptr_delta_)
				ptr_moves_ = true;
			stationary_ranges_.push_back({ ptr_delta_, begin, end });
			first_shift_op = end;
		}
		std::stable_sort(stationary_ranges_.begin(), stationary_ranges_.end());

		ptr_delta_ += std::transform_reduce(first_shift_op, subject_->ops_.end(), std::ptrdiff_t{ 0 },
			std::plus{}, std::mem_fn(&instruction::argument));
		if (ptr_delta_)
			ptr_moves_ = true;
	}

	void block_evaluation::analyze_predecessors() {
		if (ptr_movement_.ptr_moves())
			return; //result of this block is not dependent on the result of previous
		switch (subject_->predecessors_.size()) {
		case 0:
			state_ = result_state::known_constant;
			entry_value_ = 0;
			break;
		case 1:
		{
			block_evaluation const pred_analysis{ subject_->get_unique_predecessor() };

			state_ = pred_analysis.state_;
			if (state_ == result_state::known_constant)
				entry_value_ = pred_analysis.const_result_;

		}
		default: // TODO implement analysis for multiple predecessors
			break;
		}
	}

	void block_evaluation::analyze_within_block() {

		auto iterator = ptr_movement_.offset_iterator(ptr_movement_.ptr_delta());

		for (; iterator; ++iterator) {
			instruction const& inst = *iterator;
			if (inst.is_arithmetic()) {
				value_delta_ += inst.argument_;
				if (state_ == result_state::known_constant)
					const_result_ += inst.argument_;
				else if (state_ == result_state::known_not_zero)
					state_ = result_state::indeterminate_possible_overflow;
			}
			else if (inst.is_const()) {
				state_ = result_state::known_constant;
				const_result_ = inst.argument_;
			}
			else
				switch (inst.op_code_) {
				case op_code::infinite:
					has_sideeffect_ = true;
					if (inst.is_infinite_on_non_zero()) {
						state_ = result_state::known_constant;
						const_result_ = 0;
					}
					else if (inst.is_infinite_on_zero())
						state_ = result_state::known_not_zero;
					else
						MUST_NOT_BE_REACHED;
					break;
				case op_code::read:
					state_ = result_state::indeterminate_read;
					[[fallthrough]] ;
				case op_code::write:
					has_sideeffect_ = true;
					break;
				}

		}
	}

	block_evaluation::block_evaluation(basic_block* const block)
		: subject_{ block }, ptr_movement_{ subject_ } {
		assert(block);

		analyze_predecessors();
		const_result_ = entry_value_;
		analyze_within_block();
	}


	namespace {

		/*Traverses the directed graph of program's basic blocks, keeps record of all blocks that are reachable
		by the control flow and returns them. Graph of blocks is traversed by a breadth-first search using each block's
		pointers to jump and natural successors as edges - a control flow of program is imitated. Blocks that are not visited
		by this method cannot be reached by the control flow.
		Returns std::set of pointers to reachable basic blocks. */
		std::set<basic_block const*> identify_reachable_blocks(std::vector<basic_block*> const& program) {

			std::set<basic_block const*> visited;

			std::queue<basic_block*> waiting_to_visit; //A queue of basic blocks that are waiting to be visited
			waiting_to_visit.push(program.front());
			assert(waiting_to_visit.front()->ops_.front().op_code_ == op_code::program_entry);

			/*We perform a simple breadth-first search:
			In every iteration of the loop we visit one basic block (a node of a directed graph).
				1) If the block was already visited, we skip to the next iteration. From now on we are sure that the basic block was not yet visited.
				2) The basic block is marked as visited (its label is added to the set of already seen labels).
				3) All successors of this block are added to the queue.
			*/
			while (!waiting_to_visit.empty()) {
				basic_block* const block = waiting_to_visit.front();
				waiting_to_visit.pop();

				if (visited.count(block)) // (1) - If this block was already visited, continue to next iteration  
					continue;

				visited.insert(block); // (2) - Mark the basic block as visited

				if (block->jump_successor_)       // (3) - Push all successors to the queue
					waiting_to_visit.push(block->jump_successor_);
				if (block->natural_successor_)
					waiting_to_visit.push(block->natural_successor_);
			}
			return visited;
		}
	}

	std::map<std::ptrdiff_t, bool> analyze_block_lives(std::vector<basic_block*> const& program) {
		std::map<std::ptrdiff_t, bool> reachable;

		for (basic_block const* const block : program)
			reachable[block->label_] = false;

		for (basic_block const* const block : identify_reachable_blocks(program))
			reachable[block->label_] = true;

		return reachable;
	}

	incoming_value_analyzer::incoming_value_analyzer(basic_block* const block)
		: subject_{ block } {
		assert(block);

		for (basic_block* const pred : block->predecessors_)
			analyze_predecessor(pred);
	}

	void incoming_value_analyzer::analyze_predecessor(basic_block* const pred) {
		assert(pred && pred->has_successor(subject_));

		if (pred->is_pure_cjump())
			(pred->jump_successor_ == subject_ ? non_zero_seen_ : zero_seen_) = true;
		else
			if (block_evaluation const eval{ pred }; eval.has_indeterminate_value())
				zero_seen_ = non_zero_seen_ = true;
			else
				(eval.has_non_zero_result() ? non_zero_seen_ : zero_seen_) = true;
	}

	bool incoming_value_analyzer::all_non_zero() const {
		return !zero_seen_;
	}

	bool incoming_value_analyzer::all_zero() const {
		return !non_zero_seen_;
	}

}