#pragma once

#include <memory>

#include "tjs.h"

class tTVPMemoryStream;

namespace PSB {

    std::unique_ptr<tTVPMemoryStream>
    openDecompressedStream(TJS::tTJSBinaryStream *input);

} // namespace PSB
