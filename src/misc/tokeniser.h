#ifndef MC_DEV_TOKENISER_H
#define MC_DEV_TOKENISER_H

#include <vector>
#include <string>


//
// Given an input string and a set of single-character delimitors,
// scan through the string and split it into individual tokens broken by
// each delimitor (delimitors are removed from the string)
//
// e.g " < biscuits are delicious! Especially with chocolate > " using " <!>" 
//     -> ["biscuits", "are", "delicious", "Especially", "with", "chocolate"]
//
std::vector<std::string> SplitLine(const std::string& input, const std::string& delimitors);

#endif
