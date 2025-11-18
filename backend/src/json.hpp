#pragma once
#include <string>

namespace mini_json {

    inline double extract_number_after(const std::string& text, size_t from){
        size_t start = text.find_first_of("0123456789-.", from);
        if(start == std::string::npos) return 0.0;
        size_t end = text.find_first_not_of("0123456789-.", start);
        return std::stod(text.substr(start, end - start));
    }

}
