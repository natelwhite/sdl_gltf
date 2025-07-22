#pragma once
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_init.h>

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

#include "GPUResources.hpp"

struct Mesh {
	glm::vec3 pos, scale;
	glm::quat rot;
	Uint32 num_indices, first_index;
	glm::mat4x4 model_mat() const;
};

class Camera {
public:
	Camera(const glm::vec3 &t_pos, const glm::quat &t_rot, const glm::vec2 &t_dimensions)
	: m_pos(t_pos), m_rot(t_rot), m_dimensions(t_dimensions) { }
	void iterate();
	/**
	 * Update camera based on event 
	 *
	 * @param e The application's event data
	 */
	void event(SDL_Event *e);
	glm::vec3 getPosition() const { return m_pos; }
	glm::vec2 getNearFar() const { return m_near_far; }
	// returns view matrix of camera
	glm::mat4 view() const;
	// returns projection matrix of camera
	glm::mat4 proj() const;
	glm::vec3 forward() const;
	// returns up direction of camera
	glm::vec3 up() const;
	// returns right direction of camera
	glm::vec3 right() const;
private:
	glm::vec3 m_pos; // (x, y, z) in world space
	glm::quat m_rot;
	glm::vec2 m_dimensions; // (x, y) -> (width, height)
	glm::vec3 m_vel { 0, 0, 0 }; // (x, y, z) in world space
	const float m_speed { 0.5 };
	glm::vec2 m_near_far { 0.1, 1000 }; // (x, y) -> (near, far)
	std::unordered_map<SDL_Scancode, bool> m_keys;
};

class BlinnPhongPipeline {
public:
	BlinnPhongPipeline() { }
	~BlinnPhongPipeline() { }
	/**
	 * Initialize pipeline
	 *
	 * @param gpu A valid GPUDevice handle
	 */
	SDL_AppResult init(SDL_GPUDevice *gpu);
	void quit();
	/**
	 * Render 3D geometry
	 *
	 * @param cmdbuf The command buffer associated with this render pass
	 * @param color Color texture for render output
	 * @param depth Depth texture for render output
	 * @param camera The perspective to render from
	 * @param TRS_data A vector of meshes to render
	 * @param indices A buffer of vertex indices that correspond to the meshes
	 * @param verts A buffer of vertex positions that correspond to the meshes
	 * @param norms A buffer of vertex normals that correspond to the meshes
	 */
	void render(SDL_GPUCommandBuffer *cmdbuf, const GPUResource<TEXTURE> &color, const GPUResource<TEXTURE> &depth, const Camera &camera, const std::vector<Mesh> &TRS_data, const GPUResource<BUFFER> &indices, const GPUResource<BUFFER> &verts, const GPUResource<BUFFER> &norms);
private:
	GPUResource<GRAPHICS_PIPELINE> m_pipeline;
	GPUResource<SHADER> m_v_shader, m_f_shader;
	const SDL_GPUColorTargetDescription color_target { .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM };
	const SDL_GPUVertexBufferDescription buffer_desc[2] { {
		.slot = 0,
		.pitch = sizeof(glm::vec3),
		.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
		.instance_step_rate = 0,
	}, {
		.slot = 1,
		.pitch = sizeof(glm::vec3),
		.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
		.instance_step_rate = 0,
	} };
	const SDL_GPUVertexAttribute vert_attribs[2] { {
		.location = 0,
		.buffer_slot = 0,
		.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
		.offset = 0
	}, {
		.location = 1,
		.buffer_slot = 1,
		.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
		.offset = 0,
	} };
	struct VertexUniforms {
		glm::mat4 proj_view, model;
	};
	struct FragmentUniforms {
		glm::vec2 near_far;
		glm::vec3 view_pos;
	};
};

class OutlinePipeline {
public:
	OutlinePipeline() { }
	~OutlinePipeline() { }
	/**
	 * Initialize pipeline
	 *
	 * @param gpu A valid GPUDevice handle
	 */
	SDL_AppResult init(SDL_Window *window, SDL_GPUDevice *gpu);
	void quit();
	/**
	 * Render 3D geometry with outline to texture
	 *
	 * @param cmdbuf The command buffer associated with this render pass
	 * @param dest The destination texture to render to
	 * @param color Color texture for render input
	 * @param depth Depth texture for render input
	 */
	void render(SDL_GPUCommandBuffer* cmdbuf, SDL_GPUTexture *dest, GPUResource<TEXTURE> &color, GPUResource<TEXTURE> &depth);
private:
	GPUResource<GRAPHICS_PIPELINE> m_pipeline;
	GPUResource<SHADER> m_v_shader, m_f_shader;
	GPUResource<SAMPLER> m_sampler;
	SDL_GPUColorTargetDescription m_color_target;
};

