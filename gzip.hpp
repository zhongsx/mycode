/*
	code from https://github.com/waveto/node-compress
*/

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <zlib.h>

const static int CHUNK = 16384;

class gzip_compress
{
 private:
	z_stream strm;	
 public:
	gzip_compress()
	{

	}

	~gzip_compress()
	{

	}

	int gzip_init(int level=Z_DEFAULT_COMPRESSION)
	{
		int ret;
		/* allocate deflate state */
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		ret = deflateInit2(&strm, level, Z_DEFLATED, 16+MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
		
		return ret;
	}
	/*
		Compress gzip data
		 data      原数据
		 data_len  原数据长度
		 out       压缩后数据 (设定为空)
		 out_len   压缩后数据长度
		 注：自动realloc
	*/
	int gzip_deflate(const char* data, int data_len, char** out, int* out_len) 
	{
		int ret;
		char* temp;
		int i=1;

		*out = NULL;
		*out_len = 0;
		ret = 0;

		if (data_len == 0)
			return 0;

		while(data_len > 0) 
		{    
			if (data_len > CHUNK) {
				strm.avail_in = CHUNK;
			} else {
				strm.avail_in = data_len;
			}

			strm.next_in = (Bytef*)data;
			do {
				temp = (char *)realloc(*out, CHUNK*i +1);
				if (temp == NULL) {
					return Z_MEM_ERROR;
				}
				*out = temp;
				strm.avail_out = CHUNK;
				strm.next_out = (Bytef*)*out + *out_len;
				ret = deflate(&strm, Z_NO_FLUSH);
				assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
				*out_len += (CHUNK - strm.avail_out);
				i++;
			} while (strm.avail_out == 0);

			data += CHUNK;
			data_len -= CHUNK;
		}
		return ret;
	}

	/*
		out       压缩后数据 
	    out_len   压缩后数据长度	
		注：自动realloc
	*/
	int gzip_end(char** out, int*out_len)
	{
		int ret;
		char* temp;
		int i = 1;

		*out = NULL;
		*out_len = 0;
		strm.avail_in = 0;
		strm.next_in = NULL;

		do {
			temp = (char *)realloc(*out, CHUNK*i);
			if (temp == NULL) {
				return Z_MEM_ERROR;
			}
			*out = temp;
			strm.avail_out = CHUNK;
			strm.next_out = (Bytef*)*out + *out_len;
			ret = deflate(&strm, Z_FINISH);
			assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
			*out_len += (CHUNK - strm.avail_out);
			i++;
		} while (strm.avail_out == 0);
    

		deflateEnd(&strm);
		return ret;
	}

protected:
	static int GzipInit(gzip_compress* gzip)
	{
		gzip = new gzip_compress();

		int level = Z_DEFAULT_COMPRESSION;
		int  ret = gzip->gzip_init(level);

		return ret;
	}

	static int GzipCompress(gzip_compress* gzip, const char* data, int dlen, char* buff, int blen)
	{
		if (data == NULL || buff == NULL || dlen <= 0 ||blen <= 0)  return -1;

		char* out;
		int out_size;
		int r = gzip->gzip_deflate(data, dlen, &out, &out_size);
		if (out_size == 0) 
		{
			return 0;		
		}
		r = (blen > out_size) ? out_size: blen;
		memcpy(buff, out, r);
		free(out);
		
		return r;
	}

	static int GzipEnd(gzip_compress* gzip, char* buff, int blen)
	{
		if (buff == NULL || blen <= 0) return -1;

		char* out;
		int out_size;
		int r = gzip->gzip_end(&out, &out_size);
		if (out_size == 0)
		{
			return 0;
		}
		r = (blen > out_size) ? out_size: blen;
		memcpy(buff, out, r);
		free(out);

		if (gzip)
			delete gzip;

		return r;
	}
};

class gzip_decompress
{
private:
	z_stream strm;
public:
	gzip_decompress()
	{

	}

	~gzip_decompress()
	{

	}

	int gzip_init()
	{
		/* allocate inflate state */
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		strm.avail_in = 0;
		strm.next_in = Z_NULL;
		int ret = inflateInit2(&strm, 16+MAX_WBITS);
		return ret;
	}
	/*
		DeCompress gzip data
		 data      原数据
		 data_len  原数据长度
		 out       解压后数据 (设定为空)
		 out_len   解压后数据长度
		 注：自动realloc
	*/
	int gzip_inflate(const char* data, int data_len, char** out, int* out_len)
	{
		int ret;
		char* temp;
		int i=1;

		*out = NULL;
		*out_len = 0;

		if (data_len == 0)
			return 0;
		
		while(data_len>0) 
		{    
			if (data_len>CHUNK)
			{
				strm.avail_in = CHUNK;
			} else
			{
				strm.avail_in = data_len;
			}

			strm.next_in = (Bytef*)data;
			do 
			{
				temp = (char *)realloc(*out, CHUNK*i);
				if (temp == NULL)
				{
					return Z_MEM_ERROR;
				}
				*out = temp;
				strm.avail_out = CHUNK;
				strm.next_out = (Bytef*)*out + *out_len;
				ret = inflate(&strm, Z_NO_FLUSH);
				assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
				switch (ret) 
				{
				case Z_NEED_DICT:
					ret = Z_DATA_ERROR;     /* and fall through */
				case Z_DATA_ERROR:
				case Z_MEM_ERROR:
					(void)inflateEnd(&strm);
					return ret;
				}
				*out_len += (CHUNK - strm.avail_out);
				i++;
			} while (strm.avail_out == 0);
			data += CHUNK;
			data_len -= CHUNK;
		}
		return ret;
	}

	void gzip_end() 
	{
		inflateEnd(&strm);
	}

protected:
	static int GzipInit(gzip_decompress* gzip)
	{
		gzip = new gzip_decompress();

		int  ret = gzip->gzip_init();

		return ret;
	}
	
	static int GzipDeCompress(gzip_decompress* gzip, const char* data, int dlen, char* buff, int blen)
	{
		if (data == NULL || buff == NULL || dlen <= 0 ||blen <= 0)  return -1;

		char* out;
		int out_size;
		int r = gzip->gzip_inflate(data, dlen, &out, &out_size);
		if (out_size == 0) 
		{
			return 0;		
		}
		r = (blen > out_size) ? out_size: blen;
		memcpy(buff, out, r);
		free(out);
		
		return r;
	}

	static int GzipEnd(gzip_decompress* gzip)
	{
		gzip->gzip_end();
		if (gzip)
			delete gzip;
		return 0;
	}
};