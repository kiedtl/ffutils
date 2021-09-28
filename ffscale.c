#include <arpa/inet.h>
#include <err.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))

enum FF_Status {
	FF_S_Success,
	FF_S_BadMagic,
	FF_S_ExpectedMoar,
	FF_S_IOError,
	FF_S_AllocError,
};

struct FF_Pixel {
	uint16_t r;
	uint16_t g;
	uint16_t b;
	uint16_t a;
};

struct FF {
	size_t height;
	size_t width;
	struct FF_Pixel *pixels; 
};

enum FF_Status
efread(void *p, size_t s, size_t n, FILE *fp)
{
	if (fread(p, s, n, fp) != n) {
		if (ferror(fp)) {
			return FF_S_IOError;
		} else {
			return FF_S_ExpectedMoar;
		}
	}

	return FF_S_Success;
}

enum FF_Status
read_ff(struct FF *out, FILE *fp)
{
	enum FF_Status e;
	uint8_t magic[8];
	uint32_t buf[2];

	e = efread(magic, sizeof(uint8_t), ARRAY_LEN(magic), fp);
	if (e != FF_S_Success) return e;

	if (strncmp(magic, "farbfeld", sizeof(magic)))
		return FF_S_BadMagic;

	e = efread(buf, sizeof(uint32_t), ARRAY_LEN(buf), fp);
	if (e != FF_S_Success) return e;

	out->width = ntohl(buf[0]);
	out->height = ntohl(buf[1]);

	size_t sz = out->width * out->height;
	out->pixels = calloc(sz, sizeof(struct FF_Pixel));
	if (out->pixels == NULL) return FF_S_AllocError;

	for (size_t i = 0; i < sz; ++i) {
		uint16_t buf[4];

		e = efread(buf, sizeof(uint16_t), ARRAY_LEN(buf), fp);
		if (e != FF_S_Success) return e;

		out->pixels[i].r = ntohs(buf[0]);
		out->pixels[i].g = ntohs(buf[1]);
		out->pixels[i].b = ntohs(buf[2]);
		out->pixels[i].a = ntohs(buf[3]);
	}

	return FF_S_Success;
}

void
scale_ff(struct FF *ff, struct FF *out, size_t scale)
{
	out->width = ff->width * scale;
	out->height = ff->height * scale;

	size_t sz = out->width * out->height;
	out->pixels = calloc(sz, sizeof(struct FF_Pixel));
	// TODO: handle calloc failure

	for (size_t y = 0; y < ff->height; ++y) {
		for (size_t x = 0; x < ff->width; ++x) {
			struct FF_Pixel px = ff->pixels[y * ff->width + x];

			for (size_t dy = 0; dy < scale; ++dy) {
				for (size_t dx = 0; dx < scale; ++dx) {
					size_t coordy = (y * scale) + dy;
					size_t coordx = (x * scale) + dx;
					out->pixels[coordy * out->width + coordx] = px;
				}
			}
		}
	}
}

void
write_ff(struct FF *ff, FILE *fp)
{
	/* write farbfeld headers */
	fputs("farbfeld", fp);
	uint32_t tmp;
	tmp = htonl(ff->width);
	fwrite(&tmp, sizeof(tmp), 1, fp);
	tmp = htonl(ff->height);
	fwrite(&tmp, sizeof(tmp), 1, fp);

	/* write image row by row */
	uint16_t *rowbuf = malloc(ff->width * (4 * sizeof(uint16_t)));

	for (size_t y = 0; y < ff->height; ++y) {
		for (size_t x = 0; x < ff->width; ++x) {
			struct FF_Pixel px = ff->pixels[y * ff->width + x];

			rowbuf[4 * x + 0] = htons(px.r);
			rowbuf[4 * x + 1] = htons(px.g);
			rowbuf[4 * x + 2] = htons(px.b);
			rowbuf[4 * x + 3] = htons(px.a);
		}

		fwrite(rowbuf, sizeof(uint16_t), ff->width * 4, fp);
		memset(rowbuf, 0, ff->width * (4 * sizeof(uint16_t)));
	}

	free(rowbuf);
}

int
main(int argc, char **argv)
{
	if (argc != 2) {
		errx(1, "usage: %s [pixel-size] < input.ff > output.ff", argv[0]);
	}

	if (isatty(STDIN_FILENO)) {
		errx(1, "refusing to read farfeld image from terminal input.");
	}

	if (isatty(STDOUT_FILENO)) {
		errx(1, "refusing to write farbfeld image to the terminal.");
	}

	// TODO: handle invalid inputs (negative, nan, float, zero, etc)
	double scale_raw = strtod(argv[1], NULL);
	size_t scale = (size_t)scale_raw;

	struct FF pre;
	struct FF new;
	enum FF_Status s;

	s = read_ff(&pre, stdin);
	if (s != FF_S_Success) switch (s) {
	break; case FF_S_BadMagic:
		errx(1, "bad farbfeld magic value.");
	break; case FF_S_ExpectedMoar:
		errx(1, "unexpected end of file.");
	break; case FF_S_IOError:
		err(1, "io error");
	break; case FF_S_AllocError:
		err(1, "allocation error");
	break; default:
		errx(1, "unreachable");
	break;
	};

	scale_ff(&pre, &new, scale);
	write_ff(&new, stdout);

	free(pre.pixels);
	free(new.pixels);
}
