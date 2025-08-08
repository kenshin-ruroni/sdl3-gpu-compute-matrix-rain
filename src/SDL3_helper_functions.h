/*
 * helper_functions.h
 *
 *  Created on: 4 août 2025
 *      Author: descourt
 */

#pragma once
#include <stdlib.h>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include "SDL3/SDL.h"
#include "SDL3_image/SDL_image.h"

typedef struct Context
{

	SDL_Window *window;
	SDL_GPUDevice *device;

} Context;

enum class SHADER_COMPILE_STAGE {VERTEX = 0,FRAGMENT=1,COMPUTE=2};
enum class SHADER_COMPILE_LANG {HLSL = 0,GLSL=1};

namespace cmd{

	std::string exec(const char* cmd) {
	    std::array<char, 128> buffer;
	    std::string result;
	    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
	    if (!pipe) {
	        throw std::runtime_error("popen() failed!");
	    }
	    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
	        result += buffer.data();
	    }
	    return result;
	}

}
inline bool compileShaderFromSource(SHADER_COMPILE_STAGE &stage,SHADER_COMPILE_LANG &lang,const char* BasePath,const char* shaderFilename){


	bool compilation_status = true;
	std::string stage_str = "vertex";
	switch(stage){
	case SHADER_COMPILE_STAGE::FRAGMENT:
		stage_str = "fragment";
		break;
	case SHADER_COMPILE_STAGE::COMPUTE:
		stage_str = "compute";
		break;
	default:
		break;
	}

	std::string lang_str = "glsl";
	switch(lang){
	case SHADER_COMPILE_LANG::HLSL:
		lang_str = "hlsl";
		break;
	default:
		break;
	}

	printf("compiling file '%s/%s' with stage = '%s' and lang = '%s'... \n",BasePath,shaderFilename,stage_str.c_str(),lang_str.c_str());
	char compile_cmd[1024];
	try{
		SDL_snprintf(compile_cmd, sizeof(compile_cmd), "glslc -fshader-stage=%s -x %s  %s/%s -o %s/%s.spv",stage_str.c_str(),lang_str.c_str(), BasePath, shaderFilename,BasePath, shaderFilename);
		std::string output = cmd::exec(compile_cmd);
		printf("compilation successful...\n");
	}
	catch(std::exception &e){
		printf("compilation failed with errors %s \n",e.what());
		compilation_status = false;
	}

	return compilation_status;
}

inline int SDL_gpu_create_context(int width,int height,Context* context, SDL_WindowFlags windowFlags,const char* window_name ="sdl3 window"){

	context->device = SDL_CreateGPUDevice(
			SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL,
			true,
			NULL);

		if (context->device == NULL)
		{
			SDL_Log("GPUCreateDevice failed");
			return -1;
		}

		context->window = SDL_CreateWindow(window_name, width, height, windowFlags);
		if (context->window == NULL)
		{
			SDL_Log("CreateWindow failed: %s", SDL_GetError());
			return -1;
		}

		if (!SDL_ClaimWindowForGPUDevice(context->device, context->window))
		{
			SDL_Log("GPUClaimWindow failed");
			return -1;
		}

		return 0;
}

inline SDL_GPUShader* SDL_gpu_load_and_create_shader(
	const char* BasePath,
	SDL_GPUDevice* device,
	const char* shaderFilename,
	Uint32 samplerCount,
	Uint32 uniformBufferCount,
	Uint32 storageBufferCount,
	Uint32 storageTextureCount,
	SHADER_COMPILE_STAGE compile_stage,
	bool compile = false,
	SHADER_COMPILE_LANG compile_lang=SHADER_COMPILE_LANG::HLSL
) {
	if ( compile ){

			bool compilation_success = compileShaderFromSource(compile_stage,compile_lang,BasePath,shaderFilename);
			if ( !compilation_success){
				printf("exiting...");
				exit(-1);
			}

		}
	// Auto-detect the shader stage from the file name for convenience
	SDL_GPUShaderStage stage;

	switch(compile_stage){
		case SHADER_COMPILE_STAGE::FRAGMENT:
			stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
			break;
		case SHADER_COMPILE_STAGE::VERTEX:
			stage = SDL_GPU_SHADERSTAGE_VERTEX;
			break;
		default:
			break;
		}

	char fullPath[256];
	SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(device);
	SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
	const char *entrypoint;

	if (backendFormats & SDL_GPU_SHADERFORMAT_SPIRV) {
		SDL_snprintf(fullPath, sizeof(fullPath), "%s/%s.spv", BasePath, shaderFilename);
		format = SDL_GPU_SHADERFORMAT_SPIRV;
		entrypoint = "main";
	} else if (backendFormats & SDL_GPU_SHADERFORMAT_MSL) {
		SDL_snprintf(fullPath, sizeof(fullPath), "%s/%s.msl", BasePath, shaderFilename);
		format = SDL_GPU_SHADERFORMAT_MSL;
		entrypoint = "main0";
	} else if (backendFormats & SDL_GPU_SHADERFORMAT_DXIL) {
		SDL_snprintf(fullPath, sizeof(fullPath), "%s/%s.dxil", BasePath, shaderFilename);
		format = SDL_GPU_SHADERFORMAT_DXIL;
		entrypoint = "main";
	} else {
		SDL_Log("%s", "Unrecognized backend shader format!");
		return NULL;
	}

	size_t codeSize;
	Uint8 * code = (Uint8 *) SDL_LoadFile(fullPath, &codeSize);
	if (code == NULL)
	{
		SDL_Log("Failed to load shader from disk! %s", fullPath);
		return NULL;
	}

	SDL_GPUShaderCreateInfo shaderInfo = {
		.code_size = codeSize,
		.code = code,
		.entrypoint = entrypoint,
		.format = format,
		.stage = stage,
		.num_samplers = samplerCount,
		.num_storage_textures = storageTextureCount,
		.num_storage_buffers = storageBufferCount,
		.num_uniform_buffers = uniformBufferCount,
	};
	SDL_GPUShader* shader = SDL_CreateGPUShader(device, &shaderInfo);
	SDL_free(code);
	if (shader == NULL)
	{

		std::string vertex_type = stage == SDL_GPU_SHADERSTAGE_FRAGMENT ? "fragment shader" : "vertex shader";
		std::string msg = "Failed to create " + vertex_type;
		SDL_Log(msg.c_str());
		return NULL;
	}
	return shader;
}

inline SDL_GPUComputePipeline* SDL_gpu_create_compute_pipeline_from_shader(
	const char* BasePath,
	SDL_GPUDevice* device,
	const char* shaderFilename,
	SDL_GPUComputePipelineCreateInfo *createInfo,
	bool compile = false,
	SHADER_COMPILE_STAGE stage = SHADER_COMPILE_STAGE::VERTEX,
	SHADER_COMPILE_LANG lang=SHADER_COMPILE_LANG::HLSL
) {
	if ( compile ){

		bool compilation_success = compileShaderFromSource(stage,lang,BasePath,shaderFilename);
		if ( !compilation_success){
			printf("exiting...");
			exit(-1);
		}

	}
	char fullPath[256];
	SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(device);
	SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
	const char *entrypoint;

	if (backendFormats & SDL_GPU_SHADERFORMAT_SPIRV) {
		SDL_snprintf(fullPath, sizeof(fullPath), "%s/%s.spv", BasePath, shaderFilename);
		format = SDL_GPU_SHADERFORMAT_SPIRV;
		entrypoint = "main";
	} else if (backendFormats & SDL_GPU_SHADERFORMAT_MSL) {
		SDL_snprintf(fullPath, sizeof(fullPath), "%s/%s.msl", BasePath, shaderFilename);
		format = SDL_GPU_SHADERFORMAT_MSL;
		entrypoint = "main0";
	} else if (backendFormats & SDL_GPU_SHADERFORMAT_DXIL) {
		SDL_snprintf(fullPath, sizeof(fullPath), "%s/%s.dxil", BasePath, shaderFilename);
		format = SDL_GPU_SHADERFORMAT_DXIL;
		entrypoint = "main";
	} else {
		SDL_Log("%s", "Unrecognized backend shader format!");
		return NULL;
	}

	size_t codeSize;
	Uint8 * code = (Uint8 *)SDL_LoadFile(fullPath, &codeSize);
	if (code == NULL)
	{
		SDL_Log("Failed to load compute shader from disk! %s", fullPath);
		return NULL;
	}

	// Make a copy of the create data, then overwrite the parts we need
	SDL_GPUComputePipelineCreateInfo newCreateInfo = *createInfo;
	newCreateInfo.code = code;
	newCreateInfo.code_size = codeSize;
	newCreateInfo.entrypoint = entrypoint;
	newCreateInfo.format = format;

	SDL_GPUComputePipeline* pipeline = SDL_CreateGPUComputePipeline(device, &newCreateInfo);

	SDL_free(code);
	if (pipeline == NULL)
	{
		SDL_Log("Failed to create compute pipeline!");

		return NULL;
	}
	return pipeline;
}

SDL_Surface* SDL_gpu_load_image(const char* BasePath, const char* imageFilename, int desiredChannels)
{
	char fullPath[256];
	SDL_Surface *result;
	SDL_PixelFormat format;

	SDL_snprintf(fullPath, sizeof(fullPath), "%s/%s", BasePath, imageFilename);

	std::filesystem::path extension = std::filesystem::path(fullPath).extension();
	const char* ext = extension.c_str();

	if ( strncasecmp(ext,".jpg",4) == 0){
		result = IMG_Load(fullPath);
	}
	else if (strncasecmp(ext,".bmp",4) == 0){
		result = SDL_LoadBMP(fullPath);
	}

	if (result == NULL)
	{
		SDL_Log("Failed to load BMP: %s", SDL_GetError());
		return NULL;
	}

	if (desiredChannels == 4)
	{
		format = SDL_PIXELFORMAT_ABGR8888;
	}
	else
	{
		SDL_assert(!"Unexpected desiredChannels");
		SDL_DestroySurface(result);
		return NULL;
	}
	if (result->format != format)
	{
		SDL_Surface *next = SDL_ConvertSurface(result, format);
		SDL_DestroySurface(result);
		result = next;
	}

	return result;
}

inline bool AppLifecycleWatcher(void *userdata, SDL_Event *event)
{
	/* This callback may be on a different thread, so let's
	 * push these events as USER events so they appear
	 * in the main thread's event loop.
	 *
	 * That allows us to cancel drawing before/after we finish
	 * drawing a frame, rather than mid-draw (which can crash!).
	 */
	if (event->type == SDL_EVENT_DID_ENTER_BACKGROUND)
	{
		SDL_Event evt;
		evt.type = SDL_EVENT_USER;
		evt.user.code = 0;
		SDL_PushEvent(&evt);
	}
	else if (event->type == SDL_EVENT_WILL_ENTER_FOREGROUND)
	{
		SDL_Event evt;
		evt.type = SDL_EVENT_USER;
		evt.user.code = 1;
		SDL_PushEvent(&evt);
	}
	return false;
}



 template <typename T> inline SDL_GPUBuffer *SDL_gpu_create_gpu_buffer(uint32_t size,Context *context, SDL_GPUBufferUsageFlags usage)
{
	size_t size_in_bytes = size * sizeof(T);
	SDL_GPUBufferCreateInfo buffer_info;
	buffer_info.usage = usage;
	buffer_info.size = size_in_bytes;
	buffer_info.props = 0;
	SDL_GPUBuffer* gpu_buffer = SDL_CreateGPUBuffer(context->device,&buffer_info );
	return gpu_buffer;
}

 inline void SDL_gpu_copy_data_to_gpu_vram(size_t size_in_bytes,void *data, Context *context,SDL_GPUTransferBuffer* transfer_buffer){
 	// instantiate a pointer belonging to GPU device VRAM and copy data from CPU RAM to GPU VRAM
 		 Uint32* transfer_ptr = (Uint32 *)SDL_MapGPUTransferBuffer(context->device,transfer_buffer,false);
 		 SDL_memcpy(transfer_ptr,data,size_in_bytes);
 		 SDL_UnmapGPUTransferBuffer(context->device,transfer_buffer);
 }

template <typename T>  inline SDL_GPUTransferBuffer *SDL_gpu_acquire_gpu_transfer_buffer(uint32_t size_in_bytes, Context *context)
{
	SDL_GPUTransferBufferCreateInfo transfer_buffer_info;
	transfer_buffer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
	transfer_buffer_info.size = size_in_bytes,
	transfer_buffer_info.props = 0;
	SDL_GPUTransferBuffer* transfer_buffer = SDL_CreateGPUTransferBuffer(context->device,&transfer_buffer_info);
	return transfer_buffer;
}




inline void SDL_gpu_upload_gpu_buffer(SDL_GPUCopyPass* copyPass,uint32_t buffer_size_in_bytes,SDL_GPUBuffer *gpu_buffer,SDL_GPUTransferBuffer *gpu_transfer_buffer){

	SDL_GPUTransferBufferLocation transfer_buffer_location;
	transfer_buffer_location.transfer_buffer = gpu_transfer_buffer;
	transfer_buffer_location.offset = 0;
	SDL_GPUBufferRegion gpu_buffer_region;
	gpu_buffer_region.buffer = gpu_buffer;
	gpu_buffer_region.offset = 0;
	gpu_buffer_region.size = buffer_size_in_bytes;
	SDL_UploadToGPUBuffer
	(
		copyPass,
		&transfer_buffer_location,
		&gpu_buffer_region,
		false
	);
}

inline void SDL_gpu_upload_gpu_texture_to_gpu(SDL_GPUCopyPass* copyPass,uint32_t texture_region_depth,uint32_t texture_width,size_t texture_height,SDL_GPUTexture* image_texture, SDL_GPUTransferBuffer* texture_image_transfer_buffer){

	SDL_GPUTextureTransferInfo texture_transfer_info = {
		.transfer_buffer = texture_image_transfer_buffer,
		.offset = 0 /* Zeros out the rest */
	};
	SDL_GPUTextureRegion texture_region = {
	.texture = image_texture,
	.w = texture_width,
	.h = texture_height,
	.d = texture_region_depth
	};

	// upload image data
	SDL_UploadToGPUTexture(
			copyPass,
			&texture_transfer_info ,
			&texture_region,
			false
		);
}

