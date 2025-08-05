// Each #kernel tells which function to compile; you can have many kernels
#pragma kernel CSDrawGlyphs

// Create a RenderTexture with enableRandomWrite flag and set it
// with cs.SetTexture



RWTexture2D<float4> out_image;
Texture2D<float4> in_image;

StructuredBuffer<int> glyphs;

struct Symbol
{
	int size;
	int visible;
	int _state;
	int pos_x;
	int pos_y;
	int id_glyph;
	float blending;
	float speed;
	uint color;
};

StructuredBuffer<Symbol> symbols;

uniform int sizeX;
uniform int sizeY;

uniform float3 teinte;
uniform float3 glyph_color;

uniform int glyph_high;
uniform int glyph_low;

void DrawPixel(uint2 id_in, float4 color, int size, float blending)
{
	uint3 id;
	for (int i = 0; i < size; i++)
	{
		id.x = id_in.x - i;
		for (int j = 0; j < size; j++)
		{
			id.y = id_in.y - j;
			float f = length(in_image[id.xy])*0.25;
			out_image[id.xy] =   ( color *  in_image[id.xy] * float4(teinte.xyz,1) ) ;
		}
	}
}

float4 ComputeAverageBackgroundColor(uint2 id_in, int size)
{
	uint2 id;
	float4 color = float4(0, 0, 0, 1);

	for (int i = 0; i < size*8; i++)
	{
		id.x = id_in.x - i;
		for (int j = 0; j < size*8; j++)
		{
			id.y = id_in.y - j;
			color += in_image[id];
		}
	}
	color /= (64*size*size);
	return color;
}


void DrawGlyph(uint2 pos, float blending, float4 color, int size)
{
	uint mask = 1; // 2^32
	// on va de la droite vers la gauche sur les data de glyph
	uint2 id_out;
	int ii = glyph_low;
	id_out.x = pos.x;
	id_out.y = pos.y;

	//out_image[id_out.xy] = float4(1, 1, 0, blending);

	float f = length(in_image[pos])*0.25;
	float4 background =   in_image[pos] * float4(f *teinte.xyz, 1); // float4(0, 0, 0, 1); // in_image[pos];// *float4(0.5, 0.9, 0.5, 1);// float4(0.1, 0.8, 0.1, 1);
	float4 foreground = float4(glyph_color.xyz, 1);// *background; // +(1 - blending)  *background;// +0.5* (in_image[pos] * float4(teinte.xyz, 1)); // *float4(0.1, 1, 0.5, 1);

	for (int j = 0; j < 2; j++)
	{
		for (int i = 0; i < 32; i++) // 32 bits
		{
			if ( (ii & mask) == mask) // le bit est a 1
			{
				DrawPixel(id_out, foreground, size, blending);
			}
			else
			{
				DrawPixel(id_out, background, size, blending);
				//out_image[id_out.xy] = in_image[id_out.xy];
			}
			id_out.x -= size;
			if (fmod(i + 1, 8) == 0) // on change de ligne
			{
				id_out.y -= size;
				id_out.x = pos.x;
			}
			mask *= 2; // on shift vers la gauche
		}

		id_out.x = pos.x;
		ii = glyph_high;
		mask = 1;

	}
}



[numthreads(32,1,1)]
void CSDrawGlyphs (uint3 id : SV_DispatchThreadID)
{
	Symbol symbol = symbols[id.x];
	glyph_high = glyphs[symbol.id_glyph];
	glyph_low = glyphs[symbol.id_glyph + 1];
	uint2 pos;
	pos.x = symbol.pos_x ;
	pos.y = symbol.pos_y ;
	// a partir du symbol coloron recalcule sont float4 correspondant
	int size = 1;
	float4 average = ComputeAverageBackgroundColor(pos, size);
	float f = length(average)*0.25;
	float4 color =   average * float4(teinte.xyz,1) ;// *average; // ComputeAverageBackgroundColor(pos, size);  //in_image[id.xy] ; // +  //
	DrawGlyph(pos,1,color,size);

}

#pragma kernel CSCombineImages
[numthreads(32, 32, 1)]
void CSCombineImages(uint3 id : SV_DispatchThreadID)
{

	out_image[id.xy] =  (1-blending)*out_image[id.xy]+  blending * in_image[id.xy];

}

#pragma kernel CSInitializeOutImage
[numthreads(32, 32, 1)]
void CSInitializeOutImage(uint3 id : SV_DispatchThreadID)
{

	out_image[id.xy] = float4(0, 0, 0, 1); //in_image[id.xy] *float4(teinte.xyz, 1);  // in_image[id.xy]; // + in_image[id.xy] * float4(0, 1, 0.5, 1);

}