#pragma once
#include "basic_block.h"
#include "instruction.h"
#include "utils.h"


#include <string_view>
#include <vector>
#include <cassert>
#include <optional>
#include <string>
#include <set>
#include <memory>
#include <ostream>

namespace bf {



	class program_code {

		std::set<std::unique_ptr<basic_block>> block_owners_;
		std::set<basic_block*> mutable_pointers_;

	public:
		void assert_invariants() const;

		/*Searches for orphaned basic blocks and removes them from the given std::vector.
		Orphaned blocks are those that have lost all connections to other basic blocks, i.e. they neither any predecessor,
		nor any successor. Many optimization routines use orphaning as a sign that the block can be safely deleted.*/
		std::ptrdiff_t erase_orphaned_blocks();





	};


}

