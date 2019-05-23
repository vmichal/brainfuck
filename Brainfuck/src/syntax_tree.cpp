#include "syntax_tree.h"
#include <stack>
#include <cassert>


namespace bf {


	void syntax_tree::add_instruction(instruction const&i) {
		push_back(i);
	}

	void syntax_tree::add_instruction(instruction && i) {
		push_back(i);
	}

	void syntax_tree::add_instruction(instruction_type type, int argument, int source_offset) {
		emplace_back(type, argument, source_offset);
	}


	void syntax_tree::relocate_jump_targets() {

		std::stack<int> loops;

		for (int offset = 0; offset < static_cast<int>(size()); offset++)
			if (instruction_type type = at(offset).type_; type == instruction_type::loop_begin) //open new loop
				loops.push(offset); //save this bracket's offset
			else if (type == instruction_type::loop_end) {//if we find an end, set targets for both opening as well as closing bracket
				assert(!loops.empty());
				at(loops.top()).argument_ = offset + 1;
				at(offset).argument_ = loops.top() + 1;
				loops.pop(); //and close the loop
			} //other instructions are ignored

		assert(loops.empty()); //Valid program must have empty loop stack
	}
}