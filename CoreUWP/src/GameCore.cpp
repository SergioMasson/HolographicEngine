// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.

#include "pch.h"
#include "GameCore.h"
#include "SystemTime.h"
#include "Input/GameInput.h"

using namespace winrt::Windows::ApplicationModel;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::ViewManagement;
using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Composition;
using namespace std::placeholders;
using namespace winrt;

using winrt::Windows::ApplicationModel::Core::CoreApplication;
using winrt::Windows::ApplicationModel::Core::CoreApplicationView;
using winrt::Windows::ApplicationModel::Activation::IActivatedEventArgs;
using winrt::Windows::Foundation::TypedEventHandler;

//namespace Graphics
//{
//	extern ColorBuffer g_GenMipsBuffer;
//}

namespace GameCore
{
	using namespace Graphics;

	const bool TestGenerateMips = false;

	void InitializeApplication(IGameApp& game)
	{
		//TODO(Sergio): Implement graphics stuff.
		Graphics::Initialize();
		SystemTime::Initialize();
		GameInput::Initialize();
		game.Startup();
	}

	void TerminateApplication(IGameApp& game)
	{
		game.Cleanup();
		GameInput::Shutdown();
	}

	bool UpdateApplication(IGameApp& game)
	{
		return true;
		//	EngineProfiling::Update();

		float DeltaTime = Graphics::GetFrameTime();

		GameInput::Update(DeltaTime);
		//	EngineTuning::Update(DeltaTime);

		game.Update(DeltaTime);
		game.RenderScene();

		//	PostEffects::Render();

		//	if (TestGenerateMips)
		//	{
		//		GraphicsContext& MipsContext = GraphicsContext::Begin();

		//		// Exclude from timings this copy necessary to setup the test
		//		MipsContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_GENERIC_READ);
		//		MipsContext.TransitionResource(g_GenMipsBuffer, D3D12_RESOURCE_STATE_COPY_DEST);
		//		MipsContext.CopySubresource(g_GenMipsBuffer, 0, g_SceneColorBuffer, 0);

		//		EngineProfiling::BeginBlock(L"GenerateMipMaps()", &MipsContext);
		//		g_GenMipsBuffer.GenerateMipMaps(MipsContext);
		//		EngineProfiling::EndBlock(&MipsContext);

		//		MipsContext.Finish();
		//  }

		//	GraphicsContext& UiContext = GraphicsContext::Begin(L"Render UI");
		//	UiContext.TransitionResource(g_OverlayBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		//	UiContext.ClearColor(g_OverlayBuffer);
		//	UiContext.SetRenderTarget(g_OverlayBuffer.GetRTV());
		//	UiContext.SetViewportAndScissor(0, 0, g_OverlayBuffer.GetWidth(), g_OverlayBuffer.GetHeight());
		//game.RenderUI(UiContext);

		//	EngineTuning::Display(UiContext, 10.0f, 40.0f, 1900.0f, 1040.0f);

		//	UiContext.Finish();

		Graphics::Present();
		return !game.IsDone();
	}

	// Default implementation to be overridden by the application
	bool IGameApp::IsDone(void)
	{
		return GameInput::IsFirstPressed(GameInput::kKey_escape);
	}

	IGameApp* m_game;

	winrt::agile_ref<winrt::Windows::UI::Core::CoreWindow> g_window;

	struct App : implements<App, IFrameworkViewSource, IFrameworkView>
	{
	public:
		App() {}

		// IFrameworkView Methods.
		virtual void Initialize(CoreApplicationView const& applicationView);
		virtual void Load(winrt::hstring const& entryPoint);
		virtual void Run(void);
		virtual void SetWindow(CoreWindow const& window);
		virtual void Uninitialize(void);

		IFrameworkView CreateView()
		{
			return *this;
		}

	protected:
		// Event Handlers.
		void OnActivated(CoreApplicationView const& applicationView, Activation::IActivatedEventArgs const& args);
		void OnSuspending(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::ApplicationModel::SuspendingEventArgs const& args);
		void OnResuming(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args);

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_TV_TITLE)
		void OnWindowSizeChanged(CoreWindow const& sender, WindowSizeChangedEventArgs const& args);
		void OnWindowClosed(CoreWindow const& sender, CoreWindowEventArgs const& args);
		void OnVisibilityChanged(CoreWindow const& sender, VisibilityChangedEventArgs const& args);
		void OnPointerPressed(CoreWindow const& sender, PointerEventArgs const& args);
		void OnPointerMoved(CoreWindow const& sender, PointerEventArgs const& args);
		void OnKeyDown(CoreWindow const& sender, KeyEventArgs const& args);
		void OnKeyUp(CoreWindow const& sender, KeyEventArgs const& args);
#endif

	private:
		bool m_windowClosed;
		bool m_windowVisible;
		volatile bool m_IsRunning;
		volatile bool m_IsCapturingPointer;
		float m_PointerX, m_PointerY;
	};

	// Called by the system.  Perform application initialization here, hooking application wide events, etc.
	void App::Initialize(CoreApplicationView const& applicationView)
	{
		applicationView.Activated(std::bind(&App::OnActivated, this, _1, _2));
	}

	// Called when we are provided a window.
	void App::SetWindow(CoreWindow const& window)
	{
		// We record the window pointer now, but you can also call this function to retrieve it:
		//     CoreWindow::GetForCurrentThread()
		g_window = window;

		//Holographic Space can only be created after the app has a CoreWindow.
		Graphics::CreateHolographicScene(window);

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_TV_TITLE)
		window.SizeChanged(std::bind(&App::OnWindowSizeChanged, this, _1, _2));
		window.VisibilityChanged(std::bind(&App::OnVisibilityChanged, this, _1, _2));
		window.Closed(std::bind(&App::OnWindowClosed, this, _1, _2));
		window.KeyDown(std::bind(&App::OnKeyDown, this, _1, _2));
		window.KeyUp(std::bind(&App::OnKeyUp, this, _1, _2));
		window.PointerPressed(std::bind(&App::OnPointerPressed, this, _1, _2));
		window.PointerMoved(std::bind(&App::OnPointerMoved, this, _1, _2));
#endif
	}

	void App::Load(winrt::hstring const& entryPoint)
	{
		InitializeApplication(*m_game);
	}

	// Called by the system after initialization is complete.  This implements the traditional game loop.
	void App::Run()
	{
		while (m_IsRunning)
		{
			// ProcessEvents will throw if the process is exiting, allowing us to break out of the loop.  This will be
			// cleaned up when we get proper process lifetime management in a future release.
			g_window.get().Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
			m_IsRunning = UpdateApplication(*m_game);
		}
	}

	void App::Uninitialize()
	{
		Graphics::Terminate();
		TerminateApplication(*m_game);
		Graphics::Shutdown();
	}

	// Called when the application is activated.  For now, there is just one activation kind - Launch.
	void App::OnActivated(CoreApplicationView const& sender, IActivatedEventArgs const& args)
	{
		m_IsRunning = true;
		m_IsCapturingPointer = false;
	}

	void App::OnSuspending(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::ApplicationModel::SuspendingEventArgs const& args) {}

	void App::OnResuming(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args) {}

	void App::OnWindowSizeChanged(CoreWindow const& sender, WindowSizeChangedEventArgs const& args)
	{
		winrt::Windows::Foundation::Rect bounds = sender.Bounds();
		Graphics::Resize((uint32_t)(bounds.Width), (uint32_t)(bounds.Height));
	}

	void App::OnWindowClosed(CoreWindow const& sender, CoreWindowEventArgs const& args)
	{
		m_IsRunning = false;
	}

	void App::OnVisibilityChanged(CoreWindow const& sender, VisibilityChangedEventArgs const& args) {}

	void App::OnPointerPressed(CoreWindow const& sender, PointerEventArgs const& args)
	{
		//DEBUGPRINT("Pointer pressed (%f, %f)", args->CurrentPoint->RawPosition.X, args->CurrentPoint->RawPosition.Y);
		if (m_IsCapturingPointer)
		{
			sender.ReleasePointerCapture();
			m_IsCapturingPointer = false;
			//DEBUGPRINT("Pointer released");
		}
		else
		{
			sender.SetPointerCapture();
			m_IsCapturingPointer = true;
			m_PointerX = args.CurrentPoint().RawPosition().X;
			m_PointerY = args.CurrentPoint().RawPosition().Y;
			//DEBUGPRINT("Pointer captured");
		}
	}

	void App::OnPointerMoved(CoreWindow const& sender, PointerEventArgs const& args)
	{
		if (!m_IsCapturingPointer)
			return;

		float OldX = m_PointerX;
		float OldY = m_PointerY;

		m_PointerX = args.CurrentPoint().RawPosition().X;
		m_PointerY = args.CurrentPoint().RawPosition().Y;

		//DEBUGPRINT("Pointer moved (%f, %f)", m_PointerX, m_PointerY);
		//DEBUGPRINT("Pointer was (%f, %f)", OldX, OldY);
		//GameInput::SetMouseMovement(m_PointerX)
	}

	void App::OnKeyDown(CoreWindow const& sender, KeyEventArgs const& args)
	{
		GameInput::SetKeyState(args.VirtualKey(), true);
	}

	void App::OnKeyUp(CoreWindow const& sender, KeyEventArgs const& args)
	{
		GameInput::SetKeyState(args.VirtualKey(), false);
	}

	void RunApplication(IGameApp& app, const wchar_t* className)
	{
		m_game = &app;
		(void)className;

		//Required in order to user winrt libraries.
		winrt::init_apartment();
		CoreApplication::Run(make<App>());
	}
}