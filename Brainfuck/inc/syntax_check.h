#ifndef SYNTAX_CHECK_H
#define SYNTAX_CHECK_H
#pragma once

#include <string>
#include <vector>
#include <string_view>

namespace bf {

	/*Structure representing a syntax error encountered during syntax validation.*/
	struct syntax_error {
		std::string message_; //string containing the error message 
		//line at which the syntax error occured; numbered from one
		std::ptrdiff_t line_;
		//Index within this line at which the error happened; numbered from one
		std::ptrdiff_t char_offset_;

		template<typename TAG>
		syntax_error(TAG&& msg, std::ptrdiff_t const line, std::ptrdiff_t const offset)
			: message_{ std::forward<TAG>(msg) }, line_{ line }, char_offset_{ offset }
		{}


	};

	/*Relational operator for syntax errors. If the lines are same, the smaller syntax error is identified by lower char_offset.
	Otherwise orderes errors by line numbers. */
	[[nodiscard]]
	bool operator<(syntax_error const& lhs, syntax_error const& rhs) noexcept;

	/*Traverses passed source code and searches for syntactic errors (mismatching brackets).
	Returns true if the source_code is ok, false otherwise.
	Returns as soon as an error is found and does not return any additional information about occuring errors, this function is therefore
	the faster alternative if only syntax validity is in question.*/
	[[nodiscard]]
	bool syntax_validation_is_ok(std::string_view source_code);

	/*Traverses passed source code and searches for syntactic errors (mismatching brackets). Vector of found syntactic errors is returned.
	This function goes through the entire source code and returns all possible information, it is therefore not appropriate
	when only the validity of syntax is in question but specific errors are not requested. */
	[[nodiscard]]
	std::vector<syntax_error> syntax_validation_detailed(std::string_view source_code);
} //namespace bf

#endif