#include "GPUResources.hpp"
#include <SDL3/SDL_filesystem.h>

// load a shader
SDL_GPUShader* createShader(SDL_GPUDevice *gpu, GPUResource<SHADER> *shader, const std::string &filename, const Uint32 &num_samplers, const Uint32 &num_storage_textures, const Uint32 &num_storage_buffers, const Uint32 &num_uniform_buffers) {
	// Auto-detect the shader stage from the file name for convenience
	SDL_GPUShaderStage stage;
	if (filename.contains(".vert")) {
		stage = SDL_GPU_SHADERSTAGE_VERTEX;
	} else if (filename.contains(".frag")) {
		stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
	} else {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid shader stage!");
		return nullptr;
	}

	SDL_GPUShaderFormat valid_formats = SDL_GetGPUShaderFormats(gpu);
	SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
	std::string entrypoint;
	std::string shader_bin;
	std::string file_extension;

	if (valid_formats & SDL_GPU_SHADERFORMAT_SPIRV) {
		format = SDL_GPU_SHADERFORMAT_SPIRV;
		shader_bin = "shaders/bin/SPIRV/";
		file_extension = ".spv";
		entrypoint = "main";
	} else if (valid_formats & SDL_GPU_SHADERFORMAT_MSL) {
		format = SDL_GPU_SHADERFORMAT_MSL;
		shader_bin = "shaders/bin/MSL/";
		file_extension = ".msl";
		entrypoint = "main0";
	} else if (valid_formats & SDL_GPU_SHADERFORMAT_DXIL) {
		format = SDL_GPU_SHADERFORMAT_DXIL;
		shader_bin = "shaders/bin/DXIL/";
		file_extension = ".dxil";
		entrypoint = "main";
	} else {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Backend shader format is not supported");
		return nullptr;
	}

	const std::string full_path { SDL_GetBasePath() + shader_bin + filename + file_extension };

	size_t codeSize;
	void* code = SDL_LoadFile(full_path.data(), &codeSize);
	if (code == NULL) {
		SDL_Log("Failed to load shader from disk! %s", full_path.data());
		return nullptr;
	}

	shader->info = {
		.code_size = codeSize,
		.code = static_cast<Uint8*>(code),
		.entrypoint = entrypoint.data(),
		.format = format,
		.stage = stage,
		.num_samplers = num_samplers,
		.num_storage_textures = num_storage_textures,
		.num_storage_buffers = num_storage_buffers,
		.num_uniform_buffers = num_uniform_buffers,
	};
	shader->create(gpu);
	SDL_free(code);
	return shader->get();
}

