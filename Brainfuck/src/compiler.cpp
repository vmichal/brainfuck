#include "compiler.h"
#include "syntax_check.h"
#include "cli.h"
#include "utils.h"

#include <execution>
#include <iostream>
#include <numeric>
#include <map>
#include <cassert>
#include <algorithm>
#include <string_view>
#include <charconv>
#include <iomanip>
#include <stack>

namespace bf {


	/*Structure containing information about the result of a compilation; Mainly vector of syntax errors
	and optionally syntax_tree provided compilation was successful are present.*/
	struct compilation_result {
		std::string source_code_; //source code that has been compiled
		std::vector<syntax_error> syntax_errors_; //vector of encountered syntax_errors
		std::vector<std::unique_ptr<basic_block>> basic_blocks_; //compiled instructions.

		template<typename A, typename B, typename C>
		compilation_result(A&& source_code, B&& syntax_errors, C&& basic_blocks) noexcept
			: source_code_{ std::forward<A>(source_code) },
			syntax_errors_{ std::forward<B>(syntax_errors) },
			basic_blocks_{ std::forward<C>(basic_blocks) }
		{}
	};

	namespace previous_compilation {

		/*Internal unique pointer to the result of last compilation. After the first compilation finishes, a meaningful value is set;
		until then contains nullptr.*/
		std::unique_ptr<compilation_result> prev_compilation_result;

		std::string& source_code() {
			assert(ready());
			return prev_compilation_result->source_code_;
		}

		std::vector<syntax_error>& syntax_errors() {
			assert(ready());
			return prev_compilation_result->syntax_errors_;
		}

		std::vector<instruction> generate_executable_code() {
			assert(ready());

			//TODO fix

			std::vector<instruction> res;
			res.reserve(std::accumulate(prev_compilation_result->basic_blocks_.cbegin(), prev_compilation_result->basic_blocks_.cend(), std::size_t(0),
				[](std::size_t const tmp, std::unique_ptr<basic_block> const& block) -> std::size_t {
					return tmp + block->ops_.size();
				}));


			for (auto const& block : prev_compilation_result->basic_blocks_)
				res.insert(res.end(), block->ops_.cbegin(), block->ops_.cend());

			return res;
		}

		std::vector<std::unique_ptr<basic_block>> const& basic_blocks() {
			assert(ready());
			return prev_compilation_result->basic_blocks_;
		}

		std::vector<std::unique_ptr<basic_block>>& basic_blocks_mutable() {
			assert(ready());
			return prev_compilation_result->basic_blocks_;
		}

		bool successful() {
			assert(ready());
			assert(prev_compilation_result->syntax_errors_.empty() == !prev_compilation_result->basic_blocks_.empty());
			return !prev_compilation_result->basic_blocks_.empty();
		}

		bool ready() {
			return static_cast<bool>(prev_compilation_result);
		}

	} //namespace bf::previous_compilation

	class compiler {

		std::vector<instruction> instructions_;
		std::vector<instruction const*> labels_; //addresses of labels (jump targets)
		std::vector<instruction*> jumps_; //vector of pointers to jump instructions

		/*Performs cleanup of data stored from the previous compilation and prepares the object to compile new code.*/
		void reset_compiler_state() {
			instructions_.clear();
			jumps_.clear();
			labels_.clear();

		}


		/*The Brainfuck compiler frontend. Converts source code to instructions one by one without any optimizations or analysis. The returned vector is however potentially ready
		for execution as all jumps in the code get relocated to point to destinations correctly. */
		void generate_intermediate_code(std::string_view const source_code) {
			//invalid source code cannot appear here; it must be tested against functions from the syntax_check.h header
			assert(is_syntactically_valid(source_code)); //one more test for the validity of the program

			instructions_.reserve(2 + source_code.size()); //reserve enough space for all instructions and program prologue and epilogue
			instructions_.push_back({ op_code::program_entry, 1, 0 }); //the prologue - program's entry instruction

			for (std::size_t source_offset = 0; source_offset < source_code.size(); ++source_offset)  //loop though the code char by char
				switch (source_code[source_offset]) { //and add a new instruction if the char is a command
				case '+': instructions_.push_back({ op_code::inc, 1, source_offset });	  break;
				case '-': instructions_.push_back({ op_code::inc, -1, source_offset });	  break;
				case '>': instructions_.push_back({ op_code::right, 1, source_offset });  break;
				case '<': instructions_.push_back({ op_code::right, -1, source_offset }); break;
				case ',': instructions_.push_back({ op_code::read, 1, source_offset });   break;
				case '.': instructions_.push_back({ op_code::write, 1, source_offset });  break;
				case '[':
					instructions_.push_back({ op_code::jump, std::ptrdiff_t{0xdead'beef}, source_offset }); //Argument will be resolved later
					jumps_.push_back(&instructions_.back());
					break;
				case ']':
					instructions_.push_back({ op_code::jump_not_zero, std::ptrdiff_t{0xdead'beef}, source_offset });
					jumps_.push_back(&instructions_.back());
					break;
					//any other characater is only a comment, therefore we ignore it
				}

			instructions_.push_back({ op_code::program_exit, 1, source_code.size() }); //the epilogue of the program
			assert(jumps_.size() % 2 == 0); //there must be an even number of jump instructions 
		}

		/*Identifies labels in the generated intermediate code and prepares this information for usage during the construction of basic blocks.*/
		void identify_labels() {

			assert(instructions_.size() >= 2); //at least program_entry and program_exit must be present
			//make sure that the generated source_code has proper format
			assert(instructions_.front().op_code_ == op_code::program_entry && instructions_.back().op_code_ == op_code::program_exit);
			assert(jumps_.size() % 2 == 0); //there must be an even number of jump instructions 
			assert(std::all_of(jumps_.cbegin(), jumps_.cend(), std::mem_fn(&instruction::is_jump)));

			/*Each label marks a single leader. Leaders of basic blocks are found by applying the following algorithm:
				1) The first instruction is a leader.
				2) Conditional jumps at closing brace are leaders of their own single instruction blocks
				3) Instructions after opening brackets are leaders of the loop body.
				*/

				//For unconditional jumps, their destination (conditional jump) is a leader 
				//For conditional jumps, their destination and well as the instruction after them (fallthrough) are leaders
				//Two additional labels for program_entry and one past program_exit instructions
			std::size_t const expected_label_count = jumps_.size() + jumps_.size() / 2 + 2;
			labels_.reserve(expected_label_count);

			labels_.push_back(&instructions_.front()); //the entry instruction is a leader

			for (instruction const* const jump : jumps_)  //see comment five lines above
				if (jump->op_code_ == op_code::jump)
					labels_.push_back(jump + 1);
				else if (jump->op_code_ == op_code::jump_not_zero) {
					labels_.push_back(jump);
					labels_.push_back(jump + 1);
				}
				else
					MUST_NOT_BE_REACHED;

			labels_.push_back(&instructions_.back() + 1); //one more label pointing at the past the end instruction
			assert(labels_.size() == expected_label_count); //all precalculated capacity should have been filled

			//there may be multiple labels on the same instruction with this code (two ']' in a row)
			auto const unique_end = std::unique(labels_.begin(), labels_.end()); //remove multiple labels on the same instruction (may happen because of an empty loop)
			labels_.erase(unique_end, labels_.end()); //remove trailing elements
		}

		void resolve_jump_targets() const {
			assert(instructions_.size() >= 2); //at least program_entry and program_exit must be present
			//make sure that the generated source_code has proper format
			assert(instructions_.front().op_code_ == op_code::program_entry && instructions_.back().op_code_ == op_code::program_exit);
			assert(jumps_.size() % 2 == 0); //there must be an even number of jump instructions 
			assert(std::all_of(std::execution::seq, jumps_.begin(), jumps_.end(), std::mem_fn(&instruction::is_jump)));
			//assert that all jump instructions are contained in the jumps_ vector
			assert(jumps_.size() == std::size_t(std::count_if(std::execution::seq, instructions_.begin(), instructions_.end(), std::mem_fn(&instruction::is_jump))));
			assert(std::all_of(std::execution::seq, instructions_.begin(), instructions_.end(), [this](instruction const& i) {return i.is_jump() ? std::find(jumps_.begin(), jumps_.end(), &i) != jumps_.end() : true; }));

			//we have a std::vector<instruction *> containing jump instructions
			//and a std::vector<instruction *> with labels. Element at labels_[i] is the address of instruction under the i-th label

			//number of the next label to be processed. Start at one (it is incremented after the loop body)
			auto next_label = labels_.cbegin();
			std::stack<std::pair<instruction*, std::ptrdiff_t>> opened_loops; //pair of (pointer to the unconditional jump, number of label pointing to the following instruction)

			for (instruction* const jump : jumps_)
				switch (jump->op_code_) { //for each jump instruction perform an operation
				case op_code::jump:   //for opening brace instructions (the unconditional jump):
					next_label = std::find(next_label, labels_.cend(), jump + 1);
					assert(next_label != labels_.cend());
					opened_loops.emplace(jump, std::distance(labels_.cbegin(), next_label)); //push the address of this jump instruction and the corresponding label's index. 
					break;

				case op_code::jump_not_zero:
				{
					assert(!opened_loops.empty()); //in valid code there still has to be some loop remaining
					next_label = std::find(next_label, labels_.cend(), jump);
					assert(next_label != labels_.cend());
					auto const [uncond_jump, target_label_for_cj] = opened_loops.top();

					//The destination for conditional jump from the closing brace is the label at the instruction following the opening brace
					jump->destination_ = target_label_for_cj;

					//The destination for unconditional jump from the opening brace is the label at the closing brace
					uncond_jump->destination_ = std::distance(labels_.cbegin(), next_label);


					opened_loops.pop();   //Must be the last statement, because structured binding to tuple-like structure introduces references! Otherwise dangling reference
					break;
				}
				ASSERT_NO_OTHER_OPTION;
				}

			//all instructions must have their destination different from 0xdead'beef placeholder value
			assert(std::none_of(std::execution::seq, instructions_.begin(), instructions_.end(), [](instruction const& i) {return i.is_jump() ? i.destination_ == 0xdead'beef : false; }));
		}

		std::vector<std::unique_ptr<basic_block>> construct_program_blocks() const {
			{ //A lot of sanity checks...
				assert(labels_.size() >= 2); //at least two labels must exist (entry one and the one past the end)
				assert(instructions_.size() >= 2); //at least program_entry and program_exit must be present
				//make sure that the generated source_code has proper format
				assert(instructions_.front().op_code_ == op_code::program_entry && instructions_.back().op_code_ == op_code::program_exit);
				assert(jumps_.size() % 2 == 0); //there must be an even number of jump instructions 
				assert(std::all_of(std::execution::seq, jumps_.begin(), jumps_.end(), std::mem_fn(&instruction::is_jump)));
				//assert that all jump instructions are contained in the jumps_ vector
				assert(jumps_.size() == std::size_t(std::count_if(std::execution::seq, instructions_.begin(), instructions_.end(), std::mem_fn(&instruction::is_jump))));
				assert(std::all_of(std::execution::seq, instructions_.begin(), instructions_.end(), [this](instruction const& i) {return i.is_jump() ? std::find(jumps_.begin(), jumps_.end(), &i) != jumps_.end() : true; }));
				//all instructions must have their destination different from 0xdead'beef placeholder value
				assert(std::all_of(std::execution::seq, instructions_.begin(), instructions_.end(), [](instruction const& i) {return i.is_jump() ? i.destination_ != 0xdead'beef : true; }));
			}

			std::vector<std::unique_ptr<basic_block>> basic_blocks;
			basic_blocks.reserve(labels_.size() - 1);

			for (std::size_t index = 0; index < labels_.size() - 1; ++index)
				basic_blocks.push_back(std::make_unique<basic_block>(static_cast<std::ptrdiff_t>(index),
					std::vector(labels_[index], labels_[index + 1]))); //instructions 


			assert(!basic_blocks.empty()); //otherwise the following loop would be infinite
			for (std::size_t i = 0; i < basic_blocks.size() - 1; ++i)
				switch (instruction & last_instruction = basic_blocks[i]->ops_.back(); last_instruction.op_code_) {
				case op_code::jump:
				case op_code::jump_not_zero:

					basic_blocks[i]->jump_successor_ = basic_blocks[last_instruction.destination_].get();
					basic_blocks[last_instruction.destination_]->predecessors_.insert(basic_blocks[i].get());
					//We set the target of this jump to some invalid value, because during the process of optimizations, we'll be using pointers
					last_instruction.destination_ = 0xdead'beef;

					if (last_instruction.op_code_ == op_code::jump)
						break;

				default:
					basic_blocks[i]->natural_successor_ = basic_blocks[i + 1].get();
					basic_blocks[i + 1]->predecessors_.insert(basic_blocks[i].get());

					break;
				}

			return basic_blocks;
		}

	public:

		std::vector<std::unique_ptr<basic_block>> compile(std::string_view const code) {

			/* STEPS OF THE COMPILATION PROCESS:
				starting conditions:
					compiler object in an possibly undefined state if some conmpilation had already been perfromed
					source code of the to-be-compiled program. It must be syntactically correct (otherwise an exception or crash occures)

				1) Perform an assertion that the source code is valid as it's expected to be free of any syntax errors when given to the compiler.
						- It does not matter whether the first and second steps are swapped, because should the code is invalid a crash will occur anyway.

				2) Reset the compiler's state.

				3) Generate the íntermediate code - transform the given source code into a sequence of IR instructions.

				4) Cache addresses of jump instructions to prevent repeating excess searches.
					- Should be done simultaneously with the previous step

				5) Indentify and number labels (targets of jump instructions).
					- Found labels mark instructions, that will later become the leaders of basic blocks.

				6) Resolve destinations of jump instructions to target labels instead of instructions.

				7) Construct a net of basic blocks, each having an ID equal to the label denoting its leader instruction.

				8) End of algorithm. The entry block is returned.

			*/

			assert(is_syntactically_valid(code));

			reset_compiler_state();

			generate_intermediate_code(code); //performs steps 3 and 4

			identify_labels();

			resolve_jump_targets();

			return construct_program_blocks();
		}

	};

	namespace {

		/*Wrapper namespace for types and functions for compile_callback. One shall not pollute global namespace.*/
		namespace compile_callback_helper {
			/*Tries to validate and compile given code and set global variable last_compilation_result
			according to the compilation's outcome.	If there are no errors, new syntax_tree is generated,
			otherwise only list of errors is saved. Returns true if compilation was OK.*/
			bool do_compile(std::string code) { // takes code by value, because it is moved later
				thread_local static compiler compiler;

				if (is_syntactically_valid(code)) { //first perform quick scan for errors. If there are none, proceed with compilation
					auto code_blocks = compiler.compile(code);
					assert(!code_blocks.empty()); //must be true, as the code had already undergone a syntax check
					previous_compilation::prev_compilation_result = std::make_unique<compilation_result>(std::move(code), std::vector<syntax_error>{},
						std::move(code_blocks)); //move the entry_block pointer
					return true; //return true indicating that compilation did not encounter any errors
				}
				else { //quick scan found some errors. Scan again collecting all possible information
					std::vector<syntax_error> syntax_errors = syntax_validation_detailed(code);
					assert(syntax_errors.size()); //must contain some errors; we can assert this just for fun :D
					previous_compilation::prev_compilation_result = std::make_unique<compilation_result>(std::move(code), std::move(syntax_errors), std::vector<std::unique_ptr<basic_block>>{}); //empty vector for illegal code
					return false; //indicate that compilation failed
				}
			}

			/*Reads the source code for compilation and reports errors if reading does not succeed.
			If the arguments are ok, returns std::optional containing the source code. An empty object is returned otherwise.*/
			std::optional<std::string> get_source_code(std::string_view const source, std::string_view const arg) {
				if (source == "code") //second arg is raw source code
					return std::string{ arg };

				//the first arg is "file", therefore the second one is file name
				if (source == "file") {
					std::optional<std::string> const file_content = utils::read_file(arg);
					if (!file_content.has_value()) //if file doesn't exist, print error
						cli::print_command_error(cli::command_error::file_not_found);

					//return the optional. On success this holds a valid source code, on failure this is constructed from std::nullopt
					return file_content;
				}

				//first argument is not valid - print error
				cli::print_command_error(cli::command_error::argument_not_recognized);
				return std::nullopt;
			}

		} //namespace bf::`anonymous`::compile_callback_helper


		/*Function callback for cli command "compile".
		Expects two arguments, first is specification whether code or filename is specified, the second being either
		the source code or name of file. The function first checks whether the code denotes a valid program and then compiles
		it to symbolic instruction. If compilation succeeds, program in saved internally and may be flashed to cpu emulator'r memory
		using the command "install"*/
		int compile_callback(cli::command_parameters_t const& argv) {
			namespace helper = compile_callback_helper;
			if (int const ret_code = utils::check_command_argc(3, 3, argv))
				return ret_code; //there must be three arguments

			std::optional<std::string> source_code = helper::get_source_code(argv[1], argv[2]);
			if (!source_code.has_value())
				return 4;

			//call helper function trying to compile the source code
			bool const success = helper::do_compile(std::move(*source_code));
			if (!success) {//compilation failed due to errors
				std::size_t const err_count = previous_compilation::syntax_errors().size();
				std::cout << "Found " << err_count << " error" << utils::print_plural(err_count)
					<< ". You may print more details using the \"errors\" command.\n";
				return 1;
			}
			//compilation was successful 
			std::size_t const instruction_count = previous_compilation::generate_executable_code().size();
			std::cout << "Successfully compiled " << instruction_count << " instruction" << utils::print_plural(instruction_count) << ".\n";
			return 0;
		}

		/*Wrapper namespace for types and functions used by "errors_callback". Does not pollute global namespace, thankfully*/
		namespace errors_callback_helper {

			int print_error_detail(std::size_t const index) {
				assert(previous_compilation::ready() && !previous_compilation::successful()); //if there was no compilation or it completed ok, we have an error

				if (std::size_t const error_count = previous_compilation::syntax_errors().size(); index >= error_count) {
					std::cerr << "Requested index " << index << " is out of bounds. Valid range is [0, " << error_count << ").\n";
					return 5;	
				}
				syntax_error const& error = previous_compilation::syntax_errors()[index];
				std::string source_code_line{ *utils::get_line(previous_compilation::source_code(), error.location_.line_) };
				for (std::size_t i = 0; i < source_code_line.size(); ++i) //TODO comment this
					if (source_code_line[i] == '\t')
						source_code_line.replace(i, 1, cli::TAB_WIDTH, ' ');

				std::cout << std::right << std::setw(5) << index << std::left << ". syntax error: " << error.message_
					<< " at (" << error.location_.line_ << ", " << error.location_.column_
					<< ") {\n\t" << source_code_line << "\n\t" << std::string(error.location_.column_ - 1, ' ') << "^\n}\n";
				return 0;
			}

			/*Prints all syntax errors to stdout. If there had been no errors, does nothing.
			if print_full is true, prints full information about all errors.*/
			int print_error_summary() {
				assert(previous_compilation::ready() && !previous_compilation::successful()); //if there was no compilation or it completed ok, we have an error
				int index = 0;
				for (auto const& error : previous_compilation::syntax_errors())
					std::cout << std::right << std::setw(5) << index++ << std::left << ". syntax error: "
					<< error.message_ << " at (" << error.location_.line_ << ", " << error.location_.column_ << ").\n";
				return 0;
			}

		} //namespace bf::`anonymous`::namespace errors_callback_helper

		/*Function callback for cli command "errors".
		Expects single argument and depending on its value does various stuff and prints all syntax errors if some compilation had alredy been performed
		If there had been no compilation performed, function just returns.*/
		int errors_callback(cli::command_parameters_t const& argv) {
			namespace helper = errors_callback_helper;
			if (int const ret_code = utils::check_command_argc(2, 2, argv))
				return ret_code;
			if (!previous_compilation::ready()) {//there hasn't been any compilation performed yet
				std::cerr << "No compilation has been performed. Compile the program with the \"compile\" command first.\n";
				return 3;
			}
			if (previous_compilation::successful()) {
				std::cout << "Previous compilation was successful.\n";
				return 0;
			}

			if (argv[1] == "all")
				return helper::print_error_summary();
			else if (argv[1] == "full")
				for (std::size_t i = 0; i < previous_compilation::syntax_errors().size(); ++i)
					helper::print_error_detail(i);
			else if (argv[1] == "count") {
				if (std::size_t const count = previous_compilation::syntax_errors().size(); count == 1u)
					std::cout << "There has been one error.\n";
				else
					std::cout << "There have been " << count << " errors.\n";
				return 0;
			}
			else if (std::optional<int> const index = utils::parse_nonnegative_argument(argv[1]); index.has_value())
				return helper::print_error_detail(*index);
			else {
				cli::print_command_error(cli::command_error::argument_not_recognized);
				return 4;
			}
		}

	} //namespace bf::`anonymous namespace`


	void compiler_initialize() {
		ASSERT_IS_CALLED_ONLY_ONCE;
		using namespace bf::cli;
		add_command("compile", command_category::compilation, "Compiles given source code.",
			"Usage: \"compile\" (\"code\" | \"file\") argument\n"
			"argument is either string of characters interpreted as source code if \"code\" is specified\n"
			"or a name of file containing the source code in case \"file\" is specified.\n"
			"Additional information about the outcome of the compilation can be queried by commands from the \"compilation\" group."
			, &compile_callback);

		add_command("errors", command_category::compilation, "Queries the results of previous compilation and prints syntax errors.",
			"Usage: \"errors\" argument\n"
			"Single argument is expected and its meaning is heavily dependent on context.\n"
			"\targument == \"all\" => list of syntax errors is simply printed out\n"
			"\targument == \"full\" => similar list is printed, but every error is printed with all known details\n"
			"\targument == \"count\" => prints number of syntax errors\n"
			"\targument == non-negative number => prints information about a single error specified by the number\n"
			"\targument value of \"full\" has therefore the same effect as consecutive calls of this command specifying err numbers in increasing order.\n"
			, &errors_callback);
	}


} // namespace bf