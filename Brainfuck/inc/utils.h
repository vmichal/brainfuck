#ifndef UTILS_H
#define UTILS_H
#pragma once

#include "cli.h"

#include <cassert>
#include <string_view>
#include <optional>
#include <vector>
#include <algorithm>
#include <cassert>

//Defines static local boolean and asserts that the code is run just once
#define ASSERT_IS_CALLED_ONLY_ONCE static bool _____first_time___ = true; assert(_____first_time___); _____first_time___ = false;


#define MAKE_STRING_FROM_LINE_DETAIL(x) #x
#define MAKE_STRING_FROM_LINE(x) MAKE_STRING_FROM_LINE_DETAIL(x)
#define LINE_STRING MAKE_STRING_FROM_LINE(__LINE__)

//Supplies a default option for a switch statement for which reaching the default option is an error
#define ASSERT_NO_OTHER_OPTION default: assert(false); \
	throw std::exception{"There shouldn't have been any other branch in this switch statement. " __FILE__ ":" LINE_STRING};     /*shall not be reached by the control flow*/ 

//Defines a breanch or statement in execution that shouldn't have been reached. Crashes painfully whilst showing information about the crash
#define MUST_NOT_BE_REACHED do { assert(false); throw std::exception{"This statement shouldn't have been reached! " __FILE__ ":" LINE_STRING}; } while (0)

namespace bf::utils {

	//Conditionally return the first or second parameter depending on the value of count
	template<typename INT, typename = std::enable_if_t<std::is_integral_v<INT>>> [[nodiscard]]
		inline constexpr char const* print_plural(INT const count, char const* const singular, char const* const plural) {
		return count == static_cast<INT const>(1) ? singular : plural;
	}

	//Conditionally return "s" if more than one of something is requested
	template<typename INT, typename = std::enable_if_t<std::is_integral_v<INT>>> [[nodiscard]]
		inline constexpr char const* print_plural(INT const count) {
		return print_plural(count, "", "s");
	}


	/*Splits passed string_view using whitespace as delimiters,
	removes all additional whitespace and returns tokens in a vector.
	Strings enclosed in quotes are interpreted as single tokens without splitting.
	If quotes are left unclosed, function behaves as if the closing quotes were appended.*/
	[[nodiscard]]
	std::vector<std::string_view> split_to_tokens(std::string_view str);

	/*Splits passed string_view using \n as delimiter and returns all lines.*/
	[[nodiscard]]
	std::vector<std::string_view> split_to_lines(std::string_view str);

	/*Returns string_view of line numbered line_num starting at one within the string str.
	New-line chars are not included.*/
	[[nodiscard]]
	std::optional<std::string_view> get_line(std::string_view str, std::ptrdiff_t line_num);

	/*Read contents of file into a string and returns it. If file_name does not
	specify a valid path to file, empty std::optional is returned.*/
	[[nodiscard]]
	std::optional<std::string> read_file(std::string_view file_name);

	/*Prompts the user for an answer to a yes/no question.
	Returns true if user agrees, false otherwise*/
	[[nodiscard]]
	bool prompt_user_yesno();

	/*Checks, whether actual number of parameters including command name is between min and max.
	In case it is not, prints error message and returns non zero.
	Returns zero on success, 1 if there are not enough arguments, 2 if there are too many.*/
	[[nodiscard]]
	int check_command_argc(std::ptrdiff_t min, std::ptrdiff_t max, cli::command_parameters_t const& argv);

	/*Attempts to parse passed string_view as a non-negativesigned integer. Returns empty optional on fail.*/
	[[nodiscard]]
	std::optional<int> parse_nonnegative_argument(std::string_view view);


	/*Attempts to parse passed string_view as a positive signed integer. Returns empty optional on fail.*/
	[[nodiscard]]
	std::optional<int> parse_positive_argument(std::string_view view);

	/*Attempts to parse passed string_view as signed integer. Returns empty optional on fail.*/
	[[nodiscard]]
	std::optional<int> parse_int_argument(std::string_view view);

	template<typename Iter, typename Pred>
	struct ranges_iterator {
	private:

		Iter begin_;
		Iter const end_;
		Pred pred_;

		struct sentinel_t {};

	public:

		ranges_iterator(Iter const begin, Iter const end, Pred pred)
			: end_{ end }, pred_{ std::move(pred) } {
			begin_ = std::find_if(begin, end, pred);
		}

		std::pair<Iter, Iter> operator*() {
			assert(pred_(*begin_));

			auto const beg = begin_;
			auto const end = std::find_if_not(begin_, end_, pred_);

			begin_ = std::find_if(end, end_, pred_);
			return { beg, end };
		}

		void operator++() {}

		bool operator!=(sentinel_t) const {
			return begin_ != end_;
		}

		ranges_iterator& begin() { return *this; }
		sentinel_t end() { return {}; };
	};

	template<typename Iter, typename Pred>
	ranges_iterator<Iter, Pred> iterate_ranges_if(Iter begin, Iter end, Pred pred) {
		return { begin, end, pred };
	}

} //namespace bf::utils 

#endif