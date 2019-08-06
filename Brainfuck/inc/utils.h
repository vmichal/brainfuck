#pragma once

#include "cli.h"

#include <cassert>
#include <string_view>
#include <optional>
#include <vector>

//Defines static local boolean and asserts that the code is run just once
#define ASSERT_CALLED_ONLY_ONCE static bool ___first_time = true; assert(___first_time); ___first_time = false;

//Supplies a default option for a switch statement for which reaching the default option is an error
#define ASSERT_NO_OTHER_OPTION default: assert(false); throw 0;     /*shall not be reached by the control flow*/ 

#define MAKE_STRING_FROM_LINE_DETAIL(x) #x
#define MAKE_STRING_FROM_LINE(x) MAKE_STRING_FROM_LINE_DETAIL(x)
#define LINE_STRING MAKE_STRING_FROM_LINE(__LINE__)
//Defines a breanch or statement in execution that shouldn't have been reached. Crashes painfully whilst showing information about the crash
#define MUST_NOT_BE_REACHED do { assert(false); throw std::exception{"This statement shouldn't have been reached! " __FILE__ ":" LINE_STRING}; } while (0)

namespace bf::utils {

	//Conditionally return the first or second parameter depending on the value of count
	template<typename INT, typename = std::enable_if_t<std::is_integral_v<INT>>>
	inline constexpr char const *print_plural(INT count, char const * singular, char const * plural) {
		return count == static_cast<INT>(1) ? singular : plural;
	}

	//Conditionally return "s" if more than one of something is requested
	template<typename INT, typename = std::enable_if_t<std::is_integral_v<INT>>>
	inline constexpr char const *print_plural(INT count) {
		return print_plural(count, "", "s");
	}


	/*Splits passed string_view using whitespace as delimiters,
	removes all additional whitespace and returns tokens in a vector.
	Strings enclosed in quotes are interpreted as single tokens without splitting.
	If quotes are left unclosed, function behaves as if the closing quotes were appended.*/
	std::vector<std::string_view> split_to_tokens(std::string_view const str);

	/*Splits passed string_view using \n as delimiter and returns all lines.*/
	std::vector<std::string_view> split_to_lines(std::string_view const str);

	/*Returns string_view of line numbered line_num starting at one within the string str.
	New-line chars are not included.*/
	std::string_view get_line(std::string_view const str, int line_num);

	/*Read contents of file into a string and returns it. If file_name does not
	specify a valid path to file, empty std::optional is returned.*/
	std::optional<std::string> read_file(std::string_view const file_name);

	/*Prompts the user for an answer to a yes/no question.
	Returns true if user agrees, false otherwise*/
	bool prompt_user_yesno();

	/*Checks, whether actual number of parameters including command name is between min and max.
	In case it is not, prints error message and returns non zero.
	Returns zero on success, 1 if there are not enough arguments, 2 if there are too many.*/
	int check_command_argc(int const min, int const max, cli::command_parameters_t const& argv);

	/*Parses passed string_view as a signed integer returned in parameter out.
	Returns value initialized std::errc iff the parsing end successfully and the parsed number is not negative.
	Otherwise returns std::errc::invalid_argument and the value of out is unchanged.*/
	std::optional<int> parse_nonnegative_argument(std::string_view const view);

	/*Parses passed string_view as a signed integer returned in parameter out.
	Returns value initialized std::errc iff the parsing end successfully and the parsed number is positive.
	Otherwise returns std::errc::invalid_argument and the value of out is unchanged.*/

	std::optional<int> parse_positive_argument(std::string_view const view);


}