#pragma once

#define CAT_(A, B) A##B
#define CAT(A, B) CAT_(A, B)  // ← forces expansion of arguments first
