#pragma once

#include <string_view>
#include <string>
#include <functional>
#include <vector>
#include <optional>
#include <exception>
#include <cassert>


namespace bf::cli {

	//Categories of commands used for sorting. Used for example when printing help
	enum class command_category {
		general, //commands that do not fit anywhere else - quit, help
		commands, //commands that define or document new commands
		user_defined, //commands defined by the user
		compilation, //commands controlling the compilation
		optimization, //commands instructing the optimization engine to perform program optimizations
		execution, //commands which control the internal cpu emulator
		debug, //debugging commands
		hooks //hooks of other commands
	};
	std::ostream& operator<<(std::ostream& str, command_category const cat); //Output operator

	

	

	//type of arguments expected by function callbacks for CLI commands
	using command_parameters_t = std::vector<std::string_view>;

	//type of function callback for CLI commands
	using callback_t = std::function<int(command_parameters_t const&)>;

	/*Adds new command to the list of recognized commands. Each command may be added only once.*/
	void add_command(std::string cmd, command_category command, std::string short_doc, std::string help, callback_t callback);
	/*Adds an alias for specified command into the internal list. Each alias may be specified only once.*/
	void add_command_alias(std::string alias, std::string cmd_name);

	/*Searches for cmd_name in list of commands and returns true on sucess.*/
	bool is_command(std::string const& cmd_name);
	bool is_command(std::string_view const cmd_name);

	/*Searches internal list of aliases for given string and returns true on success.*/
	bool is_command_alias(std::string const& alias);
	bool is_command_alias(std::string_view const cmd_name);

#if 0
	/*Adds a new hook for command cmd, which executes specified list of other commands. Called commands
	do not need to exist at the point of defining the hook - new lookup is performed every time.*/
	void add_command_hook(std::string cmd, std::vector<std::string> instructions);
#endif

	/*Initialization function of CLI commands. Shall be called only once
	from the program's main. It initializes and adds general commands to the command map.*/
	void initialize();


	/*Takes a line and attempts to execute it.
	If cmd_line starts with exclamation mark ('!'), rest of the command is executed by the OS.
	Otherwise internal map of commands is searched for the given command name. If corresponding
	command is found, it and it's hook are executed. Otherwise prints error message.

	Returned value: if command gets executed by OS, returns value received from OS itself.
	Otherwise returns value returned by internal command or negative value indicating error.*/
	int execute_command(std::string cmd_line, bool from_tty);

	/*This function is the main interactive CLI loop of the program. It never returns
	due to while (true) loop, which prompts user for command and executes it afterwards.
	Because it never returns, exiting the program shall be achieved via the "quit" cli command.*/
	void cli_command_loop();

	/*error code for possible errors, which may occure while a CLI command callback
	function is being executed*/
	enum class command_error {
		ok = 0, //no error occured 
		argument_required, //This command expects some arguments
		invalid_number_format, //Expected a number as an argument, but received some crap
		argument_not_recognized, //Some argument was not recognized (typo, invalid format etc) 
		file_not_found, //File, this command was supposed to operate on, hasn't been found
		too_many_arguments, //There have been too many arguments passed to this command
		positive_number_expected, //positive number had been expected
		non_negative_number_expected, // expected non-negative number
		value_out_of_bounds
	};

	//prints error message for specified err_code to the standard error stream
	void print_command_error(command_error const err_code);
	

}