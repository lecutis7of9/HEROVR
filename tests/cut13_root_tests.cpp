// HERO VR 0.0.78 cut13: the model's and the calibration art's root signatures, serialized and read back with no
// GPU. cut12 (a823c99) said 3 parameters of an array of 2 in the model's root: "Serialize model root signature
// HRESULT=2147942487" (console line 325 of the 161552 report) the first time the holo opened, and every model went
// UNTEXTURED for the session (initials for the portraits, no suits, the flat-lit van). This fails on that build.
#include "spidy/d3d12_renderer.hpp"
#include "spidy/model_pbr.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace spidy;
static int failures = 0;
static void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    failures += ok ? 0 : 1;
}
static const D3D12_ROOT_SIGNATURE_DESC* readBack(HRESULT (*serialize)(ComPtr<ID3DBlob>&, ComPtr<ID3DBlob>&), const char* name,
                                                 ComPtr<ID3D12RootSignatureDeserializer>& reader) {
    ComPtr<ID3DBlob> signature, error;
    const HRESULT r = serialize(signature, error);
    char line[200];
    std::snprintf(line, sizeof line, "%s root signature serializes (HRESULT 0x%08lX%s%s)", name, static_cast<unsigned long>(r),
                  error ? ": " : "", error ? static_cast<const char*>(error->GetBufferPointer()) : "");
    check(SUCCEEDED(r) && signature, line);
    if (FAILED(r) || !signature)
        return nullptr;
    const HRESULT d = D3D12CreateRootSignatureDeserializer(signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&reader));
    check(SUCCEEDED(d), std::string(name) + " root signature reads back");
    return SUCCEEDED(d) ? reader->GetRootSignatureDesc() : nullptr;
}
static bool tablesFit(const D3D12_ROOT_SIGNATURE_DESC& rs, unsigned heap) {
    for (UINT p = 0; p < rs.NumParameters; ++p) {
        if (rs.pParameters[p].ParameterType != D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
            continue;
        UINT next = 0;
        for (UINT k = 0; k < rs.pParameters[p].DescriptorTable.NumDescriptorRanges; ++k) {
            const auto& range = rs.pParameters[p].DescriptorTable.pDescriptorRanges[k];
            const UINT at = range.OffsetInDescriptorsFromTableStart == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND ? next : range.OffsetInDescriptorsFromTableStart;
            if (at + range.NumDescriptors > heap)
                return false;
            next = at + range.NumDescriptors;
        }
    }
    return true;
}
int main() {
    ComPtr<ID3D12RootSignatureDeserializer> modelReader, artReader;
    if (const auto* rs = readBack(&serializeModelRoot, "model", modelReader)) {
        check(rs->NumParameters == 3, "model root: 3 parameters (the draw sets 0 the eye, 1 the textures, 2 the light)");
        if (rs->NumParameters == 3) {
            const auto& eye = rs->pParameters[0];
            check(eye.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS && eye.Constants.Num32BitValues == 16 &&
                      eye.Constants.ShaderRegister == 0,
                  "model root 0: 16 constants at b0 (the eye's view-projection)");
            const auto& table = rs->pParameters[1];
            bool t0 = false, maps = false;
            if (table.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
                for (UINT k = 0; k < table.DescriptorTable.NumDescriptorRanges; ++k) {
                    const auto& r = table.DescriptorTable.pDescriptorRanges[k];
                    t0 = t0 || (r.BaseShaderRegister == 0 && r.NumDescriptors >= 1 && r.OffsetInDescriptorsFromTableStart == 0);
                    maps = maps || (r.BaseShaderRegister == 2 && r.NumDescriptors == 3 && r.OffsetInDescriptorsFromTableStart == 3);
                }
            check(t0 && maps, "model root 1: t0 the colours (heap 0) and t2..t4 the maps and HQ copies (heap 3..5)");
            const auto& light = rs->pParameters[2];
            check(light.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS && light.Constants.ShaderRegister == 2 &&
                      light.Constants.Num32BitValues == sizeof(model_pbr::LightConstants) / 4 && light.Constants.Num32BitValues == 24,
                  "model root 2: the light, 24 constants at b2 (model_pbr::LightConstants)");
        }
        check(tablesFit(*rs, modelHeapDescriptors), "model root: its table fits the model heap (6 descriptors)");
        check(rs->NumStaticSamplers == 1 && rs->pStaticSamplers[0].AddressU == D3D12_TEXTURE_ADDRESS_MODE_WRAP,
              "model root: the wrapping sampler (the models' UVs tile)");
    }
    if (const auto* rs = readBack(&serializeArtRoot, "art", artReader)) {
        check(rs->NumParameters == 2, "art root: 2 parameters (the eye, the atlas)");
        check(tablesFit(*rs, artHeapDescriptors), "art root: its table fits the art heap (1 descriptor) - cut12 reached 3..5");
        check(rs->NumStaticSamplers == 1 && rs->pStaticSamplers[0].AddressU == D3D12_TEXTURE_ADDRESS_MODE_CLAMP &&
                  rs->pStaticSamplers[0].MaxLOD == 4,
              "art root: the clamped sampler, 4 levels (the atlas's pictures sit side by side)");
    }
    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
