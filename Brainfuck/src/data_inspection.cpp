#include "data_inspection.h"
#include "cli.h"
#include "emulator.h"

#include <iomanip>
#include <sstream>
#include <algorithm>
#include <locale>
#include <iostream>
#include <cassert>
#include <charconv>

namespace bf::data_inspection {

	namespace {

		/*Namespace wrapping helper functions for mem_callback. Shall not pollute global namespace.*/
		namespace mem_callback_helper {

			/*Enumeration of different ways of inspecting data. Most importantly specifies the size of units by which data is inspected.*/
			enum class data_type {
				none,
				byte,
				word,
				dword,
				qword,
				character,
				instruction
			};

			int sizeof_data_type(data_type type) {
				//Map of data types to their sizes in bytes (instructions dont use void* arithmetic, but normal typed ptr => they have size one)
				static std::unordered_map<data_type, int> const data_sizes{
					{data_type::byte,		 1},
					{data_type::word,		 2},
					{data_type::dword,		 4},
					{data_type::qword,		 8},
					{data_type::character,	 1},
					{data_type::instruction, 1}
				};
				assert(data_sizes.count(type));
				return data_sizes.at(type);
			}

			/*Output stream operator for data_type*/
			std::ostream& operator<<(std::ostream& str, data_type type) {
				switch (type) {
				case data_type::byte: str << "byte"; break;
				case data_type::word: str << "16-bit word"; break;
				case data_type::dword: str << "32-bit doubleword"; break;
				case data_type::qword: str << "64-bit quadword"; break;
				case data_type::character: str << "character"; break;
				case data_type::instruction: str << "instruction"; break;
					ASSERT_NO_OTHER_OPTION
				}
				return str;
			}

			/*Enumeration of differrent ways how inspected data may be interpreted and printed (things like radix and signedness).*/
			enum class data_format {
				none,
				hex,
				oct,
				sign,
				unsign,
				character,
				instruction
			};


			/*Structure of all parameters required for a memory inspection. Specifies location and direction of inspection, its size, type of elements and printing format.*/
			struct request_params {
				int count_;						//how many elements shall be printed
				void * address_;				//pointer to the requested memory block 
				data_type type_;				//type of data
				data_format format_;			//printing format
				bool print_preceding_memory_;	//determines whether the address_ field is the beginning or the end of memory block 
			};

			/*Functions providing the service of parsing of passed arguments*/
			namespace parsing {

				/*Class which performs the process of request parsing. It was refactored into a class because of the need to keep some state during the parsing
				(already parsed pieces of information or the pointers into the parsed string) and having the state global did not really appealed to me.*/
				class request_parser {

					int count_ = 0; // holds the requested number of elements to be examined. Zero indicates an error
					data_type type_ = data_type::none; //stores information about the requested type of elements
					data_format format_ = data_format::none; //stores info about the requested format of printing

					bool error_occured_ = false;
					char const * iterator_, *end_;

					//Extracts a number from the string and interprets it as the count
					void parse_count() {
						assert(std::isdigit(*iterator_, std::locale{})); //Must be called only in appropriate situations
						if (count_ == 0) {//if no number had yet been parsed, parse it
							iterator_ = std::from_chars(iterator_, end_, count_).ptr; //iterator is incremented to the first char after the number.
							if (count_ == 0) {
								std::cerr << "Count cannot be zero.\n";
								error_occured_ = true;
							}
						}
						else { //we have already parsed a number => parse error
							std::cerr << "Count had already been specified.\n";
							error_occured_ = true;
						}
					}

					//Extract information about datatype
					void parse_type() {
						assert(types_map.count(*iterator_));
						if (type_ == data_type::none) { //no type has been specified
							type_ = types_map.at(*iterator_++);
							if (type_ == data_type::character) // if characters are requested, set both type and format
								format_ = data_format::character;
							else if (type_ == data_type::instruction)
								format_ = data_format::instruction;
						}
						else { //type had already been specified => parse error
							std::cerr << "Data type had already been specified.\n";
							error_occured_ = true;
						}
					}

					//Extract information about the printing format
					void parse_format() {
						assert(formats_map.count(*iterator_));
						if (format_ == data_format::none) { //no format has been specified
							format_ = formats_map.at(*iterator_++);
							if (format_ == data_format::character) // if characters are requested, set both type and format
								type_ = data_type::character;
							else if (format_ == data_format::instruction)
								type_ = data_type::instruction;
						}
						else { //format has already been specified => parse error
							std::cerr << "Print format had already been specified.\n";
							error_occured_ = true;
						}
					}

				public:

					/*Tries to parse specified format_string. It is searched for instructions regarding the type of elements
					as well as its print format and number of elements which shall be examined. If all information is passed
					correctly, it is returned in a structure. In case an error is encountered, returned structure has error values
					such as count_ ==-1 and both type_ and format_ == none.*/
					request_params operator()(std::string_view const format_string) {

						bool examine_before = false;

						//Loop through the passed string searching for type and format specifiers as well as a number (count)
						for (iterator_ = format_string.data(), end_ = iterator_ + format_string.size(); iterator_ != end_ && !error_occured_; ) {
							if (*iterator_ == '-') {
								++iterator_;
								if (!examine_before)
									examine_before = true;
								else {
									std::cerr << "Direction of examination has already been specified.\n";
									error_occured_ = true;
								}
							}
							else if (std::isdigit(*iterator_, std::locale{})) //we have found a number
								parse_count();
							else if (types_map.count(*iterator_)) //if the referenced char specifies an element type
								parse_type();
							else if (formats_map.count(*iterator_))  //if the referenced character specifies a format
								parse_format();
							else { // referenced char does not denote any recognized entity. That is a parse error as well.
								cli::print_command_error(cli::command_error::argument_not_recognized);
								error_occured_ = true;
								break;
							}
						}


						/*If an error has been encountered or some parameter was not specified, set variables to error state
						already the first condition should catch most of the possible errors, but those others are required to catch
						problems such as ommiting one of the parameters. Thus if any of the variables retains its initial value, we have an error.*/
						if (error_occured_ || type_ == data_type::none || format_ == data_format::none) {
							count_ = 0;
							type_ = data_type::none;
							format_ = data_format::none;
						}
						else if (count_ == 0) //if no count was given, set it to one
							count_ = 1;
						return { count_, nullptr, type_, format_, examine_before }; //return parsed information.
					}

				private:

					//static maps needed for type and format information parsing
					inline static std::unordered_map<char, data_type> const types_map{
						{'b', data_type::byte},
						{'w', data_type::word},
						{'d', data_type::dword},
						{'q', data_type::qword},
						{'c', data_type::character},
						{'i', data_type::instruction}
					};

					inline static std::unordered_map<char, data_format> const formats_map{
						{'x', data_format::hex},
						{'o', data_format::oct},
						{'u', data_format::unsign},
						{'s', data_format::sign},
					};
				};

				/*Simple enumeration describing into which address space the pointer from resolve_address shall point*/
				enum class address_space {
					//none,
					data,
					code
				};

				/*Split the passed expression to tokens - operators, register names and literals.*/
				std::vector<std::string_view> tokenize_arithmetic_expression(std::string_view const expression) {
					if (expression.empty()) //empty expression does not need to be split further, does it?
						return {};
					std::vector<std::string_view> tokens;
					std::string_view::const_iterator iter = expression.begin(), end = expression.end();

					//Check for and handle situation that the expression starts with an operator
					if (char c = *iter; c == '+') //if we start with a single prefix plus, we ignore it
						++iter;
					else if (c == '-') { //if the first token is a prefix minus, a zero literal is prepended to it
						using namespace std::string_view_literals;
						tokens.emplace_back("0"sv);
						tokens.emplace_back("-"sv);
						++iter;
					}

					for (; iter != end; ) {
						std::string_view::const_iterator end_of_token = std::next(iter);
						if (*iter == '$') //parse a name of a variable
							while (end_of_token != end && std::isalpha(*end_of_token, std::locale{}))
								++end_of_token;
						else if (std::isdigit(*iter, std::locale{}))  //parse numeric literal
							while (end_of_token != end && std::isdigit(*end_of_token, std::locale{}))
								++end_of_token;
						//operators are always single letter -> just append them to the list
						tokens.emplace_back(expression.data() + std::distance(expression.begin(), iter), std::distance(iter, end_of_token));
						iter = end_of_token;
					}
					return tokens;
				}

				/*Finite state automaton validating the given arithmetic expression.*/
				class expression_validator {

					bool expects_value_ = true;
					bool register_used_ = false;
					bool error_ = false;
					address_space address_space_;

					std::vector<std::string_view>::const_iterator current_token_;


					void match_register() {
						if (!expects_value_ || register_used_) //if we already used a register or we don't expect value, expression has an error
							error_ = true;
						if ((address_space_ == address_space::code && current_token_->compare("$pc"))
							|| (address_space_ == address_space::data && current_token_->compare("$cpr"))) {
							std::cout << "Address space conflict. Data and code reside in separate locations.\n";
							error_ = true;
						}
						register_used_ = true;
						expects_value_ = false;
					}

					void match_operator() {
						switch (current_token_->front()) {
						case '+':
						case '-':
						case '*':
							if (expects_value_)
								error_ = true;
							expects_value_ = true;
							break;
						default:
							std::cout << "Unrecognized token " << *current_token_ << " while calculating offset.\n";
							error_ = true;
						}
					}


				public:
					expression_validator(address_space space) : address_space_(space) {}

					bool operator()(std::vector<std::string_view> const& tokens) {
						if (tokens.size() % 2 == 0) //even number of tokens means that some binary operator does not have two operands
							return false;

						bool register_used = false; //a register's value can be used only once.

						for (auto token = tokens.begin(), end = tokens.end(); token != end && !error_; ++token)
							if (token->front() == '$')  //the token is a CPU's register
								match_register();
							else if (std::isdigit(token->front(), std::locale{})) { //the token is a number
								if (!expects_value_)
									return false;
								expects_value_ = false;
							}
							else  //most likely an operator or an error, of course
								match_operator();
						return error_;
					}

				};

				class expression_evaluator {

					struct abstract_node {
						virtual int evaluate() = 0;
						virtual ~abstract_node() = 0;

						virtual abstract_node *&left() = 0;
						virtual abstract_node *&right() = 0;
					};


					/*Node, whose evaluation is performed by applying the specified arithemtic operation on the results of evaluation of two subnodes.
					Is further specialized with functors from the standard library.*/
					template<typename OPERATION>
					struct operation_node : abstract_node {

						operation_node(abstract_node *l, abstract_node *r) : left_(l), right_(r) {}
						virtual ~operation_node() override { delete left_; delete right_; }

						abstract_node *left_, *right_;

						virtual int evaluate() override {
							return OPERATION{}(left_->evaluate(), right_->evaluate());
						}

						virtual abstract_node *&right() override { return right_; }
						virtual abstract_node *&left() override { return left_; }

					};

					using plus_node = operation_node<std::plus<int>>;
					using minus_node = operation_node<std::minus<int>>;
					using mul_node = operation_node<std::multiplies<int>>;


					/*Node containing number literal, whose evaluation simply yields this literal.*/
					struct literal_node : abstract_node {

						literal_node(int i) : value_(i) {}
						virtual ~literal_node() override {};

						int value_;

						virtual int evaluate() override {
							return value_;
						}

						virtual abstract_node *&right() override { assert(false); throw std::exception{ "Must not be reached!" }; }
						virtual abstract_node *&left() override { assert(false); throw std::exception{ "Must not be reached!" }; }
					};

					int evaluate_token(std::string_view const token) {
						if (std::isdigit(token.front(), std::locale{})) { //number is converted to integer
							int val;
							assert(std::from_chars(token.data(), &*token.end(), val).ptr == token.data() + token.size());
							return val;
						}
						else if (token.front() == '$') { //a variable encountered
							if (token.compare("$pc") == 0) //program counter
								return execution::emulator.program_counter();
							else if (token.compare("$cpr") == 0) //cell pointer register
								return execution::emulator.cell_pointer_offset();
							else assert(false); //validating function should have cought all errors and typos
						}
						else assert(false); //validating function should have cought all errors and typos
						return -1; //sentinel value, never reached;
					}

				public:
					int operator()(std::vector<std::string_view> const& tokens) {
						assert(tokens.size() % 2 == 1);//sanity check, the number of tokens has to be odd (all operators are binary)

						if (tokens.size() == 1u) //if we got just a single arg, evaluate it straight away
							return evaluate_token(tokens[0]);

						//otherwise build a tree using heap pointers starting with a IIFE
						abstract_node *root = [](abstract_node * const left, abstract_node * const right, char const operation) -> abstract_node* {
							switch (operation) {
							case '+':
								return new plus_node{ left, right };
							case '-':
								return new minus_node{ left, right };
							case '*':
								return new mul_node{ left, right };
								ASSERT_NO_OTHER_OPTION;
							}
							//evaluate first three tokens and estabilish an expression tree
						}(new literal_node{ evaluate_token(tokens[0]) }, new literal_node{ evaluate_token(tokens[2]) }, tokens[1].front());

						//continue building the tree one operation and one value at a time
						for (int i = 3; i < static_cast<int>(tokens.size()); i += 2) {
							literal_node * new_right = new literal_node{ evaluate_token(tokens[i + 1]) };
							switch (tokens[i].front()) {
							case '+':
								root = new plus_node{ root, new_right };
								break;
							case '-':
								root = new minus_node{ root, new_right };
								break;
							case '*':
								root->right() = new mul_node{ root->right(), new_right };
								break;
								ASSERT_NO_OTHER_OPTION
							}
						}
						int result = root->evaluate();//recursively evaluate the entire expression,
						delete root;  //delete all the allocated nodes
						return result; //and return result
					}
				};

				//pure virtual destructor must have a body since it's eventually called anyway
				inline expression_evaluator::abstract_node::~abstract_node() {}

				/*In case evaluation of expression results in a negative offset, this function recalculates this offset to make the inspected memory
				block fit in between the bounds of valid adress spaces. The memory block is effectively shrinked in the process*/
				int recalculate_negative_address(int &address_offset, int &count, data_type const requested_type) {

					assert(address_offset < 0);

					int const element_size = sizeof_data_type(requested_type);
					//the number of elements we have to skip whatever happens
					auto[skipped_count, remainder] = std::div(-address_offset, element_size);
					assert(skipped_count * element_size + remainder == -address_offset); //sanity check lul, first time working with std::div

					if (remainder) //offset is not an integer multile of element_size
						++skipped_count;

					address_offset += skipped_count * element_size; //move the byte offset by multiple of element_size
					count -= skipped_count; //and decrease the number of elements we will be printing


					if (count <= 0) {//if the offset was so far out of bounds that no part of the inspected memory block is accessible, return err value
						std::cout << "Address is too far out of bounds. Operation canceled.\n";
						return 1; //out of bounds
					}
					std::cout << "Skipping " << skipped_count << " element" << cli::print_plural(skipped_count) << " that were out of bounds.\n";
					return 0;
				}


				/*Tries to parse a string containing the address and sets the corresponding field in the referenced structure accordingly.
				If an erroneous string is given or any other error is encountered, non-zero integer is returned. If the address is specified relative to some
				cpu register then the calculation of offsets is performed and pointer directly to the requested instruction is calculated.*/
				int resolve_address(request_params & request, std::string_view const address_string) {
					address_space const expected_address_space = request.type_ == data_type::instruction ? address_space::code : address_space::data;
					std::vector<std::string_view> expression_pieces = tokenize_arithmetic_expression(address_string);

					//If the expression is invalid, there is no point trying to evaluate it
					if (expression_validator{ expected_address_space }(expression_pieces)) {
						std::cerr << "Unable to examine memory using invalid syntax for address string. Check help message for this command.\n";
						return 4;
					}

					int offset = expression_evaluator{}(expression_pieces);		//calculate the offset specified by the user
					if (request.print_preceding_memory_) //and take the possible inversion of direction into account
						offset -= request.count_ * sizeof_data_type(request.type_);

					if (offset < 0)  //if the evaluation resulted in negative number, move the address to accessible memory area and decrease count
						if (recalculate_negative_address(offset, request.count_, request.type_))
							return 1; //in case we were too far out of bounds, return err code

					if (expected_address_space == address_space::data) { //data memory shall be inspected
						if (offset >= execution::emulator.memory_size()) {//we want some data behind the end of memory
							std::cerr << "Specified address was out of bounds.\n";
							return 2;
						}
						request.address_ = static_cast<unsigned char*>(execution::emulator.memory_begin()) + offset;
					}
					else { //we are working in the instruction memory
						if (offset >= execution::emulator.instructions_size()) {//we an instruction behind the end of memory
							std::cerr << "Specified address was out of bounds.\n";
							return 3;
						}
						request.address_ = execution::emulator.instructions_begin() + offset;
					}
					return 0;
				}


				/*Parses given parameters and extracts all necessary arguments for memory inspection. These arguments are returned together with
				an error code, which signals whether the parsing phase resulted in an error or not. Zero signals that everything is allright.*/
				std::pair<request_params, int> parse_parameters(std::string_view const format, std::string_view const address) {

					request_params request = request_parser{}(format); //collect all information about type, count and format
					if (request.count_ == 0) {
						std::cerr << "Unable to examine memory using invalid syntax for format string. Check help message for this command.\n";
						return { request, 3 };
					}

					//add information about the address. 
					int ret_code = resolve_address(request, address);
					return { request, ret_code };
				}
			}

			/*Structure enabling easy bounds checking on the inspected memory region.
			Contains pair of pointers to the beginning and end of the raw memory sequence and integer
			specifying how many elements could not be inspected due to lask of more memory.*/
			template<typename ELEMENT_TYPE>
			struct inspected_memory_region {
				ELEMENT_TYPE* begin_, *end_;
				int unreachable_count_;
			};

			/*Get boundaries of memory region program may examine. Accepts an address at which inspection is to be started and requested number of elements.
			Calculates the longest possible block of raw memory starting at address which contains whole consecutive elements of template type parameter.
			This memory region is then returned as pair of pointers to the beginnign and end. Additionally the number of elements which were requested,
			but did not fit into the memory is returned as well.*/
			template<typename ELEMENT_TYPE>
			inspected_memory_region<ELEMENT_TYPE> get_inspected_data_memory_region(void *const address, int const requested_element_count) {
				//sanity check; this is an internal function which shall run safely (address must be located somewhere within cpu¨s memory)
				assert(address >= execution::emulator.memory_begin() && address < execution::emulator.memory_end());

				//We have to perform nested typecasts to prevent compile errors if emulator's memory's underlying cell type changed
				int const bytes_to_end = static_cast<int>(std::distance(static_cast<char*>(address), static_cast<char*>(execution::emulator.memory_end())));


				//Actual number of elements is the lower from maximal possible number and requested count
				int const element_count = std::min<int>(bytes_to_end / sizeof(ELEMENT_TYPE), requested_element_count);
				if (element_count == 0)
					std::cout << "No element can be printed, too close to the memory's bounds.\n";

				return { static_cast<ELEMENT_TYPE*>(address),
					static_cast<ELEMENT_TYPE*>(address) + element_count,
					requested_element_count - element_count };
			}

			/*Functions in this namespace are helpers to ease memory examination. They all behave almost the same,
			take an address at which they shall start and keep printing elements until count reaches zero or they run out of memory.
			Functions attempt to print data from memory in the form of a table with rows being memory addresses and columns offsets in bytes.*/
			namespace printer_functions {

				/*Helper function for do_print. If the given character is printable, it is returned as it is. If it denotes an escape
				sequence, a short string represenation is printed. Otherwise hex value is printed.*/
				std::string const get_readable_char_representation(char const c) {
					static  std::unordered_map<char, std::string> const escape_seqs{
						{'\0', "NUL"},
						{'\n', "LF"},
						{'\t', "HT"},
						{'\v', "VT"},
						{'\a', "BEL"},
						{'\b', "BS"},
						{'\r', "CR"}
					};
					if (std::isprint(c, std::locale{})) //alphanumericchars and punctuation is can be printed
						return { c };
					if (escape_seqs.count(c)) //escape sequences
						return escape_seqs.at(c);
					return "0x" + std::to_string(static_cast<unsigned int>(c)); //hex value for others
				}

				/*I've been writing this function for half an hour and am fed up with it, so comment has to wait for now.
				This function does the heavy lifting of printing to stdout and is a victim of severe optimizations of mine.
				Previously 5 functions were used, each operated on single width of data, but that had proved itself useless,
				messy, unmaintainable and the worst thing - template-free. It has been fixed since.

				This function prints a table filled with count pieces of data located at the requested address.
				This function accepts an address and number of elements, which shall be read from memory starting at this address.
				The type of data is passed as type template parameter, the other two template parameters are a pointer specifying
				the IO manipulator which shall be used to convert numbers to the correct radix and an integer literal specifying
				the width (number of characters) that shall be reserved for a single element's string representation to preserve
				nice table. Hopefully.

				First the memory region which is to be inspected is located and it is ensured that the function will only read
				elements from valid memory by shrinking the range so that it fits withing the boundaries of cpu's memory.
				Second a buffer is constructed and the first line containing the address offsets is printed. After that lines are
				printed one by one advancing the pointer until it traverses the entire sequence. The function then returns.
				*/
				template<typename ELEMENT_TYPE, std::ios_base &(*RADIX_MANIPULATOR)(std::ios_base&), int NUMBER_WIDTH>
				void do_print_data(void *const address, int const count) {
					//get boundaries between which it is safe to read values from memory
					inspected_memory_region<ELEMENT_TYPE> mem_region = get_inspected_data_memory_region<ELEMENT_TYPE>(address, count);
					if (count == mem_region.unreachable_count_) //we are too close to boundary, must return
						return;
					constexpr int elements_on_line = 16 / sizeof(ELEMENT_TYPE); //number of consecutive elements which will reside on single line

					std::ostringstream stream; //Output buffer enormously increasing the speed of printing
					stream << std::uppercase << std::right << std::setw(12) << "address" << std::hex; //set alignment to left and enable printing of radix-prefix
					for (int i = 0; i < elements_on_line; ++i) //print column descriptions (=offsets from the address printed at the beginning of line)
						stream << std::setw(NUMBER_WIDTH) << i * sizeof(ELEMENT_TYPE);
					stream << std::setfill('0') << std::showbase << "\n\n";

					int memory_offset = static_cast<int>(std::distance(static_cast<char*>(execution::emulator.memory_begin())
						, static_cast<char*>(static_cast<void*>(mem_region.begin_))));

					//While there are more elements than can fit on a single line, print them by lines
					while (std::distance(mem_region.begin_, mem_region.end_) >= elements_on_line) {
						//first print the pointer value and follow it with the given manipulator 
						stream << std::setw(12) << std::setfill(' ') << std::hex << memory_offset << RADIX_MANIPULATOR;
						memory_offset += 16;

						for (int i = 0; i < elements_on_line; ++i, ++mem_region.begin_) {//print values
							stream << std::setw(NUMBER_WIDTH); //reserves enough space in output stream
							if constexpr (std::is_same_v<std::remove_cv_t<ELEMENT_TYPE>, char>) //compile time if
								stream << get_readable_char_representation(*mem_region.begin_); //print as character
							else
								stream << +*mem_region.begin_; //print as a number
						}
						stream << '\n';
					}
					if (mem_region.begin_ != mem_region.end_) { //if there is a part of line remaining
						stream << std::setw(12) << std::setfill(' ') << std::hex << memory_offset << RADIX_MANIPULATOR;
						for (; mem_region.begin_ != mem_region.end_; ++mem_region.begin_) {//print the address followed by consecutive values again
							stream << std::setw(NUMBER_WIDTH); //reserve space once more
							if constexpr (std::is_same_v<std::remove_cv_t<ELEMENT_TYPE>, char>)
								stream << get_readable_char_representation(*mem_region.begin_); //print as char 
							else
								stream << +*mem_region.begin_; //print as number
						}
						stream << '\n';
					}
					if (mem_region.unreachable_count_) //requested memory would exceed cpu's internal memory
						stream << "Another " << std::dec << mem_region.unreachable_count_ << " element" << cli::print_plural(mem_region.unreachable_count_, " has", "s have")
						<< " been requested, but " << cli::print_plural(mem_region.unreachable_count_, "was", "were") << " out of bounds of cpu's memory.\n"
						"There have also been " << std::distance(static_cast<char*>(static_cast<void*>(mem_region.begin_)), static_cast<char*>(execution::emulator.memory_end()))
						<< " misaligned memory locations between last printed address and memory's boundary.\n";
					std::cout << stream.str(); //print buffer's content to stdout
				}

				/*Performs static dispatch and calls appropriate function for given combination of type (=byte width) and requested printing format.
				Calls do_print using appropriate combination of template and non-template parameters.*/
				template<typename ELEMENT_TYPE>
				void resolve_data_printer_function(void *address, int count, data_format format) {
					//constants specifying the expected ideal width of single element's string representation. Based on an educated guess
					constexpr int hex_width = 2 + sizeof(ELEMENT_TYPE) * 2 + sizeof(ELEMENT_TYPE),
						oct_width = 1 + sizeof(ELEMENT_TYPE) * 8 / 3 + 1 + sizeof(ELEMENT_TYPE),
						char_width = sizeof(ELEMENT_TYPE) + 6,
						dec_width = sizeof(ELEMENT_TYPE) * 3 + sizeof(ELEMENT_TYPE);

					static std::unordered_map<data_format, void(*)(void*, int)> const print_functions{
						{data_format::hex, do_print_data<std::make_unsigned_t<ELEMENT_TYPE>, std::hex, hex_width>}, //pass std::hex as manipulator
						{data_format::oct, do_print_data<std::make_unsigned_t<ELEMENT_TYPE>, std::oct, oct_width>}, //pass std::oct as manipulator
						{data_format::sign, do_print_data<std::make_signed_t<ELEMENT_TYPE>, std::dec, dec_width>},  //make sure a signed type is used and pass std::dec as as manipulator
						{data_format::unsign, do_print_data<std::make_unsigned_t<ELEMENT_TYPE>, std::dec, dec_width>} //pass std::dec and assure data is read using an unsigned type
					};
					assert(count > 0); //sanity check
					assert(format != data_format::instruction);

					//compile time branch to make dispatch faster if cv char is specified 
					if constexpr (std::is_same_v<std::remove_cv_t<ELEMENT_TYPE>, char>)
						return do_print_data<char, std::dec, char_width>(address, count);
					else {
						assert(print_functions.count(format));
						print_functions.at(format)(address, count);
					}
				}


				/*Printing function for instructions. Prints them one by one until the end of memory or requested count is reached.*/
				void do_print_instructions(void * const address, int count, data_format const format) {
					assert(format == data_format::instruction);
					assert(count > 0); //sanity checks
					assert(address < execution::emulator.instructions_end());
					assert(address >= execution::emulator.instructions_begin());

					std::ostringstream stream; //buffer to increase printing speed
					instruction const* current_instruction = static_cast<instruction*>(address),
						*const instructions_end = execution::emulator.instructions_end();

					//offset of the first instruction from the memory's start
					int instruction_index = static_cast<int>(std::distance<instruction const*>(execution::emulator.instructions_begin(), current_instruction));
					//saved position of program counter to print an arrow indicator at the location of instruction under the pointer
					int const pc_position = execution::emulator.program_counter();

					for (; current_instruction != instructions_end && count; ++current_instruction, --count) {
						stream << std::setw(2) << std::right << (instruction_index == pc_position ? "=>" : "");
						stream << std::setw(6) << instruction_index++ << "   "; //must be separated, else unsequenced modification of instruction_index

						if (current_instruction->type_ == instruction_type::breakpoint) { //if we encounter a breakpoint, we print the replaced instruction instead
							//address of this instruction
							int const addr = static_cast<int>(std::distance<instruction const*>(execution::emulator.instructions_begin(), current_instruction));
							instruction const& replaced_instruction = breakpoints::bp_manager.get_replaced_instruction(addr); //the replaced instruction to be printed

							auto const &breakpoints = breakpoints::bp_manager.get_breakpoints(addr); //get all breakpoints located at this address
							stream << replaced_instruction.type_ << ' ' << std::left << std::setw(12) << replaced_instruction.argument_
								<< " <= breakpoint" << cli::print_plural(breakpoints.size());
							for (auto const& bpoint : breakpoints) //and print them 
								stream << ' ' << bpoint->id_;
						}
						else
							stream << current_instruction->type_ << ' ' << current_instruction->argument_; //normal instructions are simply printed
						stream << '\n';
					}
					if (current_instruction == instructions_end) { //if we end printing due to the lask of additional instructions, notify the user
						stream << "End of memory has been reached.\n";
						if (count)
							stream << "Skipping " << count << " instruction" << cli::print_plural(count) << " .\n";
					}
					std::cout << stream.str(); //finally flush it all to the std:cout
				}
			} //namespace printer_functions

			/*Perform dynamic dispatch and call appropriate callback for given type of data. Does not do much more...*/
			void perform_print_function_resolution(request_params const& request) {
				//static const map of function pointers which are used as a callback for given data_type 
				static  std::unordered_map<data_type, void(*)(void*, int, data_format)> const print_functions{
					{data_type::byte, &printer_functions::resolve_data_printer_function<uint8_t>},
					{data_type::word, &printer_functions::resolve_data_printer_function<uint16_t>},
					{data_type::dword, &printer_functions::resolve_data_printer_function<std::uint32_t>},
					{data_type::qword, &printer_functions::resolve_data_printer_function<std::uint64_t>},
					{data_type::character, &printer_functions::resolve_data_printer_function<char>},
					{data_type::instruction, &printer_functions::do_print_instructions}
				};
				assert(print_functions.count(request.type_));
				//sanity checks of parameters
				assert(request.count_ > 0);
				assert(request.type_ != data_type::none);
				assert(request.format_ != data_format::none);
				//Sanity check. This is an internal function and therefore must operate safely 
				if (request.type_ == data_type::instruction)
					assert(execution::emulator.instructions_begin() <= request.address_ && request.address_ < execution::emulator.instructions_end());
				else
					assert(execution::emulator.memory_begin() <= request.address_ && request.address_ < execution::emulator.memory_end());

				std::cout << "Memory inspection of " << request.count_ << ' ' << request.type_ << cli::print_plural(request.count_) << '\n';

				//get the coresponding function pointer and call it passing the inspection request
				print_functions.at(request.type_)(request.address_, request.count_, request.format_);
			}

		}

		/*Callback function for the "mem" cli command. Accepts two arguments, the first one being the inspection request
		whilst the second one specifying a memory address at which the inspection shall be initiated. These arguments are
		first parsed, if this parsing succeeds, functions examining the raw memory are invoked.
		Returns zero if everything goes well, some non-zero integer otherwise.*/
		int mem_callback(cli::command_parameters_t const& argv) {
			namespace helper = mem_callback_helper;
			if (int const code = cli::check_command_argc(3, 3, argv.size()); code)
				return code;

			if (!execution::emulator.has_program()) {
				std::cerr << "CPU has neither a program to run, nor accessible memory.\n";
				/*I don't really care what the next return value should be; if someone wished and counted all the possible
				paths the control may flow during argument parsing and memory inspection, may he change this return value to the exact value.*/
				return 18;
			}

			auto const[request_params, code] = helper::parsing::parse_parameters(argv[1], argv[2]);
			if (code)
				return code;

			helper::perform_print_function_resolution(request_params);
			return 0;
		}

		/*Function callback for the "register" cli command. Prints values of CPU's registers.*/
		int registers_callback(cli::command_parameters_t const& argv) {
			if (int const code = cli::check_command_argc(1, 2, argv.size()))
				return code;

			static auto const print_pc = [] {
				std::cout << std::setw(20) << std::left << "Program Counter:";
				if (int pc = execution::emulator.program_counter(), mem = execution::emulator.instructions_size(); pc == mem)
					std::cout << "Out of bounds. Execution finished.\n";
				else
					std::cout << pc << ", valid address space at 0 through " << mem << " exclusive\n";
			};

			static auto const print_cpr = [] {
				std::cout << std::setw(20) << std::left << "Cell Pointer:" << execution::emulator.cell_pointer_offset()
					<< ", valid address space at 0 through " << execution::emulator.memory_size() << " exclusive\n";
			};


			if (argv.size() == 1) { //no argument specified => print both of them
				print_pc();
				print_cpr();
				return 0;
			}

			//a register has been specified; identify it and print it's value
			if (argv[1].compare("pc") == 0)
				print_pc();
			else if (argv[1].compare("cpr") == 0)
				print_cpr();
			else {
				cli::print_command_error(cli::command_error::argument_not_recognized);
				return 4;
			}
			return 0;
		}

	}

	void initialize() {
		ASSERT_CALLED_ONCE;

		cli::add_command("mem", cli::command_category::debug, "Examines emulator's memory",
			"Usage: \"mem\" request address\n\n"

			"Parameter address denotes the address relative to which the examination shall be performed.\n"
			"Its value may be specified as an arithmetic expression using addition, subtraction and simple multiplication of integers\n"
			"as well as using one of the variables \"$cpr\" or \"$pc\", which are replaced by the current values of emulator's\n"
			"cell pointer register ($cpr) and the program counter register ($pc) respectively. It is also important to understand, that\n"
			"instructions and data reside in separate address spaces which do not overlap. It is therefore an error\n"
			"to request instructions from an address relative to the CPR or vice versa examine data from addresses relative to PC.\n\n"

			"Parameter request specifies how and how much data shall be inspected. To fulfill its destiny, this param specifies the following:\n"
			"\ta) the number of elements to be printed (given as a positive integer),\n"
			"\tb) the type of each element (size in bytes, signedness),\n"
			"\tc) format using which the numbers shall be printed,\n"
			"\td) whether the address parameter is used as staring or end point.\n"

			"The data type is specified using a single character from the set {'b', 'w', 'd', 'q', 'c', 'i'}\n"
			"The format is specified using a single character from the set {'x', 'o', 'u', 's'}\n"
			"Each character has unambiguous meaning and therefore their order is irrelevant with the exception of the integer specifying count,\n"
			"as it must appear as a contiguous string of digits.\n"
			"Specifying count is optional however - if no number is supplied, one is assumed.\n"
			"If inspection of characters is requested using 'c', no format is expected since 'c' specifies both type and format simultaneously.\n"
			"The same applies to inspecting instructions, as 'i' specifies both the format and type as well.\n"
			"If there is a minus sign, the direction of examination is inverted. In such case the effective address (found by resolving and calculating\n"
			"the value of the expression specified as the address parameter) does not specify a location at which examination starts, but ends. Count\n"
			"elements preceding this location are printed.\n\n"

			"If the requested memory area exceeds the bounds of memory, it is shrinked by an integer multiple of type's size in bytes, this operation\n"
			"is repeated for both ends of the area to prevent access violations.\n\n"


			"Keep in mind that executable instructions and memory for data reside in entirely different address spaces, it is therefore an error\n"
			"to access data using address relative to the PC or to access instructions using address relative to the CPR. Such mismatches are reported.\n\n"

			"Element types are as follows:\n"
			"\t 'b' => Byte (8 bits)\n"
			"\t 'w' => Word (16 bits)\n"
			"\t 'd' => DoubleWord (32 bits)\n"
			"\t 'q' => QuadWord (64 bits)\n"
			"\t 'c' => Character (8 bits)\n"
			"\t 'i' => Instruction\n\n"

			"Following formats of printing are supported:\n"
			"\t 'x' => Hexadecimal (base 16)\n"
			"\t 'o' => Octal (base 8)\n"
			"\t 'u' => Unsigned decimal (base 10)\n"
			"\t 's' => Signed decimal (base 10)\n\n"

			"Examples:\n"
			"\"mem 10xq 1+2+3+4+5\"    => print ten quadwords in hexadecimal starting at address 15.\n"
			"\"mem i $pc\"             => print a single instruction pointed to by program counter.\n"
			"\"mem u8d -12+$cpr\"      => print eight unsigned doublewords starting at the CPU's CPR minus twelve .\n"
			"\"mem c2 0\"              => print two characters from the beginning of the memory.\n"
			"\"mem sb-4 $cpr\"         => print four bytes preceding the cpu's cell pointer interpreting them as signed numbers.\n"
			"\"mem i14 $pc+9\"         => print fourteen instructions starting at offset nine relative to the program counter.\n"
			"\"mem -i 11\"             => print single instruction preceding the instruction at address 11 (i.e. print instruction at address 10)."

			, &mem_callback);
		cli::add_command_alias("x", "mem");
		cli::add_command_alias("memory", "mem");

		cli::add_command("registers", cli::command_category::debug, "Prints information about CPU's registers.",
			"Usage: \"registers\" [name]\n"
			"The optional parameter name may be specified to identify either the CPU's program counter or cell pointer\n"
			"using reserved strings \"pc\" or \"cpr\" respectivelly.\n"
			"Without any parameters displays information about all registers."
			, &registers_callback);
		cli::add_command_alias("reg", "registers");
	}
}