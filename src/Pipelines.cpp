#include "Pipelines.hpp"
// initialize pipeline
SDL_AppResult BlinnPhongPipeline::init(SDL_GPUDevice *gpu) {
	if (!createShader(gpu, &m_v_shader, "PositionTransform.vert", 0, 0, 0, 1))
		return SDL_APP_FAILURE;
	if (!createShader(gpu, &m_f_shader, "SolidColorDepth.frag", 0, 0, 0, 1))
		return SDL_APP_FAILURE;
	m_pipeline.info = {
		.vertex_shader = m_v_shader.get(),
		.fragment_shader = m_f_shader.get(),
		.vertex_input_state = {
			.vertex_buffer_descriptions = buffer_desc,
			.num_vertex_buffers = SDL_arraysize(buffer_desc),
			.vertex_attributes = vert_attribs,
			.num_vertex_attributes = SDL_arraysize(vert_attribs),
		},
		.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.rasterizer_state = {
			.fill_mode = SDL_GPU_FILLMODE_FILL,
			.cull_mode = SDL_GPU_CULLMODE_FRONT,
			.front_face = SDL_GPU_FRONTFACE_CLOCKWISE
		},
		.depth_stencil_state = {
			.compare_op = SDL_GPU_COMPAREOP_LESS,
			.write_mask = 0xFF,
			.enable_depth_test = true,
			.enable_depth_write = true,
			.enable_stencil_test = false,
		},
		.target_info = {
			.color_target_descriptions = &color_target,
			.num_color_targets = 1,
			.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
			.has_depth_stencil_target = true,
		},
	};
	if (!m_pipeline.create(gpu)) { return SDL_APP_FAILURE; }
	m_v_shader.release();
	m_f_shader.release();
	return SDL_APP_CONTINUE;
}
void BlinnPhongPipeline::quit() {
	m_pipeline.release(); 
}

// render 3D geometry to color & depth texture
// the buffers, camera data, and mesh data
// are all used in the shaders
// for this pipeline
void BlinnPhongPipeline::render(SDL_GPUCommandBuffer *cmdbuf, const Camera &camera, const std::vector<Mesh> &TRS_data, const GPUResource<BUFFER> &indices, const GPUResource<BUFFER> &verts, const GPUResource<BUFFER> &norms, const GPUResource<TEXTURE> &color, const GPUResource<TEXTURE> &depth) {
	SDL_GPUColorTargetInfo color_target_info {
		.texture = color.get(),
		.clear_color = {0, 0, 0, 0},
		.load_op = SDL_GPU_LOADOP_CLEAR,
		.store_op = SDL_GPU_STOREOP_STORE
	};
	SDL_GPUDepthStencilTargetInfo depth_stencil_target_info {
		.texture = depth.get(),
		.clear_depth = 1,
		.load_op = SDL_GPU_LOADOP_CLEAR,
		.store_op = SDL_GPU_STOREOP_STORE,
		.stencil_load_op = SDL_GPU_LOADOP_CLEAR,
		.stencil_store_op = SDL_GPU_STOREOP_STORE,
		.cycle = true,
		.clear_stencil = 0,
	};
	FragmentUniforms frag_uniforms { 
		camera.getNearFar(),
		camera.getPosition()
	};
	SDL_PushGPUFragmentUniformData(cmdbuf, 0, &frag_uniforms, sizeof(frag_uniforms));
	SDL_GPURenderPass *render_pass { SDL_BeginGPURenderPass(cmdbuf, &color_target_info, 1, &depth_stencil_target_info) };
	SDL_GPUBufferBinding i_buf_binding {
		.buffer = indices.get(),
		.offset = 0
	};
	SDL_GPUBufferBinding vert_buf_bindings[2] { {
			.buffer = verts.get(),
			.offset = 0
		}, {
			.buffer = norms.get(),
			.offset = 0
	} };
	SDL_BindGPUVertexBuffers(render_pass, 0, vert_buf_bindings, SDL_arraysize(vert_buf_bindings));
	SDL_BindGPUIndexBuffer(render_pass, &i_buf_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
	SDL_BindGPUGraphicsPipeline(render_pass, m_pipeline.get());
	VertexUniforms vert_uniforms;
	vert_uniforms.proj_view = camera.proj() * camera.view();
	for (const Mesh &mesh : TRS_data) {
		vert_uniforms.model = mesh.model_mat();
		SDL_PushGPUVertexUniformData(cmdbuf, 0, &vert_uniforms, sizeof(vert_uniforms));
		SDL_DrawGPUIndexedPrimitives(render_pass, mesh.num_indices, 1, mesh.first_index, 0, 0);
	}
	SDL_EndGPURenderPass(render_pass);
}

SDL_AppResult OutlinePipeline::init(SDL_Window *window, SDL_GPUDevice *gpu) {
	if (!createShader(gpu, &m_v_shader, "Window.vert", 0, 0, 0, 0))
		return SDL_APP_FAILURE;
	if (!createShader(gpu, &m_f_shader, "DepthOutline.frag", 2, 0, 0, 1))
		return SDL_APP_FAILURE;
	m_color_target = {
		.format = SDL_GetGPUSwapchainTextureFormat(gpu, window),
		.blend_state = {
			.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
			.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.color_blend_op = SDL_GPU_BLENDOP_ADD,
			.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
			.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.alpha_blend_op = SDL_GPU_BLENDOP_ADD,
			.enable_blend = true,
		}
	};
	m_pipeline.info = {
		.vertex_shader = m_v_shader.get(),
		.fragment_shader = m_f_shader.get(),
		.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.target_info = {
			.color_target_descriptions = &m_color_target,
			.num_color_targets = 1,
		}
	};
	if (!m_pipeline.create(gpu)) { return SDL_APP_FAILURE; }
	m_v_shader.release();
	m_f_shader.release();
	m_sampler.info = {
		.min_filter = SDL_GPU_FILTER_NEAREST,
		.mag_filter = SDL_GPU_FILTER_NEAREST,
		.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
		.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
	};
	if (!m_sampler.create(gpu)) { return SDL_APP_FAILURE; }
	return SDL_APP_CONTINUE;
}
void OutlinePipeline::quit() {
	m_pipeline.release();
}
// render pipeline to destination texture
// color & depth textures are sampled
// to create the outline effect
void OutlinePipeline::render(SDL_GPUCommandBuffer* cmdbuf, SDL_GPUTexture *dest, GPUResource<TEXTURE> &color, GPUResource<TEXTURE> &depth) {
	SDL_GPUColorTargetInfo swapchain_target_info {
		.texture = dest,
		.clear_color = {0.2f, 0.5f, 0.4f, 1.0f},
		.load_op = SDL_GPU_LOADOP_CLEAR,
		.store_op = SDL_GPU_STOREOP_STORE
	};
	SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(cmdbuf, &swapchain_target_info, 1, nullptr);
	SDL_BindGPUGraphicsPipeline(render_pass, m_pipeline.get());
	SDL_GPUTextureSamplerBinding sampler_bindings[] {
		{ .texture = color.get(), .sampler = m_sampler.get() },
		{ .texture = depth.get(), .sampler = m_sampler.get() },
	};
	SDL_BindGPUFragmentSamplers(render_pass, 0, sampler_bindings, 2);
	SDL_DrawGPUPrimitives(render_pass, 6, 1, 0, 0);
	SDL_EndGPURenderPass(render_pass);
}

// returns the model matrix of mesh
glm::mat4x4 Mesh::model_mat() const {
	return { glm::scale(glm::translate(glm::mat4(1), pos), scale) * glm::mat4_cast(rot) };
}

// Camera methods
// returns view matrix of camera
glm::mat4 Camera::view() const {
	return glm::mat4_cast(m_rot) * glm::translate(glm::mat4(1), -m_pos);
}
// returns projection matrix of camera
glm::mat4 Camera::proj() const {
	return glm::perspective(SDL_PI_F * 0.25f, m_dimensions.x / m_dimensions.y, m_near_far.x, m_near_far.y);
}
// resize the lense of camera (rect)
void Camera::resize(const float &width, const float &height) {
	m_dimensions = {width, height};
}
// returns forward direction of camera
glm::vec3 Camera::forward() const {
	return glm::conjugate(m_rot) * glm::vec3(0.0f, 0.0f, -1.0f);
}
// returns up direction of camera
glm::vec3 Camera::up() const {
	return glm::conjugate(m_rot) * glm::vec3(0.0f, 1.0f, 0.0f);
}
// returns right direction of camera
glm::vec3 Camera::right() const {
	return glm::conjugate(m_rot) * glm::vec3(1.0f, 0.0f, 0.0f);
}
// returns rotation of camera
void Camera::rot(const float &pitch, const float &yaw) {
	m_rot = glm::rotate(m_rot, pitch, right());
	m_rot = glm::rotate(m_rot, yaw, {0, 1, 0});
}
// move camera based on current inputs
// called once every frame
void Camera::iterate() {
	glm::vec3 acc = {0, 0, 0};
	if (m_keys.contains(SDL_SCANCODE_W) && m_keys.at(SDL_SCANCODE_W))
		acc += m_speed * forward();
	if (m_keys.contains(SDL_SCANCODE_A) && m_keys.at(SDL_SCANCODE_A))
		acc += m_speed * -right();
	if (m_keys.contains(SDL_SCANCODE_S) && m_keys.at(SDL_SCANCODE_S))
		acc += m_speed * -forward();
	if (m_keys.contains(SDL_SCANCODE_D) && m_keys.at(SDL_SCANCODE_D))
		acc += m_speed * right();
	if (m_keys.contains(SDL_SCANCODE_E) && m_keys.at(SDL_SCANCODE_E))
		acc += m_speed * up();
	if (m_keys.contains(SDL_SCANCODE_Q) && m_keys.at(SDL_SCANCODE_Q))
		acc += m_speed * -up();
	m_vel += acc;
	m_pos += m_vel;
	const glm::vec3 drag { m_vel * -0.1f };
	m_vel += drag;
	// when velocity is close to zero, make it zero
	const float epsilon { 0.01 };
	bool set_zero_axis[] {
		glm::abs(m_vel.x) < epsilon && glm::abs(m_vel.x) > 0, // x
		glm::abs(m_vel.y) < epsilon && glm::abs(m_vel.y) > 0, // y
		glm::abs(m_vel.z) < epsilon && glm::abs(m_vel.z) > 0, // z
	};
	if (set_zero_axis[0])
		m_vel.x = 0;
	if (set_zero_axis[1])
		m_vel.y = 0;
	if (set_zero_axis[2])
		m_vel.z = 0;
}
// update inputs
// called once every input event
void Camera::event(SDL_Event *e) {
	switch(e->type) {
	case SDL_EVENT_KEY_DOWN:
		if (!m_keys.contains(e->key.scancode)) {
			m_keys.insert({e->key.scancode, true});
		} else {
			m_keys.at(e->key.scancode) = true;
		}
		break;
	case SDL_EVENT_KEY_UP: 
		m_keys.at(e->key.scancode) = false;
		break;
	case SDL_EVENT_MOUSE_MOTION: {
		const float sensitivity { 0.001f };
		float pitch { e->motion.yrel * sensitivity };
		float yaw { e->motion.xrel * sensitivity };
		m_rot = glm::rotate(m_rot, pitch, right());
		m_rot = glm::rotate(m_rot, yaw, {0, 1, 0});
		break;
	}
	case SDL_EVENT_WINDOW_RESIZED:
		resize(e->window.data1, e->window.data2);
		break;
	}
}
