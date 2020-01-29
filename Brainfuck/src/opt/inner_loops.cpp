#include "opt/inner_loops.h"
#include "anal/analysis.h"

#include <algorithm>
#include <numeric>
#include <execution>

namespace bf::opt {

	namespace {

		class inner_loop {
			basic_block* const condition_;
			basic_block* const body_;


		public:
			static std::vector<basic_block*> find_inner_loops(std::vector<basic_block*> const& program) {
				std::vector<basic_block*> res;
				res.reserve(std::count_if(std::execution::seq, program.begin(), program.end(), std::mem_fn(&basic_block::is_inner_loop)));
				std::copy_if(program.begin(), program.end(), std::back_inserter(res), std::mem_fn(&basic_block::is_inner_loop));
				return res;
			}

			inner_loop(basic_block* const cond)
				:condition_{ cond }, body_{ cond->jump_successor_ }
			{}

			[[nodiscard]]
			bool is_ok() const {
				return condition_ && condition_->is_pure_cjump() && body_
					&& !body_->is_jump() && body_->has_successor(condition_);
			}

			basic_block* cond() const { return condition_; }
			basic_block* body() const { return body_; }

		};

		bool is_inner_loop(basic_block const* condition) {
			basic_block* body = condition->jump_successor_;
			return condition && condition->is_pure_cjump() && body && !body->is_jump() && body->has_successor(condition);
		}



		std::ptrdiff_t eliminate_self_loop(basic_block* const condition) {
			assert(condition && condition->is_pure_cjump() && condition->has_self_loop());
			assert(condition->natural_successor_ != condition); //TODO this is only for testing purposes to see, whether it can happen
			assert(condition->jump_successor_ == condition);

			condition->remove_predecessor(condition);
			condition->jump_successor_ = nullptr;
			condition->ops_.front().make_infinite_on_not_zero();
			return 1;
		}


		std::ptrdiff_t eliminate_const_eval_body(basic_block* const condition) {
			inner_loop const loop{ condition };
			if (!loop.is_ok())
				return 0;

			//handful of sanity checks
			assert(loop.body()->jump_successor_ == nullptr); //
			assert(loop.body()->natural_successor_ == condition); //

			if (analysis::pointer_movement{ loop.body() }.ptr_moves())
				return 0;

			analysis::block_evaluation const body_eval{ loop.body() };
			if (body_eval.has_visible_sideeffects() || !body_eval.has_const_result() || body_eval.const_result() == 0)
				return 0;
			condition->ops_.front().make_infinite_on_not_zero();
			condition->jump_successor_ = nullptr;
			loop.body()->remove_predecessor(condition);
			return 1;
		}

	}

	std::ptrdiff_t infinite_loop_optimizer::do_optimize(basic_block* const block) {
		if (!block || !block->is_inner_loop())
			return 0;

		return block->has_self_loop() ? eliminate_self_loop(block) : eliminate_const_eval_body(block);
	}


	std::ptrdiff_t clear_loop_optimizer::do_optimize(basic_block* const condition) {
		inner_loop const loop{ condition };
		if (!loop.is_ok())
			return 0;

		//handful of sanity checks
		assert(loop.body()->jump_successor_ == nullptr); //
		assert(loop.body()->natural_successor_ == condition); //


		if (analysis::pointer_movement{ loop.body() }.ptr_moves())
			return 0;

		analysis::block_evaluation const body_eval{ loop.body() };
		if (body_eval.has_visible_sideeffects())
			return 0;

		if ((body_eval.has_const_result() && body_eval.const_result() == 0) || body_eval.value_delta()) {

			condition->ops_.front() = instruction{ op_code::load_const, 0, loop.body()->ops_.front().source_loc_ };
			condition->jump_successor_ = nullptr;
			loop.body()->remove_predecessor(condition);
			return 1;
		}
		return 0;
	}


	std::ptrdiff_t search_loop_optimizer::do_optimize(basic_block* const condition) {
		inner_loop const loop{ condition };
		if (!loop.is_ok())
			return 0;

		analysis::pointer_movement const movement{ loop.body() };

		if (movement.only_moves_ptr() && movement.ptr_delta() != 0) {
			loop.cond()->jump_successor_ = nullptr;
			loop.cond()->ops_.front().make_search(movement.ptr_delta());
			loop.body()->remove_predecessor(condition);
			return 1;
		}

		return 0;
	}




}