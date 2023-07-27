# zlib

## zlib format

See http://tools.ietf.org/html/rfc1950 (zlib format)

zlib can decode Flate data in Adobe PDF files.

zlib can't handle .zip archives. (See the directory contrib/minizip for something that can).

The `compress` and `deflate` functions produce data in the zlib format, which
is different and incompatible with the gzip format.

compress() and uncompress() may be limited to 4 GB, since they operate in a
single call.

The zlib format was designed for in-memory and communication channel
applications, and has a much more compact header and trailer and uses a
faster integrity check than gzip.

You can request that `deflate` write the gzip format instead of the zlib
format using deflateInit2(). You can also request that inflate decode the
gzip format using inflateInit2().

In HTTP "deflate" is the zlib format.
They should probably have called the second one "zlib" instead to avoid confusion with
the raw deflate compressed data format. While the HTTP 1.1 RFC 2616
correctly points to the zlib specification in RFC 1950 for the "deflate"
transfer encoding, there have been reports of servers and browsers that
incorrectly produce or expect raw deflate data per the deflate
specification in RFC 1951, most notably Microsoft. So even though the
"deflate" transfer encoding using the zlib format would be the more
efficient approach (and in fact exactly what the zlib format was designed
for), using the "gzip" transfer encoding is probably more reliable due to
an unfortunate choice of name on the part of the HTTP 1.1 authors.
Bottom line: use the gzip format for HTTP 1.1 encoding.

## deflate format

The deflate format was defined by Phil Katz.
To understand the deflate format read RFC 1951.
Also check contrib/puff directory.

inflate() and deflate() will process any amount of data correctly.
Each call of inflate() or deflate() is limited to input and output chunks
of the maximum value that can be stored in the compiler's "unsigned int"
type, but there is no limit to the number of chunks.

## gzip format

See rfc1952 (gzip format).

The gzip format was designed to retain the directory information about a
single file, such as the name and last modification date.

The gz\* functions in zlib on use the gzip format.

Both the zlib and gzip formats use the same compressed data format internally,
but have different headers and trailers around the compressed data.

gzseek() and gztell() may be limited to 4 GB depending on how
zlib is compiled. See the zlibCompileFlags() function in zlib.h.

In HTTP 1.1 "gzip" is the gzip format.
