#ifndef PROGRAM_CODE_H
#define PROGRAM_CODE_H
#pragma once
#include "syntax_check.h"
#include "utils.h"

#include <string_view>
#include <vector>
#include <cassert>
#include <optional>
#include <string>
#include <set>
#include <memory>
#include <ostream>

namespace bf {

	/*Enumeration of recognized operation codes. Fixed to width of 32bits, */
	enum class op_code : std::uint32_t {

		//no operation to be carried out - only skips a CPU cycle
		nop = 117,
		//increase cell's value (may overflow by the laws of standard modulo 2^n unsigned arithmetic)
		inc,
		//shift the cell pointer towards higher address (jumps from the end of the address space to the beginning)
		right,

		//perform an unconditional jump to destination
		jump,
		//jump to the destination iff the cell pointed to has non-zero value
		jump_not_zero,

		//input one character from stdin
		read,
		//write one character to stdout
		write,

		//pseudo instructions added by the optimizer:

		//set pointed to cell to the value of immediate
		load_const,

		//infinite loop like []
		infinite,

		//immediatelly stop the execution cycle of the CPU. 
		breakpoint,

		//formal instruction marking the program's entry
		program_entry,
		//formal instruction marking the program's exit
		program_exit
	};

	/*Returns a string representation (i.e. the mnemonic of) of the given operation code. */
	[[nodiscard]]
	std::string const& get_mnemonic(op_code code);

	/*Standard stream output operator for opcodes.*/
	std::ostream& operator<<(std::ostream& str, op_code code);


	/*Struct representing a single instruction in internal intermediate representation. Each instruction in the world of brainfuck
	has an opcode representing the operation to be carried out as well as its argument and the location (offset) relative to the beginning
	compiled source program to ease debugging process.*/
	struct instruction {
		op_code op_code_; //operation to be performed

		/*Argument to the given operation. If the operation is to perform an arithmetic calculation or reposition the CPR,
		then this argument stores the number to be added, subtracted, or by which the pointer shall be shifted, respectively.
		If op_code denotes a jump instruction, then this argument is the target adress which shall be moved into the program counter.
		In another words is argument_ in such case the adress of the corresponding bracket plus one to move just past it.*/
		union {
			std::ptrdiff_t argument_;
			std::ptrdiff_t destination_;
		};
		std::size_t source_offset_; //number of characters preceding the corresponding brainfuck instruction in the source code


		[[nodiscard]]
		std::ptrdiff_t argument() const { return argument_; }
		[[nodiscard]]
		std::ptrdiff_t destination() const { return destination_; }
		[[nodiscard]]
		std::size_t source_offset() const { return source_offset_; }
		/*Returns true iff it is legal to collapse a block of multiple consecutive occurences of this instruction. If a fold of two or more
		instructions would result in an invalid program, false is returned. Foldabe instructions are inc, dec, right and left. */
		[[nodiscard]]
		constexpr bool is_arithmetic() const { return op_code_ == op_code::inc; }
		[[nodiscard]]
		constexpr bool is_shift() const { return op_code_ == op_code::right; }
		//Returns true iff the instruction denotes an (un)conditional jump.
		[[nodiscard]]
		constexpr bool is_jump() const { return op_code_ == op_code::jump || op_code_ == op_code::jump_not_zero; }

		//Returns true iff the instruction denotes an input/output operation (reads or writes).
		[[nodiscard]]
		constexpr bool is_io() const { return op_code_ == op_code::read || op_code_ == op_code::write; }

		[[nodiscard]]
		constexpr bool is_const() const { return op_code_ == op_code::load_const; }

		[[nodiscard]]
		constexpr bool is_nop() const {
			return op_code_ == op_code::nop && argument_ == -1;
		}

		constexpr void make_nop() {
			op_code_ = op_code::nop;
			argument_ = -1;
		}

		[[nodiscard]]
		constexpr bool is_infinite() const { return op_code_ == op_code::infinite; }
		[[nodiscard]]
		constexpr bool is_infinite_on_zero() const { return is_infinite() && argument_ == 0; }
		constexpr bool is_infinite_on_non_zero() const { return is_infinite() && argument_ != 0; }

		constexpr void make_infinite_on_zero() { op_code_ = op_code::infinite; argument_ = 0; }
		constexpr void make_infinite_on_not_zero() { op_code_ = op_code::infinite; argument_ = 1; }

	};

	struct basic_block {

		std::ptrdiff_t label_;

		std::vector<instruction> ops_;

		std::set<basic_block*> predecessors_;

		basic_block* natural_successor_ = nullptr;

		basic_block* jump_successor_ = nullptr;

		//TODO add analysis flags

		basic_block(std::ptrdiff_t const label, std::vector<instruction> ops)
			: label_{ label }, ops_{ std::move(ops) }
		{}

		basic_block(basic_block const&) = delete;
		basic_block(basic_block&&) = delete;
		basic_block& operator=(basic_block const&) = delete;
		basic_block& operator=(basic_block&&) = delete;

		static constexpr basic_block* basic_block::* successor_ptrs[] = { &basic_block::natural_successor_, &basic_block::jump_successor_ };

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


	};


}

#endif