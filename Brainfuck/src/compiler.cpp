#include "compiler.h"
#include "syntax_check.h"
#include "cli.h"

#include <iostream>
#include <algorithm>
#include <cassert>
#include <string_view>
#include <charconv>
#include <iomanip>

namespace bf {


	namespace last_compilation {

		/*Internal unique pointer to the result of last compilation. After the first compilation finishes, a meaningful value is set;
		until then contains nullptr.*/
		std::unique_ptr<compilation_result> result;

		std::string &code() {
			assert(ready());
			return result->code_;
		}

		std::vector<syntax_error> & errors() {
			assert(ready());
			return result->errors_;
		}

		std::optional<::bf::syntax_tree> & syntax_tree_opt() {
			assert(ready());
			return result->syntax_tree_;
		}

		bf::syntax_tree & syntax_tree() {
			assert(ready());
			return result->syntax_tree_.value();
		}

		bool successful() {
			assert(ready());
			assert(result->errors_.empty() == result->syntax_tree_.has_value());
			return result->syntax_tree_.has_value();
		}

		bool ready() {
			return static_cast<bool>(result);
		}
	}

	std::optional<syntax_tree> generate_syntax_tree(std::string_view const code) {
		if (!perform_syntax_check_quick(code)) //if syntax check fails, return empty optional
			return std::nullopt;

		std::optional<syntax_tree> tree{ std::in_place }; //construct syntax_tree in place inside the optional

		tree->reserve(static_cast<int>(code.size())); //reserve enough space for instructions. They are converted 1:1, therefore entire code.size() may be required

		int source_offset = -1;
		for (char source_char : code) { //loop though the code char by char
			++source_offset;
			switch (source_char) { //and add new instruction to the tree
			case '+': tree->add_instruction(instruction_type::inc, 1, source_offset); break;
			case '-': tree->add_instruction(instruction_type::dec, 1, source_offset); break;
			case '<': tree->add_instruction(instruction_type::left, 1, source_offset); break;
			case '>': tree->add_instruction(instruction_type::right, 1, source_offset); break;
			case ',': tree->add_instruction(instruction_type::in, 0, source_offset); break;
			case '.': tree->add_instruction(instruction_type::out, 0, source_offset); break;
				//targets of jumps are left invalid and relocated afterward
			case ']': tree->add_instruction(instruction_type::loop_end, 0xdead'beef, source_offset); break;
			case '[': tree->add_instruction(instruction_type::loop_begin, 0xdead'beef, source_offset); break;
			}
		}
		tree->relocate_jump_targets(); //make sure jumps are correctly relocated
		return tree;
	}

	namespace {

		/*Wrapper namespace for types and functions for compile_callback. One shall not pollute global namespace.*/
		namespace compile_callback_helper {
			/*Tries to validate and compile given code and set global variable last_compilation_result
			according to the compilation's outcome.	If there are no errors, new syntax_tree is generated,
			otherwise only list of errors is saved. Returns true if compilation was OK.*/
			bool do_compile(std::string code) { //takes code by value, because it is moved later

				if (perform_syntax_check_quick(code)) { //first perform quick scan for errors. If there are none, proceed with compilation
					std::optional<syntax_tree> hopefully_contains_tree = generate_syntax_tree(code);
					assert(hopefully_contains_tree.has_value()); //must be true, as the code had already undergone a syntax check
					last_compilation::result = std::make_unique<compilation_result>(std::move(code), std::vector<syntax_error>{},
						std::move(hopefully_contains_tree)); //move the whole optional containing syntax_tree
					return true; //return true indicating that compilation did not encounter any errors
				}
				else { //quick scan found some errors. Scan again collecting all possible information
					std::vector<syntax_error> errors = perform_syntax_check_detailed(code);
					assert(errors.size()); //must contain some errors; we can assert this just for fun :D
					last_compilation::result = std::make_unique<compilation_result>(std::move(code), std::move(errors), std::nullopt); //empty optional, there can be no syntax_tree for illegal code
					return false; //indicate that compilation failed
				}
			}

			/*Parses argument passed to compile_callback and returns pair of source code and error code. If parsing succeeds, err_code is zero
			and the string contains source code for a program.*/
			std::pair<std::string, cli::command_error> parse_arguments(std::string_view const source, std::string_view const arg) {
				if (source.compare("file") == 0) //the first arg is "file", therefore the second one is file_name
					if (std::optional<std::string> file_content = cli::read_file(arg); file_content.has_value())
						return { std::move(*file_content), cli::command_error::ok }; //if reading was ok and file_content has value, steal string from file_content
					else  //file does not exist
						return { std::string{}, cli::command_error::file_not_found };
				else if (source.compare("code") == 0) //second arg is raw source code
					return { std::string{ arg }, cli::command_error::ok };
				else  //first argument was not recognized, return err
					return { std::string{}, cli::command_error::argument_not_recognized };
			}
		}


		/*Function callback for cli command "compile".
		Expects two arguments, first is specification whether code or filename is specified, the second being either
		the source code or name of file. The function first checks whether the code denotes a valid program and then compiles
		it to symbolic instruction. If compilation succeeds, program in saved internally and may be flashed to cpu emulator'r memory
		using the command "install"*/
		int compile_callback(cli::command_parameters_t const& argv) {
			namespace helper = compile_callback_helper;
			if (int const ret_code = cli::check_command_argc(3, 3, argv.size()); ret_code)
				return ret_code; //there must be three arguments

			auto[source_code, err_code] = helper::parse_arguments(argv[1], argv[2]);
			if (err_code != cli::command_error::ok) {
				cli::print_command_error(err_code);
				return static_cast<int>(err_code);
			}
			//call helper function trying to compile the source code
			bool const success = helper::do_compile(std::move(source_code));
			if (!success) {//compilation failed due to errors
				std::size_t const err_count = last_compilation::errors().size();
				std::cout << "Found " << err_count << " error" << cli::print_plural(err_count) << ". You may print more details using the \"errors\" command.\n";
			}
			else {  //compilation was successful 
				std::size_t const instruction_count = last_compilation::syntax_tree().size();
				std::cout << "Successfully compiled " << instruction_count << " instruction" << cli::print_plural(instruction_count) << ".\n";
			}
			return success;
		}

		/*Wrapper namespace for types and functions used by "errors_callback". Does not pollute global namespace, thankfully*/
		namespace errors_callback_helper {

			int print_error_detail(int index) {
				assert(last_compilation::ready() && !last_compilation::successful()); //if there was no compilation or it completed ok, we have an error

				if (int error_count = static_cast<signed>(last_compilation::errors().size()); index >= error_count) {
					std::cout << "Requested index " << index << " is out of bounds. Valid range is 0 though " << error_count - 1 << '\n';
					return 5;
				}
				syntax_error const& error = last_compilation::errors()[index];
				std::string line{ cli::get_line(last_compilation::code(), error.line_) };


				std::replace(line.begin(), line.end(), '\t', ' ');
				int chars_before = error.char_offset_ - 1;
				//int chars_after = line.size() - error.char_offset_;
				std::cout << std::right << std::setw(5) << index << std::left << "  Syntax error: " << error.message_ << " at (" << error.line_ << ", " << error.char_offset_
					<< ") {\n\t" << line << "\n\t" << std::string(chars_before, ' ') << "^\n}\n";
				return 0;
			}

			/*Prints all syntax errors to stdout. If there had been no errors, does nothing.
			if print_full is true, prints full information about all errors.*/
			int print_syntax_errors(bool print_full) {
				assert(last_compilation::ready() && !last_compilation::successful()); //if there was no compilation or it completed ok, we have an error

				if (print_full) {
					for (int i = 0; i < static_cast<int>(last_compilation::errors().size()); ++i)
						print_error_detail(i);
					return 0;
				}
				else {
					int index = 0;
					for (auto const& error : last_compilation::errors())
						std::cout << std::right << std::setw(5) << index++ << std::left << ". syntax error: "
						<< error.message_ << " at (" << error.line_ << ", " << error.char_offset_ << ").\n";
					return 0;
				}
			}
		}

		/*Function callback for cli command "errors".
		Expects single argument and depending on its value does various stuff and prints all syntax errors if some compilation had alredy been performed
		If there had been no compilation performed, function just returns.*/
		int errors_callback(cli::command_parameters_t const& argv) {
			namespace helper = errors_callback_helper;
			if (int const ret_code = cli::check_command_argc(2, 2, argv.size()); ret_code)
				return ret_code;
			if (!last_compilation::ready()) {//there hasn't been any compilation performed yet
				std::cerr << "No compilation has been performed. Compile the program with the \"compile\" command first.\n";
				return 3;
			}
			if (last_compilation::successful()) {
				std::cout << "Previous compilation was successful.\n";
				return 0;
			}
			std::string_view const& arg = argv[1];
			if (arg.compare("all") == 0)
				return helper::print_syntax_errors(false);
			else if (arg.compare("full") == 0)
				return helper::print_syntax_errors(true);
			else if (arg.compare("count") == 0) {
				if (std::size_t const count = last_compilation::errors().size(); count == 1)
					std::cout << "There has been one error.\n";
				else
					std::cout << "There have been " << last_compilation::errors().size() << " errors.\n";
				return 0;
			}
			else if (int index; std::from_chars(argv[1].data(), argv[1].data() + argv[1].size(), index).ec == std::errc{})
				return helper::print_error_detail(index);
			else {
				cli::print_command_error(cli::command_error::argument_not_recognized);
				return 4;
			}
		}
	}


	void compiler_initialize() {
		using namespace bf::cli;
		cli::add_command("compile", command_category::compilation, "Compiles given source code.",
			"Usage: \"compile\" [\"code\" or \"file\"] argument\n"
			"argument is either string of characters interpreted as source code if \"code\" is specified\n"
			"or a name of file containing the source code in case \"file\" is specified.\n"
			"Additional information about the outcome of the compilation can be queried by commands from the \"compilation\" group."
			, &compile_callback);

		cli::add_command("errors", command_category::compilation, "Queries the results of previous compilation and prints syntax errors.",
			"Usage: \"errors\" argument\n"
			"Single argument is expected and its meaning is heavily dependent on context.\n"
			"\targument == \"all\" => list of syntax errors is simply printed out\n"
			"\targument == \"full\" => similar list is printed, but every error is printed with all possible information\n"
			"\targument == \"count\" => prints number of syntax errors\n"
			"\targument == non-negative number => prints information about a single error specified by the number\n"
			"\targument value of \"full\" has therefore the same effect as consecutive calls of this command specifying err numbers in increasing order.\n"
			, &errors_callback);
	}


}