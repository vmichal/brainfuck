#include "syntax_check.h"
#include "cli.h"
#include "program_code.h"

#include <deque>
#include <algorithm>
#include <cassert>
namespace bf {


	bool operator<(syntax_error const& lhs, syntax_error const& rhs) noexcept {
		return lhs.line_ != rhs.line_ ? lhs.line_ < rhs.line_ : lhs.char_offset_ < rhs.char_offset_;
	}

	bool syntax_validation_is_ok(std::string_view const source_code) {

		//Has to perform a syntax check as fast as possible - only braces are counted, no additional information gets generated
		int opened_loops = 0; //counter of opened loops
		for (char const character : source_code)  //Compare each character of the source code with two possible loop instructions 
			if (character == '[')
				++opened_loops; //a new loop is opened
			else if (character == ']')
				if (opened_loops > 0) //if we have some loops to close, close one
					--opened_loops;
				else
					return false; //Closing a loop when there isn't any opened is a syntax error

		return opened_loops == 0; //source code's syntax is ok if there are no opened loops left
	}


	std::vector<syntax_error> syntax_validation_detailed(std::string_view const source_code) {
		std::vector<syntax_error> syntax_errors; //found syntax errors. Is returned at the end of the function
		std::deque<std::pair<int, int>> opened_loops; //deque used as a stack of pairs [line, character] == coordinates of the opening brace 
		int line_number = 1, char_offset = 0; //coordinates start at [1,1] for the first char of source code

		for (char const current_char : source_code) {//traverse each line char by char 
			++char_offset;
			switch (current_char) { //search for brackets 
			case '\n':
				++line_number; //move to the next line and reset char offset counter
				char_offset = 0;
				break;
			case '[':
				opened_loops.push_back({ line_number, char_offset }); //push current location onto the stack
				break;
			case ']':
				if (!opened_loops.empty()) //we are inside a loop => close it
					opened_loops.pop_back();
				else //no loops opened, generate an error message
					syntax_errors.emplace_back("Unexpected token ']' without matching brace", line_number, char_offset);
				break;
			}
		}

		syntax_errors.reserve(syntax_errors.size() + opened_loops.size()); //source code traversal finished => we now know how many syntax errors there are
		auto const first_sequence_end = std::next(syntax_errors.begin(), syntax_errors.size()); //middle iterator for inplace merging (end of the first sorted range)
		//TODO this ^^^^^^^^^^^^^^^^^^^^^^^^^ may be runtime error on debug confguration (MSVC)

		for (auto const [line, column] : opened_loops) //traverse the stack of opened loops FROM THE BEGINNING and generate error messages
			syntax_errors.emplace_back("Unmatched token '[' without closing brace", line, column);

		//sanity check that both sequences are sorted
		assert(std::is_sorted(syntax_errors.begin(), first_sequence_end));
		assert(std::is_sorted(first_sequence_end, syntax_errors.end()));

		//sort syntax errors by their location. Sort first by line, if lines match, sort by error offset. 
		std::inplace_merge(syntax_errors.begin(), first_sequence_end, syntax_errors.end());

		return syntax_errors;
	}

} // namespace bf
