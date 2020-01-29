#include "opt/cleanup.h"
#include "anal/analysis.h"

namespace bf::opt {

	std::ptrdiff_t empty_block_elimination::do_optimize(basic_block* const block) {
		if (!block || !block->empty())
			return 0;

		assert(block->natural_successor_ && block->jump_successor_ == nullptr);

		basic_block* const new_target = block->natural_successor_;
		for (basic_block* const pred : block->predecessors_) {
			basic_block* basic_block::* modified_connection = pred->choose_successor_ptr(block);
			assert(pred->*modified_connection == block);

			pred->*modified_connection = new_target;
			new_target->add_predecessor(pred);
		}
		block->orphan();
		return 1;

	}

	std::ptrdiff_t block_merging::do_optimize(basic_block* const block) {
		if (!block || block->is_cjump())
			return 0;

		basic_block* const my_pred = block->get_unique_predecessor();
		if (!my_pred || my_pred->is_pure_cjump())
			return 0;

		if (my_pred->is_ujump())
			my_pred->ops_.pop_back(); //Remove the jump instruction and clear pointers to successors

		my_pred->ops_.reserve(my_pred->ops_.size() + block->ops_.size());
		std::copy(block->ops_.begin(), block->ops_.end(), std::back_inserter(my_pred->ops_));

		for (auto succ : basic_block::successor_ptrs) {
			my_pred->*succ = block->*succ;
			if (my_pred->*succ)
				(my_pred->*succ)->add_predecessor(my_pred);
		}

		block->orphan();
		return 1;

	}

	std::ptrdiff_t nop_elimination::do_optimize(basic_block* const block) {

		auto const old_end = block->ops_.end();
		auto const new_end = std::remove_if(block->ops_.begin(), old_end, std::mem_fn(&instruction::is_nop));

		block->ops_.erase(new_end, old_end);
		return std::distance(new_end, old_end);
	}

	std::ptrdiff_t dead_code_elimination::do_optimize(std::vector<basic_block*>& program) {
		//Simulate the flow of control and recongnize all blocks that are reachable
		std::map<std::ptrdiff_t, bool> const live_blocks = analysis::analyze_block_lives(program);

		for (basic_block* const block : program)
			if (!live_blocks.at(block->label_))
				block->orphan();

		return erase_orphaned_blocks(program);
	}

}
/*Traverses the program's graph and marks all basic blocks that are not reachable by the control flow.
These marked blocks are said to be dead. They are orphaned and eliminated.*/