# zlib

The 'zlib' compression library provides in-memory compression and
decompression functions, including integrity checks of the uncompressed data.
This version of the library supports only one compression method (deflation)
but other algorithms will be added later and will have the same stream
interface.

Compression can be done in a single step if the buffers are large enough,
or can be done by repeated calls of the compression function. In the latter
case, the application must provide more input and/or consume the output
(providing more output space) before each call.

The compressed data format used by default by the in-memory functions is
the zlib format, which is a zlib wrapper documented in RFC 1950, wrapped
around a deflate stream, which is itself documented in RFC 1951.

The library also supports reading and writing files in gzip (.gz) format
with an interface similar to that of stdio using the functions that start
with "gz". The gzip format is different from the zlib format. gzip is a
gzip wrapper, documented in RFC 1952, wrapped around a deflate stream.

This library can optionally read and write gzip and raw deflate streams in
memory as well.

The zlib format was designed to be compact and fast for use in memory
and on communications channels. The gzip format was designed for single-
file compression on file systems, has a larger header than zlib to maintain
directory information, and uses a different, slower check method than zlib.

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

Deutsch, L.P.,"DEFLATE Compressed Data Format Specification".
Available in http://tools.ietf.org/html/rfc1951

A description of the Rabin and Karp algorithm is given in the book
"Algorithms" by R. Sedgewick, Addison-Wesley, p252.

Fiala,E.R., and Greene,D.H.
Data Compression with Finite Windows, Comm.ACM, 32,4 (1989) 490-595

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
