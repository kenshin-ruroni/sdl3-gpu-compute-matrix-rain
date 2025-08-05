//============================================================================
// Name        : sdl3-basic-computer-linux-build.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
// Library effective with Linux
#include <unistd.h>
#include "SDL3/SDL.h"
#include "SDL3_helper_functions.h"

#include "glyphs.h"

 SDL_GPUComputePipeline* compute_initialize_pipeline;
 SDL_GPUComputePipeline* compute_rasterize_glyphs_pipeline;
 SDL_GPUComputePipeline* compute_combine_images_pipeline;

 SDL_GPUTexture* image_texture;
 SDL_GPUSampler* image_sampler;

 SDL_GPUTexture* write_texture;
 SDL_GPUBuffer * symbols_compute_buffer;
 SDL_GPUBuffer * glyphs_compute_buffer;

 float teinte[3] = {0.7,0.5,0.6};

 /*
  *

  voir

  pour les bindings des SSBO

  https://wiki.libsdl.org/SDL3/SDL_GPUStorageBufferReadWriteBinding

  *
   SDL_GPUStorageTextureReadWriteBinding storage_texture_bindings[] = {
      {.texture = cubemap, .mip_level = 0, .layer = 0, .cycle = false},
      {.texture = cubemap, .mip_level = 0, .layer = 1, .cycle = false},
  };
  SDL_GPUComputePass* compute_pass = SDL_BeginGPUComputePass(
      command_buffer, storage_texture_bindings, 2, nullptr, 0);
  SDL_BindGPUComputePipeline(compute_pass, pipeline);
  SDL_DispatchGPUCompute(compute_pass, cubemap_size / 8, cubemap_size / 8, 2);
  SDL_EndGPUComputePass(compute_pass);
  *
  *
  */

 inline int Draw(Context* context)
 {
     SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(context->device);
     if (cmdbuf == NULL)
     {
         SDL_Log("AcquireGPUCommandBuffer failed: %s", SDL_GetError());
         return -1;
     }

     SDL_GPUTexture* swapchainTexture;
     Uint32 w, h;
     if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, context->window, &swapchainTexture, &w, &h)) {
         SDL_Log("WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
         return -1;
     }

     if (swapchainTexture != NULL)
     {

    	 // first pass initialize texture where we render matrix rain
    	 //
    	 SDL_GPUStorageTextureReadWriteBinding compute_initialize_textures_bindings[] =
    	 {
    		{
                 .texture = write_texture,
                 .cycle = false
            },
    		{
                 .texture = image_texture,
                 .cycle = false
            }
    	 };

         SDL_GPUComputePass* computePass = SDL_BeginGPUComputePass(
             cmdbuf,
			 compute_initialize_textures_bindings,
             2,
             NULL,
             0
         );

         SDL_BindGPUComputePipeline(computePass, compute_initialize_pipeline);

         SDL_GPUTextureSamplerBinding sampler_bindings[] =
         {
			 {
				 .texture = image_texture,
				 .sampler = image_sampler
			 }
         };
         SDL_BindGPUComputeSamplers(
                    computePass,
                    0,
					sampler_bindings,
                    1);

         SDL_GPUTexture *textures[]=
         {
        		 write_texture,
				 image_texture
         };
         SDL_BindGPUComputeStorageTextures(
         			computePass,
         			0,
					textures,
         			2
         		);
         SDL_PushGPUComputeUniformData(cmdbuf, 0, &teinte, sizeof(float)*3);
         SDL_DispatchGPUCompute(computePass, w / 8 , h / 8 , 1);

         SDL_EndGPUComputePass(computePass);
         SDL_GPUBlitRegion gpu_blit_source={
        		 .texture = write_texture,
                 .w = w,
                 .h = h
         };
         SDL_GPUBlitRegion gpu_blit_destination={
        		 .texture = swapchainTexture,
        		 .w = w,
        		 .h = h
         };
         SDL_GPUBlitInfo gpu_blit_info = {
                 .source = gpu_blit_source,
                 .destination = gpu_blit_destination,
                 .load_op = SDL_GPU_LOADOP_DONT_CARE,
                 .filter = SDL_GPU_FILTER_LINEAR
             };

         SDL_BlitGPUTexture(
             cmdbuf,
             &gpu_blit_info
         );
     }

     SDL_SubmitGPUCommandBuffer(cmdbuf);

     // second pass : draw matrix glyphs by rasterization with compute shader

     return 0;
 }



int main() {

	uint32_t *glyphs  = ComputeGlyphsDataToHighAndLowInt();


	Context context = { 0 };
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
			SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
			return 1;
	}

	SDL_AddEventWatch(AppLifecycleWatcher, NULL);

	Uint32 w = 800,h = 800;

	InitializeMatrix((int)w,(int)h);

	int r = Create_Context(w,h,&context,SDL_WINDOW_RESIZABLE,"matrix rain");


	SDL_GPUBufferCreateInfo symbols_buffer_info = {
			.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,
			.size = symbols_size_in_bytes,
			.props=0
		};
	symbols_compute_buffer = SDL_CreateGPUBuffer(
			context.device,
			&symbols_buffer_info
		);

	SDL_GPUBufferCreateInfo glyphs_buffer_info = {
				.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,
				.size = symbols_size_in_bytes,
				.props=0
			};
		glyphs_compute_buffer = SDL_CreateGPUBuffer(
				context.device,
				&symbols_buffer_info
			);

	if ( r != 0 ){
		exit(-1);
	}
	// Load the image
	SDL_Surface *imageData = LoadImage("./","crysis-2.jpg", 4);
	if (imageData == NULL)
	{
		SDL_Log("Could not load image data!");
		return -1;
	}

	if ( imageData->w != w && imageData->h != h){
		imageData = SDL_ScaleSurface(imageData,w,h,SDL_SCALEMODE_LINEAR);
		IMG_SaveJPG(imageData,"./rescaled_crysis-2.jpg",100);
	}

	// create texture for image
	const SDL_GPUTextureCreateInfo image_texture_info =
		{
			.type = SDL_GPU_TEXTURETYPE_2D,
			.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
			.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE,
			.width = (Uint32) imageData->w,
			.height = (Uint32) imageData->h,
			.layer_count_or_depth = 1,
			.num_levels = 1,
			.sample_count  = SDL_GPU_SAMPLECOUNT_1
		};

		image_texture = SDL_CreateGPUTexture(context.device, &image_texture_info);

		SDL_SetGPUTextureName(
			context.device,
			image_texture,
			"texture image"
		);

		SDL_GPUSamplerCreateInfo image_sampler_info =
		{
			.min_filter = SDL_GPU_FILTER_NEAREST,
			.mag_filter = SDL_GPU_FILTER_NEAREST,
			.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
			.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
			.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
			.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		};
		image_sampler = SDL_CreateGPUSampler(context.device, &image_sampler_info);

	    // Set up texture data in order to transfer it to GPU device
		SDL_GPUTransferBufferCreateInfo transfer_buffer_info =
		{
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = (Uint32) imageData->w * imageData->h * 4,
			.props = 0
		};
		SDL_GPUTransferBuffer* textureTransferBuffer = SDL_CreateGPUTransferBuffer(
			context.device,
			&transfer_buffer_info
		);

		Uint8* textureTransferPtr = (Uint8 *) SDL_MapGPUTransferBuffer(
			context.device,
			textureTransferBuffer,
			false
		);
		SDL_memcpy(textureTransferPtr, imageData->pixels, imageData->w * imageData->h * 4);
		SDL_UnmapGPUTransferBuffer(context.device, textureTransferBuffer);

		// Upload the image data to the GPU resources
		SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(context.device);
		SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);
		SDL_GPUTextureTransferInfo texture_transfer_info = {
			.transfer_buffer = textureTransferBuffer,
			.offset = 0, /* Zeros out the rest */
		};
		SDL_GPUTextureRegion texture_region = {
				.texture = image_texture,
				.w = (Uint32) imageData->w,
				.h = (Uint32) imageData->h,
				.d = 1
		};
		SDL_UploadToGPUTexture(
				copyPass,
				&texture_transfer_info ,
				&texture_region,
				false
			);

		SDL_EndGPUCopyPass(copyPass);
		SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
		SDL_DestroySurface(imageData);
		SDL_ReleaseGPUTransferBuffer(context.device, textureTransferBuffer);


	/*
	 *  write_texture will be used to blit with swapChainTexture
	 *  so it needs the  SDL_GPU_TEXTUREUSAGE_SAMPLER usage flag
	 */
	const SDL_GPUTextureCreateInfo write_texture_info =
		{
			.type = SDL_GPU_TEXTURETYPE_2D,
			.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
			.usage =  SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE,
			.width = w,
			.height = h,
			.layer_count_or_depth =1,
			.num_levels = 1,
			.sample_count  = SDL_GPU_SAMPLECOUNT_1
		};

    write_texture = SDL_CreateGPUTexture(context.device, &write_texture_info);

    // create first compute shader pass
    SDL_GPUComputePipelineCreateInfo compute_initialize_pipeline_info = {
            .num_samplers = 1,
            .num_readonly_storage_textures = 1,
			.num_readwrite_storage_textures = 1,
            .num_uniform_buffers = 1,
            .threadcount_x = 8,
            .threadcount_y = 8,
            .threadcount_z = 1,
        };

    compute_initialize_pipeline = CreateComputePipelineFromShader(
    	"./",
        context.device,
        "cs_initialize_out_image.comp",
        &compute_initialize_pipeline_info,
		true,SHADER_COMPILE_STAGE::COMPUTE,SHADER_COMPILE_LANG::HLSL
    );

    SDL_GPUComputePipelineCreateInfo compute_rasterize_glyphs_pipeline_info = {
                .num_samplers = 1,
                .num_readonly_storage_textures = 1,
    			.num_readwrite_storage_textures = 1,
                .num_uniform_buffers = 1,
                .threadcount_x = 8,
                .threadcount_y = 8,
                .threadcount_z = 1,
            };

    compute_rasterize_glyphs_pipeline = CreateComputePipelineFromShader(
        	"./",
            context.device,
            "cs_rasterize_glyphs.comp",
            &compute_rasterize_glyphs_pipeline_info,
    		true,SHADER_COMPILE_STAGE::COMPUTE,SHADER_COMPILE_LANG::HLSL
        );
	bool quit = false;
	while(!quit){
		SDL_Event evt;
		while (SDL_PollEvent(&evt))
		{
			if (evt.type == SDL_EVENT_QUIT)
			{
				quit = true;
			}
		}
		Draw(&context);
	}

}
