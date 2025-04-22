#include <vector>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/geometric.hpp>

#include "SDL_Context.hpp"
#include "SDL3/SDL_gpu.h"
#include "SDL3/SDL_log.h"
#include "fastgltf/tools.hpp"
#include "glm/gtc/quaternion.hpp"
#include <SDL3_shadercross/SDL_shadercross.h>

// callback function for opening files
void SDLCALL callback(void* userdata, const char* const* filelist, int filter) {
	if (!filelist) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Error in file dialog: \n\t%s", SDL_GetError());
		return;
	}
    if (!*filelist) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "No file selected from dialog");
        return;
    }
	std::string path;
    while (*filelist) {
		path += *filelist++;
    }
	if (!userdata) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "userdata is null");
		return;
	} 
	SDL_Context* ctx = static_cast<SDL_Context*>(userdata);
	std::string file_ending = path.substr(path.rfind('/'), path.length() - path.rfind('/'));
	if (!path.contains("glb")) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "file is not a GLTF file");
		return;
	}
	ctx->loadGLTF(path);
}
// load a shader
SDL_GPUShader* SDL_Context::loadShader(const std::filesystem::path &path) {
	const std::string full_path { SDL_GetBasePath() + std::string("shaders/source/") + path.string() };
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Loading shader: %s", full_path.data());
	if (!SDL_GetPathInfo(full_path.data(), nullptr)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "File does not exist\n\t%s", full_path.data());
		return nullptr;
	}
	SDL_ShaderCross_ShaderStage stage;
	if (path.string().contains("vert")) {
		stage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX;
	} else if (path.string().contains("frag")) {
		stage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
	} else {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not detect shader stage");
		return nullptr;
	}
	size_t hlsl_size;
	void *hlsl = SDL_LoadFile(full_path.data(), &hlsl_size);
	SDL_ShaderCross_HLSL_Info info {
		.source = static_cast<char*>(hlsl),
		.entrypoint = "main",
		.shader_stage = stage,
		.enable_debug = true,
	};
	size_t spirv_size;
	void *spirv = SDL_ShaderCross_CompileSPIRVFromHLSL(&info, &spirv_size);
	SDL_ShaderCross_GraphicsShaderMetadata metadata;
	if (!SDL_ShaderCross_ReflectGraphicsSPIRV(static_cast<Uint8*>(spirv), spirv_size, &metadata)) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "SDL_ShaderCross_ReflectGraphicsSPIRV failed\n\t%s", SDL_GetError());
		return nullptr;
	}
	SDL_GPUShaderFormat platform_format { SDL_GetGPUShaderFormats(m_gpu) };
	SDL_GPUShader *shader;
	if (platform_format & SDL_GPU_SHADERFORMAT_SPIRV) {
		SDL_ShaderCross_SPIRV_Info spirv_info {
			.bytecode = static_cast<Uint8*>(spirv),
			.bytecode_size = spirv_size,
			.entrypoint = info.entrypoint,
			.shader_stage = info.shader_stage,
			.enable_debug = info.enable_debug,
		};
		shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(m_gpu, &spirv_info, &metadata);
	} else {
		SDL_free(spirv);
		shader = SDL_ShaderCross_CompileGraphicsShaderFromHLSL(m_gpu, &info, &metadata);
	}
	if (!shader) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "SDL_ShaderCross_CompileGraphicsShaderFromHLSL failed\n\t%s", SDL_GetError());
		return nullptr;
	}
	SDL_free(hlsl);
	return shader;
}

SDL_AppResult SDL_Context::init() {
	m_window = SDL_CreateWindow("sdl_gltf", m_width, m_height, 0);
	if (!m_window) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed:\n\t%s", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	m_gpu = SDL_CreateGPUDevice(m_supported_formats, true, NULL);
	if (!m_gpu) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateGPUDevice failed:\n\t%s", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	if (!SDL_ClaimWindowForGPUDevice(m_gpu, m_window)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_ClaimWindowForGPUDevice failed:\n\t%s", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	if (!SDL_SetWindowRelativeMouseMode(m_window, true)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"SDL_SetWindowRelativeMouseMode failed:\n\t%s", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	// create shaders
	SDL_Log("Create shaders");
	m_geo_v_shader = loadShader("PositionTransform.vert.hlsl");
	if (!m_geo_v_shader) {
		return SDL_APP_FAILURE;
	}
	m_geo_f_shader = loadShader("SolidColorDepth.frag.hlsl");
	if (!m_geo_f_shader) {
		return SDL_APP_FAILURE;
	}
	m_pp_v_shader = loadShader("Window.vert.hlsl");
	if (!m_pp_v_shader) {
		return SDL_APP_FAILURE;
	}
	m_pp_f_shader = loadShader("DepthOutline.frag.hlsl");
	if (!m_pp_f_shader) {
		return SDL_APP_FAILURE;
	}

	SDL_Log("Create pipelines");
	// create post processing pipeline
	SDL_GPUColorTargetDescription pp_color_target_description {
		.format = SDL_GetGPUSwapchainTextureFormat(m_gpu, m_window),
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
	SDL_GPUGraphicsPipelineCreateInfo pp_create {
		.vertex_shader = m_pp_v_shader,
		.fragment_shader = m_pp_f_shader,
		.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.target_info = {
			.color_target_descriptions = &pp_color_target_description,
			.num_color_targets = 1,
		}
	};
	m_pp_pipeline = SDL_CreateGPUGraphicsPipeline(m_gpu, &pp_create);
	if (!m_pp_pipeline) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateGPUGraphicsPipeline failed:\n\t%s", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	SDL_ReleaseGPUShader(m_gpu, m_pp_v_shader);
	SDL_ReleaseGPUShader(m_gpu, m_pp_f_shader);

	// create geometry pipeline
	std::vector<SDL_GPUVertexBufferDescription> vert_buf_descriptions { {
			.slot = 0,
			.pitch = sizeof(glm::vec3),
			.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
			.instance_step_rate = 0,
		}, {
			.slot = 1,
			.pitch = sizeof(glm::vec3),
			.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
			.instance_step_rate = 0,
		}
	};
	std::vector<SDL_GPUVertexAttribute> vert_attribs { {
			.location = 0,
			.buffer_slot = 0,
			.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
			.offset = 0
		}, {
			.location = 1,
			.buffer_slot = 1,
			.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
			.offset = 0,
		}, {
			.location = 2,
			.buffer_slot = 1,
			.format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
			.offset = 0
	}};
	SDL_GPUColorTargetDescription geo_color_target_description {
		.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
	};
	SDL_GPUGraphicsPipelineCreateInfo geo_create {
		.vertex_shader = m_geo_v_shader,
		.fragment_shader = m_geo_f_shader,
		.vertex_input_state = {
			.vertex_buffer_descriptions = vert_buf_descriptions.data(),
			.num_vertex_buffers = static_cast<Uint32>(vert_buf_descriptions.size()),
			.vertex_attributes = vert_attribs.data(),
			.num_vertex_attributes = static_cast<Uint32>(vert_attribs.size()),
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
			.color_target_descriptions = &geo_color_target_description,
			.num_color_targets = 1,
			.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
			.has_depth_stencil_target = true,
		},
	};
	m_geo_pipeline = SDL_CreateGPUGraphicsPipeline(m_gpu, &geo_create);
	if (!m_geo_pipeline) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateGPUGraphicsPipeline failed:\n\t%s", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	SDL_ReleaseGPUShader(m_gpu, m_geo_v_shader);
	SDL_ReleaseGPUShader(m_gpu, m_geo_f_shader);

	SDL_Log("Create textures");
	// create textures
	SDL_GPUTextureCreateInfo depth_create {
		.type = SDL_GPU_TEXTURETYPE_2D,
		.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
		.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
		.width = m_width,
		.height = m_height,
		.layer_count_or_depth = 1,
		.num_levels = 1,
		.sample_count = SDL_GPU_SAMPLECOUNT_1,
	};
	m_depth = SDL_CreateGPUTexture(m_gpu, &depth_create);
	if (!m_depth) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateGPUTexture failed:\n\t%s", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	SDL_GPUTextureCreateInfo color_create {
		.type = SDL_GPU_TEXTURETYPE_2D,
		.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
		.width = m_width,
		.height = m_height,
		.layer_count_or_depth = 1,
		.num_levels = 1,
		.sample_count = SDL_GPU_SAMPLECOUNT_1,
	};
	m_color = SDL_CreateGPUTexture(m_gpu, &color_create);
	if (!m_color) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateGPUTexture failed:\n\t%s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	SDL_Log("Create sampler");
	// create depth sampler
	SDL_GPUSamplerCreateInfo depth_sampler_create {
		.min_filter = SDL_GPU_FILTER_NEAREST,
		.mag_filter = SDL_GPU_FILTER_NEAREST,
		.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
		.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
	};
	m_depth_sampler = SDL_CreateGPUSampler(m_gpu, &depth_sampler_create);
	if (!m_depth_sampler) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateGPUSampler failed:\n\t%s", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	loadGLTF(SDL_GetBasePath() + std::string("meshes/cubes.glb"));
	const glm::vec3 pos {60, 60, 60};
	const glm::mat4 rot_mat { glm::lookAt(pos, {0, 0, 0}, {0, 1, 0}) };
	const glm::quat rot_quat { glm::quat_cast(rot_mat) };
	const glm::vec2 dimensions { m_width, m_height };
	Camera cam {pos, rot_quat, dimensions};
	m_cams.push_back(cam);
	return SDL_APP_CONTINUE;
}

void SDL_Context::quit() {
	SDL_ReleaseGPUTexture(m_gpu, m_color);
	SDL_ReleaseGPUTexture(m_gpu, m_depth);
	SDL_ReleaseGPUSampler(m_gpu, m_depth_sampler);
	SDL_ReleaseGPUGraphicsPipeline(m_gpu, m_pp_pipeline);
	SDL_ReleaseGPUGraphicsPipeline(m_gpu, m_geo_pipeline);
	if (m_v_buf) {
		SDL_ReleaseGPUBuffer(m_gpu, m_v_buf);
	}
	if (m_i_buf) {
		SDL_ReleaseGPUBuffer(m_gpu, m_i_buf);
	}
	SDL_DestroyGPUDevice(m_gpu);
	SDL_DestroyWindow(m_window);
}

SDL_AppResult SDL_Context::event(SDL_Event *e) {
	Camera &active { m_cams.at(m_active_cam_index) };
	active.event(e);
	switch(e->type) {
	case SDL_EVENT_WINDOW_RESIZED: {
		m_width = e->window.data1;
		m_height = e->window.data2;
		// resize textures
		SDL_ReleaseGPUTexture(m_gpu, m_depth);
		SDL_GPUTextureCreateInfo depth_create {
			.type = SDL_GPU_TEXTURETYPE_2D,
			.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
			.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
			.width = m_width,
			.height = m_height,
			.layer_count_or_depth = 1,
			.num_levels = 1,
			.sample_count = SDL_GPU_SAMPLECOUNT_1,
		};
		m_depth = SDL_CreateGPUTexture(m_gpu, &depth_create);
		if (!m_depth) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateGPUTexture failed:\n\t%s", SDL_GetError());
			return SDL_APP_FAILURE;
		}
		SDL_ReleaseGPUTexture(m_gpu, m_color);
		SDL_GPUTextureCreateInfo color_create {
			.type = SDL_GPU_TEXTURETYPE_2D,
			.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
			.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
			.width = m_width,
			.height = m_height,
			.layer_count_or_depth = 1,
			.num_levels = 1,
			.sample_count = SDL_GPU_SAMPLECOUNT_1,
		};
		m_color = SDL_CreateGPUTexture(m_gpu, &color_create);
		if (!m_color) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateGPUTexture failed:\n\t%s", SDL_GetError());
			return SDL_APP_FAILURE;
		}
		break;
	}
	case SDL_EVENT_KEY_DOWN:
		switch(e->key.key) {
		case SDLK_R:
			openGLTF();
			break;
		}
		break;
	}
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_Context::iterate() {
	for (Camera &cam : m_cams) {
		cam.iterate();
	}
	SDL_GPUColorTargetInfo color_target_info {
		.texture = m_color,
		.clear_color = {0, 0, 0, 0},
		.load_op = SDL_GPU_LOADOP_CLEAR,
		.store_op = SDL_GPU_STOREOP_STORE
	};
	SDL_GPUDepthStencilTargetInfo depth_stencil_target_info {
		.texture = m_depth,
		.clear_depth = 1,
		.load_op = SDL_GPU_LOADOP_CLEAR,
		.store_op = SDL_GPU_STOREOP_STORE,
		.stencil_load_op = SDL_GPU_LOADOP_CLEAR,
		.stencil_store_op = SDL_GPU_STOREOP_STORE,
		.cycle = true,
		.clear_stencil = 0,
	};

	// push calculations to gpu shaders
	SDL_GPUCommandBuffer *cmdbuf { SDL_AcquireGPUCommandBuffer(m_gpu) };
	if (!cmdbuf) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_AcquireGPUCommandBuffer failed\n\t%s", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	SDL_GPUTexture *swapchain;
	if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, m_window, &swapchain, &m_width, &m_height)) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "SDL_WaitAndAcquireGPUSwapchainTexture failed\n\t%s", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	float aspect_ratio { static_cast<float>(m_width) / static_cast<float>(m_height) };
	if (!swapchain) { return SDL_APP_FAILURE; }
	const Camera &active { m_cams.at(m_active_cam_index) };
	FragmentUniforms frag_uniforms { 
		active.getNearFar(),
		active.getPosition()
	};
	SDL_PushGPUFragmentUniformData(cmdbuf, 0, &frag_uniforms, sizeof(frag_uniforms));

	// render geometry to color & depth textures
	SDL_GPURenderPass *render_pass { SDL_BeginGPURenderPass(cmdbuf, &color_target_info, 1, &depth_stencil_target_info) };
	if (m_i_buf && m_v_buf && m_norm_buf) {
		SDL_GPUBufferBinding i_buf_binding {
			.buffer = m_i_buf,
			.offset = 0
		};
		std::vector<SDL_GPUBufferBinding> vert_buf_bindings { {
				.buffer = m_v_buf,
				.offset = 0
			}, {
				.buffer = m_norm_buf,
				.offset = 0
		} };
		SDL_BindGPUVertexBuffers(render_pass, 0, vert_buf_bindings.data(), vert_buf_bindings.size());
		SDL_BindGPUIndexBuffer(render_pass, &i_buf_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
	}
	SDL_BindGPUGraphicsPipeline(render_pass, m_geo_pipeline);
	// calculate perspective projection & view
	// sort objects so that
	// farthest object is at start and
	// closest object is at end
	std::sort(m_objects.begin(), m_objects.end(), [active](const Mesh &a, const Mesh &b) -> bool {
		const float dist_a { glm::distance(active.getPosition(), a.pos) };
		const float dist_b { glm::distance(active.getPosition(), b.pos) };
		return dist_a ? dist_a > dist_b : dist_b;
	});
	VertexUniforms vert_uniforms;
	vert_uniforms.proj_view = active.proj() * active.view();
	for (const Mesh &mesh : m_objects) {
		vert_uniforms.model = mesh.model_mat();
		SDL_PushGPUVertexUniformData(cmdbuf, 0, &vert_uniforms, sizeof(vert_uniforms));
		SDL_DrawGPUIndexedPrimitives(render_pass, mesh.num_indices, 1, mesh.first_index, 0, 0);
	}
	SDL_EndGPURenderPass(render_pass);
// render color & depth textures to window
	SDL_GPUColorTargetInfo swapchain_target_info {
		.texture = swapchain,
		.clear_color = {0.2f, 0.5f, 0.4f, 1.0f},
		.load_op = SDL_GPU_LOADOP_CLEAR,
		.store_op = SDL_GPU_STOREOP_STORE
	};
	render_pass = SDL_BeginGPURenderPass(cmdbuf, &swapchain_target_info, 1, nullptr);
	SDL_BindGPUGraphicsPipeline(render_pass, m_pp_pipeline);
	SDL_GPUTextureSamplerBinding sampler_bindings[] {
		{ .texture = m_color, .sampler = m_depth_sampler },
		{ .texture = m_depth, .sampler = m_depth_sampler },
	};
	SDL_BindGPUFragmentSamplers(render_pass, 0, sampler_bindings, 2);
	SDL_DrawGPUPrimitives(render_pass, 6, 1, 0, 0);
	SDL_EndGPURenderPass(render_pass);
	SDL_SubmitGPUCommandBuffer(cmdbuf);
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_Context::openGLTF() {
	const std::vector<SDL_DialogFileFilter> filter = {
		{ "GLB", "glb" },
		{ "GLTF", "gltf" },
	};
	SDL_ShowOpenFileDialog(callback, this, m_window, filter.data(), filter.size(), (SDL_GetBasePath() + std::string("meshes")).data(), 1);
	return SDL_APP_CONTINUE;
}

void SDL_Context::loadGLTF(const std::filesystem::path& path) {
	m_objects.clear();
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Loading GLTF file: %s", path.c_str());
	fastgltf::Parser parser;
	fastgltf::Expected<fastgltf::GltfDataBuffer> data = fastgltf::GltfDataBuffer::FromPath(path);
	if (data.error() != fastgltf::Error::None) {
		switch(data.error()) {
		case fastgltf::Error::InvalidPath:
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Invalid path, resuming application");
			break;
		case fastgltf::Error::InvalidFileData:
		case fastgltf::Error::InvalidGLB:
		case fastgltf::Error::InvalidGltf:
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Invalid data, resuming application");
			break;
		default:
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Error occured while parsing file, resuming application");
			break;
		}
		return;
	}
	auto asset { parser.loadGltf(
			data.get(),
			path.parent_path(),
			fastgltf::Options::DecomposeNodeMatrices |
			fastgltf::Options::LoadExternalBuffers |
			fastgltf::Options::GenerateMeshIndices
	) };
	if (asset.error() != fastgltf::Error::None) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Error occured while loading gltf, resuming application");
		return;
	}
	const fastgltf::Scene scene { asset->scenes.at(asset->defaultScene.value()) };
	Uint32 v_total_buf_size, i_total_buf_size, norm_total_buf_size;
	Uint32 v_total_count, i_total_count, norm_total_count;

	// traverse nodes
	fastgltf::iterateSceneNodes(asset.get(), asset->defaultScene.value(), fastgltf::math::fmat4x4(), 
								[&](fastgltf::Node &node, fastgltf::math::fmat4x4 TRS) {
		glm::vec3 pos, scale;
		glm::quat rot;
		if (const auto TRS (std::get_if<fastgltf::TRS>(&node.transform)); TRS) {
			pos = {TRS->translation.x(), TRS->translation.y(), TRS->translation.z()};
			scale = {TRS->scale.x(), TRS->scale.y(), TRS->scale.z()};
			rot = {TRS->rotation.w(), TRS->rotation.x(), TRS->rotation.y(), TRS->rotation.z()};
		} else {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Node transforms must be decomposed");
			return;
		}
		Uint32 i_count_start { i_total_count };
		SDL_assert(node.meshIndex.has_value());
		const fastgltf::Mesh &mesh { asset->meshes.at(node.meshIndex.value()) };
		for (const fastgltf::Primitive &prim : mesh.primitives) {
			const fastgltf::Attribute *pos { prim.findAttribute("POSITION") };
			const fastgltf::Attribute *norm { prim.findAttribute("NORMAL") };
			SDL_assert(pos);
			SDL_assert(prim.indicesAccessor.has_value());
			SDL_assert(norm);
			fastgltf::Accessor &vertex_access { asset->accessors.at(pos->accessorIndex) };
			fastgltf::Accessor &index_access { asset->accessors.at(prim.indicesAccessor.value()) };
			fastgltf::Accessor &norm_access { asset->accessors.at(norm->accessorIndex) };
			SDL_assert(vertex_access.bufferViewIndex.has_value());
			SDL_assert(index_access.bufferViewIndex.has_value());
			SDL_assert(norm_access.bufferViewIndex.has_value());
			i_total_buf_size += asset->bufferViews.at(index_access.bufferViewIndex.value()).byteLength;
			i_total_count += index_access.count;
			v_total_buf_size += asset->bufferViews.at(vertex_access.bufferViewIndex.value()).byteLength;
			v_total_count += vertex_access.count;
			norm_total_buf_size += asset->bufferViews.at(norm_access.bufferViewIndex.value()).byteLength;
			norm_total_count += norm_access.count;
		}
		m_objects.emplace_back(pos, scale, rot, i_total_count - i_count_start, i_count_start);
	});
	// create buffers
	SDL_GPUBufferCreateInfo i_buf_create {
		.usage = SDL_GPU_BUFFERUSAGE_INDEX,
		.size = i_total_buf_size,
	};
	SDL_GPUBufferCreateInfo v_buf_create {
		.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
		.size = v_total_buf_size,
	};
	SDL_GPUBufferCreateInfo norm_buf_create {
		.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
		.size = norm_total_buf_size
	};
	// if there already is a buffer, release it
	if (m_i_buf) { SDL_ReleaseGPUBuffer(m_gpu, m_i_buf); }
	m_i_buf = SDL_CreateGPUBuffer(m_gpu, &i_buf_create);
	if (!m_i_buf) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateGPUBuffer failed:\n\t%s", SDL_GetError());
		return;
	}
	if (m_v_buf) { SDL_ReleaseGPUBuffer(m_gpu, m_v_buf); }
	m_v_buf = SDL_CreateGPUBuffer(m_gpu, &v_buf_create);
	if (!m_v_buf) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateGPUBuffer failed:\n\t%s", SDL_GetError());
		return;
	}
	if (m_norm_buf) { SDL_ReleaseGPUBuffer(m_gpu, m_norm_buf); }
	m_norm_buf = SDL_CreateGPUBuffer(m_gpu, &norm_buf_create);
	if (!m_norm_buf) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateGPUBuffer failed:\n\t%s", SDL_GetError());
	}

	Uint32 i_buf_offset { }, v_buf_offset { }, norm_buf_offset { };
	SDL_GPUCommandBuffer *cmdbuf { SDL_AcquireGPUCommandBuffer(m_gpu) };
	fastgltf::iterateSceneNodes(asset.get(), asset->defaultScene.value(), fastgltf::math::fmat4x4(), 
								[&, cmdbuf](fastgltf::Node &node, fastgltf::math::fmat4x4 TRS) {
		const fastgltf::Mesh &mesh { asset->meshes.at(node.meshIndex.value()) };
		// traverse primitives of this node's mesh
		for (const fastgltf::Primitive &prim : mesh.primitives) {
			// get index info
			const fastgltf::Accessor &i_access { 
				asset->accessors.at( prim.indicesAccessor.value() )
			};
			const fastgltf::BufferView &i_buffer_view {
				asset->bufferViews.at( i_access.bufferViewIndex.value() )
			};
			const Uint32 i_buf_size { static_cast<Uint32>(i_buffer_view.byteLength) };
			const fastgltf::Buffer &i_buffer { asset->buffers.at(i_buffer_view.bufferIndex) };
			// get vert info
			const fastgltf::Accessor &v_access {
				asset->accessors.at( prim.findAttribute("POSITION")->accessorIndex )
			};
			const fastgltf::BufferView &v_buffer_view { 
				asset->bufferViews.at( v_access.bufferViewIndex.value() )
			};
			const Uint32 v_buf_size { static_cast<Uint32>(v_buffer_view.byteLength) };
			const fastgltf::Buffer &v_buffer { asset->buffers.at(v_buffer_view.bufferIndex) };
			// get normals info
			const fastgltf::Accessor &norm_access {
				asset->accessors.at( prim.findAttribute("NORMAL")->accessorIndex )
			};
			const fastgltf::BufferView &norm_buffer_view {
				asset->bufferViews.at( norm_access.bufferViewIndex.value() )
			};
			const Uint32 norm_buf_size { static_cast<Uint32>(norm_buffer_view.byteLength) };
			const fastgltf::Buffer &norm_buffer {asset->buffers.at(norm_buffer_view.bufferIndex) };

			// create transfer buffer
			SDL_GPUTransferBufferCreateInfo trans_buf_create {
				.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
				.size = i_buf_size + v_buf_size + norm_buf_size,
			};
			SDL_GPUTransferBuffer *trans_buf = SDL_CreateGPUTransferBuffer(m_gpu, &trans_buf_create);
			if (!trans_buf) {
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateGPUTransferBuffer failed:\n\t%s", SDL_GetError());
				return;
			}
			Uint16 *i_data { static_cast<Uint16*>(SDL_MapGPUTransferBuffer(m_gpu, trans_buf, false)) };
			fastgltf::copyFromAccessor<Uint16>(asset.get(), i_access, i_data);
			glm::vec3 *v_data { reinterpret_cast<glm::vec3*>(i_data + i_access.count) };
			fastgltf::copyFromAccessor<glm::vec3>(asset.get(), v_access, v_data);
			glm::vec3 *norm_data { v_data + v_access.count };
			fastgltf::copyFromAccessor<glm::vec3>(asset.get(), norm_access, norm_data);
			SDL_UnmapGPUTransferBuffer(m_gpu, trans_buf);
			// move data to gpu using copy pass
			SDL_GPUCopyPass *copypass { SDL_BeginGPUCopyPass(cmdbuf) };
			SDL_GPUTransferBufferLocation trans_buf_loc {
				.transfer_buffer = trans_buf,
				.offset = 0
			};
			SDL_GPUBufferRegion i_region {
				.buffer = m_i_buf,
				.offset = i_buf_offset,
				.size = i_buf_size
			};
			SDL_GPUBufferRegion v_region {
				.buffer = m_v_buf,
				.offset = v_buf_offset,
				.size = v_buf_size
			};
			SDL_GPUBufferRegion norm_region {
				.buffer = m_norm_buf,
				.offset = norm_buf_offset,
				.size = norm_buf_size
			};
			SDL_UploadToGPUBuffer(copypass, &trans_buf_loc, &i_region, false);
			trans_buf_loc.offset += i_buf_size;
			SDL_UploadToGPUBuffer(copypass, &trans_buf_loc, &v_region, false);
			trans_buf_loc.offset += v_buf_size;
			SDL_UploadToGPUBuffer(copypass, &trans_buf_loc, &norm_region, false);
			SDL_EndGPUCopyPass(copypass);
			SDL_ReleaseGPUTransferBuffer(m_gpu, trans_buf);
			i_buf_offset += i_buf_size;
			v_buf_offset += v_buf_size;
			norm_buf_offset += norm_buf_size;
		}
	});
	SDL_SubmitGPUCommandBuffer(cmdbuf);
}

glm::mat4 Camera::view() const {
	return glm::mat4_cast(m_rot) * glm::translate(glm::mat4(1), -m_pos);
}
glm::mat4 Camera::proj() const {
	return glm::perspective(SDL_PI_F * 0.25f, m_dimensions.x / m_dimensions.y, m_near_far.x, m_near_far.y);
}
void Camera::resize(const float &width, const float &height) {
	m_dimensions = {width, height};
}
glm::vec3 Camera::forward() const {
	return glm::conjugate(m_rot) * glm::vec3(0.0f, 0.0f, -1.0f);
}
glm::vec3 Camera::up() const {
	return glm::conjugate(m_rot) * glm::vec3(0.0f, 1.0f, 0.0f);
}
glm::vec3 Camera::right() const {
	return glm::conjugate(m_rot) * glm::vec3(1.0f, 0.0f, 0.0f);
}
void Camera::rot(const float &pitch, const float &yaw) {
	m_rot = glm::rotate(m_rot, pitch, right());
	m_rot = glm::rotate(m_rot, yaw, {0, 1, 0});
}
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
