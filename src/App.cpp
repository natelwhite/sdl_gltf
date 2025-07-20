#include <fastgltf/core.hpp>

#include "App.hpp"

// callback function for opening files
void SDLCALL fileDialogue(void* userdata, const char* const* filelist, int filter) {
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
	App* ctx = static_cast<App*>(userdata);
	std::string file_ending = path.substr(path.rfind('/'), path.length() - path.rfind('/'));
	if (!path.contains("glb")) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "file is not a GLTF file");
		return;
	}
	ctx->loadGLTF(path);
}

// load a shader
SDL_GPUShader* App::createShader(GPUResource<SHADER> *shader, const std::string &filename, const Uint32 &num_samplers, const Uint32 &num_storage_textures, const Uint32 &num_storage_buffers, const Uint32 &num_uniform_buffers) {
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

	SDL_GPUShaderFormat valid_formats = SDL_GetGPUShaderFormats(m_gpu);
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
	shader->create(m_gpu);
	SDL_free(code);
	return shader->get();
}

SDL_AppResult App::init() {
	m_window = SDL_CreateWindow("sdl_gltf", m_width, m_height, m_window_flags);
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
	if (!createShader(&m_geo_v_shader, "PositionTransform.vert", 0, 0, 0, 1))
		return SDL_APP_FAILURE;
	if (!createShader(&m_geo_f_shader, "SolidColorDepth.frag", 0, 0, 0, 1))
		return SDL_APP_FAILURE;
	if (!createShader(&m_pp_v_shader, "Window.vert", 0, 0, 0, 0))
		return SDL_APP_FAILURE;
	if (!createShader(&m_pp_f_shader, "DepthOutline.frag", 2, 0, 0, 1))
		return SDL_APP_FAILURE;

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
	m_pp_pipeline.info = {
		.vertex_shader = m_pp_v_shader.get(),
		.fragment_shader = m_pp_f_shader.get(),
		.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.target_info = {
			.color_target_descriptions = &pp_color_target_description,
			.num_color_targets = 1,
		}
	};
	m_pp_pipeline.create(m_gpu);
	m_pp_v_shader.release();
	m_pp_f_shader.release();

	// create geometry pipeline
	SDL_GPUVertexBufferDescription vert_buf_descriptions[2] { {
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
	SDL_GPUVertexAttribute vert_attribs[2] { {
			.location = 0,
			.buffer_slot = 0,
			.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
			.offset = 0
		}, {
			.location = 1,
			.buffer_slot = 1,
			.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
			.offset = 0,
	}};
	SDL_GPUColorTargetDescription geo_color_target_description {
		.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
	};
	m_geo_pipeline.info = {
		.vertex_shader = m_geo_v_shader.get(),
		.fragment_shader = m_geo_f_shader.get(),
		.vertex_input_state = {
			.vertex_buffer_descriptions = vert_buf_descriptions,
			.num_vertex_buffers = SDL_arraysize(vert_buf_descriptions),
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
			.color_target_descriptions = &geo_color_target_description,
			.num_color_targets = 1,
			.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
			.has_depth_stencil_target = true,
		},
	};
	m_geo_pipeline.create(m_gpu);
	m_geo_v_shader.release();
	m_pp_f_shader.release();

	// create textures
	m_depth.info = {
		.type = SDL_GPU_TEXTURETYPE_2D,
		.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
		.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
		.width = m_width,
		.height = m_height,
		.layer_count_or_depth = 1,
		.num_levels = 1,
		.sample_count = SDL_GPU_SAMPLECOUNT_1,
	};
	if (!m_depth.create(m_gpu)) { return SDL_APP_FAILURE; }
	m_color.info = {
		.type = SDL_GPU_TEXTURETYPE_2D,
		.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
		.width = m_width,
		.height = m_height,
		.layer_count_or_depth = 1,
		.num_levels = 1,
		.sample_count = SDL_GPU_SAMPLECOUNT_1,
	};
	if (!m_color.create(m_gpu)) { return SDL_APP_FAILURE; }

	// create depth sampler
	m_depth_sampler.info = {
		.min_filter = SDL_GPU_FILTER_NEAREST,
		.mag_filter = SDL_GPU_FILTER_NEAREST,
		.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
		.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
	};
	if (!m_depth_sampler.create(m_gpu)) { return SDL_APP_FAILURE; }
	loadGLTF(SDL_GetBasePath() + std::string("meshes/cubes.glb"));
	return SDL_APP_CONTINUE;
}

void App::quit() {
	m_color.release();
	m_depth.release();
	m_depth_sampler.release();
	m_pp_pipeline.release();
	m_geo_pipeline.release();
	m_i_buf.release();
	m_v_buf.release();
	m_norm_buf.release();
	SDL_DestroyGPUDevice(m_gpu);
	SDL_DestroyWindow(m_window);
}

SDL_AppResult App::event(SDL_Event *e) {
	m_camera.event(e);
	switch(e->type) {
	case SDL_EVENT_WINDOW_RESIZED: {
		m_width = e->window.data1;
		m_height = e->window.data2;
		// resize textures
		m_depth.release();
		m_depth.info.width = m_width;
		m_depth.info.height = m_height;
		m_depth.create(m_gpu);
		m_color.release();
		m_color.info.width = m_width;
		m_color.info.height = m_height;
		m_color.create(m_gpu);
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

SDL_AppResult App::iterate() {
	m_camera.iterate();
	SDL_GPUColorTargetInfo color_target_info {
		.texture = m_color.get(),
		.clear_color = {0, 0, 0, 0},
		.load_op = SDL_GPU_LOADOP_CLEAR,
		.store_op = SDL_GPU_STOREOP_STORE
	};
	SDL_GPUDepthStencilTargetInfo depth_stencil_target_info {
		.texture = m_depth.get(),
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
	if (!swapchain) { return SDL_APP_FAILURE; }
	FragmentUniforms frag_uniforms { 
		m_camera.getNearFar(),
		m_camera.getPosition()
	};
	SDL_PushGPUFragmentUniformData(cmdbuf, 0, &frag_uniforms, sizeof(frag_uniforms));

	// render geometry to color & depth textures
	SDL_GPURenderPass *render_pass { SDL_BeginGPURenderPass(cmdbuf, &color_target_info, 1, &depth_stencil_target_info) };
	SDL_GPUBufferBinding i_buf_binding {
		.buffer = m_i_buf.get(),
		.offset = 0
	};
	SDL_GPUBufferBinding vert_buf_bindings[2] { {
			.buffer = m_v_buf.get(),
			.offset = 0
		}, {
			.buffer = m_norm_buf.get(),
			.offset = 0
	} };
	SDL_BindGPUVertexBuffers(render_pass, 0, vert_buf_bindings, SDL_arraysize(vert_buf_bindings));
	SDL_BindGPUIndexBuffer(render_pass, &i_buf_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
	SDL_BindGPUGraphicsPipeline(render_pass, m_geo_pipeline.get());
	// calculate perspective projection & view
	// sort objects so that
	// farthest object is at start and
	// closest object is at end
	std::sort(m_objects.begin(), m_objects.end(), [&](const Mesh &a, const Mesh &b) -> bool {
		const float dist_a { glm::distance(m_camera.getPosition(), a.pos) };
		const float dist_b { glm::distance(m_camera.getPosition(), b.pos) };
		return dist_a ? dist_a > dist_b : dist_b;
	});
	VertexUniforms vert_uniforms;
	vert_uniforms.proj_view = m_camera.proj() * m_camera.view();
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
	SDL_BindGPUGraphicsPipeline(render_pass, m_pp_pipeline.get());
	SDL_GPUTextureSamplerBinding sampler_bindings[] {
		{ .texture = m_color.get(), .sampler = m_depth_sampler.get() },
		{ .texture = m_depth.get(), .sampler = m_depth_sampler.get() },
	};
	SDL_BindGPUFragmentSamplers(render_pass, 0, sampler_bindings, 2);
	SDL_DrawGPUPrimitives(render_pass, 6, 1, 0, 0);
	SDL_EndGPURenderPass(render_pass);
	SDL_SubmitGPUCommandBuffer(cmdbuf);
	return SDL_APP_CONTINUE;
}

SDL_AppResult App::openGLTF() {
	const SDL_DialogFileFilter filter[2] = {
		{ "GLB", "glb" },
		{ "GLTF", "gltf" },
	};
	SDL_ShowOpenFileDialog(fileDialogue, this, m_window, filter, SDL_arraysize(filter), (SDL_GetBasePath() + std::string("meshes")).data(), 1);
	return SDL_APP_CONTINUE;
}

void App::loadGLTF(const std::filesystem::path& path) {
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

	auto sizeOfBuffer = [&](const fastgltf::Accessor &access) -> Uint32 {
		SDL_assert(access.bufferViewIndex.has_value());
		return static_cast<Uint32>(asset->bufferViews.at(access.bufferViewIndex.value()).byteLength);
	};

	auto processPrimitive = [&](const fastgltf::Primitive &prim) -> GeometryAllocationInfo {
		switch(prim.type) {
			case fastgltf::PrimitiveType::Triangles:
				break;
			default:
				SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Primitive types other than triangle lists are not supported");
				break;
		}
		const fastgltf::Attribute *pos { prim.findAttribute("POSITION") };
		const fastgltf::Attribute *norm { prim.findAttribute("NORMAL") };

		SDL_assert(prim.indicesAccessor.has_value());
		SDL_assert(pos);
		SDL_assert(norm);
		fastgltf::Accessor &index_access { asset->accessors.at(prim.indicesAccessor.value()) };
		fastgltf::Accessor &vertex_access { asset->accessors.at(pos->accessorIndex) };
		fastgltf::Accessor &normal_access { asset->accessors.at(norm->accessorIndex) };
		const GeometryAllocationInfo info {
			{ sizeOfBuffer(index_access), static_cast<Uint32>(index_access.count), },
			{ sizeOfBuffer(vertex_access), static_cast<Uint32>(vertex_access.count) },
			{ sizeOfBuffer(normal_access), static_cast<Uint32>(normal_access.count) }
		};
		return info;
	};

	auto processMesh = [processPrimitive](const fastgltf::Mesh &mesh) -> GeometryAllocationInfo {
		GeometryAllocationInfo info {};
		for (const fastgltf::Primitive &prim : mesh.primitives) {
			info += processPrimitive(prim);
		}
		return info;
	};
	GeometryAllocationInfo scene_buffer_info;
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
		const Uint32 index_start { scene_buffer_info.indices.count };
		SDL_assert(node.meshIndex.has_value());
		const fastgltf::Mesh &mesh { asset->meshes.at(node.meshIndex.value()) };
		const GeometryAllocationInfo mesh_info { processMesh(mesh) };
		scene_buffer_info += mesh_info;
		m_objects.emplace_back(pos, scale, rot, mesh_info.indices.count, index_start);
	});
	// create buffers
	m_i_buf.info = { SDL_GPU_BUFFERUSAGE_INDEX, scene_buffer_info.indices.bytes };
	m_v_buf.info = { SDL_GPU_BUFFERUSAGE_VERTEX, scene_buffer_info.verts.bytes };
	m_norm_buf.info = { SDL_GPU_BUFFERUSAGE_VERTEX, scene_buffer_info.norms.bytes };
	// if there already is a buffer, release it
	if (m_i_buf.get()) { m_i_buf.release(); }
	if (!m_i_buf.create(m_gpu)) { return; }
	if (m_v_buf.get()) { m_v_buf.release(); }
	if (!m_v_buf.create(m_gpu)) { return; }
	if (m_norm_buf.get()) { m_norm_buf.release(); }
	if (!m_norm_buf.create(m_gpu)) { return; }

	// returns allocation information for uploaded primitive
	auto uploadPrimitive = [&](SDL_GPUCopyPass *copypass, const fastgltf::Primitive &prim, const GeometryAllocationInfo &offsets) -> GeometryAllocationInfo {
		// get index info
		const fastgltf::Accessor &i_access { 
			asset->accessors.at( prim.indicesAccessor.value() )
		};
		// get vert info
		const fastgltf::Accessor &v_access {
			asset->accessors.at( prim.findAttribute("POSITION")->accessorIndex )
		};
		// get normals info
		const fastgltf::Accessor &norm_access {
			asset->accessors.at( prim.findAttribute("NORMAL")->accessorIndex )
		};

		const GeometryAllocationInfo prim_info {
			{ sizeOfBuffer(i_access), static_cast<Uint32>(i_access.count), },
			{ sizeOfBuffer(v_access), static_cast<Uint32>(v_access.count) },
			{ sizeOfBuffer(norm_access), static_cast<Uint32>(norm_access.count) }
		};
		// create transfer buffer
		SDL_GPUTransferBufferCreateInfo trans_buf_create {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = prim_info.indices.bytes + 
					prim_info.verts.bytes + 
					prim_info.norms.bytes,
		};
		SDL_GPUTransferBuffer *trans_buf = SDL_CreateGPUTransferBuffer(m_gpu, &trans_buf_create);
		if (!trans_buf) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateGPUTransferBuffer failed:\n\t%s", SDL_GetError());
			return { };
		}
		Uint16 *i_data { static_cast<Uint16*>(SDL_MapGPUTransferBuffer(m_gpu, trans_buf, false)) };
		fastgltf::copyFromAccessor<Uint16>(asset.get(), i_access, i_data);
		glm::vec3 *v_data { reinterpret_cast<glm::vec3*>(i_data + prim_info.indices.count) };
		fastgltf::copyFromAccessor<glm::vec3>(asset.get(), v_access, v_data);
		glm::vec3 *norm_data { v_data + v_access.count };
		fastgltf::copyFromAccessor<glm::vec3>(asset.get(), norm_access, norm_data);
		SDL_UnmapGPUTransferBuffer(m_gpu, trans_buf);
		// move data to gpu using copy pass
		SDL_GPUTransferBufferLocation trans_buf_loc {
			.transfer_buffer = trans_buf,
			.offset = 0
		};
		const SDL_GPUBufferRegion attribute_regions[3] {
			{ m_i_buf.get(), offsets.indices.bytes, prim_info.indices.bytes },
			{ m_v_buf.get(), offsets.verts.bytes, prim_info.verts.bytes },
			{ m_norm_buf.get(), offsets.norms.bytes, prim_info.norms.bytes }
		};
		SDL_UploadToGPUBuffer(copypass, &trans_buf_loc, &attribute_regions[0], false);
		trans_buf_loc.offset += prim_info.indices.bytes;
		SDL_UploadToGPUBuffer(copypass, &trans_buf_loc, &attribute_regions[1], false);
		trans_buf_loc.offset += prim_info.verts.bytes;
		SDL_UploadToGPUBuffer(copypass, &trans_buf_loc, &attribute_regions[2], false);
		SDL_ReleaseGPUTransferBuffer(m_gpu, trans_buf);
		return prim_info;
	};

	SDL_GPUCommandBuffer *cmdbuf { SDL_AcquireGPUCommandBuffer(m_gpu) };
	GeometryAllocationInfo offsets { };
	SDL_GPUCopyPass *copypass { SDL_BeginGPUCopyPass(cmdbuf) };
	fastgltf::iterateSceneNodes(asset.get(), asset->defaultScene.value(), fastgltf::math::fmat4x4(), 
								[&](fastgltf::Node &node, fastgltf::math::fmat4x4 TRS) {
		const fastgltf::Mesh &mesh { asset->meshes.at(node.meshIndex.value()) };
		// traverse primitives of this node's mesh
		for (const fastgltf::Primitive &prim : mesh.primitives) {
			GeometryAllocationInfo uploaded { uploadPrimitive(copypass, prim, offsets) };
			offsets += uploaded;
		}
	});
	SDL_EndGPUCopyPass(copypass);
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
