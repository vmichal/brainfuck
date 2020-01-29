#include "program_code.h"
#include <cassert>
#include <map>


namespace bf {


	void program_code::assert_invariants() const {
		//Assert that there are no references to the to be deleted blocks
		for (basic_block* const block : mutable_pointers_) {
			assert(block);
			assert(block->jump_successor_ == nullptr || (!block->jump_successor_->is_orphaned() && block->jump_successor_->predecessors_.count(block) == 1));
			assert(block->natural_successor_ == nullptr || (!block->natural_successor_->is_orphaned() && block->natural_successor_->predecessors_.count(block) == 1));
			assert(block->is_cjump() == block->is_pure_cjump());
			assert(std::none_of(block->predecessors_.begin(), block->predecessors_.end(), std::mem_fn(&basic_block::is_orphaned)));

			assert(block->jump_successor_ != block->natural_successor_ || block->jump_successor_ == nullptr);

			for (basic_block const* const predecessor : block->predecessors_) {
				assert(predecessor->has_successor(block));

			}

			for (auto successor : basic_block::successor_ptrs) {
				assert(block->*successor == nullptr || (block->*successor)->has_predecessor(block));
			}

			for ([[maybe_unused]] instruction const& inst : block->ops_) {
			}
		};

		assert(std::is_sorted(program.begin(), program.end(), basic_block::ptr_comparator{}));
	}

	/*Erases all blocks that have no predecessors. It is expected that said blocks are no longer referenced from any other blocks.*/
	std::ptrdiff_t program_code::erase_orphaned_blocks() {
		assert_invariants();

		auto const old_end = mutable_pointers_.end(); //TODO change to owning pointers
		auto const new_end = std::remove_if(mutable_pointers_.begin(), old_end, std::mem_fn(&basic_block::is_orphaned));

		mutable_pointers_.erase(new_end, old_end);

		return std::distance(new_end, old_end);
	}
} //namespace bf
