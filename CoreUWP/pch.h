#pragma once

#include "targetver.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// add headers that you want to pre-compile here
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Gaming.Input.h>
#include <winrt/Windows.Graphics.Display.h>
#include <winrt/Windows.Graphics.Holographic.h>
#include <winrt/Windows.Perception.People.h>
#include <winrt/Windows.Perception.Spatial.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Input.Spatial.h>
#include <Windows.Graphics.Directx.Direct3D11.Interop.h>
#include <wrl/client.h>
#include <dxgi.h>

#define SSE2  //indicates we want SSE2
#define SSE41 //indicates we want SSE4.1 instructions (floor and blend is available)
#define AVX2 //indicates we want AVX2 instructions (double speed!)

#ifndef AVX2
#include <xmmintrin.h> //SSE
#include <emmintrin.h> //SSE 2
#endif

#ifdef SSE41
#include <smmintrin.h> // SSE4.1
#endif

#ifdef AVX2
#include <immintrin.h> //avx2
#endif

#include <d3d11.h>
#include <d2d1_2.h>
#include <d3d11_4.h>
#include <DirectXColors.h>
#include <dwrite_2.h>

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <vector>
#include <memory>
#include <string>
#include <exception>

#include "windows.h"
#include <ppltasks.h>

#include "Utility.h"
#include "VectorMath.h"
#include "Graphics/GraphicsCore.h"
#include <windows.h>
