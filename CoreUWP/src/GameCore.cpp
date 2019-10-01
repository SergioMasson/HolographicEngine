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
using namespace winrt::Windows::Graphics::Holographic;
using namespace winrt::Windows::Perception::Spatial;
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

namespace HolographicEngine::GameCore
{
	using namespace Graphics;

	const bool TestGenerateMips = false;

	void LoadApplication(IGameApp& game)
	{
		//TODO(Sergio): Implement graphics stuff.
		Graphics::Initialize();
		SystemTime::Initialize();
		GameInput::Initialize();
	}

	void InitializeApplication(IGameApp& game)
	{
		game.Startup();
	}

	void SuspendApplication(IGameApp& game)
	{
		game.Suspend();
	}

	void ResumeApplication(IGameApp& game)
	{
		game.Resume();
	}

	void TerminateApplication(IGameApp& game)
	{
		game.Cleanup();
		GameInput::Shutdown();
	}

	bool UpdateApplication(IGameApp& game, HolographicSpace const& space, SpatialStationaryFrameOfReference const& reference)
	{
		//	EngineProfiling::Update();
		float DeltaTime = Graphics::GetFrameTime();

		GameInput::Update(DeltaTime);
		//	EngineTuning::Update(DeltaTime);

		game.Update(DeltaTime);

		// Before doing the timer update, there is some work to do per-frame
		// to maintain holographic rendering. First, we will get information
		// about the current frame.

		// The HolographicFrame has information that the app needs in order
		// to update and render the current frame. The app begins each new
		// frame by calling CreateNextFrame.
		HolographicFrame holographicFrame = space.CreateNextFrame();

		// Get a prediction of where holographic cameras will be when this frame
		// is presented.
		HolographicFramePrediction prediction = holographicFrame.CurrentPrediction();

		// Back buffers can change from frame to frame. Validate each buffer, and recreate
		// resource views and depth buffers as needed.
		Graphics::EnsureHolographicCameraResources(holographicFrame, prediction);

		if (Graphics::Render(game, holographicFrame, reference))
		{
			Graphics::Present(holographicFrame);
		}

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
		~App();
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
		//Holographic Space events

		// Asynchronously creates resources for new holographic cameras.
		void OnCameraAdded(HolographicSpace const& sender, HolographicSpaceCameraAddedEventArgs const& args);

		// Synchronously releases resources for holographic cameras that are no longer
		// attached to the system.
		void OnCameraRemoved(HolographicSpace const& sender, HolographicSpaceCameraRemovedEventArgs const& args);

		// Used to notify the app when the positional tracking state changes.
		void OnLocatabilityChanged(SpatialLocator const& sender, IInspectable const& args);

		// Used to respond to changes to the default spatial locator.
		void OnHolographicDisplayIsAvailableChanged(IInspectable const&, IInspectable const&);

		void UnregisterHolographicEventHandlers();

	private:
		bool m_windowClosed;
		bool m_windowVisible;
		bool m_canGetDefaultHolographicDisplay = false;
		bool m_canGetHolographicDisplayForCamera = false;
		bool m_canCommitDirect3D11DepthBuffer = false;

		//Windows runtime object are just a reference to the actual object itself. They can be copied!
		HolographicSpace					m_holographicSpace{ nullptr };
		SpatialLocator						m_spatialLocator{ nullptr };
		SpatialStationaryFrameOfReference	m_stationaryReferenceFrame{ nullptr };

		winrt::event_token                                          m_cameraAddedToken;
		winrt::event_token                                          m_cameraRemovedToken;
		winrt::event_token                                          m_locatabilityChangedToken;
		winrt::event_token                                          m_holographicDisplayIsAvailableChangedEventToken;
		winrt::event_token                                          m_suspendingEventToken;
		winrt::event_token                                          m_resumingEventToken;

		volatile bool m_IsRunning;
		volatile bool m_IsCapturingPointer;
		float m_PointerX, m_PointerY;
	};

	// Called by the system.  Perform application initialization here, hooking application wide events, etc.
	void App::Initialize(CoreApplicationView const& applicationView)
	{
		applicationView.Activated(std::bind(&App::OnActivated, this, _1, _2));

		// Register event handlers for app lifecycle.
		m_suspendingEventToken = CoreApplication::Suspending(bind(&App::OnSuspending, this, _1, _2));
		m_resumingEventToken = CoreApplication::Resuming(bind(&App::OnResuming, this, _1, _2));

		m_canGetHolographicDisplayForCamera = winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(L"Windows.Graphics.Holographic.HolographicCamera", L"Display");
		m_canGetDefaultHolographicDisplay = winrt::Windows::Foundation::Metadata::ApiInformation::IsMethodPresent(L"Windows.Graphics.Holographic.HolographicDisplay", L"GetDefault");
		m_canCommitDirect3D11DepthBuffer = winrt::Windows::Foundation::Metadata::ApiInformation::IsMethodPresent(L"Windows.Graphics.Holographic.HolographicCameraRenderingParameters", L"CommitDirect3D11DepthBuffer");
	}

	void App::UnregisterHolographicEventHandlers()
	{
		if (m_holographicSpace != nullptr)
		{
			// Clear previous event registrations.
			m_holographicSpace.CameraAdded(m_cameraAddedToken);
			m_cameraAddedToken = {};
			m_holographicSpace.CameraRemoved(m_cameraRemovedToken);
			m_cameraRemovedToken = {};
		}

		if (m_spatialLocator != nullptr)
		{
			m_spatialLocator.LocatabilityChanged(m_locatabilityChangedToken);
		}
	}

	App::~App()
	{
		UnregisterHolographicEventHandlers();
		HolographicSpace::IsAvailableChanged(m_holographicDisplayIsAvailableChangedEventToken);
	}

	// Called when we are provided a window.
	void App::SetWindow(CoreWindow const& window)
	{
		// We record the window pointer now, but you can also call this function to retrieve it:
		//     CoreWindow::GetForCurrentThread()
		g_window = window;

		LoadApplication(*m_game);

		UnregisterHolographicEventHandlers();

		m_holographicSpace = HolographicSpace::CreateForCoreWindow(window);

		//m_holographicSpace = std::shared_ptr<HolographicSpace>(&HolographicSpace::CreateForCoreWindow(window));

		//Holographic Space can only be created after the app has a CoreWindow.
		Graphics::AttachHolographicSpace(m_holographicSpace);

		m_cameraAddedToken = m_holographicSpace.CameraAdded(std::bind(&App::OnCameraAdded, this, _1, _2));
		m_cameraRemovedToken = m_holographicSpace.CameraRemoved(std::bind(&App::OnCameraRemoved, this, _1, _2));

		m_holographicDisplayIsAvailableChangedEventToken = HolographicSpace::IsAvailableChanged(std::bind(&App::OnHolographicDisplayIsAvailableChanged, this, _1, _2));

		OnHolographicDisplayIsAvailableChanged(nullptr, nullptr);

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
			m_IsRunning = UpdateApplication(*m_game, m_holographicSpace, m_stationaryReferenceFrame);
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

	void App::OnSuspending(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::ApplicationModel::SuspendingEventArgs const& args)
	{
		// Save app state asynchronously after requesting a deferral. Holding a deferral
		// indicates that the application is busy performing suspending operations. Be
		// aware that a deferral may not be held indefinitely; after about five seconds,
		// the app will be forced to exit.
		SuspendingDeferral deferral = args.SuspendingOperation().GetDeferral();

		Concurrency::create_task([this, deferral]()
			{
				Graphics::Trim();

				if (m_game != nullptr)
					SuspendApplication(*m_game);

				deferral.Complete();
			});
	}

	void App::OnResuming(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& args)
	{
		// Restore any data or state that was unloaded on suspend. By default, data
		// and state are persisted when resuming from suspend. Note that this event
		// does not occur if the app was previously terminated.

		if (m_game != nullptr)
			ResumeApplication(*m_game);
	}

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

	// Holographic API callback ----------------------------

	void App::OnCameraAdded(HolographicSpace const& sender, HolographicSpaceCameraAddedEventArgs const& args)
	{
		winrt::Windows::Foundation::Deferral deferral = args.GetDeferral();
		HolographicCamera holographicCamera = args.Camera();

		concurrency::create_task([this, deferral, holographicCamera]()
			{
				//
				// TODO: Allocate resources for the new camera and load any content specific to
				//       that camera. Note that the render target size (in pixels) is a property
				//       of the HolographicCamera object, and can be used to create off-screen
				//       render targets that match the resolution of the HolographicCamera.
				//

				// Create device-based resources for the holographic camera and add it to the list of
				// cameras used for updates and rendering. Notes:
				//   * Since this function may be called at any time, the AddHolographicCamera function
				//     waits until it can get a lock on the set of holographic camera resources before
				//     adding the new camera. At 60 frames per second this wait should not take long.
				//   * A subsequent Update will take the back buffer from the RenderingParameters of this
				//     camera's CameraPose and use it to create the ID3D11RenderTargetView for this camera.
				//     Content can then be rendered for the HolographicCamera.
				Graphics::AddHolographicCamera(holographicCamera);

				// Holographic frame predictions will not include any information about this camera until
				// the deferral is completed.
				deferral.Complete();
			});
	}

	void App::OnCameraRemoved(HolographicSpace const& sender, HolographicSpaceCameraRemovedEventArgs const& args)
	{
		Graphics::RemoveHolographicCamera(args.Camera());
	}

	void App::OnLocatabilityChanged(SpatialLocator const& sender, IInspectable const& args)
	{
		switch (sender.Locatability())
		{
			// Holograms cannot be rendered.
		case SpatialLocatability::Unavailable:
		{
			Utility::Printf(L"Warning! Positional tracking is SpatialLocatability::Unavailable \n");
		}
		break;

		// In the following three cases, it is still possible to place holograms using a
		// SpatialLocatorAttachedFrameOfReference.
		case SpatialLocatability::PositionalTrackingActivating:
			// The system is preparing to use positional tracking.

		case SpatialLocatability::OrientationOnly:
			// Positional tracking has not been activated.

		case SpatialLocatability::PositionalTrackingInhibited:
			// Positional tracking is temporarily inhibited. User action may be required
			// in order to restore positional tracking.
			break;

		case SpatialLocatability::PositionalTrackingActive:
			// Positional tracking is active. World-locked content can be rendered.
			break;
		}
	}

	void App::OnHolographicDisplayIsAvailableChanged(IInspectable const&, IInspectable const&)
	{
		// Get the spatial locator for the default HolographicDisplay, if one is available.
		SpatialLocator spatialLocator = nullptr;

		if (m_canGetDefaultHolographicDisplay)
		{
			HolographicDisplay defaultHolographicDisplay = HolographicDisplay::GetDefault();

			if (defaultHolographicDisplay)
				spatialLocator = defaultHolographicDisplay.SpatialLocator();
		}
		else
		{
			spatialLocator = SpatialLocator::GetDefault();
		}

		if (m_spatialLocator != spatialLocator)
		{
			// If the spatial locator is disconnected or replaced, we should discard all state that was
			// based on it.
			if (m_spatialLocator != nullptr)
			{
				m_spatialLocator.LocatabilityChanged(m_locatabilityChangedToken);
				m_spatialLocator = nullptr;
			}

			m_stationaryReferenceFrame = nullptr;

			if (spatialLocator != nullptr)
			{
				// Use the SpatialLocator from the default HolographicDisplay to track the motion of the device.
				m_spatialLocator = spatialLocator;

				// Respond to changes in the positional tracking state.
				m_locatabilityChangedToken = m_spatialLocator.LocatabilityChanged(std::bind(&App::OnLocatabilityChanged, this, _1, _2));

				// The simplest way to render world-locked holograms is to create a stationary reference frame
				// based on a SpatialLocator. This is roughly analogous to creating a "world" coordinate system
				// with the origin placed at the device's position as the app is launched.
				m_stationaryReferenceFrame = m_spatialLocator.CreateStationaryFrameOfReferenceAtCurrentLocation();
			}
		}
	}
	// ------------------------------------------------------------------------

	void RunApplication(IGameApp& app, const wchar_t* className)
	{
		m_game = &app;
		(void)className;

		//Required in order to user winrt libraries.
		winrt::init_apartment();
		CoreApplication::Run(make<App>());
	}
}