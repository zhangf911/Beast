//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_PARSE_HPP
#define BEAST_HTTP_PARSE_HPP

#include <beast/core/error.hpp>
#include <beast/core/async_completion.hpp>
#include <beast/http/header_parser.hpp>
#include <beast/http/message_parser.hpp>

namespace beast {
namespace http {

/** Parse some HTTP/1 message data from a stream.

    This function synchronously advances the state of the
    parser using the provided dynamic buffer and reading
    from the input stream as needed. The call will block
    until one of the following conditions is true:

    @li When expecting a message header, and the complete
        header is received.

    @li When expecting a chunk header, and the complete
        chunk header is received.

    @li When expecting body octets, one or more body octets
        are received.

    @li An error occurs in the stream or parser.

    This function is implemented in terms of one or more calls
    to the stream's `read_some` function. The implementation may
    read additional octets that lie past the end of the object
    being parsed. This additional data is stored in the dynamic
    buffer, which may be used in subsequent calls.

    @param stream The stream from which the data is to be read.
    The type must support the @b SyncReadStream concept.

    @param dynabuf A @b DynamicBuffer holding additional bytes
    read by the implementation from the stream. This is both
    an input and an output parameter; on entry, any data in the
    dynamic buffer's input sequence will be given to the parser
    first.

    @param parser The parser to use.

    @return The number of bytes processed from the dynamic
    buffer. The caller should remove these bytes by calling
    `consume` on the dynamic buffer.

    @throws system_error Thrown on failure.
*/
template<class SyncReadStream, class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived>
std::size_t
parse_some(SyncReadStream& stream, DynamicBuffer& dynabuf,
    basic_parser<isRequest, isDirect, Derived>& parser);

/** Parse some HTTP/1 message data from a stream.

    This function synchronously advances the state of the
    parser using the provided dynamic buffer and reading
    from the input stream as needed. The call will block
    until one of the following conditions is true:

    @li When expecting a message header, and the complete
        header is received.

    @li When expecting a chunk header, and the complete
        chunk header is received.

    @li When expecting body octets, one or more body octets
        are received.

    @li An error occurs in the stream or parser.

    This function is implemented in terms of one or more calls
    to the stream's `read_some` function. The implementation may
    read additional octets that lie past the end of the object
    being parsed. This additional data is stored in the dynamic
    buffer, which may be used in subsequent calls.

    @param stream The stream from which the data is to be read.
    The type must support the @b SyncReadStream concept.

    @param dynabuf A @b DynamicBuffer holding additional bytes
    read by the implementation from the stream. This is both
    an input and an output parameter; on entry, any data in the
    dynamic buffer's input sequence will be given to the parser
    first.

    @param parser The parser to use.

    @param ec Set to the error, if any occurred.

    @return The number of bytes processed from the dynamic
    buffer. The caller should remove these bytes by calling
    `consume` on the dynamic buffer.
*/
#if GENERATING_DOCS
template<class SyncReadStream, class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived>
std::size_t
parse_some(SyncReadStream& stream, DynamicBuffer& dynabuf,
    basic_parser<isRequest, isDirect, Derived>& parser,
        error_code& ec);
#else

template<class SyncReadStream, class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
parse_some(SyncReadStream& stream, DynamicBuffer& dynabuf,
    basic_parser<isRequest, true, Derived>& parser,
        error_code& ec);

template<class SyncReadStream, class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
parse_some(SyncReadStream& stream, DynamicBuffer& dynabuf,
    basic_parser<isRequest, false, Derived>& parser,
        error_code& ec);

#endif

/** Start an asynchronous operation to parse some HTTP/1 message data from a stream.

    This function asynchronously advances the state of the
    parser using the provided dynamic buffer and reading from
    the input stream as needed. The function call always
    returns immediately. The asynchronous operation will
    continue until one of the following conditions is true:

    @li When expecting a message header, and the complete
        header is received.

    @li When expecting a chunk header, and the complete
        chunk header is received.

    @li When expecting body octets, one or more body octets
        are received.

    @li An error occurs in the stream or parser.

    This operation is implemented in terms of zero or more calls to
    the next layer's `async_read_some` function, and is known as a
    <em>composed operation</em>. The program must ensure that the
    stream performs no other operations until this operation completes.
    The implementation may read additional octets that lie past the
    end of the object being parsed. This additional data is stored
    in the stream buffer, which may be used in subsequent calls.

    The completion handler will be called with the number of bytes
    processed from the dynamic buffer. The caller should remove
    these bytes by calling `consume` on the dynamic buffer.

    @param stream The stream from which the data is to be read.
    The type must support the @b AsyncReadStream concept.

    @param dynabuf A @b DynamicBuffer holding additional bytes
    read by the implementation from the stream. This is both
    an input and an output parameter; on entry, any data in the
    dynamic buffer's input sequence will be given to the parser
    first.

    @param parser The parser to use.

    @param handler The handler to be called when the request
    completes. Copies will be made of the handler as required.
    The equivalent function signature of the handler must be:
    @code void handler(
        error_code const& error,    // result of operation
        std::size_t bytes_used      // the number of bytes to consume
    ); @endcode
    Regardless of whether the asynchronous operation completes
    immediately or not, the handler will not be invoked from within
    this function. Invocation of the handler will be performed in a
    manner equivalent to using `boost::asio::io_service::post`.
*/
#if GENERATING_DOCS
template<class AsyncReadStream, class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived,
        class ReadHandler>
void_or_deduced
async_parse_some(AsyncReadStream& stream,
    DynamicBuffer& dynabuf, basic_parser<
        isRequest, isDirect, Derived>& parser,
            ReadHandler&& handler);
#else

template<class AsyncReadStream, class DynamicBuffer,
    bool isRequest, class Derived, class ReadHandler>
typename async_completion<
    ReadHandler, void(error_code, std::size_t)>::result_type
async_parse_some(AsyncReadStream& stream,
    DynamicBuffer& dynabuf, basic_parser<
        isRequest, true, Derived>& parser,
            ReadHandler&& handler);

template<class AsyncReadStream, class DynamicBuffer,
    bool isRequest, class Derived, class ReadHandler>
typename async_completion<
    ReadHandler, void(error_code, std::size_t)>::result_type
async_parse_some(AsyncReadStream& stream,
    DynamicBuffer& dynabuf, basic_parser<
        isRequest, false, Derived>& parser,
            ReadHandler&& handler);

#endif

//------------------------------------------------------------------------------

/** Parse an HTTP/1 message from a stream.

    This function synchronously reads from a stream and passes
    data to the specified parser. The call will block until one
    of the following conditions is true:

    @li The parser indicates no more additional data is needed.

    @li An error occurs in the stream or parser.

    This function is implemented in terms of one or more calls
    to the stream's `read_some` function. The implementation may
    read additional octets that lie past the end of the object
    being parsed. This additional data is stored in the dynamic
    buffer, which may be used in subsequent calls.

    @param stream The stream from which the data is to be read.
    The type must support the @b SyncReadStream concept.

    @param dynabuf A @b DynamicBuffer holding additional bytes
    read by the implementation from the stream. This is both
    an input and an output parameter; on entry, any data in the
    dynamic buffer's input sequence will be given to the parser
    first.

    @param parser The parser to use.

    @throws system_error Thrown on failure.
*/
template<class SyncReadStream, class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived>
void
parse(SyncReadStream& stream, DynamicBuffer& dynabuf,
    basic_parser<isRequest, isDirect, Derived>& parser);

/** Parse an HTTP/1 message from a stream.

    This function synchronously reads from a stream and passes
    data to the specified parser. The call will block until one
    of the following conditions is true:

    @li The parser indicates that no more data is needed.

    @li An error occurs in the stream or parser.

    This function is implemented in terms of one or more calls
    to the stream's `read_some` function. The implementation may
    read additional octets that lie past the end of the object
    being parsed. This additional data is stored in the dynamic
    buffer, which may be used in subsequent calls.

    @param stream The stream from which the data is to be read.
    The type must support the @b SyncReadStream concept.

    @param dynabuf A @b DynamicBuffer holding additional bytes
    read by the implementation from the stream. This is both
    an input and an output parameter; on entry, any data in the
    dynamic buffer's input sequence will be given to the parser
    first.

    @param parser The parser to use.

    @param ec Set to the error, if any occurred.
*/
template<class SyncReadStream, class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived>
void
parse(SyncReadStream& stream, DynamicBuffer& dynabuf,
    basic_parser<isRequest, isDirect, Derived>& parser,
        error_code& ec);

/** Start an asynchronous operation to parse an HTTP/1 message from a stream.

    This function is used to asynchronously read from a stream and
    pass the data to the specified parser. The function call always
    returns immediately. The asynchronous operation will continue
    until one of the following conditions is true:

    @li The parser indicates that no more data is needed.

    @li An error occurs in the stream or parser.

    This operation is implemented in terms of one or more calls to
    the next layer's `async_read_some` function, and is known as a
    <em>composed operation</em>. The program must ensure that the
    stream performs no other operations until this operation completes.
    The implementation may read additional octets that lie past the
    end of the object being parsed. This additional data is stored
    in the stream buffer, which may be used in subsequent calls.

    @param stream The stream from which the data is to be read.
    The type must support the @b AsyncReadStream concept.

    @param dynabuf A @b DynamicBuffer holding additional bytes
    read by the implementation from the stream. This is both
    an input and an output parameter; on entry, any data in the
    dynamic buffer's input sequence will be given to the parser
    first.

    @param parser The parser to use.

    @param handler The handler to be called when the request
    completes. Copies will be made of the handler as required.
    The equivalent function signature of the handler must be:
    @code void handler(
        error_code const& error // result of operation
    ); @endcode
    Regardless of whether the asynchronous operation completes
    immediately or not, the handler will not be invoked from within
    this function. Invocation of the handler will be performed in a
    manner equivalent to using `boost::asio::io_service::post`.
*/
template<class AsyncReadStream, class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived,
        class ReadHandler>
#if GENERATING_DOCS
void_or_deduced
#else
typename async_completion<
    ReadHandler, void(error_code)>::result_type
#endif
async_parse(AsyncReadStream& stream,
    DynamicBuffer& dynabuf, basic_parser<
        isRequest, isDirect, Derived>& parser,
            ReadHandler&& handler);

} // http
} // beast

#include <beast/http/impl/parse.ipp>

#endif
