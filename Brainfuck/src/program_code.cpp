#include "program_code.h"
#include <stack>
#include <cassert>


namespace bf {


	bool instruction::is_foldable() const {
		static std::unordered_set<op_code> const foldable{
			op_code::inc,
			op_code::dec,
			op_code::left,
			op_code::right
		};
		return foldable.count(op_code_);
	}

	std::ostream& operator<<(std::ostream& str, op_code t) {
		static std::unordered_map<instruction_type, const char*> strings = {
			{instruction_type::nop,                  "nop"},
			{instruction_type::inc,					 "inc"},
			{instruction_type::dec,					 "dec"},
			{instruction_type::left,			    "left"},
			{instruction_type::right,			   "right"},
			{instruction_type::loop_begin,	  "loop_begin"},
			{instruction_type::loop_end,		"loop_end"},
			{instruction_type::in,					 "in" },
			{instruction_type::out,					 "out"},
			{instruction_type::breakpoint,    "breakpoint"},
			{instruction_type::load_const,     "load_const"}
		};
		assert(strings.count(t));

		return str << strings.at(t);
	}

#if 0 //not needed right now
	std::ostream& operator<<(std::ostream& str, instruction const &i) {
		return str << i.source_offset_ << ": {" << std::setw(12) << i.type_ << ' '
			<< std::left << std::setw(8) << i.argument_ << '}';
	}
#endif 

	void syntax_tree::add_instruction(instruction const&i) {
		push_back(i);
	}

	void syntax_tree::add_instruction(instruction && i) {
		push_back(i);
	}

	void syntax_tree::add_instruction(op_code type, int argument, int source_offset) {
		emplace_back(type, argument, source_offset);
	}


	void syntax_tree::relocate_jump_targets() {

		std::stack<int> loops;

		for (int offset = 0; offset < static_cast<int>(size()); offset++)
			if (op_code type = at(offset).op_code_; type == op_code::loop_begin) //open new loop
				loops.push(offset); //save this bracket's offset
			else if (type == op_code::loop_end) {//if we find an end, set targets for both opening as well as closing bracket
				assert(!loops.empty());
				at(loops.top()).argument_ = offset + 1;
				at(offset).argument_ = loops.top() + 1;
				loops.pop(); //and close the loop
			} //other instructions are ignored

		assert(loops.empty()); //Valid program must have empty loop stack
	}
}