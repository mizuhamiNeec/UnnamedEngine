#include "GameScene.h"

#include <format>
#include <fstream>

#ifdef _DEBUG
#include "imgui/imgui.h"
#endif

#include "../ImGuiManager/ImGuiManager.h"
#include "../Lib/Console/Console.h"
#include "../Lib/Console/ConVar.h"
#include "../Lib/Console/ConVars.h"
#include "../Lib/Math/MathLib.h"
#include "../Lib/Structs/Structs.h"
#include "../Model/Model.h"
#include "../Object3D/Object3D.h"
#include "../Renderer/PipelineState.h"
#include "../Renderer/RootSignature.h"
#include "../Renderer/RootSignatureManager.h"
#include "../Renderer/VertexBuffer.h"
#include "../Sprite/Sprite.h"
#include "../Sprite/SpriteCommon.h"
#include "../TextureManager/TextureManager.h"
#include "../Engine/Model/ModelManager.h"

// TODO : メンバに移動しよう

class RootSignatureManager;
VertexBuffer* vertexBuffer;
ConstantBuffer* transformation;

Material* material;
ConstantBuffer* materialResource;

ConstantBuffer* directionalLightResource;
DirectionalLight* directionalLight;

RootSignature* rootSignature;
RootSignatureManager* rootSignatureManager;
PipelineState* pipelineState;

ModelData loadedModelData;
uint32_t modelTextureIndex;

D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU;
D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU;


MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
	MaterialData materialData; // 構築するMaterialData
	std::string line; // ファイルから読んだ1行を格納するもの
	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // とりあえず開けなかったら止める

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		// identifierに応じた処理
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			// 連結してファイルパスにする
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}

	return materialData;
}

ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename) {
	ModelData modelData; // 構築するModelData
	std::vector<Vec4> positions; // 位置
	std::vector<Vec3> normals; // 法線
	std::vector<Vec2> texcoords; // テクスチャの座標
	std::string line;

	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open());

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier; // 先頭の識別子を読む

		if (identifier == "v") {
			Vec4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			positions.push_back(position);
		} else if (identifier == "vt") {
			Vec2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);
		} else if (identifier == "vn") {
			Vec3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);
		} else if (identifier == "f") {
			Vertex triangle[3];

			// 面は三角形限定。その他は未対応
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;
				// 頂点の要素へのIndexは[位置/UV/法線] で格納されているので、分解してIndexを取得する
				std::istringstream v(vertexDefinition);
				std::string index;
				uint32_t elementIndices[3] = { 0, 0, 0 };
				int element = 0;

				while (std::getline(v, index, '/') && element < 3) {
					if (!index.empty()) {
						elementIndices[element] = std::stoi(index);
					}
					++element;
				}

				// 要素へのIndexから、実際の要素の値を取得して、頂点を構築する
				Vec4 position = positions[elementIndices[0] - 1];
				Vec2 texcoord = elementIndices[1] ? texcoords[elementIndices[1] - 1] : Vec2{ 0.0f, 0.0f };
				Vec3 normal = elementIndices[2] ? normals[elementIndices[2] - 1] : Vec3{ 0.0f, 0.0f, 0.0f };

				position.x *= -1.0f;
				normal.x *= -1.0f;
				triangle[faceVertex] = { position, texcoord, normal };
			}
			// 頂点を逆順で登録することで、周り順を逆にする
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
		} else if (identifier == "mtllib") {
			// materialTemplateLibraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;
			// 基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}

	return modelData;
}

void GameScene::Init(D3D12* renderer, Window* window, SpriteCommon* spriteCommon, Object3DCommon* object3DCommon, ModelCommon* modelCommon) {
	renderer_ = renderer;
	window_ = window;
	spriteCommon_ = spriteCommon;
	object3DCommon_ = object3DCommon;
	modelCommon_ = modelCommon;

	transform_ = {
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};

	cameraTransform_ = {
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, -2.0f}
	};

#pragma region 頂点バッファ
	// モデルの読み込み
	loadedModelData = LoadObjFile("Resources/Models", "suzanne.obj");
	// 頂点リソースを作る
	vertexBuffer = new VertexBuffer(renderer_->GetDevice(), sizeof(Vertex) * loadedModelData.vertices.size(),
		sizeof(Vertex), loadedModelData.vertices.data());

	if (vertexBuffer) {
		Console::Print("頂点バッファの生成に成功.\n", kConsoleColorCompleted);
	}
#pragma endregion

#pragma region 定数バッファ

	transformation = new ConstantBuffer(renderer_->GetDevice(), sizeof(TransformationMatrix));

	materialResource = new ConstantBuffer(renderer_->GetDevice(), sizeof(Material));
	// マテリアルにデータを書き込む
	material = materialResource->GetPtr<Material>(); // 書き込むためのアドレスを取得
	*material = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白
	material->enableLighting = true;
	material->uvTransform = Mat4::Identity();

	// ---------------------------------------------------------------------------
	// Directional Light
	// ---------------------------------------------------------------------------
	directionalLightResource = new ConstantBuffer(renderer_->GetDevice(), sizeof(DirectionalLight));
	directionalLight = directionalLightResource->GetPtr<DirectionalLight>();
	directionalLight->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	const Vec3 dir = Vec3(-1.0f, -1.0f, 1.0f);
	directionalLight->direction = dir.Normalized();
	directionalLight->intensity = 1.0f;
#pragma endregion

#pragma region ルートシグネチャ
	rootSignatureManager = new RootSignatureManager(renderer->GetDevice());

	D3D12_DESCRIPTOR_RANGE descriptorRange[1];
	descriptorRange[0] = {
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV, // SRVを使う
		.NumDescriptors = 1, // 数は一つ
		.BaseShaderRegister = 0, // 0から始まる
		.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND // Offsetを自動計算
	};

	std::vector<D3D12_ROOT_PARAMETER> modelRootParameters(4);
	modelRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う。b0のbと一致する
	modelRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	modelRootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号0とバインド。b0の0と一致する。もしb11と紐づけたいなら11となる

	modelRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	modelRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	modelRootParameters[1].Descriptor.ShaderRegister = 0;

	modelRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	modelRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	modelRootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange; // Tableの中身の配列を指定
	modelRootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	modelRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	modelRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	modelRootParameters[3].Descriptor.ShaderRegister = 1; // レジスタ番号1を使う

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {
		{
			.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR, // バイリニアフィルタ
			.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP, // 0～1の範囲外をリピート
			.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER, // 比較しない
			.MaxLOD = D3D12_FLOAT32_MAX, // ありったけのMipmapを使う
			.ShaderRegister = 0, // レジスタ番号0を使う
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL // PixelShaderで使う
		}
	};

	rootSignatureManager->CreateRootSignature("Object3d", modelRootParameters, staticSamplers,
		_countof(staticSamplers));

	if (rootSignatureManager->Get("Object3d")) {
		Console::Print("ルートシグネチャの生成に成功.\n", kConsoleColorCompleted);
	}
#pragma endregion

#pragma region パイプラインステート
	// 3D
	pipelineState = new PipelineState(D3D12_CULL_MODE_BACK, D3D12_FILL_MODE_SOLID);
	pipelineState->SetInputLayout(Vertex::inputLayout);
	pipelineState->SetRootSignature(rootSignatureManager->Get("Object3d"));
	pipelineState->SetVS(L"./Resources/Shaders/Object3d.VS.hlsl");
	pipelineState->SetPS(L"./Resources/Shaders/Object3d.PS.hlsl");


	pipelineState->Create(renderer->GetDevice());
	if (pipelineState) {
		Console::Print("パイプラインステートの生成に成功.\n", kConsoleColorCompleted);
	}
#pragma endregion

#pragma region スプライト類
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/empty.png");
	TextureManager::GetInstance()->LoadTexture("./Resources/Textures/uvChecker.png");

	modelTextureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath("./Resources/Textures/empty.png");

	for (uint32_t i = 0; i < 5; ++i) {
		Sprite* sprite = new Sprite();
		if (i % 2 == 0) {
			sprite->Init(spriteCommon_, "./Resources/Textures/empty.png");
		} else {
			sprite->Init(spriteCommon_, "./Resources/Textures/uvChecker.png");
		}
		sprite->SetPos({ 256.0f * static_cast<float>(i), 0.0f, 0.0f });
		sprite->SetSize({ 256.0f, 256.0f, 1.0f });
		sprites_.push_back(sprite);
	}
#pragma endregion

#pragma region 3Dオブジェクト類

	// .objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("axis.obj");

	object3D_ = std::make_unique<Object3D>();
	object3D_->Init(object3DCommon_, modelCommon_);
	// 初期化済みの3Dオブジェクトにモデルを紐づける
	object3D_->SetModel("axis.obj");

#pragma endregion
}

void GameScene::Update() {
	transform_.rotate.y += 0.003f;
	Mat4 worldMat = Mat4::Affine(transform_.scale, transform_.rotate, transform_.translate);
	Mat4 cameraMat = Mat4::Affine(cameraTransform_.scale, cameraTransform_.rotate, cameraTransform_.translate);
	Mat4 viewMat = cameraMat.Inverse();
	Mat4 projectionMat = Mat4::PerspectiveFovMat(
		fov_ * Math::deg2Rad, // FieldOfView 90 degree!!
		static_cast<float>(window_->GetClientWidth()) / static_cast<float>(window_->GetClientHeight()),
		0.01f,
		1000.0f
	);
	Mat4 worldViewProjMat = worldMat * viewMat * projectionMat;

	TransformationMatrix* ptr = transformation->GetPtr<TransformationMatrix>();
	ptr->wvp = worldViewProjMat;
	ptr->world = worldMat;

#ifdef _DEBUG
	ImGui::Begin("Sprites");
	for (uint32_t i = 0; i < sprites_.size(); ++i) {
		if (ImGui::CollapsingHeader(std::format("Sprite {}", i).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			Vec3 pos = sprites_[i]->GetPos();
			Vec3 rot = sprites_[i]->GetRot();
			Vec3 size = sprites_[i]->GetSize();
			Vec2 anchor = sprites_[i]->GetAnchorPoint();
			Vec4 color = sprites_[i]->GetColor();
			if (ImGui::DragFloat3(std::format("pos##sprite{}", i).c_str(), &pos.x, 1.0f)) {
				sprites_[i]->SetPos(pos);
			}
			if (ImGui::DragFloat3(std::format("rot##sprite{}", i).c_str(), &rot.x, 0.01f)) {
				sprites_[i]->SetRot(rot);
			}
			if (ImGui::DragFloat3(std::format("scale##sprite{}", i).c_str(), &size.x, 0.01f)) {
				sprites_[i]->SetSize(size);
			}
			if (ImGui::DragFloat2(std::format("anchorPoint#sprite{}", i).c_str(), &anchor.x, 0.01f)) {
				sprites_[i]->SetAnchorPoint(anchor);
			}
			if (ImGui::ColorEdit4(std::format("color##sprite{}", i).c_str(), &color.x)) {
				sprites_[i]->SetColor(color);
			}
			ImGui::Separator();

			if (ImGui::TreeNode(std::format("UV##sprite{}", i).c_str())) {
				Vec2 uvPos = sprites_[i]->GetUvPos();
				Vec2 uvSize = sprites_[i]->GetUvSize();
				float uvRot = sprites_[i]->GetUvRot();
				if (ImGui::DragFloat2(std::format("pos##uv{}", i).c_str(), &uvPos.x, 0.01f)) {
					sprites_[i]->SetUvPos(uvPos);
				}
				if (ImGui::DragFloat2(std::format("scale##uv{}", i).c_str(), &uvSize.x, 0.01f)) {
					sprites_[i]->SetUvSize(uvSize);
				}
				if (ImGui::SliderAngle(std::format("rotZ##uv{}", i).c_str(), &uvRot)) {
					sprites_[i]->SetUvRot(uvRot);
				}

				ImGui::TreePop();
			}
		}
	}
	ImGui::End();

	{
		ImGui::Begin("Loaded Texture");
		ImVec2 imageWindowSize = ImGui::GetContentRegionAvail();
		static int index = 0;
		float sliderHeight = ImGui::GetFrameHeightWithSpacing(); // スライダーの高さを取得
		float imageSize = (imageWindowSize.x < (imageWindowSize.y - sliderHeight))
			? imageWindowSize.x
			: (imageWindowSize.y - sliderHeight);

		ImGui::SliderInt("index", &index, 0, TextureManager::GetInstance()->GetLoadedTextureCount());

		/*int frameIndex = renderer_->GetSwapChain()->GetCurrentBackBufferIndex();
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = renderer_->GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();
		gpuHandle.ptr += (renderer_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 3) * frameIndex;*/

		ImTextureID tex = reinterpret_cast<ImTextureID>(renderer_->GetSRVDescriptorHeap()->
			GetGPUDescriptorHandleForHeapStart().ptr + (renderer_
				->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * index));
		//ImTextureID tex = reinterpret_cast<ImTextureID>(gpuHandle.ptr);
		ImGui::Image(tex, { imageSize, imageSize });
		ImGui::End();
	}

	{
		ImGui::Begin("Texture 0");
		static bool texindex = false;
		if (ImGui::Checkbox("Toggle Texture", &texindex)) {
			if (texindex) {
				sprites_[0]->ChangeTexture("./Resources/Textures/uvChecker.png");
			} else {
				sprites_[0]->ChangeTexture("./Resources/Textures/debugempty.png");
			}
		}
		ImGui::End();
	}
#endif

	object3D_->Update();

	for (Sprite* sprite : sprites_) {
		sprite->Update();
	}

#ifdef _DEBUG
	ImGui::Begin("Window");

	if (ImGui::CollapsingHeader("Camera")) {
		ImGui::DragFloat3("pos##cam", &cameraTransform_.translate.x, 0.01f);
		ImGui::DragFloat3("rot##cam", &cameraTransform_.rotate.x, 0.01f);
		ImGui::DragFloat("Fov##cam", &fov_, 1.0f);
	}

	if (ImGui::CollapsingHeader("transform")) {
		ImGui::DragFloat3("pos##trans", &transform_.translate.x, 0.01f);
		ImGui::DragFloat3("rot##trans", &transform_.rotate.x, 0.01f);
		ImGui::DragFloat3("scale##trans", &transform_.scale.x, 0.01f);
	}

	ImGui::End();

#pragma region cl_showpos
	if (ConVars::GetInstance().GetConVar("cl_showpos")->GetInt() == 1) {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });

		ImGuiWindowFlags windowFlags =
			ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings;

		ImVec2 windowPos = ImVec2(0.0f, 128.0f + 16.0f);
		ImVec2 windowSize = ImVec2(1080.0f, 80.0f);

		windowPos.x = ImGui::GetMainViewport()->Pos.x + windowPos.x;
		windowPos.y = ImGui::GetMainViewport()->Pos.y + windowPos.y;

		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

		ImGui::Begin("##cl_showpos", nullptr, windowFlags);

		ImVec2 textPos = ImGui::GetCursorScreenPos();

		ImDrawList* drawList = ImGui::GetWindowDrawList();

		float outlineSize = 1.0f;

		std::string text = std::format(
			"name: {}\n"
			"pos : {:.2f} {:.2f} {:.2f}\n"
			"rot : {:.2f} {:.2f} {:.2f}\n"
			"vel : {:.2f}\n",
			"unnamed",
			cameraTransform_.translate.x, cameraTransform_.translate.y, cameraTransform_.translate.z,
			cameraTransform_.rotate.x * Math::rad2Deg, cameraTransform_.rotate.y * Math::rad2Deg,
			cameraTransform_.rotate.z * Math::rad2Deg,
			0.0f
		);

		ImU32 textColor = IM_COL32(255, 255, 255, 255);
		ImU32 outlineColor = IM_COL32(0, 0, 0, 94);

		TextOutlined(
			drawList,
			textPos,
			text.c_str(),
			textColor,
			outlineColor,
			outlineSize
		);


		ImGui::PopStyleVar();

		ImGui::End();
	}
#pragma endregion

	ImGui::Begin("RenderMode");

	ImGui::End();
#endif
}

void GameScene::Render() {
	//----------------------------------------
	// オブジェクト3Dの描画設定
	object3DCommon_->Render();
	//----------------------------------------

	object3D_->Draw();

	//----------------------------------------
	// スプライト共通描画設定
	spriteCommon_->Render();
	//----------------------------------------

	for (Sprite* sprite : sprites_) {
		sprite->Draw();
	}
}

void GameScene::Shutdown() {
	for (Sprite* sprite : sprites_) {
		delete sprite;
	}

	spriteCommon_->Shutdown();
	object3DCommon_->Shutdown();

	delete vertexBuffer;
	delete transformation;
	delete materialResource;
	delete directionalLightResource;
	delete rootSignature;
	delete rootSignatureManager;
	delete pipelineState;
}
