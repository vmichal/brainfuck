#pragma once

#include "syntax_check.h"

#include <string_view>
#include <vector>
#include <string>
#include <memory>

namespace bf {

	enum class op_code : std::uint32_t {
		invalid = static_cast<std::uint32_t>(-1),
		nop = 117,
		inc,
		dec,
		left,
		right,

		loop_begin,
		loop_end,

		in,
		out,

		//pseudo instructions added by the optimizer:
		load_const,



		//instruction for debugging purposes
		breakpoint
	};

	std::ostream& operator<<(std::ostream& str, op_code t);


	/*Struct representing a single instruction in internal intermediate representation. Each instruction in the world of brainfuck
	has an opcode representing the operation to be carried out as well as its argument and the location (offset) relative to the beginning
	compiled source program to ease debugging process.*/
	struct instruction {
		op_code op_code_; //operation to be performed

		/*Argument to the given operation. If operation performes arithmetic or repositions the CPR,
		then this argument stores the number to be added, subtracted, or by which the pointer shall be shifted, respectively.
		If type_ denotes a jump instruction, then this argument is the target adress which shall be moved into the program counter.
		In another words is argument_ in such case the adress of the corresponding bracket plus one to move just past it.*/
		int argument_;
		int source_offset_; //number of characters preceding the corresponding brainfuck instruction in the source code

		//Returns true if this instruction can be folded. Foldabe instructions are inc, dec, right and left
		bool is_foldable() const;


		bool is_jump() const { return op_code_ == op_code::loop_begin || op_code_ == op_code::loop_end; }
		bool is_io() const { return op_code_ == op_code::in || op_code_ == op_code::out; }

		instruction() : instruction(op_code::invalid, -1, -1) {}

		instruction(op_code type, int argument, int source_offset)
			:op_code_(type), argument_(argument), source_offset_(source_offset) {}
	};

#if 0 //not needed right now
	std::ostream& operator<<(std::ostream& str, instruction const &i);
#endif


	struct basic_block {

		int id_;

		std::vector<basic_block*> preceding_blocks_;

		std::vector<instruction> instructions_;

		basic_block(int id) : id_(id) {}

	};

	struct program_code {

		std::vector<std::unique_ptr<basic_block>> basic_blocks_;



	};

	/*Class holding a list of the intermediate language instructions. Represents a program with capabilities of automatic 
	jump target relocation. And pseudo-vector interface*/
	class syntax_tree : public std::vector<instruction> {

	public:


		void add_instruction(instruction const&);
		void add_instruction(instruction&&);

		void add_instruction(op_code type, int argument, int source_offset);

		void relocate_jump_targets();

	};

	void relocate_jump_targets(std::vector<instruction>&);





}
