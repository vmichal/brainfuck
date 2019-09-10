#include "optimizer.h"
#include "analysis.h"

#include <algorithm>
#include <iterator>
#include <queue>
#include <execution>
#include <functional>
#include <cassert>
#include <iostream>

namespace bf::opt::global {

	std::ptrdiff_t merge_into_predecessor(basic_block* const block) {
		if (!block || block->is_cjump() || block->predecessors_.size() != 1)
			return 0;

		basic_block* const predecessor = block->get_unique_predecessor();
		if (predecessor->is_pure_cjump())
			return 0;

		if (predecessor->is_ujump())
			predecessor->ops_.pop_back(); //Remove the jump instruction and clear pointers to successors

		predecessor->ops_.reserve(predecessor->ops_.size() + block->ops_.size());
		std::copy(block->ops_.begin(), block->ops_.end(), std::back_inserter(predecessor->ops_));

		for (auto succ : basic_block::successor_ptrs) {
			predecessor->*succ = block->*succ;
			if (predecessor->*succ)
				(predecessor->*succ)->add_predecessor(predecessor);
		}

		block->orphan();
		return 1;
	}

	std::ptrdiff_t merge_into_predecessor(std::vector<basic_block*>& program) {
		assert_program_invariants(program);

		std::for_each(program.begin(), program.end(), static_cast<std::ptrdiff_t(*)(basic_block*)>(merge_into_predecessor));

		return erase_orphaned_blocks(program);
	}

	/*Traverses the program's graph and marks all basic blocks that are not reachable by the control flow.
	These marked blocks are said to be dead. They are orphaned and eliminated.*/
	std::ptrdiff_t delete_unreachable_blocks(std::vector<basic_block*>& program) {
		//Simulate the flow of control and recongnize all blocks that are reachable
		std::map<std::ptrdiff_t, bool> const live_blocks = analysis::analyze_block_lives(program);

		for (basic_block* const block : program)
			if (!live_blocks.at(block->label_))
				block->orphan();

		return erase_orphaned_blocks(program);
	}

	std::ptrdiff_t eliminate_pure_uncond_jumps(basic_block* const block) {
		if (!block || !block->is_pure_ujump())
			return 0;

		assert(block->natural_successor_ == nullptr); //sanity check that there's no natural successor. Ujumps are not allowed to have one
		assert(block->jump_successor_);


		//This block jump's destination; it will be propagated to all predecessors
		basic_block* const new_target = block->jump_successor_;

		for (basic_block* const predecessor : block->predecessors_) {
			assert(predecessor->has_successor(block));
			new_target->add_predecessor(predecessor);

			//If the predecessor already has a jump instruction, modify only its target
			if (predecessor->is_jump()) {
				basic_block* basic_block::* successor = predecessor->choose_successor_ptr(block);
				assert(predecessor->*successor == block);
				predecessor->*successor = new_target;
			}
			else {
				//Otherwise we have to move the unconditional jump to the preceding block. In a sense there's no optimization here
				assert(predecessor->jump_successor_ == nullptr);
				predecessor->natural_successor_ = nullptr;
				predecessor->jump_successor_ = new_target;
				predecessor->ops_.push_back(
					instruction{ op_code::jump, std::ptrdiff_t(0xdead'beef), block->ops_.front().source_offset_ });
			}
		}
		block->orphan();
		return 1;
	}

	std::ptrdiff_t eliminate_pure_uncond_jumps(std::vector<basic_block*>& program) {

		std::for_each(program.begin(), program.end(),
			static_cast<std::ptrdiff_t(*)(basic_block*)>(&eliminate_pure_uncond_jumps));
		return erase_orphaned_blocks(program);
	}

	std::ptrdiff_t optimize_cond_jump_destination(basic_block* const block) {
		if (!block || !block->is_pure_cjump())
			return 0;

		std::ptrdiff_t opt_count = 0;

		for (basic_block* (basic_block::* successor) : basic_block::successor_ptrs) {
			basic_block*& branch = block->*successor;
			branch->remove_predecessor(block);
			while (branch->is_pure_cjump() && branch != branch->*successor) {
				++opt_count;
				branch = branch->*successor;
			}
			branch->add_predecessor(block);
		}

		return opt_count;
	}

	std::ptrdiff_t optimize_cond_jump_destination(std::vector<basic_block*>& program) {
		return std::transform_reduce(program.begin(), program.end(), std::ptrdiff_t{ 0 }, std::plus{},
			static_cast<std::ptrdiff_t(*)(basic_block*)>(optimize_cond_jump_destination));
	}


	std::ptrdiff_t eliminate_single_entry_conditionals(basic_block* const block) {
		if (!block || !block->is_pure_cjump() || block->predecessors_.size() != 1)
			return 0;

		basic_block* const predecessor = block->get_unique_predecessor();


		analysis::block_evaluation_analyzer const pred_eval{ predecessor };

		if (pred_eval.has_indeterminate_value())
			return 0;

		//TODO make use choose_successor, has_successor, has_predecessor etc!
		basic_block* (basic_block:: * const connection) = predecessor->choose_successor_ptr(block);

		if (pred_eval.has_const_result() && pred_eval.result() == 0)
			predecessor->*connection = block->natural_successor_;
		else if (pred_eval.has_non_zero_result())
			predecessor->*connection = block->jump_successor_;
		else
			MUST_NOT_BE_REACHED;

		block->orphan();
		(predecessor->*connection)->add_predecessor(predecessor);
		return 1;
	}

	std::ptrdiff_t eliminate_single_entry_conditionals(std::vector<basic_block*>& program) {
		std::for_each(program.begin(), program.end(),
			static_cast<std::ptrdiff_t(*)(basic_block*)>(eliminate_single_entry_conditionals));
		return erase_orphaned_blocks(program);
	}

} //namespace ::bf::opt::global
