#include "emulator.h"

#include "cli.h"
#include "compiler.h"
#include <iostream>
#include <locale>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <csignal>

namespace bf::execution {



	namespace {

		/*Helper function checking the validity of program that has been flashed into the CPU's memory. If there is none, prints error message and
		returns false. vice versa if the memory contains runnable instrucitons, returns true.*/
		bool assert_emulator_has_program() {
			if (!emulator.has_program()) { //Make the calling function proceeds only if there is some program inside CPU's flash memory
				std::cerr << "The emulator has no program to run. You better compile and flash some instructions.\n";
				return false;
			}
			return true;
		}

		/*Function callback for the flash cli command.
		Expects no arguments. Does a simple check whether there is a program that could be flashed and if there is, does so.
		After the flash the cpu is reset and has its memory cleared.*/
		int flash_callback(cli::command_parameters_t const& argv) {
			if (int const code = cli::check_command_argc(1, 1, argv.size()); code)
				return code;
			if (!last_compilation::ready()) {
				std::cerr << "You must first compile a program. See the \"compilation\" group of commands, especially \"compile\".\n";
				return 4;
			}
			if (!last_compilation::successful()) {
				std::cerr << "Previous compilation encountered an error. Query its results using the \"compilation\" group of commands or perform a new one.\n"
					"Illegal code cannot be flashed into the CPU.\n";
				return 5;
			}
			emulator.flash_program(last_compilation::syntax_tree());
			emulator.reset();
			std::cout << "Code successfully flashed into the emulator's memory.\n";
			return 0;
		}

		/*Wrapper namesapce for helper functions for the run_callback. One does not simply pollute the global namespace of my programs*/
		namespace run_callback_helper {

			/*Prompts the user that program is being run. Returns true if user decides to restart the program, false otherwise.*/
			bool prompt_user_wants_restart() {
				std::cout << "A debugging session is already running. You may discard it, reset the cpu and begin the execution anew.\n"
					"Would you like to restart the program from the beginning? [Y/N]\n";
				char input;

				do {
					std::cin >> input;
					input = std::toupper(input, std::locale{});
				} while (input != 'Y' && input != 'N'); //prompt untile the user types something useful

				std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); //flush the new-line from std::cin

				return input == 'Y';
			}
		}

		/*A function callback for the run cli command.
		Does not accept any parameters, checks the state of cpu and provided it's good, resets it and commences the execution.*/
		int run_callback(cli::command_parameters_t const& argv) {
			if (int const code = cli::check_command_argc(1, 1, argv.size()); code)
				return code; //no arguments are expected

			if (!assert_emulator_has_program()) //if there is no program inside the CPU
				return 5;

			switch (emulator.state()) { //we have to take a with state of cpu in mind
			case execution_state::interrupted: //CPU had hit a breakpoint or something
				if (!run_callback_helper::prompt_user_wants_restart())
					return 0; //if the user does not want to restart the program, just leave
				[[fallthrough]]; //otherwise continue to the same reset+start code
			case execution_state::finished: //If the previous execution already ended, reset the CPU
				emulator.reset();
				[[fallthrough]];
			case execution_state::not_started: //if there is no execution in progress
				emulator.do_execute();
				return 0;
			case execution_state::running: //if it is already running
				std::cerr << "CPU is currently running, stop the execution first.\n";
				return 3;
			case execution_state::halted: //if the cpu was shut down due to an error
				std::cerr << "CPU had been halted.\n";
				return 4;
				ASSERT_NO_OTHER_OPTION
			}
		}

		/*Function callback for cli command start. Accepts no arguments, sets a temporary breakpoint at the first instruction and runs the CPU.*/
		int start_callback(cli::command_parameters_t const& argv) {
			if (int const code = cli::check_command_argc(1, 1, argv.size()))
				return code;

			if (int const code = cli::execute_command("tbreak 0", false))
				return code;
			if (int const code = cli::execute_command("run", false))
				return code;
			return 0;
		}

		/*Namespace wrapper for both step and continue CLI commands' helper functions.*/
		namespace step_and_continue_helper {

			/*A helper function for both step_callback and continue_callbakc. If the CPU is in a state prepared for execution,
			commence this execution. If it either is already running, is halted or has no execution interrupted, prints error.
			Returns zero on successful execution, nonzero otherwise.*/
			int continue_execution() {
				switch (emulator.state()) { //we have to take a with state of cpu in mind
				case execution_state::not_started: //if there is no execution in progress
				case execution_state::finished: //or the execution already ended
					std::cerr << "No execution is currently interrupted. Start a new program using \"run\" or \"start\".\n";
					return 3;
				case execution_state::running: //if it is already running
					std::cerr << "CPU is currently running.\n";
					return 4;
				case execution_state::halted: //if the cpu was shut down due to an error
					std::cerr << "CPU had been halted.\n";
					return 5;
				case execution_state::interrupted: //the only state we can influence. CPU had hit a breakpoint or something
					emulator.do_execute(); //returns after a single instruction had been executed. Run it count times
					return 0;
					ASSERT_NO_OTHER_OPTION
				}
			}

			int do_perform_steps(int step_count) {
				assert(step_count > 0);
				int code = 0;
				bool const saved_interrupt_state = emulator.suppress_stop_interrupt(),
					saved_step_state = emulator.single_step();
				emulator.single_step() = true; //set the single_step flag to make each call to continue_execution run just one instruction
				emulator.suppress_stop_interrupt() = true; //prevent the emulator from fireing "stop" command
				for (; step_count > 1; --step_count) //run up to step_count times until the call returns non zero value
					if ((code = step_and_continue_helper::continue_execution())) {
						cli::execute_command("stop", false);
						break;
					}
				emulator.suppress_stop_interrupt() = saved_interrupt_state;
				if (!code && step_count)
					if (code = step_and_continue_helper::continue_execution(); code == 0)
						--step_count;
				emulator.single_step() = saved_step_state; //reset normal run behaviour
				if (step_count) //if we had to break out, we notify the user that some stuff happened
					std::cerr << "CPU has rejected further attempts to control it. Remaining " << step_count << " step"
					<< cli::print_plural(step_count) << " had not been performed.\n";
				return code;
			}

		}

		/*Function callback fot the "continue" command. Does not do much, just makes sure arguments are ok and
		if the cpu finds itself in a valid state, makes it proceed with the execution.*/
		int continue_callback(cli::command_parameters_t const& argv) {
			if (int const code = cli::check_command_argc(1, 1, argv.size()); code)
				return code; //no arguments expected

			if (!assert_emulator_has_program()) //if there is no program, print and return
				return 6;

			return step_and_continue_helper::continue_execution();
		}

		/*Callback function for cli step command.
		Takes an optional positive integer specifying how many instructions shall be executed.*/
		int step_callback(cli::command_parameters_t const& argv) {
			if (int const code = cli::check_command_argc(1, 2, argv.size()); code)
				return code; //expects one optional argument

			if (!assert_emulator_has_program()) //if there is no program, print and return
				return 6;

			int step_count = 1;
			if (argv.size() == 2) //if we got an argument, try to parse it
				if (std::from_chars(argv[1].data(), argv[1].data() + argv[1].size(), step_count).ec != std::errc{}) {
					cli::print_command_error(cli::command_error::invalid_number_format);
					return 7; //it wasn't a valid number, return
				}
				else if (step_count <= 0) { //step_count must be positive
					cli::print_command_error(cli::command_error::non_negative_number_expected);
					return  8;
				}
			return step_and_continue_helper::do_perform_steps(step_count);
		}

		namespace redirect_command_helper {

			std::filesystem::path stdin_path, stdout_path;

			/*Helper function printing the paths to which emulated program's iostreams are redirected*/
			int print_iostreams_state() {
				std::cout << "Current state of iostreams available to the emulated program:\n"
					"STDIN  > " << (emulator.emulated_program_stdin() == &std::cin ? "debugger's stdin" : stdin_path.string())
					<< "\nSTDOUT > " << (emulator.emulated_program_stdout() == &std::cout ? "debugger's stdout" : stdout_path.string()) << '\n';
				return 0;
			}

			enum class data_stream {
				none,
				in,
				out
			};

			data_stream parse_stream_direction(std::string_view const stream) {
				if (stream.compare("out") == 0)
					return data_stream::out;
				if (stream.compare("in") == 0)
					return data_stream::in;
				cli::print_command_error(cli::command_error::argument_not_recognized);
				return data_stream::none;
			}

			int redirect_stream(data_stream str, std::string_view const new_stream_name) {
				assert(str != data_stream::none); //sanity check
				if (str == data_stream::out)
					emulator.emulated_program_stdout()->flush();

				if (new_stream_name.compare("std") == 0) //we will use standard stream
					switch (str) {
					case data_stream::in:
						emulator.emulated_program_stdin() = &std::cin;
						std::cout << "Successfully redirected input to stdin.\n";
						return 0;
					case data_stream::out:
						emulator.emulated_program_stdout() = &std::cout;
						std::cout << "Successfully redirected output to stdout.\n";
						return 0;
					}

				//else we redirect to file


				std::filesystem::path new_stream = std::filesystem::absolute(new_stream_name);

				//If the specified file does not exist, print error and return
				if (!std::filesystem::exists(new_stream)) {
					cli::print_command_error(cli::command_error::file_not_found);
					return 5;
				}

				//static buffers. This solution limits the scope while preserving the lifetime of their internal buffers
				static std::ifstream file_in;
				static std::ofstream file_out;

				switch (str) {
				case data_stream::in:
					file_in = std::ifstream{ new_stream };
					emulator.emulated_program_stdin() = &file_in;
					std::cout << "Successfully redirected input to " << new_stream << '\n';
					stdin_path = std::move(new_stream);
					break;
				case data_stream::out:
					file_out = std::ofstream{ new_stream };
					file_out.rdbuf()->pubsetbuf(nullptr, 0); //enable unbuffered output
					emulator.emulated_program_stdout() = &file_out;
					std::cout << "Successfully redirected output to " << new_stream << '\n';
					stdout_path = std::move(new_stream);
					break;
					ASSERT_NO_OTHER_OPTION;
				}
				return 0;
			}
		}

		/*Function callback for the redirect cli command.
		Accepts two arguments, first one specifying the stream to be altered, the second one naming the stream
		to which the redirection shall be performed.*/
		int redirect_callback(cli::command_parameters_t const& argv) {
			using namespace redirect_command_helper;
			if (int const code = cli::check_command_argc(1, 3, argv.size()))
				return code;

			switch (argv.size()) {
			case 1u:          //we received no args, print info
				return print_iostreams_state();
			case 2u:
				cli::print_command_error(cli::command_error::argument_required);
				return 6;
			case 3u:
				if (data_stream stream = parse_stream_direction(argv[1]); stream == data_stream::none)
					return 4;
				else
					return redirect_stream(stream, argv[2]);
				ASSERT_NO_OTHER_OPTION;
			}
		}

		/*Function callback for reset cli command. Does nothig else but resets the CPU*/
		int reset_callback(cli::command_parameters_t const& argv) {
			if (int const code = cli::check_command_argc(1, 1, argv.size()))
				return code;
			emulator.reset();
			return 0;
		}


		/*Function callback for the cli stop command. Takes no arguments and acts almost as a pseudo command especially
		useful to call it's hook.*/
		int stop_callback(cli::command_parameters_t const& argv) {
			if (int const code = cli::check_command_argc(1, 1, argv.size()))
				return code;

			if (emulator.state() == execution_state::running)
				emulator.os_interrupt() = true;
			return 0;
		}


		/*Function handler for SIGINT. Clears error flags in standard streams and sets the emulator's os_interrupt flag.*/
		extern "C" void interrupt_handler(int) {
			emulator.os_interrupt() = true;
			std::cin.clear(); //clear error states of all streams
			std::cout.clear();
			std::cerr.clear();
			std::cout << "Keyboard interrupt detected!\n";
			std::signal(SIGINT, &interrupt_handler); //register self as handler again, otherwise next SIGINT kills the program
		}
	}


	void initialize() {
		ASSERT_CALLED_ONCE;

		//register SIGINT handler
		std::signal(SIGINT, &interrupt_handler);

		cli::add_command("redirect", cli::command_category::execution, "Redirects stdin and stdout of emulated program.",
			"Usage: \"redirect\" (\"out\" or \"in\") stream_name\n"
			"or     \"redirect\" (no args)\n"
			"If no arguments are specified, print information about standard file descriptors for the emulated program.\n"
			"Otherwise this command uses stream_name as the new target within the file system for reading and writing operations performed by the emulated program.\n"
			"Special string \"std\" resets the streams to their default directions: stdin and stdout respectively."
			, &redirect_callback);

		cli::add_command("reset", cli::command_category::execution, "Resets the CPU",
			"Usage: \"reset\" (no args)\n"
			"not much else to say. It just resets the emulator.\n"
			"Reseting the CPU has the effect of zeroing out the data memory and program counter, moving the\n"
			"cell pointer to the beginning of memory and reseting all flags. It is necesarry if the CPU halted."
			, &reset_callback);

		cli::add_command("flash", cli::command_category::execution, "Loads the previously compiled program into the emulator's memory.",
			"Usage: \"flash\" (no arguments)\n"
			"If the last compilation ended successfully, loads the compiled code into cpu emulator and resets it.\n"
			, &flash_callback);

		cli::add_command("run", cli::command_category::execution, "Reset the cpu emulator and start executing flashed code.",
			"Usage: \"run\" (no args)\n"
			"Resets the cpu, clears cpu's memory and starts execution of the flashed program."
			, &run_callback);
		cli::add_command_alias("r", "run");

		cli::add_command("continue", cli::command_category::execution, "Proceed with program's execution.",
			"Usage: \"continue\" (no args)\n"
			"After the execution has been interrupted by a breakpoint or alike, continue it until another interrupt is raised\n"
			"or until the program returns."
			, &continue_callback);

		cli::add_command_alias("c", "continue");

		cli::add_command("step", cli::command_category::execution, "Step instruction(s) forward.",
			"Usage: \"step\" [count]\n"
			"Allows the cpu to execute a single instruction and then stops the execution again.\n"
			"Optional parameter count expects a positive integer specifying how many instructions shall be executed."
			, &step_callback);
		cli::add_command_alias("s", "step");

		cli::add_command("stop", cli::command_category::execution, "Stop the program's execution.",
			"Usage: \"stop\" (no args)\n"
			"Interrupts the program's execution as if a brekapoint has been hit."
			, &stop_callback);

		cli::add_command("start", cli::command_category::execution, "Initiate new execution stopping at the first instruction.",
			"Usage: \"start\" (no args)\n"
			"Begin new execution of the flashed program setting a temporary breakpoint at the first instruction and therefore interrupting again.\n"
			"Necessary to initiate step-debugging.\n"
			, &start_callback);

	}


}