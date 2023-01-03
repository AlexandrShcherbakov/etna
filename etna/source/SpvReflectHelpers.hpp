#pragma once

#include <array>
#include <spirv_reflect.h>
#include <Assert.h>


namespace etna
{

// Might break with updates of spir-v reflect, but unlikely
constexpr std::array SPV_REFLECT_ERROR_CODE_STRINGS
{
    "SUCCESS",
    "NOT_READY",
    "ERROR_PARSE_FAILED",
    "ERROR_ALLOC_FAILED",
    "ERROR_RANGE_EXCEEDED",
    "ERROR_NULL_POINTER",
    "ERROR_INTERNAL_ERROR",
    "ERROR_COUNT_MISMATCH",
    "ERROR_ELEMENT_NOT_FOUND",
    "ERROR_SPIRV_INVALID_CODE_SIZE",
    "ERROR_SPIRV_INVALID_MAGIC_NUMBER",
    "ERROR_SPIRV_UNEXPECTED_EOF",
    "ERROR_SPIRV_INVALID_ID_REFERENCE",
    "ERROR_SPIRV_SET_NUMBER_OVERFLOW",
    "ERROR_SPIRV_INVALID_STORAGE_CLASS",
    "ERROR_SPIRV_RECURSION",
    "ERROR_SPIRV_INVALID_INSTRUCTION",
    "ERROR_SPIRV_UNEXPECTED_BLOCK_DATA",
    "ERROR_SPIRV_INVALID_BLOCK_MEMBER_REFERENCE",
    "ERROR_SPIRV_INVALID_ENTRY_POINT",
    "ERROR_SPIRV_INVALID_EXECUTION_MODE",
};

#define SPV_REFLECT_SAFE_CALL(call, file)\
    do \
    { \
    auto errc = call; \
    ETNA_ASSERTF(errc == SPV_REFLECT_RESULT_SUCCESS, \
        "SPIR-V Reflect call {} failed with code {} for file {}!", \
        #call, SPV_REFLECT_ERROR_CODE_STRINGS[errc], file); \
    } \
    while (false)

static_assert(SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_EXECUTION_MODE == SPV_REFLECT_ERROR_CODE_STRINGS.size() - 1);

}
