#include <windows.h>
#include <d3d11.h>
#include <stdint.h>
#include <stdio.h>

#include "include/MinHook.h"
#pragma comment(lib, "d3d11.lib")

// Variables initialized by my injector, you need to initialize this yourself.
#pragma section(".text")
__declspec(allocate(".text")) uint64_t DiscordPresent;
uint64_t DiscordBase;

HRESULT(__fastcall* DrawIndexedOriginal)(
	ID3D11DeviceContext* pContext,
	UINT IndexCount,
	UINT StartIndexLocation,
	INT BaseVertexLocation);

/// <summary>
/// DrawIndexed hook pseudocode cleaned and decompiled straight from your favorite pasted valorant cheat BWH.
/// BWH is pasted from https://github.com/DrNseven/D3D11-Worldtoscreen-Finder so please stop buying this crap thanks,
/// this will not need an update after you have fixed it for your own injector.
/// </summary>
/// <param name="pContext"></param>
/// <param name="IndexCount"></param>
/// <param name="StartIndexLocation"></param>
/// <param name="BaseVertexLocation"></param>
/// <returns></returns>
HRESULT __fastcall DrawIndexedHook(ID3D11DeviceContext* Context, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
	static ID3D11RasterizerState* CustomState;
	ID3D11Device* Device;
	ID3D11Buffer* VertexBuffer;
	ID3D11Buffer* ConstantBuffer;
	UINT VertexWidth;
	UINT VertexBufferOffset;
	UINT ConstantWidth;
	UINT Stride;

	Context->GetDevice(&Device);
	if (!CustomState)
	{
		D3D11_RASTERIZER_DESC Desc;
		Desc.AntialiasedLineEnable = 0;
		Desc.CullMode = D3D11_CULL_MODE::D3D11_CULL_NONE;
		Desc.DepthBiasClamp = 0.f;
		Desc.ScissorEnable = FALSE;
		Desc.FillMode = D3D11_FILL_MODE::D3D11_FILL_SOLID;
		Desc.DepthBias = INT_MAX;
		Desc.DepthClipEnable = TRUE;
		Desc.FrontCounterClockwise = FALSE;
		Device->CreateRasterizerState(&Desc, &CustomState);
	}

	Context->IAGetVertexBuffers(0, 1, &VertexBuffer, &Stride, &VertexBufferOffset);
	if (VertexBuffer)
	{
		D3D11_BUFFER_DESC VertexDesc;
		VertexBuffer->GetDesc(&VertexDesc);
		VertexWidth = VertexDesc.ByteWidth;

		VertexBuffer->Release();
	}

	Context->PSGetConstantBuffers(0, 1, &ConstantBuffer);
	if (ConstantBuffer)
	{
		D3D11_BUFFER_DESC ConstantDesc;
		ConstantBuffer->GetDesc(&ConstantDesc);
		ConstantWidth = ConstantDesc.ByteWidth;

		ConstantBuffer->Release();
	}

	if (Stride == 12
		&& ConstantWidth == 256
		&& VertexWidth - 20000 <= 0x5CC60
		&& CustomState)
	{
		ID3D11RasterizerState* OldState;
		Context->RSGetState(&OldState);
		Context->RSSetState(CustomState);
		DrawIndexedOriginal(Context, IndexCount, StartIndexLocation, BaseVertexLocation);
		Context->RSSetState(OldState);
	}
	return DrawIndexedOriginal(Context, IndexCount, StartIndexLocation, BaseVertexLocation);
}

/// <summary>
/// Hijacks present to get the device context to hook DrawIndexed.
/// </summary>
/// <param name="SwapChain"></param>
/// <param name="Flags"></param>
/// <param name="Sync"></param>
/// <returns></returns>
HRESULT PresentHook(IDXGISwapChain* SwapChain, UINT SyncInterval, UINT Flags)
{
	ID3D11Device* Device;
	ID3D11DeviceContext* Context;
	SwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&Device);
	Device->GetImmediateContext(&Context);
	if (Context)
	{
		uint64_t* ContextVirtualTable = *(uint64_t**)(Context);
		uint64_t DrawIndexedFunction = ContextVirtualTable[12];

		MH_Initialize();
		MH_CreateHook((void*)DrawIndexedFunction, DrawIndexedHook, (void**)&DrawIndexedOriginal);
		MH_EnableHook((void*)DrawIndexedFunction);

		*(uint64_t*)(DiscordBase + 0x1B3080) = DiscordPresent;
	}

	return ((HRESULT(*)(IDXGISwapChain*, UINT, UINT))(DiscordPresent))(SwapChain, SyncInterval, Flags);
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
	DiscordBase = (uint64_t)GetModuleHandleA("DiscordHook64.dll");
	*(void**)(DiscordBase + 0x1B3080) = PresentHook;
	return TRUE;
}

extern "C" int _fltused = 0; // To Skip CRT Float Bullshit Error
