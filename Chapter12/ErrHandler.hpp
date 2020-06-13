#pragma once
#include <d3d12.h>
#include <cassert>
#include <string>

struct ErrHandler {
private:
    ErrHandler() = delete;

public:
    static bool checkResult(HRESULT result, ID3DBlob* errBlob) {
        if (FAILED(result)) {
    #ifdef _DEBUG
            if (errBlob!=nullptr) {
                std::string outmsg;
                outmsg.resize(errBlob->GetBufferSize());
                std::copy_n(static_cast<char*>(errBlob->GetBufferPointer()),
                    errBlob->GetBufferSize(),
                    outmsg.begin());
                OutputDebugStringA(outmsg.c_str());
            }
            assert(SUCCEEDED(result));
    #endif
            return false;
        }
        else {
            return true;
        }
 
    }
};
