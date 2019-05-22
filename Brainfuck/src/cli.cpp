#include "cli.h"
#include "emulator.h"
#include <map>
#include <charconv>
#include <vector>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <regex>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <vector>
#include <climits>

namespace bf::cli {

	/*Structure describing a command. Most importantly it contains callback and pointer to another command, which is used as hook.*/
	class command {
		std::string name_; //name of this command. It is useful to know which command we are operating on in case we get pointer using alias 
		std::string	short_doc_;  //short description of this command. Gets printed by help command when an overview of all defined commands is requested
		std::string help_; //Full help for this command including usage and meaning of its arguments
		command_category category_;  //value indicating which command group this command belongs to

		callback_t callback_;   //callback function, which executes when this command is executed.
		command * hook_ = nullptr;  //pointer to hook for this command. If none is specified, this pointer is null

	public:

		command(std::string name, command_category category, std::string doc, std::string help, callback_t callback)
			:name_{ std::move(name) }, short_doc_{ std::move(doc) }, help_{ std::move(help) }, category_{ category }, callback_{ std::move(callback) }, hook_{ nullptr } {}

		[[nodiscard]] std::string& name() { return name_; }
		[[nodiscard]] std::string const& name() const { return name_; }

		[[nodiscard]] std::string& short_doc() { return short_doc_; }
		[[nodiscard]] std::string const& short_doc() const { return short_doc_; }

		[[nodiscard]] std::string& help() { return help_; }
		[[nodiscard]] std::string const& help() const { return help_; }

		[[nodiscard]] command_category category() const { return category_; }

		[[nodiscard]] callback_t& callback() { return callback_; }
		[[nodiscard]] callback_t const& callback() const { return callback_; }

		[[nodiscard]] command *&hook() { return hook_; }
		[[nodiscard]] command * hook() const { return hook_; }

		[[nodiscard]]
		std::string get_combined_full_help() const {
			//Format the message using stringstream and return the final result
			std::ostringstream stream;
			stream << "Help for command " << std::quoted(name_) << " of the " << category_ << " category:\n" << short_doc_ << '\n' << help_ << std::endl;
			return stream.str();
		}

	};

	/*Global map which maps command name to command data such as help and callable code.*/
	std::map<std::string, command> cmd_map;
	/*Global map which maps command aliases to corresponding structures. Thanks to this it is possible
	to introduce an alias for command that hasn't yet been defined. Possible alternative would be to have
	command* as value type, but in that case it would be necessary to assure that commans are first defined.*/
	std::map<std::string, std::string> cmd_aliases;

	namespace {

		struct cli_history {
			std::vector<std::string> previous_commands_;
			bool repeat_previous_command_ = true;
		} cli_history;

		/*Searches for string "name" in list of commands and returns pointer to the requested one.
		If it does not exist, returns nullptr.*/
		command* get_command(std::string const& cmd_name) {
			//IIFE to guarantee successful initialization of vector and allocation of memory
			//must be thread local to correctly handle multiple threads performing a search simultaneously
			static thread_local std::vector<std::string_view> recursive_searches = [] {std::vector<std::string_view> vec; vec.reserve(4096); return vec; }();
			if (is_command(cmd_name)) //if cmd_name is primary command and it has been defined so
				return &cmd_map.at(cmd_name); //return pointer to the command
			if (is_command_alias(cmd_name)) { //else if it's an alias for primary command
				if (recursive_searches.size() >= 4000) { //if we nest too deep into the recursion, just break out of it
					std::cerr << "Too many recursive calls requested. I guess there is some sorcery involved.\n";
					return nullptr;
				}
				//If we have found a string (seemingly alias), which has already been tested before, recusrion has been proven. Break out of it
				if (std::any_of(recursive_searches.begin(), recursive_searches.end(),
					[&cmd_name](std::string_view const view) { return view.compare(cmd_name) == 0; })) {
					std::cout << "You thought that recursive alias will blow my program up, right? Wrong. It has raken me just "
						<< recursive_searches.size() << " recursive call" << print_plural(recursive_searches.size())
						<< " to reveal your evil plans.\n";
					return nullptr;
				}
				recursive_searches.emplace_back(cmd_name);
				//call this function recursively to search. Alias of an alias is allowed
				command * const ptr = get_command(cmd_aliases.at(cmd_name));
				recursive_searches.clear(); //delete the list of recursive searches to make the function ready for next search
				return ptr;
			}
			return nullptr; //otherwise return null
		}

		command * get_command(std::string_view const cmd_name) {
			//delegate to overloaded function using temporary string
			return get_command(std::string{ cmd_name });
		}

		/*Callback function for the CLI command "quit".
		Takes one optional parameter, which is interpreted as return value for the OS*/
		int quit_callback(command_parameters_t const& argv) {
			if (int const code = check_command_argc(1, 2, argv.size()); code)
				return code; //Accepts name of command plus one optional argument
			if (argv.size() == 2) //Optional argument had been specified
				if (int ret_code; std::from_chars(argv[1].data(), argv[1].data() + argv[1].size(), ret_code).ec != std::errc{}) {
					//Argument contained invalid characters => print error and return
					print_command_error(command_error::argument_not_recognized);
					return 2;
				}
				else //Convert legal string argument to number and exit program 
					std::exit(ret_code);
			//No argument had been specified, return zero to OS
			std::exit(0);
		}

		/*Namespace wrapping helper functions and types for "help_callback" function. Polluting global namesapce is illegal!*/
		namespace help_callback_helper {

			/*Returns a string containing some general advice on how to use the program*/
			[[nodiscard]]
			std::string get_general_help() {
				return "Basic features of the program:\n"
					"1) If the first character of a command is exclamation mark (!), it is stripped and the remaining string gets executed by the operating system's shell.\n"
					"2) CLI commands usually expect parameters, which are described in their help message. Don't be afraid to use the \"help\" command a lot, it is the "
					"best way to learn.\n"
					"3) The CLI keeps it's history of executed commands. It is possible to browse it using the \"history\" command and it is possible to retrospectively execute"
					" previous commands as well.\n"
					"4) There are hooks. Hooks are normal commands that have been linked with another one and get executed automatically right after it."
					"To learn more about hooks and the way they are defined, see the \"define\" command.\n"
					"5) There is a pseudocommand called \"stop\". It cannot be deleted or changed and is automatically executed every time the emulator's execution stops."
					"You can define a hook for this command, which is enables you for example to print the state of CPU and memory every time the execution hits"
					"a breakpoint, executes a instruction in single-step mode, receives OS interrupt and so on.\n";
			}

			/*Returns string containing short help for all defined commands. Called from help_command.*/
			[[nodiscard]]
			std::string get_all_commands_help() {
				std::ostringstream stream;

				//Append names and short description one per line
				if (cmd_map.size()) {
					stream << "\nDefined commands:\n";
					stream << std::setw(20) << std::left << "name" << std::setw(15) << "category" << "short description\n\n";
					for (auto const &[name, command] : cmd_map)
						stream << std::setw(20) << name << std::setw(15) << command.category() << command.short_doc() << '\n';
				}
				//lambda used to print a bit of information about targets of aliaes
				auto get_alias_target = [](std::string const& cmd_name) -> std::string {
					if (is_command(cmd_name))
						return "primary command";
					if (is_command_alias(cmd_name))
						return "command alias";
					return "unknown";
				};
				//Append names of aliases and the aliased commands
				if (cmd_aliases.size()) {
					stream << "\nDefined aliases:\n";
					for (auto const &[alias, name] : cmd_aliases)
						stream << std::setw(15) << std::quoted(alias) << "for " << std::setw(20) << get_alias_target(name) << std::setw(20) << std::quoted(name) << ' ' << '\n';
				}
				return stream.str();
			}

			/*Prints help of single given command. If it does not exist, returns 2, otherwise zero.*/
			[[nodiscard]]
			int print_command_help(std::string_view const cmd_name) {
				if (command const * const command = get_command(cmd_name); command) {//If command exists, print it's help
					if (is_command_alias(cmd_name)) //Name of *command is differrent from requested, that means an alias was requested
						std::cout << std::quoted(cmd_name) << " is an alias for command " << std::quoted(command->name()) << '\n';
					std::cout << command->get_combined_full_help();
					return 0;
				}
				//If the command does not exist,  return value becomes two and error is printed
				std::cerr << "Command " << std::quoted(cmd_name) << " does not exist. Try \"help\"\n";
				return 2;
			}

		}

		/*Callback function for the CLI command "help"
		Accepts single optional argument. If it is specified, attempts to retrieve help message for given command and print it,
		if given command is not defined, notifies user about this by printing error message.
		If no argument or string "all" is passed, prints list of all commands.
		Returns zero on success. If function receives too many arguments, returns 1. If specified command does not exist, returns 2.
		*/
		int help_callback(command_parameters_t const& argv) {

			int code = check_command_argc(1, 2, argv.size());  //Accepts zero or one argument plus command name
			if (code)
				return code;
			if (argv.size() == 1 || argv[1].compare("all") == 0) //no argument or string "all" => print list of commands and general help
				std::cout << help_callback_helper::get_general_help() << '\n' << help_callback_helper::get_all_commands_help();
			else  //A single argument with name of command had been passed 
				code = help_callback_helper::print_command_help(argv[1]);

			std::cout << " \nThis program's help contains currently " << cmd_map.size() << " command" << print_plural(cmd_map.size()) << " and "
				<< cmd_aliases.size() << " aliases. It is fucking glorious, right?\n";
			return code;
		}

		/*Wrapper namespace for types and other stuff used by the "define_callback" function. Protects global namespace from pollution.*/
		namespace define_callback_helper {

			/*Gets pointers to both the hook as well as the primary command and links them.*/
			int link_hook(std::string_view const target_command, std::string_view const hook_name) {
				command * const hook = get_command(hook_name),
					*const cmd = get_command(target_command);

				if (!cmd) {
					std::cerr << "Specified command " << std::quoted(target_command) << " does not exist!\n";
					return 5;
				}
				if (!hook) {
					std::cerr << "Specified hook " << std::quoted(hook_name) << " does not exist!\n";
					return 6;
				}
				cmd->hook() = hook;
				return 0;
			}

			/*Helper function defined in order to split define_callback into more segments.
			Returns pair of err_code and bool specifying whether hook is to be defined. Non zero err_code signals an error.*/
			[[nodiscard]]
			std::pair<int, bool> check_params(command_parameters_t const& argv) {
				if (int const code = check_command_argc(2, 3, argv.size()); code)
					return { code, false }; //Accepts only the command name and optinal "hook"
				bool const hook = argv[1].compare("hook") == 0;
				if (argv.size() == 2 && hook) { //Only one argument has been passed and it is the "hook" keyword => return
					print_command_error(command_error::argument_required);
					return { 3, hook };
				}
				if (argv[1].compare("stop") == 0) {
					std::cerr << "The \"stop\" command cannot be modified.\n";
					return { 4, hook };
				}
				return { 0, hook };
			}

			/*Helper function defined in order to split define_callback into more segments.
			If cmd_name already exists, this function prompts the user, whether he wants to redefine that existing command.
			If alias is to be redefined, it is delete while primary commands are left as they are.
			Returns true if process of redefinition shall proceed.*/
			[[nodiscard]]
			bool approve_redefinition(std::string const& cmd_name, bool hook) {

				//If command is already defined, prompt user whether he wants to redefine it			
				if (command const * const command_ptr = get_command(cmd_name)) {
					if (is_command_alias(cmd_name)) //cmd_name denotes an alias
						std::cout << std::quoted(cmd_name) << " is an alias for command " << std::quoted(command_ptr->name())
						<< "\nYou can change it to full primary command.\n";
					else //cmd_name denotes a primary command
						std::cout << "As it appears, command " << std::quoted(cmd_name) << " of " << command_ptr->category() << " happens to be defined.\n"
						"Commands short documentation: " << command_ptr->short_doc() << "You can change its behaviour.\n";

					std::cout << "Would you like to proceed and redefine this command? [Y/N]\n";
					char c;
					do {
						std::cin >> c;
						c = std::toupper(c, std::locale{});
					} while (c != 'N' && c != 'Y'); //prompt until the user types something meaningful
					std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); //flush EOL from stdin

					if (c == 'N') //If N was pressed, return without modifying the command
						return false;
					if (is_command_alias(cmd_name))   //When redefining an alias, we must delete the old alias to prevent having one string as command and alias simultaneously
						cmd_aliases.erase(cmd_name);  //primary command is left and will just receive new callback
				}
				if (hook)
					std::cout << "Defining " << std::quoted(cmd_name) << '\n';
				else
					std::cout << "Defining new command " << std::quoted(cmd_name) << '\n';
				//Print manual how the definition of new command works
				std::cout << "Type names of commands that shall be called, one per line.\n"
					"End definition by typing \"end\"; at that point new command will have been saved.\n"
					"Commands may be not yet known. New name lookup is performed each time\n";
				return true;
			}

			/*Helper function defined in order to split define_callback into more segments.*/
			int do_define_new_command(std::string cmd_name) {
				std::vector<std::string> instructions;
				std::string line;
				for (;;) {
					std::cout << "(define " << std::quoted(cmd_name) << ") ";
					std::getline(std::cin, line);
					if (line.compare("end") == 0) //we have found the terminating "end"
						break;
					if (!line.empty()) //Add all non empty lines to vector
						instructions.emplace_back(std::move(line));
				}
				//New callback lambda for this command
				callback_t new_callback = [internal_commands_ = std::move(instructions)](command_parameters_t const &dummy_params) -> int {
					int code = 0;
					for (std::string const & command : internal_commands_)
						code = execute_command(command, false);
					return code;
				};

				if (command * command_ptr = get_command(cmd_name)) { //if we got here and command exists, then we are redefining a primary function.
					assert(is_command(cmd_name)); //we can be sure of this, because had cmd_name been an alias, it would have been deleted. 
					command_ptr->callback() = std::move(new_callback);
				}
				else //command_ptr is null, that means we are defining new primary function, old alias has already been deleted
					add_command(cmd_name, command_category::user_defined, "No short_doc", "No help", std::move(new_callback));
				return 0;
			}
		}


		/*Callback function for cli command "define".
		It expects either one or two arguments. The name of new command is the last command, the first is optinal "hook" keyword,
		which is a signal that a hook for given command shall be defined. If there happens to be an error in argument inerpretation,
		no new command is added.
		If the added command is hook, it is named "hook"-cmd_name, where cmd_name is the name for which the hook was defined.
		I hope this makes sense. */
		int define_callback(command_parameters_t const& argv) {
			namespace helper = define_callback_helper;
			auto const[err_code, hook] = helper::check_params(argv);
			if (err_code)
				return err_code;

			if (hook && is_command_alias(argv.back())) {
				std::cerr << "Aliases cannot have a hook assigned.\n";
				return 6;
			}

			std::string cmd_name{ hook ? "hook-" : "" }; //interpret last argument as cmd_name, prepending "hook-" if it already exists
			cmd_name.append(argv.back());

			if (!helper::approve_redefinition(cmd_name, hook)) {
				std::cout << "Redefinition of command " << std::quoted(cmd_name) << " canceled.\n";
				return 4;
			}
			if (hook) {
				helper::do_define_new_command(cmd_name);
				return helper::link_hook(argv.back(), cmd_name);
			}
			else
				return helper::do_define_new_command(std::move(cmd_name));
		}

		/*Callback function for cli command "undefine"
		Expects a single argument. If that argument is command, it is erased and all
		other commands which had this one as their hook, have their hook reset to null.
		If argument is unknown or is alias, prints errors and returns non-zero number.*/
		int undefine_callback(command_parameters_t const& argv) {
			if (int const code = check_command_argc(2, 2, argv.size()); code)
				return code; //expects one argument
			if (argv[1].compare("stop") == 0) {
				std::cerr << "Cannot undefine the \"stop\" command.\n";
				return 9;
			}
			std::string cmd_name{ argv[1] };
			if (is_command(cmd_name)) { //If we can delete a command
				command * cmd = get_command(cmd_name);
				for (auto &[name, command] : cmd_map)
					if (command.hook() == cmd)  //find all commands using this command as hook
						command.hook() = nullptr; //and set their hook to null
				cmd_map.erase(cmd_name); //finally erase cmd_name itself
				std::cout << "Command " << std::quoted(cmd_name) << " erased.\n";
				return 0;
			}
			if (is_command_alias(cmd_name)) { //If we have alias, print error
				std::cerr << "Cannot undefine alias. See help for \"unalias\".\n";
				return 3;
			} //Unknown string - neither command nor alias
			std::cerr << "String " << std::quoted(cmd_name) << " couldn't be resolved. You may have made a typo or you're just stupid.\n";
			return 4;
		}

		/*Namespace wrapping helper types and functions for "document_callback" function. One does not simply pollute global namespace!*/
		namespace document_callback_helper {

			/*Return type of check_params function. Indicates which type of help will be supplied according to the parameters.*/
			enum class documentation_type {
				none, //error value. No type has been specified
				short_desc, //command will supply short_doc
				full //command will supply new full help
			};

			/*Checks parameters passed to "document_callback". Ensures right number of arguments had been passed and they make sense.
			Returns an enumerator indicating whether some error occured, and if not, specifies the type of help, which is to be supplied.*/
			[[nodiscard]]
			documentation_type check_params(command_parameters_t const & argv) {
				if (int code = check_command_argc(2, 3, argv.size()); code) //One optional plus one mandatory argument
					return documentation_type::none;

				documentation_type const documentation_type = [&] { //IIFE (immidiatelly invoked function expression) to initialize const variable with complex code
					if (argv[1].compare("short") == 0) //if optional "short" has been specified
						return documentation_type::short_desc;
					if (argv[1].compare("full") == 0) //if optional "full" has been specified
						return documentation_type::full;
					return documentation_type::none; //if neither has appeared, return special value 
				}();

				if (argv.size() == 3 && documentation_type == documentation_type::none) { //two args require the first one to be "short"/"long" 
					print_command_error(command_error::argument_not_recognized);
					return documentation_type::none;
				}

				if (argv.size() == 2)
					if (documentation_type == documentation_type::none) //just one arg(command_name) specified, all ok
						return documentation_type::full;
					else { //the single passed argument is keyword => error
						print_command_error(command_error::argument_required);
						return documentation_type::none;
					}
				return documentation_type;
			}

			/*Function extracted from document_callback to make it simpler and shorter. Prompts user,
			reads input lines, forms a string and then assigns it as new help for given command.*/
			void do_document_command(command & command, documentation_type type) {
				std::cout << "Creating new documentation for command " << std::quoted(command.name()) << " of the " << command.category()
					<< " category.\nType text as long as you enjoy it, then type \"end\" to save given string as the new help message.\n";

				std::ostringstream stream;
				std::string line;
				for (;;) {
					std::cout << "(document \"" << command.name() << "\")";
					std::getline(std::cin, line);
					if (line.compare("end") == 0) //single "end" ends reading loop
						break;
					if (!line.empty()) //append all non-empty lines
						stream << line << '\n';
				}
				line = stream.str();
				if (!line.empty())
					line.pop_back(); //get rid of extra EOL
				std::cout << "New documentation:\n" << line << '\n';
				//assign resulting string to either full help or short_doc of given command
				(type == documentation_type::full ? command.help() : command.short_doc()) = line;
			}
		}

		/*Callback function for cli command "document".
		Accepts one optional and one mandatory argument (which help shall be modified and command_name).
		If cmd_name denotes a primary command then new string containing documentation is read from the user and assigned to the command.
		Otherwise prints error message and returns non-zero exit code.
		*/
		int document_callback(command_parameters_t const & argv) {
			namespace helper = document_callback_helper;

			//try to determine which type of help is to be modified 
			helper::documentation_type documentation_type = helper::check_params(argv);
			if (documentation_type == helper::documentation_type::none) //error has occured
				return 1;

			std::string_view const& cmd_name = argv[argv.size() - 1];

			if (is_command_alias(cmd_name)) { //one does not simply document an alias
				std::cerr << "One does not simply document an alias for command... Only primary commands can have help messages associated.\n";
				return 2;
			}
			command * const command = get_command(cmd_name);
			if (!command) { //if command does not exist, print err message and return
				std::cerr << "Command " << std::quoted(cmd_name) << " does not exist. Define it first using command \"define\".\n";
				return 2;
			}
			//we've got correct name of command, so prompt for new documentation and set it

			helper::do_document_command(*command, documentation_type);
			return 0;
		}

		/*Callback function for cli command "alias"
		Accepts exactly two arguments - cmd_name and its new alias.*/
		int alias_callback(command_parameters_t const& argv) {
			if (int const code = check_command_argc(3, 3, argv.size()); code)
				return code;
			std::string cmd_name{ argv[1] }, alias{ argv[2] };

			if (is_command(alias)) {//if alias denotes a primary command, it is an error
				std::cerr << std::quoted(alias) << " is a name of a primary command. It can't be created as alias.\n";
				return 3;
			}
			if (is_command_alias(alias)) { //if it's already alias for something, leave it
				command const * const command = get_command(alias);
				std::cerr << std::quoted(alias) << " is already used as an alias for command ";
				if (command)
					std::cout << std::quoted(command->name()) << '\n';
				else
					std::cout << ", whose name and effect is yet to be discovered (in another words: it hasn't been defined)\n";
				return 4;
			}
			std::cout << "Defined new alias " << std::quoted(alias) << " for command " << std::quoted(cmd_name) << '\n';
			if (!get_command(cmd_name))
				std::cout << "This command does not exist yet, which may lead to unsafe situations.\n"
				"Either consider unlinking this alias or make sure command gets defined.\n";

			add_command_alias(std::move(alias), std::move(cmd_name)); //if we got here, alias does not exist yet, safe to add
			return 0;
		}

		/*Callback function for cli command "unalias"
		Expects single argument - name of alias to be deleted. If such alias is defined,
		unlinks it and returns zero. Otherwise returns non-zero value.*/
		int unalias_callback(command_parameters_t const& argv) {
			if (int const code = check_command_argc(2, 2, argv.size()); code)
				return code; //expects exactly one argument
			std::string alias{ argv[1] };
			if (is_command_alias(alias)) { //If we are allowed to, delete the alias
				cmd_aliases.erase(alias);
				return 0;
			}
			if (is_command(alias)) { //If alias is unknown or is primary function, print error
				std::cerr << "Cannot unlink primary command. See help for \"undefine\".\n";
				return 3;
			}
			std::cerr << "Unknown alias " << std::quoted(alias) << ". You might have made a typo or you're just stupid.\n";
			return 4;
		}

		/*Callback function for cli command "echo"
		Accepts unlimited numer of arguments and prints them all back to the stdout.*/
		int echo_callback(command_parameters_t const& argv) {
			if (int const code = check_command_argc(1, std::numeric_limits<int>::max(), argv.size()))
				return code;
			for (auto iter = argv.begin(), end = argv.end(); ++iter != end;)
				std::cout << *iter << ' ';
			std::cout << '\n';
			return 0;
		}

		/*Function callback for the cli command dont-repeat.
		Accepts no arguments and simply sets the dont_repeat variable in the global command history.*/
		int dont_repeat_callback(command_parameters_t const& argv) {
			if (int const code = check_command_argc(1, 1, argv.size()); code)
				return code;
			cli_history.repeat_previous_command_ = false;
			return 0;
		}

		namespace history_callback_helper {

			enum class requested_action {
				none,
				show,
				execute
			};

			std::pair<requested_action, int> parse_parameters(command_parameters_t const & argv) {

				requested_action requested_action = requested_action::none;
				if (argv[1].compare("show") == 0)
					requested_action = requested_action::show;
				else if (argv[1].compare("exe") == 0)
					requested_action = requested_action::execute;
				else {
					print_command_error(command_error::argument_not_recognized);
					return { requested_action::none, 4 };
				}

				int index = std::numeric_limits<int>::max();
				if (argv.size() == 3)
					if (std::from_chars(argv[2].data(), argv[2].data() + argv[2].size(), index).ec != std::errc{}) {
						print_command_error(command_error::invalid_number_format);
						return { requested_action::none, 5 };
					}
				return { requested_action, index };
			}

			int do_show(int count) {
				if (count < 0) {
					print_command_error(command_error::non_negative_number_expected);
					return 12;
				}
				int const history_size = static_cast<int>(cli_history.previous_commands_.size());
				if (count > history_size) {
					std::cout << "Requested history length is too high, shrinking to " << history_size << " commands.\n";
					count = history_size;
				}
				std::ostringstream stream;
				for (int positive_index = history_size - count, negative_index = -count; positive_index < history_size; ++positive_index, ++negative_index)
					stream << std::right << std::setw(6) << negative_index << std::setw(6) << positive_index << ": "
					<< std::quoted(cli_history.previous_commands_[positive_index]) << '\n';
				std::cout << stream.str();
				return 0;
			}

			int do_execute(int index) {
				int const history_size = static_cast<int>(cli_history.previous_commands_.size());
				if (index < 0)  //indexing from the end
					index += history_size - 1;
				if (index >= history_size || index < 0) {
					std::cerr << "Specified command index is out of bounds. There are only " << history_size << " commands in the history.\n";
					return 7;
				}
				std::string const& cmd_line = cli_history.previous_commands_[index];
				if (cmd_line.find("history") != std::string::npos && cmd_line.find("exe") != std::string::npos) {
					std::cout << "Brutal and painful crashes had taught me that retrospective execution of other history "
						"commands causes stack overflow.\nDon't do that please.\n";
					return 0;
				}
				std::cout << "Executing command " << std::quoted(cmd_line) << std::endl;
				execute_command(cmd_line, false);
				return 0;
			}
		}

		int history_callback(command_parameters_t const & argv) {
			namespace helper = history_callback_helper;
			if (int const code = check_command_argc(2, 3, argv.size()))
				return code;

			if (cli_history.previous_commands_.empty()) {
				std::cout << "One doe not simply reference program's command history when it's empty...\n";
				return 8;
			}

			auto[requested_action, index] = helper::parse_parameters(argv);
			if (requested_action == helper::requested_action::none)
				return index;

			return requested_action == helper::requested_action::show ? helper::do_show(index) : helper::do_execute(index);
		}

		int script_callback(command_parameters_t const& argv) {
			if (int const code = check_command_argc(2, 2, argv.size()))
				return code;

			if (std::optional<std::string> file_content = read_file(argv[1])) {
				for (const auto &line : split_to_lines(*file_content)) {
					if (line.empty()) continue;
					std::cout << "Executing " << std::quoted(line) << '\n';
					execute_command(static_cast<std::string>(line), false);
				}
				return 0;
			}

			print_command_error(command_error::file_not_found);
			return 4;
		}

		/*Prints welcome string at the entry to cli_command_loop*/
		void print_cli_welcome() {
			std::cout << "Brainfuck optimizing compiler and CPU emulator CLI\nVersion up to date as of " __TIMESTAMP__
				"\nCompiled at " __TIME__ " " __DATE__ "\n"

				"The emulator is currently in " << sizeof(decltype(execution::emulator)::memory_cell_t) * CHAR_BIT << "-bit mode.\n"
				"The emulator's address space is currently " << execution::emulator.memory_size() << " cell" << print_plural(execution::emulator.memory_size()) << " wide.\n\n";
			std::cout << help_callback_helper::get_general_help() << "\n\n"
				"To get started, check out the \"help\" command to learn more about the program's features.\n";
		}
	}

	std::ostream& operator<<(std::ostream& str, command_category cat) {
		switch (cat) {
		case command_category::general: str << "general"; break;
		case command_category::commands: str << "commands"; break;
		case command_category::user_defined: str << "user-defined";	break;
		case command_category::compilation: str << "compilation"; break;
		case command_category::optimization: str << "optimization"; break;
		case command_category::execution: str << "emulation"; break;
		case command_category::debug: str << "debug"; break;
		case command_category::hooks: str << "hooks"; break;
			ASSERT_NO_OTHER_OPTION
		}
		return str;
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

	int check_command_argc(int const min, int const max, std::size_t const actual) {
		//we do not really care about overflows here, noone is gonna pass two billion arguments
		if (static_cast<int>(actual) < min) { //At least min arguments expected
			print_command_error(command_error::argument_required);
			return 1;
		}
		if (static_cast<int>(actual) > max) { //At most max arguments expected
			print_command_error(command_error::too_many_arguments);
			return 2;
		}
		return 0;
	}

	void add_command(std::string cmd, command_category category, std::string short_doc, std::string help, callback_t function) {
		assert(get_command(cmd) == nullptr); //sanity check that the command has not been already defined
		std::string name = cmd; //pass all parameters as tuple to constructor of command
		cmd_map.emplace(std::piecewise_construct, std::tuple{ std::move(cmd) }, std::tuple{ std::move(name), category, std::move(short_doc), std::move(help), std::move(function) });
	}

	void add_command_alias(std::string alias, std::string cmd_name) {
		assert(get_command(alias) == nullptr); //Sanity check that the alias has not been already defined
		cmd_aliases.emplace(std::move(alias), std::move(cmd_name));
	}

	bool is_command(std::string const& cmd_name) {
		return cmd_map.count(cmd_name);
	}

	bool is_command(std::string_view const cmd_name) {
		return is_command(std::string{ cmd_name });
	}

	bool is_command_alias(std::string const& alias) {
		return cmd_aliases.count(alias);
	}

	bool is_command_alias(std::string_view const alias) {
		return is_command_alias(std::string{ alias });
	}

	int execute_command(std::string cmd_line, bool const from_tty) {
		command_parameters_t const tokens = split_to_tokens(cmd_line);
		if (tokens.empty()) { //if this command is empty, we try to repeat the last one
			if (!from_tty || !cli_history.repeat_previous_command_ || cli_history.previous_commands_.empty())
				return 0;
			return execute_command(cli_history.previous_commands_.back(), true);
		}
		if (tokens[0][0] == '!') { //if first non-whitespace character is exclamation mark, execute command in terminal
			char const * cmd = std::next(std::find(cmd_line.data(), cmd_line.data() + cmd_line.size(), '!'));
			if (cmd == cmd_line.c_str() + cmd_line.size())
				return 0;

			//expects null-trminated string; Pointers are necessary, because string_view iterators assert that they point to the same sequence
			int const code = std::system(cmd);
			//passed string beginning at the exclamation mark and ending at the last non-whitespace character
			if (from_tty) {
				std::cout << "\nOS returned " << code << ".\n";
				cli_history.previous_commands_.emplace_back(std::move(cmd_line));
			}
			return 0;
		}

		command const * const command = get_command(tokens[0]); //searches maps for corresponding command
		if (!command) { //If no such command is found, print and return
			std::cerr << "Command " << std::quoted(cmd_line) << " could not be resolved. Try \"help\"." << std::endl;
			return -1;
		}

		if (from_tty) {
			cli_history.repeat_previous_command_ = true;
			cli_history.previous_commands_.emplace_back(std::move(cmd_line));
		}

		int return_code;
		/*Pointer to hook must be acquired here. Believe me, i learnt it the hard and painful way of random crashes after executing "undefine undefine".
		In another words: this protects the program in case the called command is deleted before the hook check occures.*/
		class command const * const hook = command->hook();
		try { //As it appears, command does exist. Execute it saving the returned value
			return_code = command->callback()(tokens);
		}
		catch (...) { //If something exploded inside the callback, print err and return
			std::cerr << "Exception has been thrown while executing " << std::quoted(cli_history.previous_commands_.back()) << '.' << std::endl;
			return -2;
		}
		try { //If command returned normally, execute its hook
			if (hook)
				hook->callback()(tokens);
		}
		catch (...) { //In case something exploded in the hook, print error message and return 
			std::cerr << "Exception has been thrown while executing hook for command " << std::quoted(cli_history.previous_commands_.back()) << '.' << std::endl;
			return -3;
		}
		return return_code;
	}

	void cli_command_loop() {
		print_cli_welcome();

		std::string command_line;

		//prompt, get line and execute forever
		for (;;) {
			std::cout << "(b-fuck) ";
			do {
				std::getline(std::cin, command_line);
				/*When a signal from the OS is raised, program's standard streams get their eof- and fail-bit set.
				Their state is reset as soon as the signal handler is executed, but since that occures asynchronously
				it is necessary to make sure this loop doesn't do anything stupid in the meantime. Hopefully this is the
				only possible data race in the program.*/
			} while (std::cin.fail());
			if (command_line.empty() && std::cin.eof()) { //If we have reached EOF (of a script), we can leave the program
				std::cout << "EOF reached. Quitting.\n";
				std::exit(0);
			}
			execute_command(command_line, true);
		}
	}

	void print_command_error(command_error const err_code) {
		switch (err_code) {
		case command_error::file_not_found:                 std::cerr << "File not found.";                       break;
		case command_error::argument_required:              std::cerr << "Arguments required.";                   break;
		case command_error::too_many_arguments:             std::cerr << "Too many arguments passed.";            break;
		case command_error::value_out_of_bounds:		    std::cerr << "Value is out of acceptable bounds.";    break;
		case command_error::invalid_number_format:          std::cerr << "Invalid number format.";                break;
		case command_error::argument_not_recognized:        std::cerr << "Arguments were not recognized.";        break;
		case command_error::positive_number_expected:       std::cerr << "Positive number was expected.";         break;
		case command_error::non_negative_number_expected:   std::cerr << "Expected non-negative number.";         break;
			ASSERT_NO_OTHER_OPTION
		}
		std::cerr << " Check help for this command.\n";
	}


	void initialize() {
		ASSERT_CALLED_ONCE
			add_command("quit", command_category::general, "Exits the program.",
				"Usage: \"quit\" [return_code]\n"
				"Optional return code is returned to the OS; zero is used if it's left unspecified."
				, &quit_callback);
		add_command_alias("exit", "quit");
		add_command_alias("q", "quit");

		add_command("help", command_category::general, "Prints out help messages.",
			"Usage: \"help\" [command_name]\n"
			"If optional command_name is specified, prints help for given command.\n"
			"Otherwise lists all defined commands and their short description.\n\n"
			"Special argument \"all\" lists all defined commands as well."
			, &help_callback);
		add_command_alias("h", "help");
		add_command_alias("pls", "help");

		add_command("define", command_category::commands, "Creates new user-defined command.",
			"Usage: \"define\" [\"hook\"] cmd_name\n"
			"Adds a new command named \"cmd_name\", which allows consecutive calls to multiple built-in commands\n"
			"If \"hook\" argument is specified, a hook for specified command is created.\n"
			"Hooks are executed automatically right after their command has been run.\n"
			"Hook may be run manually as well by typing \"hook-\"cmd_name."
			, &define_callback);
		add_command_alias("def", "define");

		add_command("undefine", command_category::commands, "Undefines some previously defined command.",
			"Usage: \"undefine\" cmd_name\n"
			"If cmd_name denotes an entity in the internal list of commands, that entity is removed."
			, &undefine_callback);
		add_command_alias("undef", "undefine");

		add_command("document", command_category::commands, "Supplies documentation messages for commands.",
			"Usage: \"document\" [\"short\" or \"full\"] cmd_name\n"
			"Command \"cmd_name\" gets new help message.\n"
			"If first argument is ommitted or is \"full\", then main help message is replaced.\n"
			"If short is specified, then only the short-doc is replaced."
			, &document_callback);
		add_command_alias("doc", "document");

		add_command("alias", command_category::commands, "Creates new alias for a command.",
			"Usage: \"alias\" cmd_name alias_name\n"
			"If alias_name is not yet known, creates it as an alias for command cmd_name.\""
			"If alias_name already exists however, the command will do something, but I'm not yet sure, what."
			, &alias_callback);
		add_command("unalias", command_category::commands, "Erases specified alias from list of aliases.",
			"Usage: \"unalias\" alias_name\n"
			"If alias_name denotes an entity in internal list of commands, that entity is unlinked."
			, &unalias_callback);
		add_command_alias("unlink", "unalias");

		add_command("echo", command_category::general, "Prints it's arguments to stdout",
			"Usage: \"echo\" ...\n"
			"Accepts variadic number of arguments and prints all arguments literally to stdout."
			, &echo_callback);
		add_command("dont-repeat", command_category::general, "Prevents the previous command from repeating.",
			"Usage: \"dont-repeat\" (no args)\n"
			"Stops the previous command from repeating if enter is pressed without any text extered."
			, &dont_repeat_callback);

		add_command("history", command_category::commands, "Prints the command history or executes a previously executed command.",
			"Usage: \"history\" (\"show\" or \"run\") [count]\n"
			"If \"show\" is specified as the first argument, then at most count previously executed commands are printed with their indices.\n"
			"If \"exe\" is passed, then count is interpreted as an index into the list of previously executed commands specifying the cmd to be exeuted.\n"
			"\tUsing the python-like array access syntax it is possible to specify absolute index of the requested command using non-negative value of count.\n"
			"\tIf count is negative, then it denotes an index relative to the current command - i.e. count == -1 executes the previous command."
			, &history_callback);

		add_command("script", command_category::commands, "Executes lines of file as commands.",
			"Usage: \"script\" file\n"
			"Reads the specified file, splits the content to lines and executes them one by one as CLI commands.\n"
			"Commands executed as the consequence of batch execution are not kept in the CLI's history."
			, &script_callback);
	}
}