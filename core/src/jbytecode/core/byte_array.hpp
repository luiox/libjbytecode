/// @file        byte_array.hpp
/// @brief       拥有所有权的原始字节缓冲区描述符（ByteVector 的底层存储）。
///
/// 仅是「指针 + 长度」的薄封装，不含任何方法——容量管理、扩容、读写均由持有它的
/// ByteVector 负责。放在 ca::core 命名空间下，作为 core 模块二进制写出的基础类型。
#pragma once

#include "libca/core/datatype.hpp"

namespace ca::core {

/// @brief 「指针 + 长度」字节缓冲区。所有权语义由持有者（如 ByteVector）约定，本结构不负责释放。
struct ByteArray
{
    ca::u8*   data_;     ///< 缓冲区首地址。
    ca::usize length_;   ///< 当前有效字节数。
};

}   // namespace ca::core
