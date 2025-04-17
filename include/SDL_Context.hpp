#include <unordered_map>
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_gpu.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_init.h>

#include <fastgltf/tools.hpp>

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp>

struct Mesh {
	glm::vec3 pos, scale;
	glm::quat rot;
	Uint32 num_indices;
	glm::mat4x4 model_mat() const {
		return { glm::scale(glm::translate(glm::mat4(1), pos), scale) * glm::mat4_cast(rot) };
	}
};

struct VertexUniforms {
	glm::mat4 proj_view, model;
};

class Camera {
public:
	Camera(const glm::vec3 &t_pos, const glm::quat &t_rot, const glm::vec2 &t_dimensions)
	: m_pos(t_pos), m_rot(t_rot), m_dimensions(t_dimensions) { }
	glm::mat4 view() const;
	glm::mat4 proj() const;
	void resize(const float &width, const float &height);
	glm::vec3 forward() const;
	glm::vec3 up() const;
	glm::vec3 right() const;
	void rot(const float &pitch, const float &yaw);
	void iterate();
	void event(SDL_Event *e);
	glm::vec3 getPosition() const { return m_pos; }
	glm::vec2 getNearFar() const { return m_near_far; }
private:
	glm::vec3 m_pos; // (x, y, z) in world space
	glm::quat m_rot;
	glm::vec2 m_dimensions; // (x, y) -> (width, height)
	glm::vec3 m_vel { 0, 0, 0 }; // (x, y, z) in world space
	const float m_speed { 0.5 };
	glm::vec2 m_near_far { 20, 60 }; // (x, y) -> (near, far)
	std::unordered_map<SDL_Scancode, bool> m_keys;
};

template <>
struct fastgltf::ElementTraits<glm::vec3>
: fastgltf::ElementTraitsBase<glm::vec3, AccessorType::Vec3, float> { };

class SDL_Context {
public:
	SDL_Context() { }
	~SDL_Context() { }
	SDL_AppResult init();
	void quit();
	SDL_AppResult iterate();
	SDL_AppResult event(SDL_Event *e);
	SDL_AppResult openGLTF();
	void loadGLTF(const std::filesystem::path &path);
private:
	SDL_GPUDevice *m_gpu;
	SDL_Window *m_window;
	SDL_GPUShaderFormat m_supported_formats {
		SDL_GPU_SHADERFORMAT_SPIRV |
		SDL_GPU_SHADERFORMAT_DXIL |
		SDL_GPU_SHADERFORMAT_DXBC |
		SDL_GPU_SHADERFORMAT_METALLIB
	};
	SDL_GPUGraphicsPipeline *m_pp_pipeline, *m_geo_pipeline;
	SDL_GPUBuffer *m_v_buf, *m_i_buf, *m_norm_buf;
	SDL_GPUShader *m_geo_v_shader, *m_geo_f_shader, *m_pp_v_shader, *m_pp_f_shader;
	SDL_GPUSampler *m_depth_sampler;
	SDL_GPUTexture *m_depth, *m_color;

	Uint32 m_width { 1200 }, m_height { 900 };
	std::vector<Mesh> m_objects;
	std::vector<Camera> m_cams;
	Uint32 m_active_cam_index { };

	// helper functions
	glm::mat4x4 createLookAt(const glm::vec3 &camera_pos, const glm::vec3 &camera_target, const glm::vec3 &camera_up) const;
	SDL_GPUShader* loadShader(const std::filesystem::path &path);
};
