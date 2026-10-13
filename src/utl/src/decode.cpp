/*
   base64.cpp and base64.h

   Copyright (C) 2004-2008 René Nyffenegger

   This source code is provided 'as-is', without any express or implied
   warranty. In no event will the author be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this source code must not be misrepresented; you must not
   claim that you wrote the original source code. If you use this source code
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original source code.

   3. This notice may not be removed or altered from any source distribution.

   René Nyffenegger rene.nyffenegger@adp-gmbh.ch

*/

#include "utl/decode.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "tcl.h"

namespace utl {

// Index of each alphabet character. -1 is not in the alphabet. string::find
// walked the 64-character alphabet once per input byte; the Steiner tables
// are millions of those bytes.
const int8_t* base64Value()
{
  static int8_t table[256];
  static const bool ready = [] {
    for (int i = 0; i < 256; i++) {
      table[i] = -1;
    }
    const char alphabet[]
        = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; alphabet[i] != '\0'; i++) {
      table[static_cast<unsigned char>(alphabet[i])] = static_cast<int8_t>(i);
    }
    return true;
  }();
  (void) ready;
  return table;
}

std::string base64_decode(const std::string& encoded_string)
{
  const int8_t* const value = base64Value();
  const int in_len = static_cast<int>(encoded_string.size());
  int i = 0;
  int in_ = 0;
  unsigned char char_array_4[4];
  char char_array_3[3];
  std::string ret;
  ret.reserve(encoded_string.size() / 4 * 3);

  while (in_len > in_ && encoded_string[in_] != '='
         && value[static_cast<unsigned char>(encoded_string[in_])] >= 0) {
    char_array_4[i++] = static_cast<unsigned char>(encoded_string[in_]);
    in_++;
    if (i == 4) {
      for (i = 0; i < 4; i++) {
        char_array_4[i] = static_cast<unsigned char>(value[char_array_4[i]]);
      }

      char_array_3[0]
          = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1]
          = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

      ret.append(char_array_3, 3);
      i = 0;
    }
  }

  if (i) {
    // A short final group used to look up a 0 byte, which is not in the
    // alphabet, so find() returned npos and the shift saw -1.
    int vals[4];
    for (int j = 0; j < i; j++) {
      vals[j] = value[char_array_4[j]];
    }
    for (int j = i; j < 4; j++) {
      vals[j] = -1;
    }

    char_array_3[0] = (vals[0] << 2) + ((vals[1] & 0x30) >> 4);
    char_array_3[1] = ((vals[1] & 0xf) << 4) + ((vals[2] & 0x3c) >> 2);
    char_array_3[2] = ((vals[2] & 0x3) << 6) + vals[3];

    ret.append(char_array_3, i - 1);
  }

  return ret;
}

// From here down:
// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025-2025, The OpenROAD Authors

std::string base64_decode(const char* encoded_strings[])
{
  auto reassemble = [](const char* chunks[]) {
    std::string reassembled;
    for (const char** chunk = chunks; *chunk; ++chunk) {
      reassembled += *chunk;
    }
    return reassembled;
  };
  return base64_decode(reassemble(encoded_strings));
}

void evalTclInit(Tcl_Interp* interp, const char* inits[])
{
  std::string unencoded = base64_decode(inits);
  Tcl_Obj* obj = Tcl_NewStringObj(unencoded.c_str(), unencoded.size());
  Tcl_IncrRefCount(obj);
  if (Tcl_EvalObjEx(interp, obj, 0) != TCL_OK) {
    Tcl_Eval(interp, "$errorInfo");
    const char* tcl_err = Tcl_GetStringResult(interp);
    fprintf(stderr, "Error: TCL init script: %s.\n", tcl_err);
    exit(1);
  }
  Tcl_DecrRefCount(obj);
}

}  // namespace utl
