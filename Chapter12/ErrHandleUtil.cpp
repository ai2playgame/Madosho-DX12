#include "ErrHandleUtil.hpp"
#include <string>
#include <algorithm>

bool checkResult(HRESULT& result, ID3DBlob* errBlob)
{
    if (FAILED(result)) {
#ifndef _DEBUG
        if (errBlob != nullptr) {
            std::string outmsg;
            outmsg.resize(errBlob->GetBufferSize());
            std::copy_n(static_cast<char*>(errBlob->GetBufferPointer()),
                errBlob->GetBufferSize(),
                outmsg.begin());
            OutputDebugString(outmsg.c_str());
        }
        assert(SUCCEEDED(result));
#endif // !_DEBUG
        return false;
    }
    else {
        return true;
    }
}
