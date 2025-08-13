//============================================================================
// Name        : sdl3-basic-computer-linux-build.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================
#include <chrono>
#include <iostream>
// Library effective with Linux
#include <unistd.h>
#include "SDL3/SDL.h"

#include "matrix.h"

#include "SDL3_helper_functions.h"

#include "compute_shaders_passes.h"




 SDL_GPUComputePipeline* compute_initialize_pipeline;
 SDL_GPUComputePipeline* compute_rasterize_symbols_pipeline;
 SDL_GPUComputePipeline* compute_combine_images_pipeline;

 SDL_GPUTexture* image_texture;
 SDL_GPUSampler* image_sampler;

 SDL_GPUTexture* write_texture;
 SDL_GPUBuffer * symbols_compute_buffer;
 SDL_GPUBuffer * glyphs_compute_buffer;
 SDL_GPUBuffer * flare_kernel2d_compute_buffer;

 SDL_GPUTransferBuffer *symbols_gpu_transfer_buffer;

 int glyph_pixel_size = 1;
 int font_size = 8;

int glyph_size = glyph_pixel_size * font_size; // glyph size in pixels
uint32_t flare_data_size;


 inline void upload_symbols_to_gpu(Context* context,SDL_GPUCommandBuffer* cmdbuf){

 	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);
 	SDL_gpu_copy_data_to_gpu_vram(symbols_size_in_bytes,(void *)symbols,context,symbols_gpu_transfer_buffer);
 	SDL_gpu_upload_gpu_buffer(copyPass,symbols_size_in_bytes,symbols_compute_buffer,symbols_gpu_transfer_buffer);
 	SDL_EndGPUCopyPass(copyPass);

 }

 inline int render_matrix(Context* context,bool update)
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
    	 process_first_compute_pass( w,h,cmdbuf);

         // second pass : draw matrix glyphs by rasterization with compute shader

    	 process_second_compute_pass(w,h,cmdbuf);

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

    // if ( update)
     {
    	 update_matrix();
    	 upload_symbols_to_gpu(context,cmdbuf);
     }

	 SDL_SubmitGPUCommandBuffer(cmdbuf);



     return 0;
 }



int main() {

	if (!SDL_Init(SDL_INIT_VIDEO))
	{
			SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
			return 1;
	}

	printf(" SDL version %i  %i\n",SDL_GetVersion(),SDL_VERSION);

	Context context = { 0 };
	Uint32 w = 1800,h = w*(1080./1920.);
	int r = SDL_gpu_create_context(w,h,&context,SDL_WINDOW_RESIZABLE,"SdL gPu MaTrIx RaIn");

	int number_of_drivers = SDL_GetNumGPUDrivers();


	for (int i = 0; i < number_of_drivers;i++){
		printf("found gpu driver '%s' \n",SDL_GetGPUDriver(i));
	}
	printf(" gpu driver name from gpu device '%s' \n",SDL_GetGPUDeviceDriver(context.device));


	uint32_t *glyphs  = compute_glyphs_data();

	// flare kernel2d data
	float radius = glyph_size + 2 * 2;
	kernel_2d_data *flare_kernel2d_data = compute_flare_kernel_2d_data( radius/std::sqrt(2),0.25f);
	flare_data_size = radius*radius;
	uint32_t flare_kernel2d_buffer_size_in_bytes = flare_data_size * sizeof(kernel_2d_data);


	SDL_AddEventWatch(AppLifecycleWatcher, NULL);



	*glyphs_size =glyph_pixel_size;

	initialize_matrix((int)w,(int)h);

	if ( r != 0 ){
		exit(-1);
	}
	// Load the image
	SDL_Surface *imageData = SDL_gpu_load_image("./","crysis-2.jpg", 4);
	if (imageData == NULL)
	{
		SDL_Log("Could not load image data!");
		return -1;
	}

	if ( (Uint32)imageData->w != w && (Uint32)imageData->h != h){
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
			.min_filter = SDL_GPU_FILTER_LINEAR,
			.mag_filter = SDL_GPU_FILTER_LINEAR,
			.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
			.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
			.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
			.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		};
		image_sampler = SDL_CreateGPUSampler(context.device, &image_sampler_info);

	    // Set up texture data in order to transfer it to GPU device
		SDL_GPUTransferBufferCreateInfo texture_image_transfer_buffer_info =
		{
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = (Uint32) imageData->w * imageData->h * 4,
			.props = 0
		};
		SDL_GPUTransferBuffer* texture_image_transfer_buffer = SDL_CreateGPUTransferBuffer(
			context.device,
			&texture_image_transfer_buffer_info
		);

		SDL_gpu_copy_data_to_gpu_vram((size_t) imageData->w * imageData->h * 4,(void *)imageData->pixels,&context,texture_image_transfer_buffer);


		// Upload the image data to the GPU resources
		SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(context.device);
		SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

		SDL_gpu_upload_gpu_texture_to_gpu(copyPass,1,imageData->w,imageData->h,image_texture,texture_image_transfer_buffer);

		// upload glyphs and symbols to GPU device

		// upload glyphs to GPU VRAM
		glyphs_compute_buffer = SDL_gpu_create_gpu_buffer<uint32_t>(number_of_glyphs,&context,SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ);
		SDL_GPUTransferBuffer *glyphs_gpu_transfer_buffer = SDL_gpu_acquire_gpu_transfer_buffer<uint32_t>(glyphs_size_in_bytes,&context);
		// Transfer the data
		SDL_gpu_copy_data_to_gpu_vram(glyphs_size_in_bytes,(void *)glyphs,&context,glyphs_gpu_transfer_buffer);
		SDL_gpu_upload_gpu_buffer(copyPass,glyphs_size_in_bytes,glyphs_compute_buffer,glyphs_gpu_transfer_buffer);


		// upload symbols to GPU VRAM
		symbols_compute_buffer = SDL_gpu_create_gpu_buffer<Symbol>(number_of_symbols,&context,SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ);
		symbols_gpu_transfer_buffer = SDL_gpu_acquire_gpu_transfer_buffer<Symbol>(symbols_size_in_bytes,&context);
		// Transfer the data
		SDL_gpu_copy_data_to_gpu_vram(symbols_size_in_bytes,(void *)symbols,&context,symbols_gpu_transfer_buffer);
		SDL_gpu_upload_gpu_buffer(copyPass,symbols_size_in_bytes,symbols_compute_buffer,symbols_gpu_transfer_buffer);

		// upload symbols to GPU VRAM
		flare_kernel2d_compute_buffer = SDL_gpu_create_gpu_buffer<Symbol>(radius*radius,&context,SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ);
		SDL_GPUTransferBuffer *flare_kernel2d_compute_transfer_buffer = SDL_gpu_acquire_gpu_transfer_buffer<Symbol>(symbols_size_in_bytes,&context);
				// Transfer the data
		SDL_gpu_copy_data_to_gpu_vram(flare_kernel2d_buffer_size_in_bytes,(void *)flare_kernel2d_data,&context,flare_kernel2d_compute_transfer_buffer);
		SDL_gpu_upload_gpu_buffer(copyPass,flare_kernel2d_buffer_size_in_bytes,flare_kernel2d_compute_buffer,flare_kernel2d_compute_transfer_buffer);



		SDL_EndGPUCopyPass(copyPass);
		SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
		SDL_DestroySurface(imageData);
		SDL_ReleaseGPUTransferBuffer(context.device, texture_image_transfer_buffer);
		SDL_ReleaseGPUTransferBuffer(context.device, glyphs_gpu_transfer_buffer);

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
    SDL_GPUComputePipelineCreateInfo compute_initialize_pipeline_info={};

	compute_initialize_pipeline_info.num_samplers = 1;
	compute_initialize_pipeline_info.num_readonly_storage_textures = 1;
	compute_initialize_pipeline_info.num_readwrite_storage_textures = 1;
	compute_initialize_pipeline_info.num_uniform_buffers = 1;
	compute_initialize_pipeline_info.threadcount_x = 8,
	compute_initialize_pipeline_info.threadcount_y = 8,
	compute_initialize_pipeline_info.threadcount_z = 1,


    compute_initialize_pipeline = SDL_gpu_create_compute_pipeline_from_shader(
    	"./",
        context.device,
        "cs_initialize_out_image.comp",
        &compute_initialize_pipeline_info,
		true,SHADER_COMPILE_STAGE::COMPUTE,SHADER_COMPILE_LANG::HLSL
    );

    SDL_GPUComputePipelineCreateInfo compute_rasterize_symbols_pipeline_info={};

	compute_rasterize_symbols_pipeline_info.num_samplers = 1;
	compute_rasterize_symbols_pipeline_info.num_readonly_storage_textures = 1;
	compute_rasterize_symbols_pipeline_info.num_readwrite_storage_textures = 1;
	compute_rasterize_symbols_pipeline_info.num_readonly_storage_buffers = 3;
	compute_rasterize_symbols_pipeline_info.num_uniform_buffers = 1;
	compute_rasterize_symbols_pipeline_info.threadcount_x = 8;
	compute_rasterize_symbols_pipeline_info.threadcount_y = 1;
	compute_rasterize_symbols_pipeline_info.threadcount_z = 1;


    compute_rasterize_symbols_pipeline = SDL_gpu_create_compute_pipeline_from_shader(
        	"./",
            context.device,
            "cs_rasterize_symbols.comp",
            &compute_rasterize_symbols_pipeline_info,
    		true,SHADER_COMPILE_STAGE::COMPUTE,SHADER_COMPILE_LANG::HLSL
        );

	bool quit = false;
	bool update = true;
	while(!quit){
		SDL_Event evt;
		while (SDL_PollEvent(&evt))
		{
			if (evt.type == SDL_EVENT_QUIT)
			{
				quit = true;
			}
			/*if (evt.type == SDL_EVENT_KEY_DOWN)
			{
				update = false;
			}

			if (evt.type == SDL_EVENT_KEY_UP)
			{
				update = true;
			}*/

		}

		render_matrix(&context,true);
		//auto start = std::chrono::high_resolution_clock::now();

		//auto end = std::chrono::high_resolution_clock::now();
		//auto elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		//auto fps =1e6/ elapsed_time.count();
		//printf("fps %f elapsed time %u \n",fps,elapsed_time.count());
	}

}

