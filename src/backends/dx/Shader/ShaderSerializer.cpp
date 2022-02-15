#pragma vengine_package vengine_directx
#include <Shader/ShaderSerializer.h>
#include <Shader/ComputeShader.h>
#include <Shader/RTShader.h>
#include <DXRuntime/GlobalSamplers.h>
#include <Shader/PipelineLibrary.h>
namespace toolhub::directx {
namespace shader_ser {
struct Header {
    uint64 reflectionBytes;
    uint64 rootSigBytes;
    uint64 codeBytes;
    uint3 blockSize;
};
}// namespace shader_ser
vstd::vector<vbyte>
ShaderSerializer::Serialize(
    vstd::span<std::pair<vstd::string, Shader::Property> const> properties,
    vstd::span<vbyte> binByte,
    uint3 blockSize) {
    using namespace shader_ser;
    vstd::vector<vbyte> result;
    result.reserve(65500);
    result.resize(sizeof(Header));
    Header header{
        (uint64)SerializeReflection(properties, result),
        (uint64)SerializeRootSig(properties, result),
        (uint64)binByte.size(),
        blockSize};
    *reinterpret_cast<Header *>(result.data()) = header;
    result.push_back_all(binByte);
    return result;
}
ComputeShader *ShaderSerializer::DeSerialize(
    Device *device,
    vstd::MD5 md5,
    Visitor &visitor,
    PipelineLibrary *pipeLib) {
    using namespace shader_ser;
    vbyte const *ptr = nullptr;
    auto Get = [&]<typename T>() -> T const & {
        ptr = visitor.ReadFile(sizeof(T));
        return *reinterpret_cast<T const *>(ptr);
    };
    auto header = Get.operator()<Header>();
    ptr = visitor.ReadFile(header.reflectionBytes);
    auto refl = DeSerializeReflection(
        {ptr, header.reflectionBytes});
    ptr = visitor.ReadFile(header.rootSigBytes);
    auto rootSig = DeSerializeRootSig(
        device->device.Get(),
        {ptr, header.rootSigBytes});
    // Try pipeline library
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc;
    memset(&psoDesc, 0, sizeof(psoDesc));
    psoDesc.pRootSignature = rootSig.Get();
    ComPtr<ID3D12PipelineState> pso;
    auto psoResult = visitor.ReadFileAndPSO(header.codeBytes);
    psoDesc.CS.pShaderBytecode = psoResult.fileData;
    psoDesc.CS.BytecodeLength = psoResult.fileSize;
    psoDesc.CachedPSO.CachedBlobSizeInBytes = psoResult.psoSize;
    psoDesc.CachedPSO.pCachedBlob = psoResult.psoData;
    // Try pipeline library
    if (pipeLib) {
        pso = pipeLib->GetPipelineState(
            vstd::Guid(md5),
            psoDesc);
    }
    // No pipeline library, use PSO cache
    if (!pso) {
        if (device->device->CreateComputePipelineState(
                &psoDesc,
                IID_PPV_ARGS(pso.GetAddressOf())) != S_OK) {
            // PSO cache miss(probably driver's version or hardware transformed), discard cache
            visitor.DeletePSOFile();
            psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
            psoDesc.CachedPSO.pCachedBlob = nullptr;
            ThrowIfFailed(device->device->CreateComputePipelineState(
                &psoDesc,
                IID_PPV_ARGS(pso.GetAddressOf())));
        }
    }
    auto cs = new ComputeShader(
        header.blockSize,
        device,
        refl,
        vstd::Guid(md5),
        std::move(rootSig),
        std::move(pso));
    return cs;
}
size_t ShaderSerializer::SerializeRootSig(
    vstd::span<std::pair<vstd::string, Shader::Property> const> properties,
    vstd::vector<vbyte> &result) {
    auto lastSize = result.size();
    vstd::vector<CD3DX12_ROOT_PARAMETER, VEngine_AllocType::VEngine, 32> allParameter;
    vstd::vector<CD3DX12_DESCRIPTOR_RANGE, VEngine_AllocType::VEngine, 32> allRange;
    size_t offset = 0;
    for (auto &&kv : properties) {
        auto &&var = kv.second;

        switch (var.type) {
            case ShaderVariableType::SRVDescriptorHeap: {
                CD3DX12_DESCRIPTOR_RANGE &range = allRange[offset];
                offset++;
                range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, var.arrSize == 0 ? 1 : var.arrSize, var.registerIndex, var.spaceIndex);
                auto &&v = allParameter.emplace_back();
                memset(&v, 0, sizeof(CD3DX12_ROOT_PARAMETER));
                v.InitAsDescriptorTable(1, &range);
            } break;
            case ShaderVariableType::UAVDescriptorHeap: {
                CD3DX12_DESCRIPTOR_RANGE &range = allRange[offset];
                offset++;
                range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, var.arrSize == 0 ? 1 : var.arrSize, var.registerIndex, var.spaceIndex);
                allParameter.emplace_back().InitAsDescriptorTable(1, &range);
            } break;
            case ShaderVariableType::ConstantBuffer:
                allParameter.emplace_back().InitAsConstantBufferView(var.registerIndex, var.spaceIndex);
                break;
            case ShaderVariableType::StructuredBuffer:
                allParameter.emplace_back().InitAsShaderResourceView(var.registerIndex, var.spaceIndex);
                break;
            case ShaderVariableType::RWStructuredBuffer:
                allParameter.emplace_back().InitAsUnorderedAccessView(var.registerIndex, var.spaceIndex);
                break;
            default:
                break;
        }
    }
    auto arr = GlobalSamplers::GetSamplers();
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc(
        allParameter.size(), allParameter.data(),
        arr.size(), arr.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    ThrowIfFailed(D3D12SerializeVersionedRootSignature(
        &rootSigDesc,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf()));
    result.push_back_all(
        (vbyte const *)serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize());
    return result.size() - lastSize;
}
size_t ShaderSerializer::SerializeReflection(
    vstd::span<std::pair<vstd::string, Shader::Property> const> properties,
    vstd::vector<vbyte> &result) {
    auto lastSize = result.size();
    auto Push = [&]<typename T>(T const &v) {
        auto sz = result.size();
        result.resize(sz + sizeof(T));
        memcpy(result.data() + sz, &v, sizeof(T));
    };
    auto PushArray = [&](void const *ptr, size_t size) {
        auto sz = result.size();
        result.resize(sz + size);
        memcpy(result.data() + sz, ptr, size);
    };
    Push((size_t)properties.size());
    for (auto &&i : properties) {
        PushArray(i.first.data(), i.first.size() + 1);
        Push(i.second);
    }
    return result.size() - lastSize;
}
vstd::vector<std::pair<vstd::string, Shader::Property>> ShaderSerializer::DeSerializeReflection(
    vstd::span<vbyte const> bytes) {
    vbyte const *ptr = bytes.data();
    auto Pop = [&]<typename T>() {
        T result;
        memcpy(&result, ptr, sizeof(T));
        ptr += sizeof(T);
        return result;
    };
    auto PopString = [&] {
        vstd::string str((char const *)ptr);
        ptr += str.size() + 1;
        return str;
    };
    vstd::vector<std::pair<vstd::string, Shader::Property>> result;
    auto tarSize = Pop.operator()<size_t>();
    result.push_back_func(
        [&]() -> std::pair<vstd::string, Shader::Property> {
            return {PopString(), Pop.operator()<Shader::Property>()};
        },
        tarSize);
    return result;
}
ComPtr<ID3D12RootSignature> ShaderSerializer::DeSerializeRootSig(
    ID3D12Device *device,
    vstd::span<vbyte const> bytes) {
    ComPtr<ID3D12RootSignature> rootSig;
    ThrowIfFailed(device->CreateRootSignature(
        0,
        bytes.data(),
        bytes.size(),
        IID_PPV_ARGS(rootSig.GetAddressOf())));
    return rootSig;
}
}// namespace toolhub::directx