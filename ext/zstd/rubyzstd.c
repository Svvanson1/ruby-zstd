
#include <ruby.h>
#include <ruby/encoding.h>

#include <libzstd/zstd.h>


static VALUE compress(int argc, VALUE *argv, VALUE self)
{
  VALUE input_value;
  VALUE compression_level_value;
  rb_scan_args(argc, argv, "11", &input_value, &compression_level_value);
  
  Check_Type(input_value, RUBY_T_STRING);
  const char* input_data = RSTRING_PTR(input_value);
  size_t input_size = RSTRING_LEN(input_value);

  int compression_level;
  if (NIL_P(compression_level_value)) {
    compression_level = 1;
  } else {
    Check_Type(compression_level_value, RUBY_T_FIXNUM);
    compression_level = FIX2INT(compression_level_value);
  }

  // do compress
  size_t max_compressed_size = ZSTD_compressBound(input_size);

  VALUE output = rb_str_new(NULL, max_compressed_size);
  char* output_data = RSTRING_PTR(output);

  size_t compressed_size = ZSTD_compress((void*)output_data, max_compressed_size,
                                         (const void*)input_data, input_size, 1);

  if (ZSTD_isError(compressed_size)) {
    rb_raise(rb_eRuntimeError, "%s: %s", "compress failed", ZSTD_getErrorName(compressed_size));
  } else {
    rb_str_resize(output, compressed_size);
  }

  return output;
}

static VALUE decompress_buffered(const char* input_data, size_t input_size)
{
  const size_t outputBufferSize = 4096;

  ZSTD_DStream* const dstream = ZSTD_createDStream();
  if (dstream == NULL) {
    rb_raise(rb_eRuntimeError, "%s", "ZSTD_createDStream failed");
  }

  size_t initResult = ZSTD_initDStream(dstream);
  if (ZSTD_isError(initResult)) {
    ZSTD_freeDStream(dstream);
    rb_raise(rb_eRuntimeError, "%s: %s", "ZSTD_initDStream failed", ZSTD_getErrorName(initResult));
  }


  VALUE output_string = rb_str_new(NULL, 0);
  ZSTD_outBuffer output = { NULL, 0, 0 };

  ZSTD_inBuffer input = { input_data, input_size, 0 };
  while (input.pos < input.size) {
    output.size += outputBufferSize;
    rb_str_resize(output_string, output.size);
    output.dst = RSTRING_PTR(output_string);

    size_t readHint = ZSTD_decompressStream(dstream, &output, &input);
    if (ZSTD_isError(readHint)) {
      ZSTD_freeDStream(dstream);
      rb_raise(rb_eRuntimeError, "%s: %s", "ZSTD_decompressStream failed", ZSTD_getErrorName(readHint));
    }
  }

  ZSTD_freeDStream(dstream);
  rb_str_resize(output_string, output.pos);
  return output_string;
}

static VALUE decompress(VALUE self, VALUE input)
{
  Check_Type(input, T_STRING);
  const char* input_data = RSTRING_PTR(input);
  size_t input_size = RSTRING_LEN(input);

  uint64_t uncompressed_size = ZSTD_getDecompressedSize(input_data, input_size);

  if (uncompressed_size == 0) {
    return decompress_buffered(input_data, input_size);
  }

  VALUE output = rb_str_new(NULL, uncompressed_size);
  char* output_data = RSTRING_PTR(output);

  size_t decompress_size = ZSTD_decompress((void*)output_data, uncompressed_size,
                                           (const void*)input_data, input_size);

  if (ZSTD_isError(decompress_size)) {
    rb_raise(rb_eRuntimeError, "%s: %s", "decompress error", ZSTD_getErrorName(decompress_size));
  }

  return output;
}

void Init_zstd()
{
  VALUE mZstd = rb_const_get(rb_cObject, rb_intern("Zstd"));

  rb_define_module_function(mZstd, "compress", compress, -1);
  rb_define_module_function(mZstd, "decompress", decompress, 1);
}
