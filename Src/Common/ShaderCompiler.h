#pragma once

#include <Windows.h>
#include <dxcapi.h>
#include <wrl/client.h>
#include <string>

using Microsoft::WRL::ComPtr;




class ShaderCompilerByteCode
{
public:
	ShaderCompilerByteCode()
	{
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
		DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
	}

	IDxcBlob* Compile(const std::wstring& shaderPath, const std::wstring& entryPoint, const std::wstring& targetProfile)
	{
		if (!compiler || !utils)
			return nullptr;

		ComPtr<IDxcBlobEncoding> source;
		HRESULT hr = utils->LoadFile(shaderPath.c_str(), nullptr, &source);
		if (FAILED(hr) || !source)
		{
			OutputDebugStringW((L"DXC LoadFile failed: " + shaderPath + L"\n").c_str());
			return nullptr;
		}

		ComPtr<IDxcIncludeHandler> includeHandler;
		hr = utils->CreateDefaultIncludeHandler(&includeHandler);
		if (FAILED(hr) || !includeHandler)
			return nullptr;

		LPCWSTR args[] =
		{
			shaderPath.c_str(),
			L"-E", entryPoint.c_str(),
			L"-T", targetProfile.c_str(),
			L"-Zi", L"-Qembed_debug",
			L"-Od"
		};

		ComPtr<IDxcOperationResult> result;
		hr = compiler->Compile(
			source.Get(),
			shaderPath.c_str(),
			entryPoint.c_str(),
			targetProfile.c_str(),
			args, _countof(args),
			nullptr, 0,
			includeHandler.Get(),
			&result
		);
		if (FAILED(hr) || !result)
			return nullptr;

		HRESULT status = E_FAIL;
		hr = result->GetStatus(&status);
		if (FAILED(hr) || FAILED(status))
		{
			ComPtr<IDxcBlobEncoding> errors;
			if (SUCCEEDED(result->GetErrorBuffer(&errors)) && errors)
				OutputDebugStringA(reinterpret_cast<const char*>(errors->GetBufferPointer()));
			return nullptr;
		}

		ComPtr<IDxcBlob> shaderBlob;
		hr = result->GetResult(&shaderBlob);
		if (FAILED(hr) || !shaderBlob)
			return nullptr;

		return shaderBlob.Detach();
	}

private:
	IDxcCompiler* compiler = nullptr;
	IDxcLibrary* library = nullptr;
	IDxcUtils* utils = nullptr;
};
