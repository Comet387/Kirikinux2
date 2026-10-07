//
// Created by LiDon on 2025/9/11.
//

#include "../kr2_plugin_log.h"

#include "PSBMedia.h"

#include "UtilStreams.h"
#include "MsgIntf.h"

namespace PSB {
#define LOGGER spdlog::get("plugin")

    namespace {
        // Kirikinux2: psdlayer.tjs builds names as
        //   "psb://" + Storages.extractStorageName(file).toLowerCase() + "/" + id + ".tlg"
        // while the container is registered from whatever string was passed to
        // PSBFile.load().  Keys are compared case-insensitively (ASCII).
        std::string lowerKey(std::string s) {
            for(auto &c : s)
                if(c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c - 'A' + 'a');
            return s;
        }
    } // namespace

    const PSBResource *PSBMedia::find(const ttstr &name) const {
        const std::string key = lowerKey(name.AsStdString());
        auto it = _resources.find(key);
        if(it != _resources.end())
            return &it->second;
        // Some PIMG files name the layer resource "<id>" instead of
        // "<id>.tlg"; psdlayer.tjs always asks for "<id>.tlg".
        const auto slash = key.rfind('/');
        const auto dot = key.rfind('.');
        if(dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
            it = _resources.find(key.substr(0, dot));
            if(it != _resources.end())
                return &it->second;
        }
        return nullptr;
    }

    void PSBMedia::NormalizeDomainName(ttstr &name) {
        tjs_int dotIndex = name.IndexOf(TJS_W('.'));
        if(dotIndex == -1)
            return;
        name = name.SubString(0, dotIndex) +
            name.SubString(dotIndex, name.GetLen()).AsLowerCase();
    }

    void PSBMedia::NormalizePathName(ttstr &name) {
        // province(_p), mask(_m)
    }

    bool PSBMedia::CheckExistentStorage(const ttstr &name) {
        return find(name) != nullptr;
    }

    tTJSBinaryStream *PSBMedia::Open(const ttstr &name, tjs_uint32 flags) {
        const PSBResource *found = find(name);
        if(found == nullptr) {
            // Kirikinux2: do not insert an empty resource for a miss.
            TVPThrowExceptionMessage(TJS_W("Cannot open storage %1"),
                                     ttstr(TJS_W("psb://")) + name);
        }
        const auto &res = *found;
        auto memoryStream = new tTVPMemoryStream();
        if(!res.data.empty())
            memoryStream->WriteBuffer(res.data.data(), res.data.size());
        memoryStream->Seek(0, TJS_BS_SEEK_SET);
        return memoryStream;
    }

    void PSBMedia::GetListAt(const ttstr &name, iTVPStorageLister *lister) {
        LOGGER->error("TODO: PSBMedia GetListAt");
    }

    void PSBMedia::GetLocallyAccessibleName(ttstr &name) {
        // Resources only exist in memory: not locally accessible.
        name.Clear();
    }

    void PSBMedia::add(const std::string &name,
                       const std::shared_ptr<PSBResource> &resource) {
        if(resource == nullptr)
            return;
        this->_resources[lowerKey(name)] = *resource;
    }
} // namespace PSB