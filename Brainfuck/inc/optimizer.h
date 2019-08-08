
#ifndef OPTIMIZER_H
#define OPTIMIZER_H
#pragma once

#include "program_code.h"

#include <optional>
#include <unordered_set>

namespace bf::optimizations {

	enum class opt_level_t {
		op_folding = 1 << 0,
		dead_code_elimination = 1 << 1,
		const_propagation = 1 << 2,
		loop_analysis = 1 << 3,

		all = ~static_cast<std::underlying_type_t<opt_level_t>>(0)
	};

	/*Get optimization bitmask from its name. If optimization_name is not recognized, returns value none.*/
	[[nodiscard]]
	std::optional<opt_level_t> get_opt_by_name(std::string_view const optimization_name);



	/*Perform optimizations on given program.
	Syntax tree "program" undergoes optimizations specified by the bitfield "optimizations"
	and new syntax_tree is constructed.*/
	void perform_optimizations(std::vector<std::unique_ptr<basic_block>> & program_basic_blocks, std::unordered_set<opt_level_t> const& optimizations);

	/*Initialize CLI commands that control the function of optimizer.
	Shall be called only once.*/
	void initialize();

}  //namespace bf::optimizationS

#endif