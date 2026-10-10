#pragma once

/// commons 子库测试的轻量级公共头。
///
/// commons 的测试历史上 #include "test_utils.hpp" 主要是为了拿到
/// `using namespace ca::str;`（从而可直接写 ZUtf8StringRef），它们并不依赖
/// core/test/test_utils.hpp 里的 class 文件解析辅助函数。这里只提供这部分
/// 公共声明，避免把 core 的重型测试工具拷进每个子库。

#include "libca/core/any.hpp"
#include <string>
#include <vector>

// jbytecode的core、commons、tree头文件
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

// commons
#include "jbytecode/commons/remapper.hpp"
#include "jbytecode/commons/instruction_adapter.hpp"
#include "jbytecode/commons/generator_adapter.hpp"
#include "jbytecode/commons/advice_adapter.hpp"
#include "jbytecode/commons/analyzer_adapter.hpp"
#include "jbytecode/commons/local_variables_sorter.hpp"
#include "jbytecode/commons/code_size_evaluator.hpp"
#include "jbytecode/commons/remapper_visitors.hpp"
#include "jbytecode/commons/serial_version_uid_adder.hpp"
#include "jbytecode/commons/static_init_merger.hpp"
#include "jbytecode/commons/try_catch_block_sorter.hpp"
#include "jbytecode/commons/jsr_inliner_adapter.hpp"
#include "jbytecode/commons/signature_remapper.hpp"
#include "jbytecode/commons/method.hpp"

// libca字符串
#include "libca/str/utf8_string.hpp"

using namespace ca::str;
using namespace jbytecode::core;
using namespace jbytecode::commons;
using namespace jbytecode::tree;