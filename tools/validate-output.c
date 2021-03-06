/*
 * Copyright (C) 2011 PixFC Team (pixelfc@gmail.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public  License as published by the
 * Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common.h"
#include "conversion_blocks.h"
#include "pixfc-sse.h"
#include "pixfmt_descriptions.h"
#include "platform_util.h"
#include "rgb_image.h"
#include "test-utils.h"

#ifdef __INTEL_CPU__
#include <emmintrin.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Formats and flags to restrict the comparison to, if specified on the cmdline
static	PixFcPixelFormat		restrict_to_src_fmt = PixFcFormatCount;
static	PixFcPixelFormat		restrict_to_dst_fmt = PixFcFormatCount;
static  PixFcFlag				restrict_to_flags = PixFcFlag_Default;
//
static	uint8_t					max_diff = 0;
// In / out buffers
static	__m128i	*				in = NULL;
static	__m128i	*				out = NULL;	// buffer holding SSE converted data
static	__m128i	*				out_scalar = NULL;// buffer holding scalar converted data for comparison



//
// Use the conversion block at the provided index to convert image of given width and height in in buffer to out buffer
static uint32_t		do_image_conversion(uint32_t index, void* in, void *out, uint32_t w, uint32_t h) {
	struct PixFcSSE *	pixfc = NULL;
	uint32_t	result;
	
	// Create struct pixfc
	result = create_pixfc_for_conversion_block(index, &pixfc, w, h);
	if (result != PixFc_OK) {
		pixfc_log("Error create struct pixfc (%d)\n", result);
		return result;
	}
	
	// Perform conversion
	pixfc->convert(pixfc, in, out);
		
	destroy_pixfc(pixfc);
	
	return 0;
}

static int		compare_8bit_output_buffers(uint8_t* out_sse, uint8_t* out_scalar, PixFcPixelFormat fmt, uint32_t width, uint32_t height, uint8_t max_diff) {
	uint32_t 	i;
	uint32_t 	buffer_size = IMG_SIZE(fmt, width, height);
	uint32_t	max_diff_seen = 0;
	for(i = 0; i < buffer_size; i++) {
		if(abs(*out_scalar - *out_sse) > max_diff_seen)
				max_diff_seen = abs(*out_scalar - *out_sse);
		if(abs(*out_scalar - *out_sse) > max_diff) {
			printf("Pixel %ux%u (offset: %u 8-bit ptr) differ by %u : SSE: %hhu - Scalar: %hhu\n",
					(i * pixfmt_descriptions[fmt].bytes_per_pix_denom / pixfmt_descriptions[fmt].bytes_per_pix_num) % width + 1,
					(i * pixfmt_descriptions[fmt].bytes_per_pix_denom / pixfmt_descriptions[fmt].bytes_per_pix_num) / width + 1,
					i,
					abs(*out_scalar - *out_sse), *out_sse, *out_scalar);
			return -1;
		}
		out_scalar++;
		out_sse++;
	}

	printf("Max diff seen: %u\n", max_diff_seen);

	return 0;
}

#define COMPARE_VALUES(component)\
	if(abs(scalar_val - sse_val) > max_diff_seen)\
		max_diff_seen = abs(scalar_val - sse_val);\
	if(abs(scalar_val - sse_val) > max_diff) {\
		printf(component " @ Pixel %ux%u (offset: %llu - 32bit ptr) differ by %u : SSE: %hu - Scalar: %hu\n",\
		(pixel + 1), line,\
		(unsigned long long)((sse_ptr - out_sse) * 4),\
		abs(scalar_val - sse_val), sse_val, scalar_val);\
		return -1;\
	}
static int		compare_v210_output_buffers(uint32_t* out_sse, uint32_t* out_scalar, PixFcPixelFormat fmt, uint32_t width, uint32_t height, uint8_t max_diff) {
	uint32_t 	line = 0;
	uint32_t 	pixel = 0;
	uint32_t 	bytes_per_row = ROW_SIZE(fmt,  width);
	uint32_t* 	scalar_ptr = out_scalar;
	uint32_t* 	sse_ptr = out_sse;
	uint16_t 	scalar_val;
	uint16_t 	sse_val;
	uint32_t	max_diff_seen = 0;

	while(line++ < height) {
		while(pixel < width){
			scalar_val = (*scalar_ptr & 0x3FF);
			sse_val = (*sse_ptr & 0x3ff);
			COMPARE_VALUES("U");

			scalar_val = ((*scalar_ptr >> 10) & 0x3FF);
			sse_val = ((*sse_ptr >> 10) & 0x3ff);
			COMPARE_VALUES("Y1");

			scalar_val = ((*scalar_ptr++ >> 20) & 0x3FF);
			sse_val = ((*sse_ptr++ >> 20) & 0x3ff);
			COMPARE_VALUES("V");

			scalar_val = (*scalar_ptr & 0x3FF);
			sse_val = (*sse_ptr & 0x3ff);
			COMPARE_VALUES("Y2");

			scalar_val = ((*scalar_ptr >> 10) & 0x3FF);
			sse_val = ((*sse_ptr >> 10) & 0x3ff);
			COMPARE_VALUES("U");

			scalar_val = ((*scalar_ptr++ >> 20) & 0x3FF);
			sse_val = ((*sse_ptr++ >> 20) & 0x3ff);
			COMPARE_VALUES("Y3");

			scalar_val = (*scalar_ptr & 0x3FF);
			sse_val = (*sse_ptr & 0x3ff);
			COMPARE_VALUES("V");

			scalar_val = ((*scalar_ptr >> 10) & 0x3FF);
			sse_val = ((*sse_ptr >> 10) & 0x3ff);
			COMPARE_VALUES("Y4");

			scalar_val = ((*scalar_ptr++ >> 20) & 0x3FF);
			sse_val = ((*sse_ptr++ >> 20) & 0x3ff);
			COMPARE_VALUES("U");

			scalar_val = (*scalar_ptr & 0x3FF);
			sse_val = (*sse_ptr & 0x3ff);
			COMPARE_VALUES("Y5");

			scalar_val = ((*scalar_ptr >> 10) & 0x3FF);
			sse_val = ((*sse_ptr >> 10) & 0x3ff);
			COMPARE_VALUES("V");

			scalar_val = ((*scalar_ptr++ >> 20) & 0x3FF);
			sse_val = ((*sse_ptr++ >> 20) & 0x3ff);
			COMPARE_VALUES("Y6");

			pixel += 6;
		}
		scalar_ptr = out_scalar + line * bytes_per_row / 4;
		sse_ptr = out_sse + line * bytes_per_row / 4;
		pixel = 0;
	}

	printf("Max diff seen: %u\n", max_diff_seen);

	return 0;
}

static int		compare_r210_output_buffers(uint32_t* out_sse, uint32_t* out_scalar, PixFcPixelFormat fmt, uint32_t width, uint32_t height, uint8_t max_diff) {
	uint32_t 	line = 0;
	uint32_t 	pixel = 0;
	uint32_t 	bytes_per_row = ROW_SIZE(fmt,  width);
	uint32_t* 	scalar_ptr = out_scalar;
	uint32_t* 	sse_ptr = out_sse;
	uint32_t	scalar_le;
	uint32_t	sse_le;
	uint16_t 	scalar_val;
	uint16_t 	sse_val;
	uint32_t	max_diff_seen = 0;
	
	while(line++ < height) {
		while(pixel < width){
			scalar_le = (*scalar_ptr >> 24) & 0x000000FF;
			scalar_le |= (*scalar_ptr >> 8) & 0x0000FF00;
			scalar_le |= (*scalar_ptr << 8) & 0x00FF0000;
			scalar_le |= (*scalar_ptr << 24)& 0xFF000000;

			sse_le = (*sse_ptr >> 24) & 0x000000FF;
			sse_le |= (*sse_ptr >> 8) & 0x0000FF00;
			sse_le |= (*sse_ptr << 8) & 0x00FF0000;
			sse_le |= (*sse_ptr << 24)& 0xFF000000;

			scalar_val = (scalar_le & 0x3FF);
			sse_val = (sse_le & 0x3ff);
			COMPARE_VALUES("B");
			
			scalar_val = ((scalar_le >> 10) & 0x3FF);
			sse_val = ((sse_le >> 10) & 0x3ff);
			COMPARE_VALUES("G");
			
			scalar_val = ((scalar_le >> 20) & 0x3FF);
			sse_val = ((sse_le >> 20) & 0x3ff);
			COMPARE_VALUES("R");
			
			scalar_ptr++;
			sse_ptr++;
			pixel++;
		}
		scalar_ptr = out_scalar + line * bytes_per_row / 4;
		sse_ptr = out_sse + line * bytes_per_row / 4;
		pixel = 0;
	}
	
	printf("Max diff seen: %u\n", max_diff_seen);
	
	return 0;
}

static int		compare_r10k_output_buffers(uint32_t* out_sse, uint32_t* out_scalar, PixFcPixelFormat fmt, uint32_t width, uint32_t height, uint8_t max_diff) {
	uint32_t 	line = 0;
	uint32_t 	pixel = 0;
	uint32_t 	bytes_per_row = ROW_SIZE(fmt,  width);
	uint32_t* 	scalar_ptr = out_scalar;
	uint32_t* 	sse_ptr = out_sse;
	uint32_t	scalar_le;
	uint32_t	sse_le;
	uint16_t 	scalar_val;
	uint16_t 	sse_val;
	uint32_t	max_diff_seen = 0;
	
	while(line++ < height) {
		while(pixel < width){
			scalar_le = (*scalar_ptr >> 24) & 0x000000FF;
			scalar_le |= (*scalar_ptr >> 8) & 0x0000FF00;
			scalar_le |= (*scalar_ptr << 8) & 0x00FF0000;
			scalar_le |= (*scalar_ptr << 24)& 0xFF000000;
			
			sse_le = (*sse_ptr >> 24) & 0x000000FF;
			sse_le |= (*sse_ptr >> 8) & 0x0000FF00;
			sse_le |= (*sse_ptr << 8) & 0x00FF0000;
			sse_le |= (*sse_ptr << 24)& 0xFF000000;
			
			scalar_val = ((scalar_le >> 2) & 0x3FF);
			sse_val = ((sse_le >> 2) & 0x3ff);
			COMPARE_VALUES("B");
			
			scalar_val = ((scalar_le >> 12) & 0x3FF);
			sse_val = ((sse_le >> 12) & 0x3ff);
			COMPARE_VALUES("G");
			
			scalar_val = ((scalar_le >> 22) & 0x3FF);
			sse_val = ((sse_le >> 22) & 0x3ff);
			COMPARE_VALUES("R");
			
			scalar_ptr++;
			sse_ptr++;
			pixel++;
		}
		scalar_ptr = out_scalar + line * bytes_per_row / 4;
		sse_ptr = out_sse + line * bytes_per_row / 4;
		pixel = 0;
	}
	
	printf("Max diff seen: %u\n", max_diff_seen);
	
	return 0;
}


static int		compare_output_buffers(void* out_sse, void* out_scalar, PixFcPixelFormat fmt, uint32_t width, uint32_t height, uint8_t max_diff) {
	if (fmt == PixFcV210)
		return compare_v210_output_buffers((uint32_t*)out_sse, (uint32_t*)out_scalar, fmt, width, height, max_diff);
	else if (fmt == PixFcR210)
		return compare_r210_output_buffers((uint32_t*)out_sse, (uint32_t*)out_scalar, fmt, width, height, max_diff);
	else if (fmt == PixFcR10k)
		return compare_r10k_output_buffers((uint32_t*)out_sse, (uint32_t*)out_scalar, fmt, width, height, max_diff);
	else
		return compare_8bit_output_buffers((uint8_t*)out_sse, (uint8_t*)out_scalar, fmt, width, height, max_diff);
}

static void 	print_usage(char *prog_name) {
	printf("Usage: %s [ threshold ] [<src_pix_fmt> <dest_pix_fmt> <flags>]\n", prog_name);
	printf("Compare SSE conversion with scalar conversion, and stop if a difference\n");
	printf("between two values is greater than the provided threshold.\n");
	printf("Where <src_pix_fmt> and <dest_pix_fmt> are one of:\n");
	print_known_pixel_formats();
	printf("and flag is a bitmask from the following values:\n");
	print_known_flags();
}

static void 		parse_args(int argc, char **argv) {
	uint32_t	pix_fmt_offset = 1;
		
	// Check and parse args
	if ((argc != 1) && (argc != 2) && (argc != 4) && (argc != 5)) {
		print_usage(argv[0]);
		exit(1);
	}

	if ((argc == 2) || (argc == 5)) {
		max_diff = atoi(argv[1]);
		pix_fmt_offset = 2;
	} else {
		max_diff = 5;
	}

	if ((argc == 4) || (argc == 5)){
		restrict_to_src_fmt = find_matching_pixel_format(argv[pix_fmt_offset]);
		if (restrict_to_src_fmt == PixFcFormatCount) {
			printf("Unknown pixel format '%s'\n", argv[pix_fmt_offset]);
			exit(1);
		}

		restrict_to_dst_fmt = find_matching_pixel_format(argv[pix_fmt_offset + 1]);
		if (restrict_to_dst_fmt == PixFcFormatCount) {
			printf("Unknown pixel format '%s'\n", argv[pix_fmt_offset + 1]);
			exit(1);
		}

        printf("Using:\n - source pixel format '%s'\n", pixfmt_descriptions[restrict_to_src_fmt].name);
        printf(" - dest pixel format '%s'\n", pixfmt_descriptions[restrict_to_dst_fmt].name);
		restrict_to_flags = get_matching_flags(argv[pix_fmt_offset + 2]);
		printf(" - flags: ");
		print_flags(restrict_to_flags);
	}
}

static uint32_t		setup_input_buffer(const InputFile* in_file) {
	static PixFcPixelFormat 	prev_input_format = PixFcFormatCount;

	// If we have changed input format...
	if (in_file->format != prev_input_format){
		// Release previous image and load image in new format
		ALIGN_FREE(in);

		// Try to get RGB image if possible
		if (fill_buffer_with_rgb_image(in_file->format, in_file->width, in_file->height, (void**) &in) != 0) {
			// Otherwise, use whatever is specified in the InputFile struct
			if (get_buffer_from_input_file(in_file, (void **) &in) != 0) {
				pixfc_log("Error setting up input buffer");
				return 1;
			}
		}

		prev_input_format = in_file->format;
	}

	return 0;
}

static int32_t		setup_output_buffers(uint32_t refresh_scalar_buffer, PixFcPixelFormat dest_format, uint32_t width, uint32_t height) {
    uint32_t	result;
    __m128i *	output_buffer = NULL;

    result = allocate_aligned_buffer(dest_format, width, height, (void **)&output_buffer);
    if (result == -2) {
		pixfc_log("Invalid width / height for SSE conversion of pixel format '%s'\n", pixfmt_descriptions[dest_format].name);
		return -2;
	} else if (result != 0) {
    	pixfc_log("Error allocating SSE output buffer\n");
        return -1;
    }

    ALIGN_FREE(out);
    out = output_buffer;

    //  Re-allocate scalar buffer only if required
    if (refresh_scalar_buffer != 0) {
        ALIGN_FREE(out_scalar);

        if (allocate_aligned_buffer(dest_format, width, height, (void **)&out_scalar) != 0) {
            pixfc_log("Error allocating scalar output buffer\n");
            return -1;
        }
    }

	return 0;
}
static int		check_sse_conversion_block(uint32_t sse_conv_index) {
	PixFcPixelFormat 	src_fmt = conversion_blocks[sse_conv_index].source_fmt;
	PixFcPixelFormat 	dst_fmt = conversion_blocks[sse_conv_index].dest_fmt;
	int32_t				scalar_conv_index;
	uint32_t			scalar_conv_changed = 0;
	PixFcFlag			scalar_flags;
	PixFcFlag			sse_flags;
	const InputFile*	in_file = NULL;
	uint32_t			in_file_index = 0;
	uint32_t			result;
	//
	static int32_t		prev_scalar_conv_index = -1;
	static uint32_t		prev_scalar_conv_width = 0;
	static uint32_t		prev_scalar_conv_height = 0;

	// Find input file
	while ((in_file = find_input_file_for_format(src_fmt, in_file_index++)) != NULL) {
		// Enforce flags if specified on the command line
		sse_flags = synthesize_pixfc_flags(sse_conv_index);
		if ((restrict_to_flags != PixFcFlag_Default) && (sse_flags != restrict_to_flags))
			return 0;

		// Synthetise scalar conversion flags from the SSE conversion flags:
		// Turn off SSE-related flags and add NoSSEFLoat
		scalar_flags = (sse_flags & (~(PixFcFlag_NoSSE | PixFcFlag_SSE2Only | PixFcFlag_SSE2_SSSE3Only | PixFcFlag_SSE2_SSSE3_SSE41Only))) | PixFcFlag_NoSSEFloat;

		// prepare input buffers
		if (setup_input_buffer(in_file) != 0) {
			pixfc_log("Error setting up input buffer\n");
			return -1;
		}

		printf("Comparing '%s' with ", conversion_blocks[sse_conv_index].name);

		// Find the non-sse conversion block matching the given conversion block at 'index'
		scalar_conv_index = find_conversion_block_index(src_fmt, dst_fmt, scalar_flags, in_file->width, in_file->height, ROW_SIZE(src_fmt, in_file->width), ROW_SIZE(dst_fmt, in_file->width));
		if (scalar_conv_index == -1) {
			pixfc_log("Error finding non-sse conversion matching '%s'\n", conversion_blocks[sse_conv_index].name);
			return -1;
		}

		//
		// Sanity checks
		if ((conversion_blocks[scalar_conv_index].source_fmt != src_fmt) || (conversion_blocks[scalar_conv_index].dest_fmt != dst_fmt)) {
			pixfc_log("SSE and scalar conversions' source and dest formats do not match.\n");
			return -1;
		}
		// The sse conversion's attribute should be equal to the scalar one minus the NONSSE_FLOAT attribute
		if (conversion_blocks[sse_conv_index].attributes != (conversion_blocks[scalar_conv_index].attributes & ~(NONSSE_FLOAT_CONVERSION))) {
			pixfc_log("SSE and scalar conversions' flags do not match.\n");
			return -1;
		}

		printf("'%s' %dx%d\n", conversion_blocks[scalar_conv_index].name, in_file->width, in_file->height);

		// Check if the scalar conversion routine and/or if the resolution has changed.
		if ((scalar_conv_index != prev_scalar_conv_index) || (in_file->width != prev_scalar_conv_width) || (in_file->height != prev_scalar_conv_height)) {
			scalar_conv_changed = 1;
			prev_scalar_conv_index = scalar_conv_index;
			prev_scalar_conv_width = in_file->width;
			prev_scalar_conv_height = in_file->height;
		}

		// Prepare output buffers
		result = setup_output_buffers(scalar_conv_changed, dst_fmt, in_file->width, in_file->height);
		if (result == -2)
			// invalid width / height for given dest pixel format, move on to next conversion
			continue;
		else if (result != 0) {
			pixfc_log("Error setting up output buffer\n");
			return -1;
		}


		// Do SSE conversion
		result = do_image_conversion(sse_conv_index, in, out, in_file->width, in_file->height);
		if (result == PixFc_UnsupportedSourceImageDimension)
			// invalid width / height for given dest pixel format, move on to next conversion
			continue;
		else if (result != 0)
			return -1;

		// Do scalar conversion if required
		if (scalar_conv_changed) {
		   if (do_image_conversion(scalar_conv_index, in, out_scalar, in_file->width, in_file->height) != 0)
				return -1;
		}

		// Compare the two output buffers
		if (compare_output_buffers(out, out_scalar, dst_fmt, in_file->width, in_file->height, max_diff) != 0) {
			char sse_filename[128] = {0};
			char scalar_filename[128] = {0};
			char input_filename[128] = {0};
	
			SNPRINTF(input_filename, sizeof(sse_filename), "%d_%d-%s_to_%s-input_buffer", in_file->width, in_file->height, pixfmt_descriptions[src_fmt].name, pixfmt_descriptions[dst_fmt].name);
			SNPRINTF(sse_filename, sizeof(sse_filename), "%d_%d-%s_to_%s-sse_buffer", in_file->width, in_file->height, pixfmt_descriptions[src_fmt].name, pixfmt_descriptions[dst_fmt].name);
			SNPRINTF(scalar_filename, sizeof(scalar_filename), "%d_%d-%s_to_%s-scalar_buffer", in_file->width, in_file->height, pixfmt_descriptions[src_fmt].name, pixfmt_descriptions[dst_fmt].name);
	
			printf("Dumping input, scalar and sse buffers\n");
			write_buffer_to_file(src_fmt, in_file->width, in_file->height, input_filename, in);
			write_buffer_to_file(dst_fmt, in_file->width, in_file->height, sse_filename, out);
			write_buffer_to_file(dst_fmt, in_file->width, in_file->height, scalar_filename, out_scalar);
			return 1;
		} else {
			printf("OK\n");
		}
	}

	if ((in_file == NULL) && (in_file_index == 1)){
		pixfc_log("Error looking for input file for format '%s'\n", pixfmt_descriptions[src_fmt].name);
		return -1;
	}

	return 0;
}

int 			main(int argc, char **argv) {
	uint32_t				index;
	uint32_t				result = 0;

	// Parse args
	parse_args(argc, argv);

	// Loop over all conversion blocks
	for(index = 0; index < conversion_blocks_count; index++) {
		// if we need to restrict to src / dst formats, enforce it now
		if (((restrict_to_src_fmt != PixFcFormatCount) && (conversion_blocks[index].source_fmt != restrict_to_src_fmt))
				|| ((restrict_to_dst_fmt != PixFcFormatCount) && (conversion_blocks[index].dest_fmt != restrict_to_dst_fmt)))
			continue;

		// Make sure we skipped the scalar float conversion routines
		// (we dont want to compare them with themselves)
		if ((conversion_blocks[index].required_cpu_features == CPUID_FEATURE_NONE) && ((conversion_blocks[index].attributes & NONSSE_FLOAT_CONVERSION) != 0))
			continue;

		// Make sure the CPU has the required features
		if (does_cpu_support(conversion_blocks[index].required_cpu_features) != 0) {
			printf("  (skipped %s - CPU missing required features)\n",
				   conversion_blocks[index].name);
			continue;
		}

		result = check_sse_conversion_block(index);
		if (result != 0)
			break;

		// Add a blank line if the next conversion block uses different
		// source or destinaton format
		if (((index + 1) < conversion_blocks_count)
			&& ((conversion_blocks[(index + 1)].source_fmt !=
				 conversion_blocks[index].source_fmt)
				|| (conversion_blocks[(index + 1)].dest_fmt !=
					conversion_blocks[index].dest_fmt)))
			printf("\n");
	}
	
	ALIGN_FREE(in);
	ALIGN_FREE(out);			   
	ALIGN_FREE(out_scalar);
	
	return result;
}


