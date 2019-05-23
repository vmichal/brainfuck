#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <unordered_set>

namespace bf {

	enum class instruction_type : std::uint32_t {
		invalid = static_cast<std::uint32_t>(-1),
		nop = 0,
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




		breakpoint
	};

	std::ostream& operator<<(std::ostream& str, instruction_type t);
	/*Struct representing a single instruction in internal abstract language. Each instruction in the world of brainfuck
	has a type representing the operation to be carried out as well as its argument and the location (offset) relative to the
	compiled source program to ease debugging process.*/
	struct instruction {
		instruction_type type_; //operation to be performed

		/*Argument to the given operation. If operation performes arithmetic or repositions the CPR,
		then this argument stores the number to be added, subtracted, or by which the pointer shall be shifted, respectively.
		If type_ denotes a jump instruction, then this argument is the target adress which shall be moved into the program counter.
		In another words is argument_ in such case the adress of the corresponding bracket plus one to move just past it.*/
		int argument_;
		int source_offset_; //number of characters preceding the corresponding brainfuck instruction in the source code

		//Returns true if this instruction can be folded. Foldabe instructions are inc, dec, right and left
		bool is_foldable() const;


		bool is_jump() const { return type_ == instruction_type::loop_begin || type_ == instruction_type::loop_end; }
		bool is_io() const { return type_ == instruction_type::in || type_ == instruction_type::out; }

		instruction() : instruction(instruction_type::invalid, -1, -1) {}

		instruction(instruction_type type, int argument, int source_offset)
			:type_(type), argument_(argument), source_offset_(source_offset) {}
	};

#if 0 //not needed right now
	std::ostream& operator<<(std::ostream& str, instruction const &i);
#endif

	/*Structure representing a syntax error encountered during syntax validation.*/
	struct syntax_error {
		std::string message_; //string containing the error message 
		//line at which the syntax error occured; numbered from one
		int line_;
		//Index within this line at which the error happened; numbered from one
		int char_offset_;

		syntax_error(std::string msg, int line, int offset) : message_(std::move(msg)), line_(line), char_offset_(offset) {}
	};

	/*Traverses passed source code and searches for syntactic errors (mismatching brackets). Returns true if code is ok, false otherwise.
	Returns as soon as an error is found and does not return any additional information about occuring errors, therefore is this function
	the faster alternative if only syntax validity is in question.*/
	bool perform_syntax_check_quick(std::string_view const source_code);

	/*Traverses passed source code and searches for syntactic errors (mismatching brackets). Vector of
	found syntactic errors is returned. Has to go through the entire source code and returns all possible information,
	therefore this function is not appropriate is only validity of syntax is in question but specific errors are not requested. */
	std::vector<syntax_error> perform_syntax_check_detailed(std::string_view const source_code);
}