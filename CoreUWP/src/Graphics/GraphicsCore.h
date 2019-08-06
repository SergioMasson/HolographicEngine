//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard
//

#pragma once

#include "GraphicsCommon.h"
#include "StereographicCamera.h"
#include "pch.h"

namespace Graphics
{
#ifndef RELEASE
	extern const GUID WKPDID_D3DDebugObjectName;
#endif
	void Initialize(void);
	void AttachHolographicSpace(winrt::Windows::Graphics::Holographic::HolographicSpace const& space);

	void AddHolographicCamera(winrt::Windows::Graphics::Holographic::HolographicCamera const& camera);

	void RemoveHolographicCamera(winrt::Windows::Graphics::Holographic::HolographicCamera const& camera);

	void Resize(uint32_t width, uint32_t height);
	void Terminate(void);
	void Shutdown(void);
	void Present(void);

	extern uint32_t g_DisplayWidth;
	extern uint32_t g_DisplayHeight;

	// Returns the number of elapsed frames since application start
	uint64_t GetFrameCount(void);

	// The amount of time elapsed during the last completed frame.  The CPU and/or
	// GPU may be idle during parts of the frame.  The frame time measures the time
	// between calls to present each frame.
	float GetFrameTime(void);

	// The total number of frames per second
	float GetFrameRate(void);

	extern uint32_t g_DisplayWidth;
	extern uint32_t g_DisplayHeight;

	extern ID3D11Device* g_Device;
	extern winrt::agile_ref<winrt::Windows::UI::Core::CoreWindow> g_window;

	// Back buffer resources, etc. for attached holographic cameras.
	extern std::map<UINT32, std::unique_ptr<StereographicCamera>>      g_cameraResources;
	extern std::mutex												   g_cameraResourcesLock;

	//extern CommandListManager g_CommandManager;
	//extern ContextManager g_ContextManager;

	//extern D3D_FEATURE_LEVEL g_D3DFeatureLevel;
	//extern bool g_bTypedUAVLoadSupport_R11G11B10_FLOAT;
	//extern bool g_bEnableHDROutput;

	//extern DescriptorAllocator g_DescriptorAllocator[];
	//inline D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1)
	//{
	//	return g_DescriptorAllocator[Type].Allocate(Count);
	//}

	//extern RootSignature g_GenerateMipsRS;
	//extern ComputePSO g_GenerateMipsLinearPSO[4];
	//extern ComputePSO g_GenerateMipsGammaPSO[4];

	//enum eResolution { k720p, k900p, k1080p, k1440p, k1800p, k2160p };

	//extern BoolVar s_EnableVSync;
	//extern EnumVar TargetResolution;

	// Device-based resources for holographic cameras are stored in a std::map. Access this list by providing a
	// callback to this function, and the std::map will be guarded from add and remove
	// events until the callback returns. The callback is processed immediately and must
	// not contain any nested calls to UseHolographicCameraResources.
	// The callback takes a parameter of type std::map<UINT32, std::unique_ptr<DX::CameraResources>>&
	// through which the list of cameras will be accessed.
	template<typename RetType, typename LCallback>
	RetType UseHolographicCameraResources(const LCallback& callback)
	{
		std::lock_guard<std::mutex> guard(m_cameraResourcesLock);
		return callback(m_cameraResources);
	}


#if defined(_DEBUG)
// Check for SDK Layer support.
	inline bool SdkLayersAvailable()
	{
		HRESULT hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_NULL,       // There is no need to create a real hardware device.
			0,
			D3D11_CREATE_DEVICE_DEBUG,  // Check for the SDK layers.
			nullptr,                    // Any feature level will do.
			0,
			D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Windows Runtime apps.
			nullptr,                    // No need to keep the D3D device reference.
			nullptr,                    // No need to know the feature level.
			nullptr                     // No need to keep the D3D device context reference.
		);

		return SUCCEEDED(hr);
	}
#endif
}