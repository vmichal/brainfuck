#pragma once

#include "program_code.h"

#include <vector>
#include <execution>
#include <numeric>

namespace bf::opt {

	enum class opt_level_t {
		all = ~static_cast<std::underlying_type_t<opt_level_t>>(0),
	};

	/*Get optimization bitmask from its name.*/
	[[nodiscard]]
	std::optional<opt_level_t> get_opt_by_name(std::string_view optimization_name);

	void perform_optimizations(std::vector<std::unique_ptr<basic_block>>& program, std::set<opt_level_t> const& optimizations);

	struct global_optimizer_pass {
		virtual std::ptrdiff_t optimize(std::vector<basic_block*>&) = 0;
	};

	struct peephole_optimizer_pass : public global_optimizer_pass {

		virtual std::ptrdiff_t optimize(basic_block*) = 0;
		virtual std::ptrdiff_t optimize(std::vector<basic_block*>&) = 0;
	};

	/*Initialize CLI commands that control the function of optimizer.
	Shall be called only once.*/
	void initialize();

}

#define DEFINE_GLOBAL_OPTIMIZER_PASS(name)																							\
																																	\
class name : public global_optimizer_pass {																							\
																																	\
	static std::ptrdiff_t do_optimize(std::vector<basic_block*>& program);															\
																																	\
	std::ptrdiff_t optimize(std::vector<basic_block*>& program) override {															\
		return do_optimize(program);																								\
	}																																\
};																																	

#define DEFINE_PEEPHOLE_OPTIMIZER_PASS(name)																						\
																																	\
class name : public peephole_optimizer_pass {																						\
																																	\
	static std::ptrdiff_t do_optimize(basic_block*);																				\
																																	\
	static std::ptrdiff_t do_optimize(std::vector<basic_block*>& program) {															\
		return std::transform_reduce(std::execution::seq, program.begin(), program.end(), std::ptrdiff_t{ 0 },						\
			std::plus{}, static_cast<std::ptrdiff_t(*)(basic_block*)>(do_optimize));												\
	}																																\
																																	\
	std::ptrdiff_t optimize(basic_block* const block) override {																	\
		return do_optimize(block);																									\
	}																																\
	std::ptrdiff_t optimize(std::vector<basic_block*>& program) override {															\
		return do_optimize(program);																								\
	}																																\
};