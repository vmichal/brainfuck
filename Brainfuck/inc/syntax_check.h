#pragma once

#include"source_location.h"

#include <string>
#include <vector>
#include <string_view>

namespace bf {


	/*Structure representing a syntax error encountered during syntax validation.*/
	struct syntax_error {
		std::string message_; //string containing the error message 

		source_location location_;

		syntax_error(std::string msg, source_location const location)
			: message_{ std::move(msg) }, location_{ location }
		{}

	};

	/*Traverses passed source code and searches for syntactic errors (mismatching brackets).
	Returns true if the source_code is ok, false otherwise.
	Returns as soon as an error is found and does not return any additional information about occuring errors, this function is therefore
	the faster alternative if only syntax validity is in question.*/
	[[nodiscard]]
	bool is_syntactically_valid(std::string_view source_code);

	/*Traverses passed source code and searches for syntactic errors (mismatching brackets). Vector of found syntactic errors is returned.
	This function goes through the entire source code and returns all possible information, it is therefore not appropriate
	when only the validity of syntax is in question but specific errors are not requested. */
	[[nodiscard]]
	std::vector<syntax_error> syntax_validation_detailed(std::string_view source_code);
} //namespace bf

