#pragma once

#include "source_location.h"

#include <ostream>
#include <cstdint>
#include <string>

namespace bf::IR {

	/*Enumeration of recognized operation codes. Fixed to width of 8 bits */
	enum class op_code : std::uint8_t {

		//no operation to be carried out - only skips a CPU cycle
		nop = 117,
		//increase cell's value (may overflow by the laws of standard modulo 2^n unsigned arithmetic)
		inc,
		dec,
		//shift the cell pointer towards higher address (jumps from the end of the address space to the beginning)
		right,
		left,

		//perform an unconditional jump to destination
		branch,
		//jump to the destination iff the cell pointed to has non-zero value
		branch_nz,

		//input one character from stdin
		read,
		//write one character to stdout
		write,

		//pseudo instructions added by the optimizer:
		search_right,
		search_left,

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
	class instruction {

	protected:

		op_code const op_code_; //operation to be performed

		source_location const source_loc_; //Location within the original source code



		instruction(op_code op_code, source_location loc)
			:op_code_{ op_code }, source_loc_{ loc } {}

	public:

		virtual ~instruction() = default;

		[[nodiscard]]
		op_code get_op_code() const { return op_code_; }

		[[nodiscard]]
		source_location get_source_location() const { return source_loc_; }

		[[nodiscard]]
		constexpr bool is_arithmetic() const { return op_code_ == op_code::inc || op_code_ == op_code::dec; }

		[[nodiscard]]
		constexpr bool is_shift() const { return op_code_ == op_code::right || op_code_ == op_code::left; }

		//Returns true iff the instruction denotes an (un)conditional jump.
		[[nodiscard]]
		constexpr bool is_jump() const { return op_code_ == op_code::branch || op_code_ == op_code::branch_nz; }

		//Returns true iff the instruction denotes an input/output operation (reads or writes).
		[[nodiscard]]
		constexpr bool is_io() const { return op_code_ == op_code::read || op_code_ == op_code::write; }

		[[nodiscard]]
		constexpr bool is_const() const { return op_code_ == op_code::load_const; }

		[[nodiscard]]
		constexpr bool is_nop() const { return op_code_ == op_code::nop; }

		[[nodiscard]]
		constexpr bool is_infinite() const { return op_code_ == op_code::infinite; }

		[[nodiscard]]
		constexpr bool is_search() const { return op_code_ == op_code::search_left || op_code_ == op_code::search_right; }

	};
}