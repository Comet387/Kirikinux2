#include <cassert>
#include <cstdint>
#include <memory>
#include "PSBLz4Stream.h"

#include "UtilStreams.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace PSB {
    namespace {

        struct Lz4FrameInfo {
            int BlockSize = 0;
            bool IndependentBlocks = false;
            bool HasBlockChecksum = false;
            bool HasContentLength = false;
            bool HasContentChecksum = false;
            bool HasDictionary = false;
            long OriginalLength = 0;
            int DictionaryId = 0;

            explicit Lz4FrameInfo(TJS::tTJSBinaryStream *input) {
                input->SetPosition(4);
                const tjs_uint8 flags = input->ReadI8LE();
                const int version = flags >> 6;
                if(version != 1) {
                    throw std::runtime_error("Invalid LZ4 compressed stream.");
                }
                IndependentBlocks = 0 != (flags & 0x20);
                HasBlockChecksum = 0 != (flags & 0x10);
                HasContentLength = 0 != (flags & 8);
                HasContentChecksum = 0 != (flags & 4);
                HasDictionary = 0 != (flags & 1);

                const tjs_int code = input->ReadI8LE();
                switch((code >> 4) & 7) {
                    case 4:
                        BlockSize = 0x10000;
                        break;
                    case 5:
                        BlockSize = 0x40000;
                        break;
                    case 6:
                        BlockSize = 0x100000;
                        break;
                    case 7:
                        BlockSize = 0x400000;
                        break;
                    default:
                        throw std::runtime_error(
                            "Invalid LZ4 compressed stream.");
                }

                if(HasContentLength) {
                    tjs_uint64 length =
                        static_cast<tjs_uint64>(input->ReadI32LE());
                    const tjs_uint32 lengthex = input->ReadI32LE();
                    length |= static_cast<tjs_uint64>(lengthex) << 32;
                    OriginalLength = static_cast<long>(length);
                }
                if(HasDictionary) {
                    DictionaryId = input->ReadI32LE();
                }
                input->ReadI8LE();
            }
        };

        constexpr int kMinMatch = 4;
        constexpr int kLastLiterals = 5;
        constexpr int kMFLimit = 12;
        constexpr int kMatchLengthBits = 4;
        constexpr int kMatchLengthMask = 0xF;
        constexpr int kRunMask = 0xF;

        void copyOverlapped(tjs_uint8 *data, int src, int dst, int count) {
            if(count <= 0) {
                return;
            }
            if(dst > src) {
                while(count > 0) {
                    const int chunk = std::min(dst - src, count);
                    std::memcpy(data + dst, data + src, chunk);
                    dst += chunk;
                    count -= chunk;
                }
            } else {
                std::memcpy(data + dst, data + src, count);
            }
        }

        int decompressBlock(tjs_uint8 *block, tjs_int32 blockSize,
                            tjs_uint8 *output, tjs_int32 outputSize) {
            int src = 0;
            const int iend = blockSize;
            int dst = 0;
            const int oend = outputSize;

            for(;;) {
                const int token = block[src++];
                int length = token >> kMatchLengthBits;
                if(kRunMask == length) {
                    int n = 0;
                    do {
                        n = block[src++];
                        length += n;
                    } while((src < iend - kRunMask) && (0xFF == n));
                    if(dst + length < dst || src + length < src) {
                        throw std::runtime_error(
                            "Invalid LZ4 compressed stream.");
                    }
                }

                const int copyEnd = dst + length;
                if((copyEnd > oend - kMFLimit) ||
                   (src + length > iend - (3 + kLastLiterals))) {
                    if((src + length != iend) || copyEnd > oend) {
                        throw std::runtime_error(
                            "Invalid LZ4 compressed stream.");
                    }
                    std::memcpy(output + dst, block + src, length);
                    src += length;
                    dst += length;
                    break;
                }
                std::memcpy(output + dst, block + src, length);
                src += length;
                dst = copyEnd;

                std::uint16_t recOffset = 0;
                std::memcpy(&recOffset, block + src, 2);
                const int offset = recOffset;
                src += 2;
                const int match = dst - offset;
                if(match < 0) {
                    throw std::runtime_error("Invalid LZ4 compressed stream.");
                }

                length = token & kMatchLengthMask;
                if(kMatchLengthMask == length) {
                    int n = 0;
                    do {
                        n = block[src++];
                        if(src > iend - kLastLiterals) {
                            throw std::runtime_error(
                                "Invalid LZ4 compressed stream.");
                        }
                        length += n;
                    } while(0xFF == n);
                    if(dst + length < dst) {
                        throw std::runtime_error(
                            "Invalid LZ4 compressed stream.");
                    }
                }
                length += kMinMatch;

                copyOverlapped(output, match, dst, length);
                dst += length;
            }
            return dst;
        }

    } // namespace

    std::unique_ptr<tTVPMemoryStream>
    decompressLz4Stream(TJS::tTJSBinaryStream *orgStream) {
        Lz4FrameInfo info(orgStream);
        auto stream =
            std::make_unique<tTVPMemoryStream>(nullptr, info.OriginalLength);

        bool eof = false;
        tjs_int dataBuffSize = info.BlockSize;
        std::vector<tjs_uint8> dataBuff(dataBuffSize, 0);
        tjs_int dataSize = 0;
        tjs_int blockBuffSize = 0;
        std::vector<tjs_uint8> blockBuff;

        while(!eof) {
            tjs_int32 blockSize = 0;
            if(4 != orgStream->Read(&blockSize, 4)) {
                throw std::runtime_error("EndOfStreamException");
            }
            if(0 == blockSize) {
                eof = true;
                dataSize = 0;
                if(info.HasContentChecksum) {
                    orgStream->ReadI32LE();
                }
            } else if(blockSize < 0) {
                dataSize = blockSize & 0x7FFFFFFF;
                if(dataSize > dataBuffSize) {
                    dataBuffSize = dataSize;
                    dataBuff.assign(dataBuffSize, 0);
                }
                dataSize = orgStream->Read(dataBuff.data(), dataSize);
                if(info.HasBlockChecksum) {
                    orgStream->ReadI32LE();
                }
            } else {
                const tjs_int32 currentBlockSize = blockSize;
                if(currentBlockSize > blockBuffSize) {
                    blockBuffSize = currentBlockSize;
                    blockBuff.assign(blockBuffSize, 0);
                }
                if(currentBlockSize !=
                   orgStream->Read(blockBuff.data(), currentBlockSize)) {
                    throw std::runtime_error("EndOfStreamException");
                }
                dataSize = decompressBlock(blockBuff.data(), currentBlockSize,
                                           dataBuff.data(), dataBuffSize);
                if(info.HasBlockChecksum) {
                    orgStream->ReadI32LE();
                }
            }

            stream->Write(dataBuff.data(), dataSize);
        }

        stream->SetPosition(0);
        return stream;
    }

} // namespace PSB
