#include "syntax_check.h"
#include "cli.h"
#include "syntax_tree.h"
#include <stack>
#ifdef _MSC_VER
#include <execution>
#endif
#include <algorithm>
#include <cassert>
#include <memory>
#include <iostream>
#include <iomanip>
#include <fstream>

namespace bf {

	bool instruction::is_foldable() const {
		static std::unordered_set<instruction_type> foldable{
			instruction_type::dec,
			instruction_type::inc,
			instruction_type::right,
			instruction_type::left
		};
		return foldable.count(type_);
	}

	std::ostream& operator<<(std::ostream& str, instruction_type t) {
		static std::unordered_map<instruction_type, const char*> strings = {
			{instruction_type::inc,					 "inc"},
			{instruction_type::dec,					 "dec"},
			{instruction_type::left,			    "left"},
			{instruction_type::right,			   "right"},
			{instruction_type::loop_begin,	  "loop_begin"},
			{instruction_type::loop_end,		"loop_end"},
			{instruction_type::in,					 "in" },
			{instruction_type::out,					 "out"},
			{instruction_type::breakpoint,    "breakpoint"}
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

	bool perform_syntax_check_quick(std::string_view const source_code) {
		//Has to perform much faster syntactic checking
		int opened_loops = 0; //function only counts number of currently opened loops
		for (char const c : source_code)  //each char is compared with two possible loop instructions 
			if (c == '[')
				++opened_loops; //open new loop
			else if (c == ']')
				if (opened_loops) //if we have some loops to close, close one
					--opened_loops;
				else
					return false; //we are closing unopened loop => error
		return opened_loops == 0; //source code's syntax is ok if there are no opened loops left
	}


	std::vector<syntax_error> perform_syntax_check_detailed(std::string_view const source_code) {
		std::vector<syntax_error> errors;
		std::stack<std::pair<int, int>> loops; //stack of pairs [line, character]
		int line = 1, character = 0;
		for (char const c : source_code) {//traverse each line char by char 
			++character;
			switch (c) { //search for brackets 
			case '\n':
				++line;
				character = 0;
				break;
			case '[':
				loops.push({ line, character }); //push current location on the stack
				break;
			case ']':
				if (!loops.empty()) //we are inside a loop => close it
					loops.pop();
				else //no loops opened, add error message
					errors.emplace_back("Unexpected token ']', no loop currently opened", line, character);
				break;
			}
		}
		while (!loops.empty()) {  //while we have unclosed loops, pop one and add error message
			auto[line, ch] = loops.top();
			loops.pop();
			errors.emplace_back("Unmatched token '[' without closing brace", line, ch);
		}
		std::sort(
#ifdef _MSC_VER //Clang and GCC don't like std::execution
			std::execution::par_unseq,
#endif			
			errors.begin(), errors.end(), //sort syntax_errors by their location, may be executed in parallel and unsequenced
			[](syntax_error const &a, syntax_error const &b) -> bool {
				return a.line_ == b.line_ ? a.char_offset_ < b.char_offset_ : a.line_ < b.line_;
			}); //if lines are the same, orderes by character index. Otherwise orderes errors by line numbers
		return errors;
	}
} //namespace bf