#pragma once

#include "syntax_tree.h"
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace bf {

	/*Structure containing information about the result of a compilation; Mainly vector of syntax errors
	and optionally syntax_tree provided compilation was successful are present.*/
	struct compilation_result {
		std::string code_; //code that has been compiled
		std::vector<syntax_error> errors_; //vector of encountered syntax_errors
		std::optional<syntax_tree> syntax_tree_; //compiled code. Holds value iff errors_ is empty

		compilation_result(std::string code, std::vector<syntax_error> errors, std::optional<syntax_tree> syntax_tree)
			: code_(std::move(code)), errors_(std::move(errors)), syntax_tree_(std::move(syntax_tree)) {}
	};

	/*Namespace wrapping functions observing the state of lastly performed compilation. Allows to query the validity of
	this result (i.e. if some compilation had been run), encountered syntax errors, source code as well as compiled code.*/
	namespace last_compilation {

		//Returns reference to the source code of previous compilation
		std::string &code();

		//Returns vector of syntax errors encountered during compilation
		std::vector<syntax_error> & errors();

		//Returns the compiled code inside std::optional. Does not throw regardless of compilation result state
		std::optional<::bf::syntax_tree> & syntax_tree_opt();

		//Returns the compiled code from last compilation. If it does not exist, throws
		bf::syntax_tree & syntax_tree();

		//Returns true iff the compilation had been completed successfully. If an error had been found, returns false
		bool successful();

		//Returns true iff the result of last compilation is in valid state and may be queried.
		bool ready();
	};



	/*Parses the given source code and attempts to construct a syntax_tree. If a syntax error is encountered,
	returns empty std::optional. If the code is ok, converts all instructions forming a syntax tree without any optimizations.*/
	std::optional<syntax_tree> generate_syntax_tree(std::string_view const code);


	/*Initialization function of cli commands controling compilation. Shall be called only once from main.*/
	void compiler_initialize();

}