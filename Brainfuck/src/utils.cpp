#include "utils.h"

#include <regex>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <charconv>

namespace bf::utils {

	int check_command_argc(int const min, int const max, cli::command_parameters_t const &argv) {
		//we do not really care about overflows here, noone is gonna pass two billion arguments
		int const actual = static_cast<int>(argv.size());
		if (actual < min) { //At least min arguments expected
			cli::print_command_error(cli::command_error::argument_required);
			return 1;
		}
		if (actual > max) { //At most max arguments expected
			cli::print_command_error(cli::command_error::too_many_arguments);
			return 2;
		}
		return 0;
	}


	std::vector<std::string_view> split_to_tokens(std::string_view const str) {
		/*Static local regex matching argument tokens one by one. In case somebody wasn't sure how to read these, a brief explanation follows.
		\s*		any number of whitespace (skipped from selection)
		(		Submatch number one is the one we are interested in. It contains the string of non whitespace characters, which shall become the extracted token
		([^"]\S*?)  match anything but quotes and continue matching any number of non whitespace characters captured lazily
		|(".*?("|$))) or match quotes and any number of lazily matched characters followed by either quotes or EOL
		(\s|$)  either whitespace or end of line in case the last argument is being matched (skipped from selection)
		After the given string has been matched against this regex, we will have ended up with range of simple tokens and those more complex tokens
		preceded by quotes and possibly ending with quotes as well. We are obliged to to get rid of these before returning.
		*/
		static std::regex const token_regex{ R"(\s*(([^"]\S*?)|(".*?("|$)))(\s|$))", std::regex_constants::optimize | std::regex_constants::ECMAScript };
		using token_iterator = std::regex_token_iterator<std::string_view::const_iterator>; //I use this convenience using declaration to simplify the call to std::transform

		std::vector<std::string_view> tokens;

		//transform range represented by regex_token_iterators using lambda. The iterator traverses the string trying to match it against token_regex 
		std::transform(token_iterator{ str.begin(), str.end(), token_regex, 1 }, //and selecting just the first submatch on success
			token_iterator{}, std::back_inserter(tokens),  //results of transformations are being push_backed into the vector during the transformation
			[](std::sub_match<std::string_view::const_iterator> const &match) -> std::string_view { //take each sub_match and transform it into a string_view
				std::string_view token{ &*match.first, static_cast<std::size_t>(std::distance(match.first, match.second)) };
				if (token.front() == '\"') {   //if this submatch starts with quotes, we need to get rid of them by advancing the iterator one char forward
					token.remove_prefix(1);
					if (token.back() == '\"') //if it ends with quotes, move end of submatch one char back. We have to count to -1 to get the last character
						token.remove_suffix(1);
				} //return std::string_view constructed from pointer and number of characters between the beginning and end of match 
				return token;
			}
		);
		return tokens;
	}

	std::vector<std::string_view> split_to_lines(std::string_view str) {
		//This function has very similar implementation as function above (split_to_tokens), so excuse the lack of comments, it's 2:37 a.m.
		static std::regex const new_line_regex{ "\n" }; //matches any number of characters other that new-line
		using token_iterator = std::regex_token_iterator<std::string_view::const_iterator>;

		std::vector<std::string_view> result;

		//iterate over passed string_view trying to match regular expression. Call lambda on each match and append result to vector 
		//we are interested in submatch -1 (="stuff that was left unmatched" as stated at cppreference.com)
		std::transform(token_iterator{ str.begin(), str.end(), new_line_regex, -1 },
			token_iterator{}, std::back_inserter(result),
			[](std::sub_match<std::string_view::const_iterator>const &match) -> std::string_view {
				//constructs std::string_view from pointer and size by means of uniform initialization 
				//(offset of current match from the beginning of view and length of current match)
				return { &match.first[0], static_cast<std::size_t>(std::distance(match.first, match.second)) };
			}
		);

		return result;
	}

	std::string_view get_line(std::string_view const str, int line_num) {
		assert(line_num > 0);
		std::string_view::const_iterator after_new_line = str.begin();
		for (--line_num; line_num; --line_num) {
			after_new_line = std::find(after_new_line, str.end(), '\n');
			if (after_new_line == str.end())
				return { str.data(), 0 }; //invalid value
			std::advance(after_new_line, 1);
		}
		return { &*after_new_line, static_cast<std::size_t>(std::distance(after_new_line,
			std::find(std::next(after_new_line), str.end(), '\n'))) };
	}

	std::optional<std::string> read_file(std::string_view file_name) {
		std::filesystem::path name{ file_name };
		if (std::filesystem::exists(name) && !std::filesystem::is_directory(name)) {
			//allocate a buffer long enough for the content of file
			std::string file_content(static_cast<std::string::size_type>(std::filesystem::file_size(name)), '\0');
			std::ifstream{ name }.read(file_content.data(), file_content.size()); //read directly into buffer
			file_content.resize(std::strlen(file_content.c_str()));
			return file_content;
		}
		return std::nullopt; //empty std::optional in case file_name isn't a path to valid file
	}

	bool prompt_user_yesno() {
		std::cout << "Please, choose yes or no. [Y/N].\t";
		char input_char;
		do {
			std::cin >> input_char;
			input_char = std::toupper(input_char, std::locale{});
		} while (input_char != 'Y' && input_char != 'N'); //prompt until the user types something meaningful
		std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); //flush EOL from stdin

		return input_char == 'Y';
	}

	std::optional<int> parse_nonnegative_argument(std::string_view const view) {
		int result;
		if (std::from_chars(view.data(), view.data() + view.size(), result).ec != std::errc{}) {
			cli::print_command_error(cli::command_error::invalid_number_format);
			return std::nullopt;
		}
		if (result < 0) {
			cli::print_command_error(cli::command_error::non_negative_number_expected);
			return std::nullopt;
		}
		return result;
	}

	std::optional<int> parse_positive_argument(std::string_view const view) {
		if (std::optional<int> const result = parse_nonnegative_argument(view); !result.has_value() || result.value() == 0)
			return std::nullopt;
		else
			return result;
	}

}