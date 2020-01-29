#pragma once

#include "instruction.h"

#include <vector>
#include <cstddef>
#include <set>
#include <memory>

namespace bf::IR {

	class basic_block {

		std::ptrdiff_t label_;

		std::vector<std::unique_ptr<instruction>> ops_;

		std::set<basic_block*> predecessors_;

		//TODO add analysis flags

	public:

		basic_block(std::ptrdiff_t const label)
			: label_{ label } {}

		basic_block(basic_block const&) = delete;
		basic_block(basic_block&&) = delete;
		basic_block& operator=(basic_block const&) = delete;
		basic_block& operator=(basic_block&&) = delete;

		[[nodiscard]]
		bool has_terminator() const { return ops_.size() > 0 && ops_.back()->is_jump(); }

		[[nodiscard]]
		instruction const* get_terminator() const { return has_terminator() ? ops_.back().get() : nullptr; }

		void add_instruction(std::unique_ptr<instruction>)

#if 0
		[[nodiscard]]
		bool is_orphaned() const {
			return natural_successor_ == nullptr && jump_successor_ == nullptr
				&& predecessors_.empty() && ops_.empty();
		}

		void orphan() {
			if (jump_successor_) {//unbind the block from its successors
				jump_successor_->remove_predecessor(this);
				jump_successor_ = nullptr;
			}

			if (natural_successor_) {
				natural_successor_->remove_predecessor(this);
				natural_successor_ = nullptr;
			}

			for (basic_block* const predecessor : predecessors_) {
				if (predecessor->jump_successor_ == this)
					predecessor->jump_successor_ = nullptr;
				else if (predecessor->natural_successor_ == this)
					predecessor->natural_successor_ = nullptr;
			}
			predecessors_.clear();
			ops_.clear();
		}

		[[nodiscard]]
		bool empty() const { return ops_.empty(); }

		[[nodiscard]]
		bool is_pure_cjump() const { return ops_.size() == 1u && ops_.front().op_code_ == op_code::jump_not_zero; }
		[[nodiscard]]
		bool is_pure_ujump() const { return ops_.size() == 1u && ops_.front().op_code_ == op_code::jump; }

		[[nodiscard]]
		bool is_inner_loop() const { return is_pure_cjump() && jump_successor_->has_successor(this); }

		[[nodiscard]]
		bool is_jump() const { return is_ujump() || is_cjump(); }

		[[nodiscard]]
		bool is_cjump() const { return !empty() && ops_.back().op_code_ == op_code::jump_not_zero; }
		[[nodiscard]]
		bool is_ujump() const { return !empty() && ops_.back().op_code_ == op_code::jump; }

		[[nodiscard]]
		bool has_self_loop() const { return has_predecessor(this); }

		void remove_predecessor(basic_block* const block) {
			assert(has_predecessor(block));
			predecessors_.erase(block);
		}

		void add_predecessor(basic_block* const block) {
			assert(!has_predecessor(block));
			predecessors_.insert(block);
		}

		[[nodiscard]]
		basic_block* get_unique_predecessor() const { return predecessors_.size() == 1 ? *predecessors_.begin() : nullptr; }

		[[nodiscard]]
		bool has_successor(basic_block const* const successor) const {
			return  natural_successor_ == successor || jump_successor_ == successor;
		}

		[[nodiscard]]
		bool has_predecessor(basic_block const* const pred) const {
			return predecessors_.count(const_cast<basic_block*>(pred));
		}

		[[nodiscard]]
		basic_block* basic_block::* choose_successor_ptr(basic_block const* successor) {
			assert(has_successor(successor));
			return natural_successor_ == successor ? &basic_block::natural_successor_ : &basic_block::jump_successor_;
		}

		[[nodiscard]]
		basic_block* basic_block::* choose_other_successor(basic_block const* successor) {
			assert(has_successor(successor));
			return natural_successor_ != successor ? &basic_block::natural_successor_ : &basic_block::jump_successor_;
		}

		struct ptr_comparator {
			bool operator()(basic_block const* const a, basic_block const* const b) {
				return a->label_ < b->label_;
			}
		};

#endif
	};



}