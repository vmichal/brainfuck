#ifndef PROGRAM_CODE_H
#define PROGRAM_CODE_H
#pragma once
#include "syntax_check.h"

#include <string_view>
#include <vector>
#include <optional>
#include <string>
#include <memory>
#include <ostream>

namespace bf {

	/*Enumeration of recognized operation codes. Fixed to width of 32bits, */
	enum class op_code : std::uint32_t {

		//sentinel value for an invalid operation code
		invalid = ~static_cast<std::underlying_type_t<op_code>>(0),

		//no operation to be carried out - only skips a CPU cycle
		nop = 117,
		//increase cell's value (may overflow by the laws of standard modulo 2^n unsigned arithmetic)
		inc,
		//decrease cell's value (mya underflow by the laws of standard modulo 2^n unsigned arithmetic)
		dec,
		//shift the cell pointer towards lower address (jumps from the beginning of the address space to the end)
		left,
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

		/*Returns true iff it is legal to collapse a block of multiple consecutive occurences of this instruction. If a fold of two or more
		instructions would result in an invalid program, false is returned. Foldabe instructions are inc, dec, right and left. */
		[[nodiscard]]
		constexpr bool is_foldable() const {

			switch (op_code_) {
			case op_code::inc: case op_code::dec:
			case op_code::left:	case op_code::right:
				return true;
			default:
				return false;
			};
		}
		//Returns true iff the instruction denotes an (un)conditional jump.
		[[nodiscard]]
		constexpr bool is_jump() const { return op_code_ == op_code::jump || op_code_ == op_code::jump_not_zero; }
		//Returns true iff the instruction denotes an input/output operation (reads or writes).
		[[nodiscard]]
		constexpr bool is_io() const { return op_code_ == op_code::read || op_code_ == op_code::write; }


		instruction() : instruction(op_code::invalid, -1, std::numeric_limits<std::size_t>::max()) {}

		instruction(op_code const op_code, std::ptrdiff_t const argument, std::size_t const source_offset)
			: op_code_{ op_code }, argument_{ argument }, source_offset_{ source_offset } {}

		~instruction() noexcept = default;
		instruction(instruction const& copy) noexcept = default;
		instruction(instruction&& move) noexcept = default;
		instruction& operator=(instruction const& copy) noexcept = default;
		instruction& operator=(instruction&& move) noexcept = default;


		//Returns the mnemonic of this instruction.
		[[nodiscard]]
		std::string const& mnemonic() const { return get_mnemonic(op_code_); }
	};

	struct basic_block {
		std::ptrdiff_t label_;

		std::vector<instruction> ops_;

		std::vector<basic_block*> predecessors_;

		basic_block* natural_successor_;

		basic_block* jump_successor_;

		template<typename A, typename B>
		basic_block(std::ptrdiff_t const label, A&& ops, B&& pred, basic_block* const natural_succ, basic_block* const jump_succ)
			: label_{ label }, ops_{ std::forward<A&&>(ops) }, predecessors_{ std::forward<B&&>(pred) },
			natural_successor_{ natural_succ }, jump_successor_{ jump_succ }
		{}


	};
}

#include <sstream>
#include <iomanip>
#include <iostream>
namespace bf {


	inline void print_basic_blocks(std::vector<std::unique_ptr<basic_block>> const& basic_blocks) {
		std::ostringstream buffer;

		buffer << "\n\n\n\n\n";

		for (auto const& block_ptr : basic_blocks) {
			buffer << "Basic block id " << block_ptr->label_ << ", instruction count: " << block_ptr->ops_.size() << '\n';
			int offset = 0;
			for (instruction const& i : block_ptr->ops_)
				buffer << "\t\t" << std::setw(4) << ++offset << ": " << i.op_code_ << ' '
				<< (i.is_jump() ? (block_ptr->jump_successor_->label_) : i.argument_) << "\n";
			buffer << "\n\n";
		}
		std::cout << buffer.str();
	}

}

#endif