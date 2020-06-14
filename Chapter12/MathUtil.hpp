#pragma once
#include <vector>

struct MathUtil {
static std::vector<float> calcGaussianWeights(size_t count, float s) {
    std::vector<float> weights(count); // ウェイト配列返却用
    float x = 0.0f;
    float total = 0.0f;
    for (auto& wgt : weights) {
        wgt = expf(-(x*x) / (2 * s*s));
        total += wgt;
        x += 1.0f;
    }
    // 真ん中を中心に左右に広がるように作りますので
    // 左右という事で2倍します。しかしその場合は中心の0のピクセルが
    // 重複してしまいますのでe^0=1ということで最後に1を引いて辻褄が合うようにしています。
    total = total * 2.0f - 1.0f;
    // 足して１になるようにする
    for (auto& wgt : weights) {
        wgt /= total;
    }
    return weights;  
}
};
