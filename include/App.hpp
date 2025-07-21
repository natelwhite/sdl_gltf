#pragma once
#include <unordered_map>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_init.h>

#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

#include "GPUResources.hpp"
#include "Pipelines.hpp"

template <>
struct fastgltf::ElementTraits<glm::vec3>
: fastgltf::ElementTraitsBase<glm::vec3, AccessorType::Vec3, float> { };

struct GPUBufferAllocationInfo {
	Uint32 bytes { }, count { };
	GPUBufferAllocationInfo& operator += (const GPUBufferAllocationInfo &other) {
		bytes += other.bytes;
		count += other.count;
		return *this;
	}
};
struct GeometryAllocationInfo {
	GPUBufferAllocationInfo indices, verts, norms;
	GeometryAllocationInfo& operator += (const GeometryAllocationInfo &other) {
		indices += other.indices;
		verts += other.verts;
		norms += other.norms;
		return *this;
	}
};

class App {
public:
	App() { }
	~App() { }
	SDL_AppResult init();
	void quit();
	SDL_AppResult iterate();
	SDL_AppResult event(SDL_Event *e);
	SDL_AppResult openGLTF();
	void loadGLTF(const std::filesystem::path &path);
private:
	SDL_GPUShaderFormat m_supported_formats {
		SDL_GPU_SHADERFORMAT_SPIRV |
		SDL_GPU_SHADERFORMAT_DXIL |
		SDL_GPU_SHADERFORMAT_MSL
	};
	SDL_WindowFlags m_window_flags {
		SDL_WINDOW_RESIZABLE
	};
	SDL_GPUDevice *m_gpu;
	SDL_Window *m_window;
	OutlinePipeline m_outline_pipeline;
	BlinnPhongPipeline m_blinnphong_pipeline;
	GPUResource<BUFFER> m_i_buf, m_v_buf, m_norm_buf;
	GPUResource<TEXTURE> m_color, m_depth;

	Uint32 m_width { 1200 }, m_height { 900 };
	std::vector<Mesh> m_objects;
	Camera m_camera {
		{-40, 40, -40},
		glm::quat_cast(glm::lookAt(glm::vec3{-40, 40, -40}, {0, 0, 0}, {0, 1, 0})),
		{m_width, m_height}
	};

};

