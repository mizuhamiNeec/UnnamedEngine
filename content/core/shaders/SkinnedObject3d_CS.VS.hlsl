#include "SkinnedObject3d.hlsli"

// コンピュートシェーダーで変換済みの頂点データ
struct TransformedVertex {
    float4 position;
    float2 uv;
    float3 normal;
    float3 worldPosition;
};

// 変換済み頂点バッファ
StructuredBuffer<TransformedVertex> gTransformedVertices : register(t0);

struct VertexShaderInput {
    uint vertexID : SV_VertexID;
};

VertexShaderOutput VSMain(VertexShaderInput input) {
    VertexShaderOutput output;
    
    // コンピュートシェーダーで変換済みのデータを取得
    TransformedVertex transformedVertex = gTransformedVertices[input.vertexID];
    
    // 出力に設定
    output.position = transformedVertex.position;
    output.texcoord = transformedVertex.uv;
    output.normal = transformedVertex.normal;
    output.worldPosition = transformedVertex.worldPosition;
    
    return output;
}
