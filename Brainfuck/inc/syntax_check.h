#pragma once

#include <string>
#include <vector>
#include <string_view>

namespace bf {

	/*Structure representing a syntax error encountered during syntax validation.*/
	struct syntax_error {
		std::string message_; //string containing the error message 
		//line at which the syntax error occured; numbered from one
		int line_;
		//Index within this line at which the error happened; numbered from one
		int char_offset_;

		template<typename A, typename B, typename C>
		syntax_error(A&& msg, B&& line, C&& offset) noexcept
			: message_(std::forward<A&&>(msg)), line_(std::forward<B&&>(line)), char_offset_(std::forward<C&&>(offset))
		{}


	};

	/*Relational operator for syntax errors. If the lines are same, the smaller syntax error is identified by lower char_offset.
	Otherwise orderes errors by line numbers. */
	bool operator<(syntax_error const& lhs, syntax_error const& rhs);

	/*Traverses passed source code and searches for syntactic errors (mismatching brackets).
	Returns true if the source_code is ok, false otherwise.
	Returns as soon as an error is found and does not return any additional information about occuring errors, this function is therefore
	the faster alternative if only syntax validity is in question.*/
	bool syntax_validation_is_ok(std::string_view const source_code);

	/*Traverses passed source code and searches for syntactic errors (mismatching brackets). Vector of found syntactic errors is returned.
	This function goes through the entire source code and returns all possible information, it is therefore not appropriate
	when only the validity of syntax is in question but specific errors are not requested. */
	std::vector<syntax_error> syntax_validation_detailed(std::string_view const source_code);
}