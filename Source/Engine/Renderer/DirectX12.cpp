#include "DirectX12.h"

#ifdef _DEBUG

#include <cassert>
#include <dxgidebug.h>
#include <format>
#include "../Lib/Utils/StrUtils.h"

#include "../Lib/Math/Matrix/Mat4.h"
#include "../Lib/Console/Console.h"
#include "../Lib/Utils/ClientProperties.h"

//ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename) {
//	ModelData modelData; // 構築するModelData
//	std::vector<Vec4> positions; // 位置
//	std::vector<Vec3> normals; // 法線
//	std::vector<Vec2> texcoords; // テクスチャの座標
//	std::string line;
//
//	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
//	assert(file.is_open());
//
//	while (std::getline(file, line)) {
//		std::string identifier;
//		std::istringstream s(line);
//		s >> identifier; // 先頭の識別子を読む
//
//		if (identifier == "v") {
//			Vec4 position;
//			s >> position.x >> position.y >> position.z;
//			position.w = 1.0f;
//			positions.push_back(position);
//		} else if (identifier == "vt") {
//			Vec2 texcoord;
//			s >> texcoord.x >> texcoord.y;
//			texcoord.y = 1.0f - texcoord.y;
//			texcoords.push_back(texcoord);
//		} else if (identifier == "vn") {
//			Vec3 normal;
//			s >> normal.x >> normal.y >> normal.z;
//			normals.push_back(normal);
//		} else if (identifier == "f") {
//			VertexData triangle[3];
//
//			// 面は三角形限定。その他は未対応
//			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
//				std::string vertexDefinition;
//				s >> vertexDefinition;
//				// 頂点の要素へのIndexは[位置/UV/法線] で格納されているので、分解してIndexを取得する
//				std::istringstream v(vertexDefinition);
//				uint32_t elementIndices[3];
//				for (int32_t element = 0; element < 3; ++element) {
//					std::string index;
//					std::getline(v, index, '/'); // 区切りでインデックスを読んでいく
//					elementIndices[element] = std::stoi(index);
//				}
//
//				// 要素へのIndexから、実際の要素の値を取得して、頂点を構築する
//				Vec4 position = positions[elementIndices[0] - 1];
//				Vec2 texcoord = texcoords[elementIndices[1] - 1];
//				Vec3 normal = normals[elementIndices[2] - 1];
//				/*VertexData vertex = { position, texcoord, normal };
//				modelData.vertices.push_back(vertex);*/
//
//				position.x *= -1.0f;
//				normal.x *= -1.0f;
//				triangle[faceVertex] = {position, texcoord, normal};
//			}
//			// 頂点を逆順で登録することで、周り順を逆にする
//			modelData.vertices.push_back(triangle[2]);
//			modelData.vertices.push_back(triangle[1]);
//			modelData.vertices.push_back(triangle[0]);
//		} else if (identifier == "mtllib") {
//			// materialTemplateLibraryファイルの名前を取得する
//			std::string materialFilename;
//			s >> materialFilename;
//			// 基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
//			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
//		}
//	}
//
//	return modelData;
//}

//MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
//	MaterialData materialData; // 構築するMaterialData
//	std::string line; // ファイルから読んだ1行を格納するもの
//	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
//	assert(file.is_open()); // とりあえず開けなかったら止める
//
//	while (std::getline(file, line)) {
//		std::string identifier;
//		std::istringstream s(line);
//		s >> identifier;
//
//		// identifierに応じた処理
//		if (identifier == "map_Kd") {
//			std::string textureFilename;
//			s >> textureFilename;
//			// 連結してファイルパスにする
//			materialData.textureFilePath = directoryPath + "/" + textureFilename;
//		}
//	}
//
//	return materialData;
//}

DirectX12::DirectX12() {
}

DirectX12::~DirectX12() = default;

/// <summary>
/// DirectX12の初期化を行います
/// </summary>
/// <param name="window">描画するウィンドウクラス</param>
void DirectX12::Init(Window* window) {
	// UNDONE : エラーチェック
	window_ = window;

#ifdef _DEBUG
	EnableDebugLayer();
#endif

	CreateDevice();

	//==============================
	// DescriptorSizeを取得しておく
	//==============================
	descriptorSizeSRV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	descriptorSizeRTV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorSizeDSV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

#ifdef _DEBUG
	SetInfoQueueBreakOnSeverity();
#endif

	CreateCommandQueue();
	CreateCommandList();
	CreateSwapChain();

	// RTV用のヒープでディスクリプタの数は2。RTVはShader内で触るものではないので、ShaderVisibleはfalse
	rtvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
	// SRV用のヒープでディスクリプタの数は128。SRVはShader内で触るものなので、ShaderVisibleはtrue
	srvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);
	// DSV用のヒープでディスクリプタの数は1。DSVはShader内で触るものではないので、ShaderVisibleはfalse
	dsvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	GetResourceFromSwapChain();
	CreateRTV();

	Console::Print("Complete Init DirectX12.\n");

	/* FenceとEventを生成する */
	// 初期値0でFenceを作る
	hr_ = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
	assert(SUCCEEDED(hr_));

	// FenceのSignalを持つためのイベントを作成する
	fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	assert(fenceEvent_ != nullptr);

	InitializeDxc();

	/* RootSignatureを生成する */
	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature = {};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRange[0].NumDescriptors = 1; // 数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

	/* RootParameter */
	// RootParameter作成。複数設定できるので配列。今回は結果1つだけなので長さ1の配列
	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う。 b0のbと一致する
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号0とバインド。 b0の0と一致する。もしb11と紐づけたいなら11となる

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange; // Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[3].Descriptor.ShaderRegister = 1; // レジスタ番号1を使う

	descriptionRootSignature.pParameters = rootParameters; // ルートパラメータ配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters); // 配列の長さ

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 0～1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0; // レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズしてバイナリにする
	hr_ = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob_,
		&errorBlob_);
	if (FAILED(hr_)) {
		Console::Print(static_cast<char*>(errorBlob_->GetBufferPointer()));
		assert(false);
	}
	// バイナリを元に生成
	hr_ = device_->CreateRootSignature(0, signatureBlob_->GetBufferPointer(), signatureBlob_->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr_));

	// DepthStencilTextureをウィンドウのサイズで作成
	depthStencilResource = CreateDepthStencilTextureResource(kClientWidth, kClientHeight);

	// DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // Format。基本的にはResourceに合わせる
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // 2DTexture
	// DSVHeapの先頭にDSVをつくる
	device_->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc,
		dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart());

	/* InputLayout(インプットレイアウト) */
	// InputLayoutの設定を行う
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	/* BlendStateの設定を行う */
	// BlendStateの設定
	D3D12_BLEND_DESC blendDesc = {};
	// すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	/* RasterizerStateの設定を行う */
	// RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc = {};
	// 裏面(時計回り)を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// Shaderをコンパイルする
	vertexShaderBlob_ = CompileShader(L"./Shaders/Object3d.VS.hlsl", L"vs_6_0", dxcUtils_.Get(), dxcCompiler_.Get(),
		includeHandler_.Get());
	assert(vertexShaderBlob_ != nullptr);

	pixelShaderBlob_ = CompileShader(L"./Shaders/Object3d.PS.hlsl", L"ps_6_0", dxcUtils_.Get(), dxcCompiler_.Get(),
		includeHandler_.Get());
	assert(pixelShaderBlob_ != nullptr);

	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
	// Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	// 書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	// 比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	/* PSOを生成する */
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc = {};
	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get(); // RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc; // InputLayout
	graphicsPipelineStateDesc.VS = {
		vertexShaderBlob_->GetBufferPointer(), vertexShaderBlob_->GetBufferSize()
	}; // VertexShader
	graphicsPipelineStateDesc.PS = {
		pixelShaderBlob_->GetBufferPointer(), pixelShaderBlob_->GetBufferSize()
	}; // PixelShader
	graphicsPipelineStateDesc.BlendState = blendDesc; // BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc; // RasterizerState
	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	// 利用するトポロジ(形状)のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定(気にしなくて良い)
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 実際に生成
	hr_ = device_->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
	assert(SUCCEEDED(hr_));

	directionalLightResource_ = CreateBufferResource(device_.Get(), sizeof(DirectionalLight));
	directionalLightData_ = nullptr;
	directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));
	directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData_->direction = { 0.0f, 0.0f, 1.0f };
	directionalLightData_->intensity = 1.0f;

	//// 球の描画
	//const uint32_t kSubdivision = 32; // 分割数

	//vertNum = kSubdivision * kSubdivision * 6;

	// モデル読み込み
	//modelData = LoadObjFile("Resources/Models", "axis.obj");

	vertexResourceMesh_ = CreateBufferResource(device_.Get(), sizeof(VertexData) * modelData.vertices.size());

	materialResourceMesh_ = CreateBufferResource(device_.Get(), sizeof(Material));
	// マテリアルにデータを書き込む
	materialDataMesh_ = nullptr;
	// 書き込むためのアドレスを取得
	materialResourceMesh_->Map(0, nullptr, reinterpret_cast<void**>(&materialDataMesh_));
	*materialDataMesh_ = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白
	materialDataMesh_->enableLighting = true;
	materialDataMesh_->uvTransform = Mat4::Identity();

	// WVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	transformationMatrixResourceMesh_ = CreateBufferResource(device_.Get(), sizeof(TransformationMatrix));
	// データを書き込む
	// 書き込むためのアドレスを取得
	transformationMatrixResourceMesh_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataMesh_));
	// 単位行列を書き込んでおく
	transformationMatrixDataMesh_->wvp = Mat4::Identity();

	/* VertexBufferViewを作成する */
	// 頂点バッファビューを作成する
	// リソースの戦闘のアドレスから使う
	vertexBufferViewMesh_.BufferLocation = vertexResourceMesh_->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点3つ分のサイズ
	vertexBufferViewMesh_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * modelData.vertices.size());
	// 1頂点あたりのサイズ
	vertexBufferViewMesh_.StrideInBytes = sizeof(VertexData);

	/* Resourceにデータを書き込む */
	// 頂点リソースにデータを書き込む
	// 書き込むためのアドレスを取得
	vertexResourceMesh_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());
	// 頂点データをリソースにコピー

	//	// 経度分割1つ分の角度 φ
	//	const float kLonEvery = std::numbers::pi_v<float> *2.0f / static_cast<float>(kSubdivision);
	//	// 緯度分割1つ分の角度 θ
	//	const float kLatEvery = std::numbers::pi_v<float> / static_cast<float>(kSubdivision);
	//
	//	// 経度の方向に分割
	//	for (int latIndex = 0; latIndex < kSubdivision; ++latIndex) {
	//		float lat = -std::numbers::pi_v<float> *0.5f + kLatEvery * latIndex; // θ
	//
	//		float cosLat = cos(lat);
	//		float sinLat = sin(lat);
	//		float cosLatEvery = cos(lat + kLatEvery);
	//		float sinLatEvery = sin(lat + kLatEvery);
	//
	//		// 経度の方向に分割しながら線を描く
	//		for (int lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {
	//			uint32_t start = (latIndex * kSubdivision + lonIndex) * 6;
	//			float lon = lonIndex * kLonEvery; // φ
	//
	//			float cosLon = cos(lon);
	//			float sinLon = sin(lon);
	//			float cosLonEvery = cos(lon + kLonEvery);
	//			float sinLonEvery = sin(lon + kLonEvery);
	//
	//#pragma region 三角形1
	//			// 頂点にデータを入力する。基準点a
	//			vertexData[start] = {
	//				{
	//					cosLat * cosLon,
	//					sinLat,
	//					cosLat * sinLon,
	//					1.0f
	//				},
	//				{
	//					static_cast<float>(lonIndex) / static_cast<float>(kSubdivision),
	//					1.0f - static_cast<float>(latIndex) / static_cast<float>(kSubdivision)
	//				},
	//				{
	//					vertexData[start].position.x,
	//					vertexData[start].position.y,
	//					vertexData[start].position.z
	//				}
	//			};
	//			// 頂点にデータを入力する。b
	//			vertexData[start + 1] = {
	//				{
	//					cosLatEvery * cosLon,
	//					sinLatEvery,
	//					cosLatEvery * sinLon,
	//					1.0f
	//				},
	//				{
	//					static_cast<float>(lonIndex) / static_cast<float>(kSubdivision),
	//					1.0f - static_cast<float>(latIndex + 1) / static_cast<float>(kSubdivision)
	//				},
	//				{
	//					vertexData[start + 1].position.x,
	//					vertexData[start + 1].position.y,
	//					vertexData[start + 1].position.z
	//				}
	//			};
	//			// 頂点にデータを入力する。c
	//			vertexData[start + 2] = {
	//				{
	//					cosLat * cosLonEvery,
	//					sinLat,
	//					cosLat * sinLonEvery,
	//					1.0f
	//				},
	//				{
	//					static_cast<float>(lonIndex + 1) / static_cast<float>(kSubdivision),
	//					1.0f - static_cast<float>(latIndex) / static_cast<float>(kSubdivision)
	//				},
	//				{
	//					vertexData[start + 2].position.x,
	//					vertexData[start + 2].position.y,
	//					vertexData[start + 2].position.z
	//				}
	//			};
	//#pragma endregion
	//
	//#pragma region 三角形2
	//			// 頂点にデータを入力する。c
	//			vertexData[start + 3] = {
	//				{
	//					cosLat * cosLonEvery,
	//					sinLat,
	//					cosLat * sinLonEvery,
	//					1.0f
	//				},
	//				{
	//					static_cast<float>(lonIndex + 1) / static_cast<float>(kSubdivision),
	//					1.0f - static_cast<float>(latIndex) / static_cast<float>(kSubdivision)
	//				},
	//				{
	//					vertexData[start + 3].position.x,
	//					vertexData[start + 3].position.y,
	//					vertexData[start + 3].position.z
	//				}
	//			};
	//
	//			// 頂点にデータを入力する。b
	//			vertexData[start + 4] = {
	//				{
	//					cosLatEvery * cosLon,
	//					sinLatEvery,
	//					cosLatEvery * sinLon,
	//					1.0f
	//				},
	//				{
	//					static_cast<float>(lonIndex) / static_cast<float>(kSubdivision),
	//					1.0f - static_cast<float>(latIndex + 1) / static_cast<float>(kSubdivision)
	//				},
	//				{
	//					vertexData[start + 4].position.x,
	//					vertexData[start + 4].position.y,
	//					vertexData[start + 4].position.z
	//				}
	//			};
	//
	//			// 頂点データを入力する。d
	//			vertexData[start + 5] = {
	//				{
	//					cosLatEvery * cosLonEvery,
	//					sinLatEvery,
	//					cosLatEvery * sinLonEvery,
	//					1.0f
	//				},
	//				{
	//					static_cast<float>(lonIndex + 1) / static_cast<float>(kSubdivision),
	//					1.0f - static_cast<float>(latIndex + 1) / static_cast<float>(kSubdivision)
	//				},
	//				{
	//					vertexData[start + 5].position.x,
	//					vertexData[start + 5].position.y,
	//					vertexData[start + 5].position.z
	//				}
	//			};
	//#pragma endregion
	//
	//			vertexData[start].normal = Vec3(cosLat * cosLon, sinLat, cosLat * sinLon).Normalize();
	//			vertexData[start + 1].normal = Vec3(cosLatEvery * cosLon, sinLatEvery, cosLatEvery * sinLon).Normalize();
	//			vertexData[start + 2].normal = Vec3(cosLat * cosLonEvery, sinLat, cosLat * sinLonEvery).Normalize();
	//			vertexData[start + 3].normal = Vec3(cosLat * cosLonEvery, sinLat, cosLat * sinLonEvery).Normalize();
	//			vertexData[start + 4].normal = Vec3(cosLatEvery * cosLon, sinLatEvery, cosLatEvery * sinLon).Normalize();
	//			vertexData[start + 5].normal = Vec3(cosLatEvery * cosLonEvery, sinLatEvery, cosLatEvery * sinLonEvery).Normalize();
	//		}
	//	}

	//==============================
	// Texture1
	//==============================
	// Textureを読んで転送する
	DirectX::ScratchImage mipImages = LoadTexture(modelData.material.textureFilePath);
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	textureResourceUvChecker_ = CreateTextureResource(device_.Get(), metadata).Get();
	UploadTextureData(textureResourceUvChecker_.Get(), mipImages);

	// metaDataを下にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = static_cast<UINT>(metadata.mipLevels);

	// SRVを作成するDescriptorHeapの場所を決める
	textureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap_.Get(), descriptorSizeSRV, 1);
	textureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap_.Get(), descriptorSizeSRV, 1);

	// SRVの生成
	device_->CreateShaderResourceView(textureResourceUvChecker_.Get(), &srvDesc, textureSrvHandleCPU);

	//==============================
	// Texture2 SRV
	//==============================
	// 2枚目のTextureを読んで転送する
	DirectX::ScratchImage mipImages2 = LoadTexture("../../../Resources/Textures/Debugempty.png");
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	textureResourceMonsterBall_ = CreateTextureResource(device_.Get(), metadata2).Get();
	UploadTextureData(textureResourceMonsterBall_.Get(), mipImages2);

	// metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2 = {};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc2.Texture2D.MipLevels = static_cast<UINT>(metadata2.mipLevels);

	// SRVを作成するDescriptorHeapの場所を決める
	textureSrvHandleCPU2 = GetCPUDescriptorHandle(srvDescriptorHeap_.Get(), descriptorSizeSRV, 2);
	textureSrvHandleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap_.Get(), descriptorSizeSRV, 2);
	// SRVの生成
	device_->CreateShaderResourceView(textureResourceMonsterBall_.Get(), &srvDesc2, textureSrvHandleCPU2);

	/* ViewportとScissor(シザー) */
	// ビューポート
	// クライアント領域のサイズと一緒にして画面全体に表示
	viewport_.Width = kClientWidth;
	viewport_.Height = kClientHeight;
	viewport_.TopLeftX = 0;
	viewport_.TopLeftY = 0;
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;

	// シザー矩形
	// 基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect_.left = 0;
	scissorRect_.right = kClientWidth;
	scissorRect_.top = 0;
	scissorRect_.bottom = kClientHeight;

	// Sprite用の頂点リソースを作る
	vertexResourceSprite_ = CreateBufferResource(device_.Get(), sizeof(VertexData) * 6);

	// 頂点バッファービューを作成する
	vertexBufferViewSprite_ = {};
	// リソースの先頭アドレスから使う
	vertexBufferViewSprite_.BufferLocation = vertexResourceSprite_->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferViewSprite_.SizeInBytes = sizeof(VertexData) * 6;
	// 1頂点あたりのサイズ
	vertexBufferViewSprite_.StrideInBytes = sizeof(VertexData);

	vertexDataSprite = nullptr;
	vertexResourceSprite_->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));

	// 1枚目の三角形
	vertexDataSprite[0].position = { 0.0f, 360.0f, 0.0f, 1.0f }; // 左下
	vertexDataSprite[0].texcoord = { 0.0f, 1.0f };
	vertexDataSprite[0].normal = { 0.0f, 0.0f, -1.0f };
	vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[1].texcoord = { 0.0f, 0.0f };
	vertexDataSprite[1].normal = { 0.0f, 0.0f, -1.0f };
	vertexDataSprite[2].position = { 640.0f, 360.0f, 0.0f, 1.0f };
	vertexDataSprite[2].texcoord = { 1.0f, 1.0f };
	vertexDataSprite[2].normal = { 0.0f, 0.0f, -1.0f };
	// 2枚目の三角形
	vertexDataSprite[3].position = { 0.0f, 0.0f, 0.0f, 1.0f }; // 左上
	vertexDataSprite[3].texcoord = { 0.0f, 0.0f };
	vertexDataSprite[3].normal = { 0.0f, 0.0f, -1.0f };
	vertexDataSprite[4].position = { 640.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[4].texcoord = { 1.0f, 0.0f };
	vertexDataSprite[4].normal = { 0.0f, 0.0f, -1.0f };
	vertexDataSprite[5].position = { 640.0f, 360.0f, 0.0f, 1.0f };
	vertexDataSprite[5].texcoord = { 1.0f, 1.0f };
	vertexDataSprite[5].normal = { 0.0f, 0.0f, -1.0f };

	// インデックスリソース
	indexResourceSprite_ = CreateBufferResource(device_.Get(), sizeof(uint32_t) * 6);

	// リソースの先頭アドレスから使う
	indexBufferViewSprite_.BufferLocation = indexResourceSprite_->GetGPUVirtualAddress();
	// 使用するリソースのサイズはインデックス6つ分のサイズ
	indexBufferViewSprite_.SizeInBytes = sizeof(uint32_t) * 6;
	// インデックスはuint32_tとする
	indexBufferViewSprite_.Format = DXGI_FORMAT_R32_UINT;

	// インデックスリソースにデータを書き込む
	uint32_t* indexDataSprite = nullptr;
	indexResourceSprite_->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
	indexDataSprite[0] = 0;
	indexDataSprite[1] = 1;
	indexDataSprite[2] = 2;
	indexDataSprite[3] = 1;
	indexDataSprite[4] = 4;
	indexDataSprite[5] = 5;

	// Sprite用のTransformationMatrix用のリソースを作る。Mat4 1つ分のサイズを用意する
	transformationMatrixResourceSprite_ = CreateBufferResource(device_.Get(), sizeof(TransformationMatrix));
	// データを書き込む
	transformationMatrixDataSprite_ = nullptr;

	materialResourceSprite_ = CreateBufferResource(device_.Get(), sizeof(Material));
	// マテリアルにデータを書き込む
	materialDataSprite_ = nullptr;
	// 書き込むためのアドレスを取得
	materialResourceSprite_->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite_));
	*materialDataSprite_ = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白
	// スプライトはライティングしない
	materialDataSprite_->enableLighting = false;
	materialDataSprite_->uvTransform = Mat4::Identity();

	// 書き込むためのアドレスを取得
	transformationMatrixResourceSprite_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite_));
	// 単位行列を書き込んでおく
	transformationMatrixDataSprite_->wvp = Mat4::Identity();

	transformSprite_ = {
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};

	uvTransformSprite_ = {
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 3.0f},
		{5.0f, 5.0f, 0.0f}
	};
}

void DirectX12::PreRender() {
	/* コマンドを積み込んで確定させる */
	// これから書き込むバックバッファのインデックスを取得
	UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

	/* TransitionBarrierを張る コード */
	// TransitionBarrierの設定
	D3D12_RESOURCE_BARRIER barrier = {};
	// 今回のバリアはTransition
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	// Noneにしておく
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	// バリアを張る対象のリソース。現在のバックバッファに対して行う
	barrier.Transition.pResource = swapChainResources_[backBufferIndex].Get();
	// 遷移前(現在)のResourceState
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	// 遷移語のResourceState
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	// TransitionBarrierを張る
	commandList_->ResourceBarrier(1, &barrier);

	// 描画先のRTVとDSVを設定する
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = GetCPUDescriptorHandle(dsvDescriptorHeap_.Get(), descriptorSizeDSV, 0);
	commandList_->OMSetRenderTargets(1, &rtvHandles_[backBufferIndex], false, &dsvHandle);
	// 指定した深度で画面全体をクリアする
	commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	// 指定した色で画面全体をクリアする
	float clearColor[] = { 0.89f, 0.5f, 0.03f, 1.0f }; // RGBAの順
	commandList_->ClearRenderTargetView(rtvHandles_[backBufferIndex], clearColor, 0, nullptr);

	// 描画用のDescriptorHeapの設定
	ComPtr<ID3D12DescriptorHeap> descriptorHeaps[] = { srvDescriptorHeap_.Get() };
	commandList_->SetDescriptorHeaps(1, descriptorHeaps->GetAddressOf());

	/* コマンドを積む */
	commandList_->RSSetViewports(1, &viewport_); // viewportを設定
	commandList_->RSSetScissorRects(1, &scissorRect_); // Scissorを設定
	// RootSignatureを設定。PSOに設定しているけど別途設定が必要
	commandList_->SetGraphicsRootSignature(rootSignature_.Get());
	commandList_->SetPipelineState(graphicsPipelineState_.Get()); // PSOを設定
	commandList_->IASetVertexBuffers(0, 1, &vertexBufferViewMesh_); // VBVを設定

	/* CBVを設定する */
	// マテリアルCBufferの場所を設定
	commandList_->SetGraphicsRootConstantBufferView(0, materialResourceMesh_->GetGPUVirtualAddress());

	// wvp用のCBufferの場所を確定
	commandList_->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceMesh_->GetGPUVirtualAddress());

	// SRVのDescriptorTableの先頭を設定。2はrootParameter[2]である。
	commandList_->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);

	// DirectionalLight rootparam = 3
	commandList_->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());

	// 形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えておけば良い
	commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	// 描画! (DrawCall/ドローコール)。3頂点で1つのインスタンス。インスタンスについては今後
	commandList_->DrawInstanced(static_cast<UINT>(modelData.vertices.size()), 1, 0, 0);

	// ==============================
	// スプライト
	// ==============================

	// Spriteの描画。変更が必要なものだけ変更する
	commandList_->IASetVertexBuffers(0, 1, &vertexBufferViewSprite_); // VBVを設定

	commandList_->IASetIndexBuffer(&indexBufferViewSprite_); // IBVを設定
	// マテリアルCBufferの場所を設定
	commandList_->SetGraphicsRootConstantBufferView(0, materialResourceSprite_->GetGPUVirtualAddress());
	// TransformationMatrixCBufferの場所を設定
	commandList_->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite_->GetGPUVirtualAddress());
	// SRVのDescriptorTableの設定
	commandList_->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
	// 描画!(DrawCall/ドローコール)6個のインデックスを使用し1つのインスタンスを描画。その他は当面0で良い
	//commandList_->DrawIndexedInstanced(6, 1, 0, 0, 0);
	//commandList_->DrawInstanced(6, 1, 0, 0);

	// 実際のcommandListのImGuiの描画コマンドを積む
	//ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList_.Get());

	/* 画面表示できるようにする */
	// 画面に描く処理はすべて終わり、画面に映すので、状態を遷移
	// 今回はRenderTargetからPresentにする
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	// TransitionBarrierを張る
	commandList_->ResourceBarrier(1, &barrier);

	// コマンドリストの内容を確定させる。すべてのコマンドを積んでからCloseすること
	hr_ = commandList_->Close();
	assert(SUCCEEDED(hr_));

	/* コマンドをキックする */
	// GPUにコマンドリストの実行を行わせる
	ID3D12CommandList* commandLists[] = { commandList_.Get() };
	commandQueue_->ExecuteCommandLists(1, commandLists);
	// GPUとOSに画面の交換を行うよう通知する
	swapChain_->Present(1, 0);

	/* GPUにSignal(シグナル)を送る */
	// Fenceの値を更新
	fenceValue_++;
	// GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
	commandQueue_->Signal(fence_.Get(), fenceValue_);

	// Fenceの値が指定したSignal値にたどり着いているか確認する
	// GetCompletedValueの初期値はFence作成時に渡した初期値
	if (fence_->GetCompletedValue() < fenceValue_) {
		// 指定したSignalにたどり着いていないので、たどり着くまで待つようにイベントを設定する
		fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
		// イベント待つ
		WaitForSingleObject(fenceEvent_, INFINITE);
	}

	// 次のフレーム用のコマンドリストを準備
	hr_ = commandAllocator_->Reset();
	assert(SUCCEEDED(hr_));
	hr_ = commandList_->Reset(commandAllocator_.Get(), nullptr);
	assert(SUCCEEDED(hr_));

	Mat4 uvTransformMatrix = Mat4::Scale(uvTransformSprite_.scale);
	uvTransformMatrix = uvTransformMatrix * Mat4::RotateZ(uvTransformSprite_.rotate.z);
	uvTransformMatrix = uvTransformMatrix * Mat4::Translate(uvTransformSprite_.translate);
	materialDataSprite_->uvTransform = uvTransformMatrix;

	// Sprite用のWorldViewProjectionMatrixを作る
	Mat4 worldMatrixSprite = Mat4::Affine(transformSprite_.scale, transformSprite_.rotate, transformSprite_.translate);
	Mat4 viewMatrixSprite = Mat4::Identity();
	Mat4 projectionMatrixSprite = Mat4::MakeOrthographicMat(0.0f, 0.0f, static_cast<float>(kClientWidth),
		static_cast<float>(kClientHeight), 0.0f,
		100.0f);
	TransformationMatrix worldViewProjectionMatrixSprite = {
		worldMatrixSprite * viewMatrixSprite * projectionMatrixSprite,
		worldMatrixSprite
	};
	*transformationMatrixDataSprite_ = worldViewProjectionMatrixSprite;
}

void DirectX12::CreateCommandQueue() {
	constexpr D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		0,
		D3D12_COMMAND_QUEUE_FLAG_NONE,
		0
	};
	hr_ = device_->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue_)); // コマンドキューを生成する
	assert(SUCCEEDED(hr_)); // コマンドキューの生成がうまくいかなかったので起動できない
	Console::Print("Complete create CommandQueue.\n");
}

void DirectX12::CreateCommandList() {
	// コマンドアロケータを生成する
	hr_ = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
	// コマンドアロケータの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr_));

	// コマンドリストを生成する
	hr_ = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr,
		IID_PPV_ARGS(&commandList_));
	// コマンドリストの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr_));
	Console::Print("Complete create CommandList.\n");
}

void DirectX12::CreateSwapChain() {
	// スワップチェーンを生成する
	swapChainDesc_.BufferCount = kFrameBufferCount; // ダブルバッファ
	//swapChainDesc_.Width = windowConfig.clientWidth; // 画面の幅。ウィンドウのクライアント領域を同じものにしておく
	//swapChainDesc_.Height = windowConfig.clientHeight; // 画面の高さ。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc_.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式
	swapChainDesc_.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとして利用する
	swapChainDesc_.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // モニタにうつしたら、中身を破棄
	swapChainDesc_.SampleDesc.Count = 1; // マルチサンプルしない

	// コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	hr_ = dxgiFactory_->CreateSwapChainForHwnd(
		commandQueue_.Get(),
		window_->GetWindowHandle(),
		&swapChainDesc_,
		nullptr,
		nullptr,
		reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf())
	);
	assert(SUCCEEDED(hr_));

	Console::Print("Complete create SwapChain.\n");
}

ComPtr<ID3D12DescriptorHeap> DirectX12::CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_TYPE heapType,
	const UINT numDescriptors,
	const bool shaderVisible) const {
	ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	if (shaderVisible) {
		descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	} else {
		descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	}
	const HRESULT hr = device_->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));
	Console::Print(std::format("{:08x}\n", hr));
	return descriptorHeap;
}

void DirectX12::GetResourceFromSwapChain() {
	swapChainResources_.resize(kFrameBufferCount); // フレームバッファ数でリサイズ

	for (unsigned int i = 0; i < kFrameBufferCount; ++i) {
		hr_ = swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainResources_[i])); // SwapChainからResourceを引っ張ってくる
		assert(SUCCEEDED(hr_)); // うまく取得できなければ起動できない
	}
}

void DirectX12::CreateRTV() {
	// RTVの設定
	rtvDesc_.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 出力結果を書き込む
	rtvDesc_.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2Dテクスチャとして書き込む

	for (unsigned int i = 0; i < kFrameBufferCount; ++i) {
		rtvHandles_[i] = GetCPUDescriptorHandle(rtvDescriptorHeap_.Get(), descriptorSizeRTV, i);
		device_->CreateRenderTargetView(swapChainResources_[i].Get(), &rtvDesc_, rtvHandles_[i]);
	}
	Console::Print("Complete create RenderTargetView.\n");
}

void DirectX12::InitializeDxc() {
	/* DXCの初期化 */
	// dxcCompilerを初期化
	hr_ = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
	assert(SUCCEEDED(hr_));
	hr_ = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
	assert(SUCCEEDED(hr_));

	// 現時点でincludeはしないが、includeに対応するための設定を行っておく
	hr_ = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
	assert(SUCCEEDED(hr_));
}

void DirectX12::EnableDebugLayer() {
	ComPtr<ID3D12Debug6> debugController;

	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		// デバッグレイヤーを有効化する
		debugController->EnableDebugLayer();
		// 更にGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
}

void DirectX12::CreateDevice() {
	// ==============================
	// Create Factory
	// ==============================
	UINT dxgiFlags = DXGI_CREATE_FACTORY_DEBUG; // デバッグ有効化時にはDXGI_CREATE_FACTORY_DEBUG フラグを付与する
	hr_ = CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&dxgiFactory_));
	assert(SUCCEEDED(hr_));

	// ==============================
	// ハードウェアアダプターの検索
	// ==============================
	ComPtr<IDXGIAdapter4> useAdapter;
	for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
		IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC3 adapterDesc;
		hr_ = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr_));
		// ソフトウェアアダプタでなければ採用!
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			Console::Print(StrUtils::ToString(std::format(L"Use Adapter : {}\n", adapterDesc.Description))); // 採用したアダプタの情報をログに出力。
			break;
		}
		useAdapter = nullptr; // ソフトウェアアダプタの場合は見なかったことにする
	}
	assert(useAdapter != nullptr); // 適切なアダプタが見つからなかったので起動できない

	// ==============================
	// D3D12デバイスの作成
	// ==============================
	// 機能レベルとログ出力用の文字列
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0
	};
	const char* featureLevelStrings[] = { "12.2", "12.1", "12.0" };

	// 高い順に生成できるか試していく
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		hr_ = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device_)); // 採用したアダプターでデバイスを生成
		// 指定した機能レベルでデバイスが生成できたかを確認
		if (SUCCEEDED(hr_)) {
			Console::Print(std::format("FeatureLevel : {}\n", featureLevelStrings[i])); // 生成できたのでログ出力を行ってループを抜ける
			break;
		}
	}
	assert(device_ != nullptr); // デバイスの生成がうまくいかなかったので起動できない
	Console::Print("Complete create D3D12Device.\n"); // 初期化完了のログを出す
}

void DirectX12::SetInfoQueueBreakOnSeverity() const {
	ID3D12InfoQueue* infoQueue = nullptr;
	if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		// やばいエラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		// エラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		// 警告時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		// 抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] = {
			// Windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
			// https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};

		// 抑制するレベル
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		// 指定したメッセージの表示を抑制する
		infoQueue->PushStorageFilter(&filter);

		// 解放
		infoQueue->Release();
	}
}

IDxcBlob* DirectX12::CompileShader(const std::wstring& filePath, const wchar_t* profile, IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler, IDxcIncludeHandler* includeHandler) {
	/* 1. hlslファイルを読む */
	// これからシェーダーをコンパイルする旨をログに出す
	Console::Print(StrUtils::ToString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)), kConsoleColorWait);
	// hlslファイルを読む
	IDxcBlobEncoding* shaderSource = nullptr;
	hr_ = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	// 読めなかったら止める
	assert(SUCCEEDED(hr_));
	// 読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8; // UTF8の文字コードであることを通知

	/* 2. Compileする */
	LPCWSTR arguments[] = {
		filePath.c_str(), // コンパイル対象のhlslファイル名
		L"-E", L"main", // エントリーポイントの指定。基本的にmain以外にはしない
		L"-T", profile, // ShaderProfileの設定
		L"-Zi", L"-Qembed_debug", // デバッグ用の情報を埋め込む
		L"-Od", // 最適化を外しておく
		L"-Zpr", // メモリレイアウトは行優先
	};

	// 実際にShaderをコンパイルする
	IDxcResult* shaderResult = nullptr;
	hr_ = dxcCompiler->Compile(
		&shaderSourceBuffer, // 読み込んだファイル
		arguments, // コンパイルオプション
		_countof(arguments), // コンパイルオプションの数
		includeHandler, // includeが含まれた諸々
		IID_PPV_ARGS(&shaderResult) // コンパイル結果
	);

	// コンパイルエラーではなくdxcが起動できないなど致命的な状況
	assert(SUCCEEDED(hr_));

	/* 3. 警告・エラーが出ていないか確認する */
	// 警告・エラーが出てきたらログに出して止める
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Console::Print(shaderError->GetStringPointer(), kConsoleColorError);
		// 警告・エラーダメゼッタイ
		assert(false);
	}

	/* 4. Compile結果を受け取って返す */
	// コンパイル結果から実行用のバイナリ部分を取得
	IDxcBlob* shaderBlob = nullptr;
	hr_ = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr_));
	// 成功したらログを出す
	Console::Print(StrUtils::ToString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)), kConsoleColorCompleted);
	// もう使わないリソースを開放
	shaderSource->Release();
	shaderResult->Release();
	// 実行用のバイナリを返却
	return shaderBlob;
}

ComPtr<ID3D12Resource> DirectX12::CreateBufferResource(const ComPtr<ID3D12Device>& device, const size_t sizeInBytes) {
	// リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties = {};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // UploadHeapを使う

	D3D12_RESOURCE_DESC resourceDesc = {};
	// バッファリソース。テクスチャの場合はまた別の設定をする
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes; // リソースのサイズ
	// バッファの場合はこれらは1にする決まり
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	// バッファの場合はこれにする決まり
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// 実際にリソースを作る
	ComPtr<ID3D12Resource> bufferResource = nullptr;
	hr_ = device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&bufferResource)
	);

	assert(SUCCEEDED(hr_));
	return bufferResource;
}

ComPtr<ID3D12Resource> DirectX12::CreateDepthStencilTextureResource(int32_t width, int32_t height) const {
	// 生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Width = width; // Textureの幅
	resourceDesc.Height = height; // Textureの高さ
	resourceDesc.MipLevels = 1; // mipmapの数
	resourceDesc.DepthOrArraySize = 1; // 奥行きor配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // DepthStencilとして利用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // DepthStencilとして使う通知

	// 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // vRAM上に作る

	// 深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.DepthStencil.Depth = 1.0f; // 1.0f (最大値)でクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// Resourceの生成
	ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device_->CreateCommittedResource(
		&heapProperties, // Heapの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし。
		&resourceDesc, // Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度値を書き込む状態にしておく
		&depthClearValue, // Clear最適値
		IID_PPV_ARGS(&resource)
	); // 作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));
	Console::Print(std::format("{:08x}\n", hr));

	return resource;
}

Transform DirectX12::TransformSprite() const {
	return transformSprite_;
}

void DirectX12::SetTransformSprite(const Transform& transformSprite) {
	transformSprite_ = transformSprite;
}

Transform DirectX12::GetUvTransformSprite() const {
	return uvTransformSprite_;
}

void DirectX12::SetUvTransformSprite(const Transform& uvTransform) {
	uvTransformSprite_ = uvTransform;
}

DirectionalLight* DirectX12::GetDirectionalLightData() const {
	return directionalLightData_;
}

VertexData* DirectX12::GetVertexDataSphere() const {
	return vertexData;
}

void DirectX12::SetTransformationMatrixData(const TransformationMatrix& newtTransformationMatrix) const {
	*transformationMatrixDataMesh_ = newtTransformationMatrix;
}

void DirectX12::SetMaterialData(const Material& matData) const {
	*materialDataMesh_ = matData;
}

Material* DirectX12::GetMaterialData() const {
	return materialDataMesh_;
}

ID3D12Device* DirectX12::Device() const {
	return device_.Get();
}

DXGI_SWAP_CHAIN_DESC1 DirectX12::SwapChainDesc() const {
	return swapChainDesc_;
}

D3D12_RENDER_TARGET_VIEW_DESC DirectX12::RtvDesc() const {
	return rtvDesc_;
}

ID3D12DescriptorHeap* DirectX12::SrvDescriptorHeap() const {
	return srvDescriptorHeap_.Get();
}

DirectX::ScratchImage DirectX12::LoadTexture(const std::string& filePath) {
	// テクスチャファイルを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image = {};
	std::wstring filePathW = StrUtils::ToString(filePath);
	HRESULT hr = LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	// ミップマップの作成
	DirectX::ScratchImage mipImages = {};
	hr = GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0,
		mipImages);
	assert(SUCCEEDED(hr));

	// ミップマップ付きのデータを返す
	return mipImages;
}

ComPtr<ID3D12Resource> DirectX12::CreateTextureResource(const ComPtr<ID3D12Device>& device,
	const DirectX::TexMetadata& metadata) {
	// metadataを下にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Width = static_cast<UINT>(metadata.width); // Textureの幅
	resourceDesc.Height = static_cast<UINT>(metadata.height); // Textureの高さ
	resourceDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels); // mipmapの数
	resourceDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize); // 奥行き or 配列Textureの配列数
	resourceDesc.Format = metadata.format; // TextureのFormat
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定。
	resourceDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension); // Textureの次元数。普段使っているのは2次元

	// 利用するHeapの設定。非常に特殊な運用。02_04exで一般的なケース版がある
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM; // 細かい設定を行う
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK; // WriteBackポリシーでCPUアクセス可能
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0; // プロセッサの近くに配置

	// Resourceの生成
	ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties, // Heapの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし。
		&resourceDesc, // Resourceの設定
		D3D12_RESOURCE_STATE_GENERIC_READ, // 初回のResourceState。Textureは基本読むだけ
		nullptr, // Clear最適値。使わないのでnullptr
		IID_PPV_ARGS(&resource) // 作成するResourceポインタへのポインタ
	);
	assert(SUCCEEDED(hr));
	Console::Print(std::format("{:08x}\n", hr));

	return resource;
}

void DirectX12::UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages) {
	// Meta情報を取得
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	// 全MipMapについて
	for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {
		// MipMapLevelを指定して各Imageを取得
		const DirectX::Image* img = mipImages.GetImage(mipLevel, 0, 0);
		// Textureに転送
		HRESULT hr = texture->WriteToSubresource(
			static_cast<UINT>(mipLevel),
			nullptr, // 全領域へコピー
			img->pixels, // 元データアドレス
			static_cast<UINT>(img->rowPitch), // 1ラインサイズ
			static_cast<UINT>(img->slicePitch) // 1枚サイズ
		);
		assert(SUCCEEDED(hr));
		Console::Print(std::format("{:08x}\n", hr));
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectX12::GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap,
	const uint32_t descriptorSize, const uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += static_cast<unsigned long long>(descriptorSize) * index;
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectX12::GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap,
	const uint32_t descriptorSize, const uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += static_cast<unsigned long long>(descriptorSize) * index;
	return handleGPU;
}

bool* DirectX12::GetUseMonsterBall() {
	return &useMonsterBall;
}

#endif