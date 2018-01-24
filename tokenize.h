// tokenize.h
// Author: Nat Tuck

#ifndef TOKENIZE_H
#define TOKENIZE_H

#include "svec.h"

// This returns a newly allocated svec.
// Caller is responsible for freeing it with free_svec(svec*)
svec* tokenize(char* text);

#endif
