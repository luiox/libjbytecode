/// @file        jbytecode.hpp
/// @author      canrad (1517807724@qq.com)
/// @brief       libjbytecode 统一入口头文件
/// @version     0.1
/// @date        2026-05-12
/// @copyright   Copyright (c) 2026
///
/// 包含 libjbytecode 全部公共 API：
///   - core/      底层 Core API（ClassReader/Writer、Opcodes、Type、Handle 等）
///   - tree/      Tree API（ClassNode、MethodNode、InsnList 等）
///   - commons/   工具适配器（Remapper、Adapter 体系）
///   - analysis/  字节码分析（Frame、Analyzer、Interpreter）
///
/// 可按需单独包含子模块头文件以减少编译时间。
#pragma once

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

// analysis
#include "jbytecode/analysis/basic_interpreter.hpp"
#include "jbytecode/analysis/basic_verifier.hpp"
#include "jbytecode/analysis/simple_verifier.hpp"
#include "jbytecode/analysis/source_interpreter.hpp"
#include "jbytecode/analysis/interpreter.hpp"
#include "jbytecode/analysis/analyzer.hpp"

// util 包 — 调试/追踪/验证工具
#include "jbytecode/util/printer.hpp"
#include "jbytecode/util/textifier.hpp"
#include "jbytecode/util/codegen.hpp"
#include "jbytecode/util/asmifier_support.hpp"
#include "jbytecode/util/textifier_support.hpp"
#include "jbytecode/util/trace_class_visitor.hpp"
#include "jbytecode/util/check_adapters.hpp"
#include "jbytecode/util/trace_signature_visitor.hpp"

namespace jbytecode {

const char* get_lib_version();
}
