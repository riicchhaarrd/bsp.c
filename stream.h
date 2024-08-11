#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

enum
{
	STREAM_SEEK_BEG,
	STREAM_SEEK_CUR,
	STREAM_SEEK_END
};

typedef struct Stream_s
{
	void *ctx;

	int64_t (*tell)(struct Stream_s *s);
	/* This function returns zero if successful, or else it returns a non-zero value. */
	int (*seek)(struct Stream_s *s, int64_t offset, int whence);

	int (*name)(struct Stream_s *stream, char *buffer, size_t size);
	int (*eof)(struct Stream_s *stream);
	size_t (*read)(struct Stream_s *stream, void *ptr, size_t size, size_t nmemb);
} Stream;

static size_t stream_read_buffer(Stream *s, void *ptr, size_t n)
{
	return s->read(s, ptr, n, 1);
}

#define stream_read(s, ptr) stream_read_buffer(&(s), &(ptr), sizeof(ptr))

static int stream_read_line(Stream *s, char *line, size_t max_line_length)
{
	size_t n = 0;
	line[n] = 0;

	int eol = 0;
	int eof = 0;
	size_t offset = 0;
	while(!eol)
	{
		uint8_t ch = 0;
		if(0 == s->read(s, &ch, 1, 1) || !ch)
		{
			eof = 1;
			break;
		}
		if(n + 1 >= max_line_length) // n + 1 account for \0
		{
			break;
		}
		switch(ch)
		{
			case '\r': break;
			case '\n': eol = 1; break;
			default: line[n++] = ch; break;
		}
	}
	line[n] = 0;
	return eof;
}
