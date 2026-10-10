#pragma once

#include "libca/core/any.hpp"
#include <string>
#include <vector>

// jbytecode的core、tree、analysis头文件
// core
#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/constants.hpp"
#include "jbytecode/core/type.hpp"
#include "jbytecode/core/primitives.hpp"
#include "jbytecode/core/byte_vector.hpp"
#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/writers.hpp"
#include "jbytecode/core/visitors.hpp"

// tree
#include "jbytecode/tree/nodes.hpp"
#include "jbytecode/tree/insn_list.hpp"
#include "jbytecode/tree/insn_type_traits.hpp"

// analysis
#include "jbytecode/analysis/basic_interpreter.hpp"
#include "jbytecode/analysis/basic_verifier.hpp"
#include "jbytecode/analysis/simple_verifier.hpp"
#include "jbytecode/analysis/source_interpreter.hpp"
#include "jbytecode/analysis/interpreter.hpp"
#include "jbytecode/analysis/analyzer.hpp"

// libca字符串
#include "libca/str/utf8_string.hpp"

using namespace ca::str;
using namespace jbytecode::core;
using namespace jbytecode::tree;
using namespace jbytecode::analysis;
