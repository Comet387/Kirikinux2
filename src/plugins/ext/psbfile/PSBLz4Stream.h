#pragma once

#include <memory>

#include "tjs.h"

class tTVPMemoryStream;

namespace PSB {

    std::unique_ptr<tTVPMemoryStream>
    decompressLz4Stream(TJS::tTJSBinaryStream *input);

} // namespace PSB
