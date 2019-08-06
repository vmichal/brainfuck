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
		invalid = static_cast<std::uint32_t>(-1),

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
	std::string const& get_mnemonic(op_code const code);

	/*Standard stream output operator for opcodes.*/
	std::ostream& operator<<(std::ostream& str, op_code const code);


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
			int argument_;
			int destination_;
		};
		int source_offset_; //number of characters preceding the corresponding brainfuck instruction in the source code

		/*Returns true iff it is legal to collapse a block of multiple consecutive occurences of this instruction. If a fold of two or more
		instructions would result in an invalid program, false is returned. Foldabe instructions are inc, dec, right and left. */
		bool is_foldable() const;

		//Returns true iff the instruction denotes an (un)conditional jump.
		bool is_jump() const { return op_code_ == op_code::jump || op_code_ == op_code::jump_not_zero; }
		//Returns true iff the instruction denotes an input/output operation (reads or writes).
		bool is_io() const { return op_code_ == op_code::read || op_code_ == op_code::write; }


		instruction() : instruction(op_code::invalid, -1, std::size_t(-1)) {}

		instruction(op_code const op_code, int const argument, int const source_offset)
			: op_code_(op_code), argument_(argument), source_offset_(source_offset) {}

		//Returns the mnemonic of this instruction.
		std::string const& mnemonic() const { return get_mnemonic(op_code_); }
	};

	struct basic_block {
		int label_;

		std::vector<instruction> ops_;

		std::vector<std::shared_ptr<basic_block>> predecessors_;

		std::shared_ptr<basic_block> natural_successor_;

		std::shared_ptr<basic_block> jump_successor_;

		template<typename A, typename B, typename C, typename D>
		basic_block(int const label, A&& ops, B&& pred, C&& nat_succ, D&& jmp_succ)
			: label_(label), ops_(std::forward<A&&>(ops)), predecessors_(std::forward<B&&>(pred)),
			natural_successor_(std::forward<C&&>(nat_succ)), jump_successor_(std::forward<D&&>(jmp_succ))
		{}


	};

}