#pragma once

#include "program_code.h"
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace bf {

	/*Namespace wrapping functions observing the state of lastly performed compilation. Allows to query the validity of
	this result (i.e. if some compilation had been run), encountered syntax errors, source code as well as compiled code.*/
	namespace prev_compilation {

		//Returns reference to the source code of previous compilation
		std::string& source_code();

		//Returns vector of syntax errors encountered during compilation
		std::vector<syntax_error>& syntax_errors();

		//Returns the compiled code from last compilation. If it does not exist, throws
		std::vector<instruction> generate_executable_code();

		//Returns a vector of all basic blocks making up this program
		std::vector<std::shared_ptr<basic_block>> const& basic_blocks();
		std::vector<std::shared_ptr<basic_block>>& basic_blocks_mutable();

		//Returns true iff the compilation had been completed successfully. If an error had been found, returns false
		bool successful();

		//Returns true iff the result of last compilation is in valid state and may be queried.
		bool ready();
	};



	/*Initialization function of cli commands controling compilation. Shall be called only once from main.*/
	void compiler_initialize();

}