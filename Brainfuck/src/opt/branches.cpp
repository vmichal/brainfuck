#include "opt/branches.h"
#include "anal/analysis.h"

namespace bf::opt {


	std::ptrdiff_t pure_ujump_elimination::do_optimize(basic_block* const block) {
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
				predecessor->ops_.push_back({ op_code::jump, std::ptrdiff_t{0xdead'beef}, block->ops_.front().source_loc_ });
			}
		}
		block->orphan();
		return 1;

	}

	std::ptrdiff_t cjump_destination_optimization::do_optimize(basic_block* const block) {
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

	std::ptrdiff_t single_entry_cjump_optimization::do_optimize(basic_block* const block) {
		if (!block || !block->is_pure_cjump() || block->predecessors_.size() != 1)
			return 0;

		basic_block* const predecessor = block->get_unique_predecessor();

		analysis::block_evaluation const pred_eval{ predecessor };

		if (pred_eval.has_indeterminate_value())
			return 0;

		basic_block* basic_block::* const connection = predecessor->choose_successor_ptr(block);

		if (pred_eval.has_const_result() && pred_eval.const_result() == 0)
			predecessor->*connection = block->natural_successor_;
		else if (pred_eval.has_non_zero_result())
			predecessor->*connection = block->jump_successor_;
		else
			MUST_NOT_BE_REACHED;

		block->orphan();
		(predecessor->*connection)->add_predecessor(predecessor);
		return 1;
	}

}