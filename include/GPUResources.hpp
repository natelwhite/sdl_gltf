#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_init.h>

// define create & release function for 
// objects that must be released
// using a SDL_GPUDevice handle
template<typename> struct GPUResourceTraits { };
template<> struct GPUResourceTraits<SDL_GPUTexture> {
	using info = SDL_GPUTextureCreateInfo;
	static constexpr auto resource_type = "Texture";
	static constexpr auto create = SDL_CreateGPUTexture;
	static constexpr auto release = SDL_ReleaseGPUTexture;
};
template<> struct GPUResourceTraits<SDL_GPUSampler> {
	using info = SDL_GPUSamplerCreateInfo;
	static constexpr auto resource_type = "Sampler";
	static constexpr auto create = SDL_CreateGPUSampler;
	static constexpr auto release = SDL_ReleaseGPUSampler;
};
template<> struct GPUResourceTraits<SDL_GPUBuffer> {
	using info = SDL_GPUBufferCreateInfo;
	static constexpr auto resource_type = "Buffer";
	static constexpr auto create = SDL_CreateGPUBuffer;
	static constexpr auto release = SDL_ReleaseGPUBuffer;
};
template<> struct GPUResourceTraits<SDL_GPUShader> {
	using info = SDL_GPUShaderCreateInfo;
	static constexpr auto resource_type = "Shader";
	static constexpr auto create = SDL_CreateGPUShader;
	static constexpr auto release = SDL_ReleaseGPUShader;
};

// A GPUResource will automatically get the create & release function from GPUResourceTraits
template<typename Type> class GPUResource {
public:
	GPUResource() { }
	~GPUResource() { }
	Type* create(SDL_GPUDevice* t_gpu, const GPUResourceTraits<Type>::info &info) {
		gpu = t_gpu;
		ptr = GPUResourceTraits<Type>::create(gpu, &info);
		if (!ptr) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Creating %s - Fail!\n\tERR: %s", GPUResourceTraits<Type>::resource_type, SDL_GetError());
		} else {
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Creating %s - Success!", GPUResourceTraits<Type>::resource_type);
		}
		return ptr;
	}
	void release() {
		GPUResourceTraits<Type>::release(gpu, ptr); 
		gpu = nullptr;
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Released %s", GPUResourceTraits<Type>::resource_type);
	}
	Type* get() const { return ptr; }
	GPUResource(const GPUResource &other) = delete;
	GPUResource(const GPUResource &&other) = delete;
	GPUResource<Type>& operator=(const GPUResource<Type>&) = delete;
private:
	Type* ptr;
	SDL_GPUDevice *gpu;
};

class GPUMan {
public:
	GPUMan() {}
	~GPUMan() {}
	SDL_AppResult init(SDL_Window *window, const SDL_GPUShaderFormat &format_flags, const bool &debug_mode) {
		m_gpu = SDL_CreateGPUDevice(m_supported_formats, true, NULL);
		if (!m_gpu) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateGPUDevice failed:\n\t%s", SDL_GetError());
			return SDL_APP_FAILURE;
		}
		if (!SDL_ClaimWindowForGPUDevice(m_gpu, window)) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_ClaimWindowForGPUDevice failed:\n\t%s", SDL_GetError());
			return SDL_APP_FAILURE;
		}
		return SDL_APP_CONTINUE;
	}
private:
	SDL_GPUShaderFormat m_supported_formats {
		SDL_GPU_SHADERFORMAT_SPIRV |
		SDL_GPU_SHADERFORMAT_DXIL |
		SDL_GPU_SHADERFORMAT_METALLIB
	};
	SDL_GPUDevice *m_gpu;
};

