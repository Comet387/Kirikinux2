//
// Created by lidong on 2025/1/31.
// TODO: implement psbfile.dll plugin
// ref: https://github.com/number201724/psbfile
// ref: https://github.com/UlyssesWu/FreeMote
//
#include "../kr2_plugin_log.h"
#include <cassert>

#include "tjs.h"
#include "ncbind.hpp"
#include "PSBFile.h"
#include "PSBHeader.h"
#include "PSBMedia.h"
#include "PSBMediaRegistry.h"
#include "PSBValue.h"

#define NCB_MODULE_NAME TJS_W("psbfile.dll")

#define LOGGER spdlog::get("plugin")

using namespace PSB;
// Kirikinux2: the psb:// media object lives in PSBMediaRegistry.cpp.  This file
// used to declare its own `static PSBMedia *psbMedia` that was never assigned,
// so PSBFile.load() dereferenced a null pointer (SIGSEGV when UILoader /
// psdlayer.tjs loaded the first .pimg).  Always go through the registry.

void initPsbFile() { initPSBMedia(); }

// Register resources under the storage exactly as passed and under its bare
// file name, which is what psdlayer.tjs uses to build psb:// layer names.
static void registerWithAliases(const ttstr &path, const PSBFile &file) {
    std::vector<ttstr> containers{ path };
    const ttstr bare = TVPExtractStorageName(path);
    if(!bare.IsEmpty() && bare != path)
        containers.push_back(bare);
    PSB::registerRootResources(containers, file);
}

void deInitPsbFile() { deInitPSBMedia(); }

static tjs_error getRoot(tTJSVariant *r, tjs_int n, tTJSVariant **p,
                         iTJSDispatch2 *obj) {
    auto *self = ncbInstanceAdaptor<PSB::PSBFile>::GetNativeInstance(obj);
    if(self == nullptr)
        return TJS_E_NATIVECLASSCRASH;
    iTJSDispatch2 *dic = TJSCreateCustomObject();
    auto objs = self->getObjects();
    if(objs != nullptr) {
        for(const auto &[k, v] : *objs) {
            tTJSVariant tmp = v->toTJSVal();
            dic->PropSet(TJS_MEMBERENSURE, ttstr{ k }.c_str(), nullptr, &tmp,
                         dic);
        }
    }
    *r = tTJSVariant{ dic, dic };
    dic->Release();
    return TJS_S_OK;
}

static tjs_error load(tTJSVariant *r, tjs_int count, tTJSVariant **p,
                      iTJSDispatch2 *obj) {
    bool loadSuccess = true;
    auto *self = ncbInstanceAdaptor<PSB::PSBFile>::GetNativeInstance(obj);
    if(self == nullptr)
        return TJS_E_NATIVECLASSCRASH;
    if(count != 1) {
        return TJS_E_BADPARAMCOUNT;
    }

    if((*p)->Type() == tvtString) {
        ttstr path{ **p };
        if(self->loadPSBFile(path)) {
            // Expose the resources as psb://<path>/<key> (nested keys joined
            // with '/', top-level keys exactly as before).
            registerWithAliases(path, *self);
        } else {
            LOGGER->info("cannot load psb file : {}", path.AsStdString());
            loadSuccess = false;
        }
    } else if((*p)->Type() == tvtOctet) {
        LOGGER->critical("PSBFile::load stream no implement!");
        loadSuccess = false;
    } else {
        return TJS_E_INVALIDPARAM;
    }

    if(r != nullptr)
        *r = tTJSVariant(loadSuccess);
    return TJS_S_OK;
}

// 因为有两种版本的psbfile插件调用方式不一样
// TODO: 第一种（新) 实现有问题, 可能忽略了某些东西
// var psbfile = new PSBFile();
// psbfile.load("xxxx.PIMG");
// 第二种（旧)
// new PSBFile("xxxx.PIMG");

template <typename T>
class PSBFileConvertor {
    typedef ncbTypeConvertor::Stripper<PSBFile>::Type ClassT;
    typedef ncbInstanceAdaptor<ClassT> AdaptorT;

public:
    PSBFileConvertor() = default;
    virtual ~PSBFileConvertor() = default;

    virtual void operator()(T *&dst, const tTJSVariant &src) {
        if(src.Type() == tvtObject) {
            dst = AdaptorT::GetNativeInstance(src.AsObjectNoAddRef());
        }
    }

    void operator()(tTJSVariant &dst, const T *&src) {
        if(src != nullptr) {
            if(iTJSDispatch2 *adpObj = AdaptorT::CreateAdaptor(src)) {
                dst = tTJSVariant(adpObj, adpObj);
                adpObj->Release();
            }
        } else {
            dst.Clear();
        }
    }
};

NCB_SET_CONVERTOR(PSBFile, PSBFileConvertor<PSBFile>);
NCB_SET_CONVERTOR(const PSBFile *, PSBFileConvertor<const PSBFile>);

static tjs_error PSBFileFactory(PSBFile **result, tjs_int count,
                                tTJSVariant **params, iTJSDispatch2 *_) {
    PSBFile *psbFile = nullptr;
    if(count == 0) {
        psbFile = new PSBFile();
    } else if(count == 1 && (*params)->Type() == tvtString) {
        ttstr path{ *params[0] };
        psbFile = new PSBFile();
        if(psbFile->loadPSBFile(path))
            registerWithAliases(path, *psbFile);
    } else {
        return TJS_E_INVALIDPARAM;
    }
    *result = psbFile;
    return TJS_S_OK;
}

NCB_REGISTER_CLASS(PSBFile) {
    Factory(PSBFileFactory);
    RawCallback(TJS_W("root"), &getRoot, 0, 0);
    RawCallback(TJS_W("load"), &load, 0);
}

NCB_PRE_REGIST_CALLBACK(initPsbFile);
NCB_POST_UNREGIST_CALLBACK(deInitPsbFile);
