#pragma once
#include <string>
#include <cctype>

// ---------------------------------------------------------------------
// Minimal JSON helper for extracting numbers WITHOUT a full JSON parser
// ---------------------------------------------------------------------
namespace mini_json {

    // Extracts a floating-point number immediately after a given position.
    // Example:
    //   text = "... \"c\": 419.25 ..."
    //   extract_number_after(text, pos_of_c_plus_3)  ->  419.25
    //
    inline double extract_number_after(const std::string& s, size_t start)
    {
        // Move to first digit or minus sign
        while (start < s.size() &&
            !(std::isdigit(s[start]) || s[start] == '-' || s[start] == '+'))
        {
            start++;
        }

        if (start >= s.size())
            return 0.0;

        const char* begin = s.c_str() + start;
        char* end = nullptr;

        double val = std::strtod(begin, &end);
        return val;
    }

}

