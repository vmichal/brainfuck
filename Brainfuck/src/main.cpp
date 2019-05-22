#include <iostream>

#include "emulator.h"
#include "cli.h"
#include "compiler.h"
#include "optimizer.h"
#include "breakpoint.h"
#include "data_inspection.h"


namespace bf {

	void initialize_commands(){
		cli::initialize();
		execution::initialize();
		compiler_initialize();
		breakpoints::initialize();
		data_inspection::initialize();
		optimizer_initialize();
	}
}


int main(int argc, char **argv) {
	std::ios::sync_with_stdio(false);

	bf::initialize_commands();

	bf::cli::cli_command_loop();
}
