Project Brainfuck - Optimizing compiler and virtual machine for the Brainfuck programming language with a command line interface.  
© Vojtìch Michal 2019 

Bug reports and pull requests - vojta.michall@email.cz

This toolchain is fully compliant with the Brainfuck language's specification at https://esolangs.org/wiki/brainfuck as of end of March 2019.


1.Principles:	
	Main principles held by the project's design:
		1) Preparation for releasing as open source
		2) High code portability
		3) Modern maintainable code following with good principles and guidelines in mind
		4) Good and consistent coding style
		5) Avoiding code coupling
	

	High code portability:
		The project is being written with the huge number of options when it comes to target architecture and operation system. System specific features
		are therefore avoided as much as possible with the exception of two Windows specific calls in file src/emulator_cli.cpp, where within the body
		of int redirect_command_helper::print_iostreams_state() the "where" cmd.exe command is executed.
		Requirements on the size of architecture's data types are non-existent. Wherever possible the program operates on the native architecture's word size
		and if a fixed width in bits is required, it is explicitly stated by the usage of standard fixed integer data types from the header.
		Portability between compilers has also been preserved, as no MSVC specific features have been used while designing the program.
	
	Modern code 
		The project is written using a wide range of features of the C++ programming language with emphasis especially on additions by the C++14 and C++17 standard.
		Just to name a few, the most heavily used C++14 features are constexpr std::array, the <iomanip> standard header, features from C++17 include structured bindings, 
		std::optional, std::string_view, compile-time branching with if constexpr, init statement for if, attributes and so on.
		
	Good and consistent coding style
		First of all, the used naming convention is snake case, so words in identifies are separated by underscore. All identifiers are written using lowercase letters and digits. 
		Macros are written in all letters uppercase. The code base has no signs of Hungarian naming convention, class-type member variables however have their name followed 
		by a trailing underscore to differentiate them from ordinary variables with static or automatic storage duration. 
		The principle of const-correctness has been preserved as much as was possible. All non-mutating member functions have been marked const and all variables with automatic 
		storage duration have their type decorated by the const keyword provided they're not expected to change value. This way the compiler and static analysis can pinpoint
		possible errors as well as reason about the required semantics more efficiently.  
	
	Avoiding code coupling
		One of the most important principles, which dictates that global state shall be as scarce as possible and that different parts of the code base (i.e. different translation units) 
		are independent interfacing only via the system of include header files. With the exception of objects and constructs that are meant to exist in only one global instance
		such as the CPU emulator, breakpoint manager or details of earlier compilation, no global state is left without control. Thanks to the mechanism of accessing and mutating commands of the 
		Command Line Interface (CLI) via dedicated functions, adding new commands prove to be unimaginably easy, while asserting that the new code is not coupled with other sections of the program.
		Thanks to this approach it is easy to make use of the currently existent interface and expose it to a set of new possible functionalities while safely hiding the implementation details.

	Preparation for releasing as open source
		The entire code base is filled with exhaustive comments describing the purpose of nearly each function, types, objects, namespaces, lambda expressions, variables, expressions.
		CLI commands themselves are documented very well right inside the program and their full functionality can be understood by reading the output of the "help" command. Believe me, 
		the help message for the "mem" cli command consist of nearly 2.5k characters. More complex commands contain examples of their usage with a detailed explanation.

2. General description:
		The program has several key components: an emulator of CPU, compiler and command line interface, which are supported by additional facilities like the breakpoint manager, 
		optimization engine, data examination engine or syntax validation. The highest possible view of the program's usage is the typical compile-debug cycle of software development.
		Compilation and CPU execution use an intermediate language in order to increase the efficiency as well as maintainability of the program.

	Command Line Interface
		The project is using the CLI as the only means of interaction, it does not currently accept any parameters from the OS. When the program stars executing, an infinite loop is entered, which 
		reads input from the user and executes the requested command. Commands may be tweaked to please any needs the user may evolve. It is possible to alias old commands giving them 
		a synonymous name, as well as defining entirely new commands. An important mechanism is hooking. A hook is a normal command, which is additionally linked to some target command.
		When the target command gets executed, the hook is executed as well. This proves useful when printing emulator's registers and memory content every time the execution stops,
		which is an enormous help and means a speed up while debugging. 
		The CLI keeps an internal history of previously executed commands. This history can be queried by the user, who has the option of retrospective execution (executing a command from history).
		This component is controlled by the "commands" category of commands. 

	Compiler
		The process of compilation accepts source code in Brainfuck and emits instructions in the internal IL. These generated syntax trees can be subject to basic optimizations.
		If the compiled source code does not fulfil the requirements posed on valid Brainfuck programs (the program is ill-formed), diagnosis is printed. Using the "errors" cli command
		the user can later query all the available information about the previous compilation, such as error count, their coordinates or short code insight to learn what went sideways.
		This component is controlled by the "compilation" category of commands. 
		
	Emulator
		The central component of this program. A full featured microprocessor emulator with RISC instruction set (inspired by the ARM architecture). Uses the internal IL language as instruction set
		executing compiled syntax trees. Executed instructions manipulate the internal data-memory with fixed size (which is however specified as a compile-time constant, therefore the size can be altered
		at any time, which takes place after the source code base gets recompiled) and registers including the FLAGS register inspired by both x86 and ARM (a friend of mine accused me of a too strong
		bias towards ARM microprocessors, therefore some variety had to be included...). Redirection of emulator's standard IO streams is supported as well.
		This component is controlled by the "emulation" category of commands. 

	Debugger
		While the emulator is capable of running the compiled programs sequentially like a normal CPU would do, it is from time to time necessary to debug running programs. The debugger is tightly 
		bound to the emulator and exposes functionality, which can influence the way emulator executes the flashed program. These functionalities include setting breakpoints, single stepping through 
		the program while printing values of registers, values stored in memory or view upcoming instructions. Inspection of emulator's memory uses the most effective and complex code in the entire 
		program, therefore had to be extensively tested to prove the code's correctness. It is also the place, where most intense template metaprogramming had to be used.
		This component is controlled by the "debug" category of commands. 

3. Example usage:
		Please note, that all commands have comprehensive help built into the program. Don't be afraid to type "help" optionally followed by the name of some command you are interested in at any time. 
		Imagine the most general case. You have written a short Brainfuck program and want to compile it and execute.
		I doubt you have a Brainfuck program lying around you, so we can use this short example:
		Hopefully you have some experience with programming in Brainfuck, so just a few comments will suffice.

		++++++++++          //Set the first cell to 10
		[-					//loop 10 times setting the values of following cells in the process
		>+++++++
		>++++++++++
		>+++++++++++
		>+++++++++++
		>+++++++++++
		>+++
		>++++++++++++
		>+++++++++++
		>+++++++++++
		>+++++++++++
		>++++++++++
		>+++
		<<<<<<<<<<<<          //Return to the loop variable
		]
		>++					//set the cells 1 to 12 to the right values of ASCII chars "Hello world!"
		>+
		>--
		>--
		>+
		>++
		>-
		>+
		>++++
		>--
		>>+++
		[<]					//get to the beginning (first zero cell)
		>		
		[.>]				//and keep printing char by char until a zero is found

		As you may have recognized, the program prints string "Hello world!" to the standard output stream.
		You can copy and paste this program into a text file; the indentation is not necessary, as the BF language ignores all other characters. 
		Therefore the following program is equivalent.

			++++++++++[->+++++++>++++++++++>+++++++++++>+++++++++++>+++++++++++>+++>++++++++++++>+++++++++++>+++++++++++>+++++++++++>++++++++++>+++<<<<<<<<<<<<]>++>+>-->-->+>++>->+>++++>-->>+++[<]>[.>]

		First you should compile the code. To do so, use the "compile" command specifying "file" as the first parameter and typing the saved file's path as the second argument. 
		If everything runs smoothly, the compiler will notify you that 189 instructions have been compiled. We will cover compile errors later in this guide.

		After your program has been compiled, you can either flash it into the emulator's memory and prepare it for execution straight away, or optimize it first.
		I suggest the latter, optimizing by folding operations as it makes debugging much simpler. If you agree with me, run "optimize op_folding". You should see a list
		of optimizations as they are performed and the program's execution tree is restructured and jump labels redirected. This optimized code is much shorter and more expressive, 
		therefore debugging won't be so intimidating.

		Now flash the program into emulators memory using the "flash command". At this point the emulator will have been reset, memory zeroed out and new program loaded, whilst 
		the emulator will be awaiting the commencing of execution. To make sure that the program is correctly flashed and the emulator ready, run the "registers" command.
		You can see the values of PC and CP registers of the emulator. The format (n out of max) specifies that the register currently stores value n and that the corresponding
		address space is max addresses wide. In another words for PC it's the length of program, for CPR it's the size of memory in bytes. In my case, program contains 59 instructions.

		I would like to print them using the "mem" command. Please, read its help message, I don't want to repeat myself. Running "x 16i 0" (While using GDB on Linux I've fallen in love
		with the "x" command that I had to make my own version) prints me first 16 instructions of this program; the arrow in the left column shows me, which instruction is referenced
		by the program counter.

		Just to start we can run the emulator without any interruptions using the "run" command. We should see "Hello world!" printing! hooray! Our program works!
		We can, however, take a closer look at the execution. I will define a hook for the "stop" command, which prints some info about the emulator's state. Please, read the 
		description of command "define". I will define it simply as

			registers
			x 16bx 0
			x 6i $pc-1
			end

		I like to see what the previously executed instruction was, therefore i don't print 6 instructions starting at PC, but already one instruction before it. In case i would be
		too close to address space boundaries, the x command identifies such access and shrinks the requested memory region to prevent access violations.

		I could now run the "stop" command, but the only thing I would see would be the memory after the execution. Which is interesting as well, so just run it.

		The easiest way to start debugging is reading the help for commands of the "debug" command category. You can either set a breakpoint (using "break" or "tbreak") somewhere in code 
		and run the emulator, which breaks the execution when the breakpoint is hit. Other option is sending the SIGINT system signal (Ctrl-C from shell), which gets handled 
		and sets the OS interrupt flag inside the emulator's registers. The third option is the command "start", which creates a temporary breakpoint at the first instruction, which is
		then immediately hit and emulator is interrupted before the first instruction executes. You can then either use "continue" to resume execution until another breakpoint is 
		encountered, or use "step" to execute a single instruction at a time making it easy to trace possible programming errors.

		Breakpoints can be disabled using the "disable" command, which makes the emulator behave as if they weren't in the code at all. You can also give an ignore count to a breakpoint.
		If a breakpoint with non-zero ignore count is encountered, it's ignore count is decreased, but the execution goes on.

		As a side note, breakpoints created by the "tbreak" command are temporary and are erased immediately after they are hit. They are best for navigating the source code without 
		polluting the breakpoint list with temporary breakpoints.
