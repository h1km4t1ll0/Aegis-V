/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

#if defined(__GNUC__) && !defined(__clang__)

   #define COMPILER_NAME            "gcc"
   #define COMPILER_MAJOR           __GNUC__
   #define COMPILER_MINOR           __GNUC_MINOR__
   #define COMPILER_PATCHLEVEL      __GNUC_PATCHLEVEL__

#elif defined(__clang__)

   #define COMPILER_NAME            "clang"
   #define COMPILER_MAJOR           __clang_major__
   #define COMPILER_MINOR           __clang_minor__
   #define COMPILER_PATCHLEVEL      __clang_patchlevel__

#elif defined(__TINYC__)

   #define COMPILER_NAME            "tcc"
   #define COMPILER_MAJOR           0
   #define COMPILER_MINOR           9
   #define COMPILER_PATCHLEVEL      28

#else

   #error Compiler not supported

#endif
