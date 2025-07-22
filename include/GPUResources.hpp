#pragma once
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>

#include <string>

enum RESOURCE_TYPES {
	TEXTURE,
	SAMPLER,
	BUFFER,
	TRANSFER_BUFFER,
	SHADER,
	GRAPHICS_PIPELINE
};

// define create & release function for 
// objects that must be released
// using a SDL_GPUDevice handle
template<RESOURCE_TYPES> struct GPUResourceTraits { };
template<> struct GPUResourceTraits<TEXTURE> {
	using info = SDL_GPUTextureCreateInfo;
	using type = SDL_GPUTexture;
	static constexpr auto description = "Texture";
	static constexpr auto create = SDL_CreateGPUTexture;
	static constexpr auto release = SDL_ReleaseGPUTexture;
};
template<> struct GPUResourceTraits<SAMPLER> {
	using info = SDL_GPUSamplerCreateInfo;
	using type = SDL_GPUSampler;
	static constexpr auto description = "Sampler";
	static constexpr auto create = SDL_CreateGPUSampler;
	static constexpr auto release = SDL_ReleaseGPUSampler;
};
template<> struct GPUResourceTraits<BUFFER> {
	using info = SDL_GPUBufferCreateInfo;
	using type = SDL_GPUBuffer;
	static constexpr auto description = "Buffer";
	static constexpr auto create = SDL_CreateGPUBuffer;
	static constexpr auto release = SDL_ReleaseGPUBuffer;
};
template<> struct GPUResourceTraits<SHADER> {
	using info = SDL_GPUShaderCreateInfo;
	using type = SDL_GPUShader;
	static constexpr auto description = "Shader";
	static constexpr auto create = SDL_CreateGPUShader;
	static constexpr auto release = SDL_ReleaseGPUShader;
};
template<> struct GPUResourceTraits<GRAPHICS_PIPELINE> {
	using info = SDL_GPUGraphicsPipelineCreateInfo;
	using type = SDL_GPUGraphicsPipeline;
	static constexpr auto description = "Graphics Pipeline";
	static constexpr auto create = SDL_CreateGPUGraphicsPipeline;
	static constexpr auto release = SDL_ReleaseGPUGraphicsPipeline;
};
template<> struct GPUResourceTraits<TRANSFER_BUFFER> {
	using info = SDL_GPUTransferBufferCreateInfo;
	using type = SDL_GPUTransferBuffer;
	static constexpr auto description = "Transfer Buffer";
	static constexpr auto create = SDL_CreateGPUTransferBuffer;
	static constexpr auto release = SDL_ReleaseGPUTransferBuffer;
};

// A GPUResource will automatically get the create & release function from GPUResourceTraits
template<RESOURCE_TYPES TYPE> class GPUResource {
public:
	GPUResource() { }
	~GPUResource() { }
	// All pointers stored inside resource.info must be valid
	// when resource.create is called
	GPUResourceTraits<TYPE>::info info;
	// creates a gpu resource using the current value of resource.info
	GPUResourceTraits<TYPE>::type* create(SDL_GPUDevice* t_gpu) {
		gpu = t_gpu;
		ptr = GPUResourceTraits<TYPE>::create(gpu, &info);
		if (!ptr) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Creating %s - Fail!\n\t%s", GPUResourceTraits<TYPE>::description, SDL_GetError());
		} else {
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Creating %s - Success!", GPUResourceTraits<TYPE>::description);
		}
		return ptr;
	}
	void release() {
		if (!gpu) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Tried to release %s, but the GPU device is invalid", GPUResourceTraits<TYPE>::description);
			return;
		} else if (!ptr) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Tried to release %s, but there is no resource to release, did you create it?", GPUResourceTraits<TYPE>::description);
			return;
		}
		GPUResourceTraits<TYPE>::release(gpu, ptr); 
		gpu = nullptr;
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Released %s", GPUResourceTraits<TYPE>::description);
	}
	GPUResourceTraits<TYPE>::type* get() const { return ptr; }
	GPUResource(const GPUResource &other) = delete;
	GPUResource(const GPUResource &&other) = delete;
	GPUResource<TYPE>& operator=(const GPUResource<TYPE>&) = delete;
private:
	GPUResourceTraits<TYPE>::type *ptr;
	SDL_GPUDevice *gpu;
};

// helper functions
// returns nullptr on failure, valid SDL_GPUShader* otherwise
// if successful, the return value is also stored within the GPUResource
// otherwise, the GPUResource remains untouched
SDL_GPUShader* createShader(SDL_GPUDevice *gpu, GPUResource<SHADER> *shader, const std::string &filename, const Uint32 &num_samplers, const Uint32 &num_storage_textures, const Uint32 &num_storage_buffers, const Uint32 &num_uniform_buffers);
