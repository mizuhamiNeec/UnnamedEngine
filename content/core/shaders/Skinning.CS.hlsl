// 頂点データ
struct SkinnedVertex {
    float4 position;
    float2 uv;
    float3 normal;
    float4 boneWeights;
    uint4  boneIndices;
};

// 変換後の頂点データ
struct TransformedVertex {
    float4 position;
    float2 uv;
    float3 normal;
    float3 worldPosition;
};

// ボーン変換行列
struct BoneMatrices {
    float4x4 bones[256];
};

struct TransformationMatrix {
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

ConstantBuffer<BoneMatrices>         gBoneMatrices         : register(b0);
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b1);

// 入力頂点バッファ (読み取り専用)
StructuredBuffer<SkinnedVertex> gInputVertices : register(t0);

// 出力頂点バッファ (書き込み専用)
RWStructuredBuffer<TransformedVertex> gOutputVertices : register(u0);

// これは一度にComputeShader内のThreadを起動できる数である
// thread 1つ1つがmain関数を実行する
// numthreadの数値はそれぞれ左からx,y,zと呼び、いくつかの制約がある
// z <= 64
// x * y * z <= 1024
// 1 <= x && 1 <= y && 1 <= z
// x,y,zはコンパイル時定数でなければならない
// 実際に起動するthreadの個数はx*y*zなので、今回のケースでは1024 * 1 * 1 = 1024 threadが一度に起動することになる
[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint vertexIndex = DTid.x;
    
    // バッファサイズを超えた場合は処理をスキップ
    uint inputCount, stride;
    gInputVertices.GetDimensions(inputCount, stride);
    if (vertexIndex >= inputCount) {
        return;
    }
    
    // 入力頂点データを取得
    SkinnedVertex inputVertex = gInputVertices[vertexIndex];
    
    // スキニング計算
    float4 skinnedPosition = float4(0, 0, 0, 0);
    float3 skinnedNormal   = float3(0, 0, 0);
    
    // 最大4つのボーンによる変換を適用
    for (int i = 0; i < 4; ++i) {
        uint  boneIndex = inputVertex.boneIndices[i];
        float weight    = inputVertex.boneWeights[i];
        
        if (weight > 0.0f && boneIndex < 256) {
            float4x4 boneMatrix = gBoneMatrices.bones[boneIndex];
            skinnedPosition += mul(inputVertex.position, boneMatrix) * weight;
            skinnedNormal += mul(inputVertex.normal, (float3x3)boneMatrix) * weight;
        }
    }
    
    // ウェイトの合計をチェック
    float weightSum = dot(inputVertex.boneWeights, float4(1, 1, 1, 1));
    if (weightSum < 0.01f) {
        skinnedPosition = inputVertex.position;
        skinnedNormal   = inputVertex.normal;
    } else {
        skinnedPosition.w = 1.0f;
    }
    
    // 出力データを作成
    TransformedVertex outputVertex;
    outputVertex.position = mul(skinnedPosition, gTransformationMatrix.WVP);
    outputVertex.uv = inputVertex.uv;
    outputVertex.normal = normalize(mul(skinnedNormal, (float3x3)gTransformationMatrix.WorldInverseTranspose));
    outputVertex.worldPosition = mul(skinnedPosition, gTransformationMatrix.World).xyz;
    
    // 結果を出力バッファに書き込み
    gOutputVertices[vertexIndex] = outputVertex;
}