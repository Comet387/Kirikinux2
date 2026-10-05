#include "ImageMetadata.h"

#include <cctype>
#include <cstdlib>

namespace PSB {

    bool tryParseTexIndex(const std::string &texName, std::uint32_t &index) {
        size_t texIdx = texName.rfind("tex");
        if(texIdx == std::string::npos)
            return false;

        std::string remaining = texName.substr(texIdx);

        // first run of decimal digits after "tex" (was boost::regex "\\d+")
        size_t b = 0;
        while(b < remaining.size() &&
              !std::isdigit(static_cast<unsigned char>(remaining[b])))
            ++b;
        if(b == remaining.size())
            return false;
        size_t e = b;
        while(e < remaining.size() &&
              std::isdigit(static_cast<unsigned char>(remaining[e])))
            ++e;
        if(e - b > 9)
            return false;
        index = static_cast<std::uint32_t>(
            std::strtoul(remaining.substr(b, e - b).c_str(), nullptr, 10));
        return true;
    }

    std::optional<std::uint32_t>
    ImageMetadata::getTextureIndex(const std::string &texName) {
        auto ends_with = [](const std::string &s, const char *suf) {
            const std::string t(suf);
            return s.size() >= t.size() &&
                s.compare(s.size() - t.size(), t.size(), t) == 0;
        };
        if(ends_with(texName, "tex") || ends_with(texName, "tex#000") ||
           ends_with(texName, "tex000")) {
            return 0;
        }

        std::uint32_t index;
        if(!tryParseTexIndex(texName, index)) {
            return {};
        }

        return index;
    }
} // namespace PSB