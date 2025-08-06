#pragma once

#include "SDL3/SDL.h"

extern  SDL_GPUComputePipeline* compute_initialize_pipeline;
extern SDL_GPUComputePipeline* compute_rasterize_glyphs_pipeline;
extern SDL_GPUComputePipeline* compute_combine_images_pipeline;

extern SDL_GPUTexture* image_texture;
extern SDL_GPUSampler* image_sampler;

extern SDL_GPUTexture* write_texture;

extern SDL_GPUBuffer * symbols_compute_buffer;
extern SDL_GPUBuffer * glyphs_compute_buffer;


float teinte[4] = {0.9,0.5,0.3,1.0};
float glyphs_color[4] = {0.2,0.5,0.8,1.0};

inline void process_first_compute_pass(uint32_t &w,uint32_t &h,SDL_GPUCommandBuffer* cmdbuf){

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
     SDL_PushGPUComputeUniformData(cmdbuf, 0, teinte, sizeof(float)*4);
     SDL_DispatchGPUCompute(computePass, w / 8 , h / 8 , 1);

     SDL_EndGPUComputePass(computePass);

 }

 inline void process_second_compute_pass(uint32_t &w,uint32_t &h,SDL_GPUCommandBuffer* cmdbuf){

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

	      SDL_BindGPUComputePipeline(computePass, compute_rasterize_glyphs_pipeline);

	      SDL_GPUBuffer *gpu_buffers[] = {
	    	glyphs_compute_buffer,
			symbols_compute_buffer
	      };

	      SDL_BindGPUComputeStorageBuffers(
	      			computePass,
	      			0,
					gpu_buffers
	      			,
	      			2
	      		);

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
	      SDL_PushGPUComputeUniformData(cmdbuf, 0, teinte, sizeof(float)*8);
	      SDL_DispatchGPUCompute(computePass, w / 8 , h / 8 , 1);

	      SDL_EndGPUComputePass(computePass);


 }
