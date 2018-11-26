#pragma once

#include "pch.h"
#include "Common\DeviceResources.h"
#include "UWPDirectX12_newMain.h"

namespace UWPDirectX12_new
{
	// アプリのメイン エントリ ポイントです。Windows シェルでアプリを接続し、アプリケーション ライフサイクル イベントを処理します。
	ref class App sealed : public Windows::ApplicationModel::Core::IFrameworkView
	{
	public:
		App();

		//IFrameworkView メソッド.
		virtual void Initialize(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView);
		virtual void SetWindow(Windows::UI::Core::CoreWindow^ window);
		virtual void Load(Platform::String^ entryPoint);
		virtual void Run();
		virtual void Uninitialize();

	protected:
		// アプリケーション ライフサイクル イベント ハンドラー。
		void OnActivated(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView, Windows::ApplicationModel::Activation::IActivatedEventArgs^ args);
		void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ args);
		void OnResuming(Platform::Object^ sender, Platform::Object^ args);

		// ウィンドウ イベント ハンドラー。
		void OnWindowSizeChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::WindowSizeChangedEventArgs^ args);
		void OnVisibilityChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::VisibilityChangedEventArgs^ args);
		void OnWindowClosed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CoreWindowEventArgs^ args);

		// DisplayInformation イベント ハンドラー。
		void OnDpiChanged(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);
		void OnOrientationChanged(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);
		void OnDisplayContentsInvalidated(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);

	private:
		//m_deviceResources 用のプライベート アクセサはデバイス除去エラーから保護します。
		std::shared_ptr<DX::DeviceResources> GetDeviceResources();

		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::unique_ptr<UWPDirectX12_newMain> m_main;
		bool m_windowClosed;
		bool m_windowVisible;
	};
}

ref class Direct3DApplicationSource sealed : Windows::ApplicationModel::Core::IFrameworkViewSource
{
public:
	virtual Windows::ApplicationModel::Core::IFrameworkView^ CreateView();
};
