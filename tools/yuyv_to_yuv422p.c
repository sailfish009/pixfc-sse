/*
 * yuyv_to_yuv422p.c
 *
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

/*
 * Converts a raw YUYV file to YUV422P and save the result in a new file.
 */


#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>


static uint32_t 		width;
static uint32_t 		height;
static uint8_t*			input_file;

int32_t 	get_buffer_from_file(char *filename, void **buffer) {
	uint8_t *						file_buffer;
	struct stat						file_stat;
	int								fd;
	int32_t							bytes_read = 0;


	// open file
	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		printf("Error opening input file '%s'\n", filename);
		return -1;
	}

	// get file size
	if (fstat(fd, &file_stat) != 0) {
		printf("Error stat'ing file\n");
		close(fd);
		return -1;
	}

	// allocate buffer
	file_buffer = (uint8_t*) malloc(file_stat.st_size);

	// place data in buffer
	while(bytes_read < file_stat.st_size)
		bytes_read += read(fd, (file_buffer + bytes_read), (file_stat.st_size - bytes_read));

	*buffer = file_buffer;

	close(fd);
	return bytes_read;
}

 void write_buffer_to_file(char *filename, void * in, uint32_t buf_size) {
	int32_t			fd;
	uint32_t		count = 0;
	uint8_t			*buffer = (uint8_t *) in;


	fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
	if (fd == -1) {
		printf("Error opening the file for writing\n");
		return;
	}

	while (count < buf_size) {
		int ret = write(fd, (buffer + count), (buf_size - count));
		if (ret < 0) {
			printf("Error writing PPM file contents\n");
			break;
		}
		count += ret;
	}

	close(fd);
}

void parse_args(int argc, char **argv) {
	if (argc != 4) {
		printf("Usage: %s <input_file> <width> <height>\n", argv[0]);
		exit(1);
	}

	input_file = argv[1];
	width = atoi(argv[2]);
	height = atoi(argv[3]);
}

int main (int argc, char **argv) {
	uint8_t*			input;
	uint8_t*			src;
	int32_t				input_size;
	uint32_t			output_size;
	uint8_t*			output;
	uint8_t*			y_plane;
	uint8_t*			u_plane;
	uint8_t*			v_plane;
	uint32_t			pixel_count;

	// Make sure we have the right args
	parse_args(argc, argv);

	pixel_count = width * height;

	// Read the input file
	input_size = get_buffer_from_file(input_file, (void **)&input);

	output_size = width * height * 2;

	output = (uint8_t *) malloc(output_size);
	y_plane = output;
	u_plane = y_plane + pixel_count;
	v_plane = u_plane + pixel_count / 2;
	src = input;

	// Do conversion
	while(pixel_count > 0) {
		*y_plane++ = *src++;
		*u_plane++ = *src++;
		*y_plane++ = *src++;
		*v_plane++ = *src++;

		pixel_count -= 2;
	}

	// Save result
	write_buffer_to_file("output.yuv422p", output, output_size);

	free(output);
	free(input);
}

