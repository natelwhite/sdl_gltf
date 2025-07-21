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

	// init pipelines
	if (m_blinnphong_pipeline.init(m_gpu) == SDL_APP_FAILURE)
		return SDL_APP_FAILURE;
	if (m_outline_pipeline.init(m_window, m_gpu) == SDL_APP_FAILURE)
		return SDL_APP_FAILURE;

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

	loadGLTF(SDL_GetBasePath() + std::string("meshes/cubes.glb"));
	return SDL_APP_CONTINUE;
}

void App::quit() {
	m_color.release();
	m_depth.release();
	m_blinnphong_pipeline.quit();
	m_outline_pipeline.quit();
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
	// update camera data & sort objects by distance from camera
	m_camera.iterate();
	std::sort(m_objects.begin(), m_objects.end(), [&](const Mesh &a, const Mesh &b) -> bool {
		const float dist_a { glm::distance(m_camera.getPosition(), a.pos) };
		const float dist_b { glm::distance(m_camera.getPosition(), b.pos) };
		return dist_a ? dist_a > dist_b : dist_b;
	});

	SDL_GPUCommandBuffer *cmdbuf { SDL_AcquireGPUCommandBuffer(m_gpu) };
	if (!cmdbuf) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_AcquireGPUCommandBuffer failed\n\t%s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	// render geometry to color & depth textures
	m_blinnphong_pipeline.render(cmdbuf, m_color, m_depth, m_camera, m_objects, m_i_buf, m_v_buf, m_norm_buf);

	// render color & depth textures to window
	SDL_GPUTexture *swapchain;
	if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, m_window, &swapchain, &m_width, &m_height)) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "SDL_WaitAndAcquireGPUSwapchainTexture failed\n\t%s", SDL_GetError());
		return SDL_APP_FAILURE;
	}
	if (!swapchain) { return SDL_APP_FAILURE; }
	m_outline_pipeline.render(cmdbuf, swapchain, m_color, m_depth);
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

