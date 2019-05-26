#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <unordered_set>

namespace bf {

	

	

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
	bool syntax_check_quick_is_ok(std::string_view const source_code);

	/*Traverses passed source code and searches for syntactic errors (mismatching brackets). Vector of
	found syntactic errors is returned. Has to go through the entire source code and returns all possible information,
	therefore this function is not appropriate is only validity of syntax is in question but specific errors are not requested. */
	std::vector<syntax_error> perform_syntax_check_detailed(std::string_view const source_code);
}