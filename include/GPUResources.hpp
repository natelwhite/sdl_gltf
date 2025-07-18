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
template<> struct GPUResourceTraits<SDL_GPUGraphicsPipeline> {
	using info = SDL_GPUGraphicsPipelineCreateInfo;
	static constexpr auto resource_type = "Graphics Pipeline";
	static constexpr auto create = SDL_CreateGPUGraphicsPipeline;
	static constexpr auto release = SDL_ReleaseGPUGraphicsPipeline;
};

// A GPUResource will automatically get the create & release function from GPUResourceTraits
template<typename Type> class GPUResource {
public:
	GPUResource() { }
	~GPUResource() { }
	// All pointers stored inside resource.info must be valid
	// when resource.create is called
	GPUResourceTraits<Type>::info info;
	// creates a gpu resource using the current value of resource.info
	Type* create(SDL_GPUDevice* t_gpu) {
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
