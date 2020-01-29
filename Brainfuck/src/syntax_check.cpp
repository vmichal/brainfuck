#include "syntax_check.h"
#include "cli.h"
#include "program_code.h"

#include <deque>
#include <algorithm>
#include <cassert>
namespace bf {


	bool operator<(source_location const& lhs, source_location const& rhs) noexcept {
		return lhs.line_ != rhs.line_ ? lhs.line_ < rhs.line_ : lhs.column_ < rhs.column_;
	}

	bool is_syntactically_valid(std::string_view const source_code) {

		//Has to perform a syntax check as fast as possible - only braces are counted, no additional information gets generated
		int opened_loops = 0; //counter of opened loops
		for (char const character : source_code)  //Compare each character of the source code with two possible loop instructions 
			if (character == '[')
				++opened_loops; //a new loop is opened
			else if (character == ']')
				if (opened_loops) //if we have some loops to close, close one
					--opened_loops;
				else
					return false; //Closing a loop when there isn't any opened is a syntax error

		return opened_loops == 0; //source code's syntax is ok if there are no opened loops left
	}

	static_assert(cli::TAB_WIDTH > 0 && (cli::TAB_WIDTH & (cli::TAB_WIDTH - 1)) == 0,
		"::bf::cli::TAB_WIDTH must be a power of two for ::bf::syntax_validation_detailed'S implementation."); 


	std::vector<syntax_error> syntax_validation_detailed(std::string_view const source_code) {
		std::vector<syntax_error> syntax_errors; //found syntax errors
		std::deque<source_location> opened_loops; //stack of coordinates of opening braces 
		source_location current_loc{ 1,0 };

		for (char const character : source_code) {//traverse whole source code char by char counting whitespaces and validating loops
			++current_loc.column_;

			switch (character) {
			case '\n': current_loc = { current_loc.line_ + 1, 0 }; break;
			case '\t': //tab == 8 spaces or less ('\t' aligns to a multiple of eight)
				current_loc.column_ = (current_loc.column_ + ::bf::cli::TAB_WIDTH) & ~std::ptrdiff_t(::bf::cli::TAB_WIDTH - 1);
				break;
			case '[':
				opened_loops.push_back(current_loc); //push current location onto the stack
				break;
			case ']':
				if (!opened_loops.empty()) //we are inside a loop => close it
					opened_loops.pop_back();
				else //no loops opened, generate an error message
					syntax_errors.emplace_back("Unexpected token ']' not preceded by a matching '['", current_loc);
				break;
			}
		}

		if (opened_loops.empty()) //If there's no unclosed loop, we have found all errors and we can return early
			return syntax_errors;

		/*We know that there is exactly opened_loops.size() >= 1 more errors and both sequences are sorted by their source location.
		Generate remaining syntax errors from unclosed loops and merge both sequences together. */

		syntax_errors.reserve(syntax_errors.size() + opened_loops.size());
		std::size_t const first_seq_length = syntax_errors.size();

		for (auto const& location : opened_loops) //traverse the stack of opened loops FROM THE BEGINNING and generate error messages
			syntax_errors.emplace_back("Unmatched token '[' without matching closing brace ']'", location);

		auto const middle_iterator = std::next(syntax_errors.begin(), first_seq_length);

		std::inplace_merge(syntax_errors.begin(), middle_iterator, syntax_errors.end(),
			[](auto const& a, auto const& b)-> bool {return a.location_ < b.location_; });

		return syntax_errors;
	}

} // namespace bf
