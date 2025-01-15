#include "Texture.h"

#include <Lib/Console/Console.h>
#include <Renderer/D3D12Utils.h>

Texture::Texture(
	const std::string& filePath, ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
	SrvAllocator* srvAllocator
) {
	filePath_ = filePath;
	device_ = device;
	commandList_ = commandList;
	srvAllocator_ = srvAllocator;
	LoadFromFile(filePath);
	CreateTextureResource();
	UploadTextureData();
	CreateShaderResourceView();
}

void Texture::Unload() {
	textureResource_.Reset();
	uploadHeap_.Reset();
	srvCPUHandle_ = {};
	srvGPUHandle_ = {};
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetSrvCPUHandle() const {
	return srvCPUHandle_;
}

D3D12_GPU_DESCRIPTOR_HANDLE Texture::GetSrvGPUHandle() const {
	return srvGPUHandle_;
}

const DirectX::TexMetadata& Texture::GetMetaData() const {
	return metadata_;
}

ComPtr<ID3D12Resource> Texture::GetResource() const { return textureResource_; }

void Texture::LoadFromFile(const std::string& filePath) {
	std::wstring wFilePath(filePath.begin(), filePath.end());
	HRESULT hr = LoadFromWICFile(wFilePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, &metadata_, mipImages_);
	if (FAILED(hr)) {
		Console::Print("テクスチャの読み込みに失敗しました: " + filePath, { 1.0f, 0.0f, 0.0f, 1.0f });
		throw std::runtime_error("テクスチャの読み込みに失敗しました: " + filePath);
	}
}

void Texture::CreateTextureResource() {
	textureResource_ = D3D12Utils::CreateTextureResource(device_, metadata_);
}

void Texture::UploadTextureData() {
	uploadHeap_ = D3D12Utils::UploadTextureData(device_, commandList_, textureResource_, mipImages_);
}

void Texture::CreateShaderResourceView() {
	D3D12_CPU_DESCRIPTOR_HANDLE srvCPUHandle = srvAllocator_->Allocate();
	srvGPUHandle_ = srvAllocator_->GetGPUHandle(srvCPUHandle);
	D3D12Utils::CreateSRVForTexture2D(device_, textureResource_.Get(), metadata_, srvCPUHandle);
	srvCPUHandle_ = srvCPUHandle;
}

