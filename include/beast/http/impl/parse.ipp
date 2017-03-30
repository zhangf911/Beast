//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_HTTP_IMPL_PARSE_IPP_HPP
#define BEAST_HTTP_IMPL_PARSE_IPP_HPP

#include <beast/http/concepts.hpp>
#include <beast/http/error.hpp>
#include <beast/core/bind_handler.hpp>
#include <beast/core/handler_helpers.hpp>
#include <beast/core/handler_ptr.hpp>
#include <beast/core/stream_concepts.hpp>
#include <boost/assert.hpp>
#include <boost/optional.hpp>

namespace beast {
namespace http {

namespace detail {

template<class Stream, class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived,
        class Handler>
class parse_some_buffer_op
{
    struct data
    {
        bool cont;
        Stream& s;
        DynamicBuffer& db;
        basic_parser<isRequest, isDirect, Derived>& p;
        boost::optional<typename
            DynamicBuffer::mutable_buffers_type> mb;
        boost::optional<typename
            Derived::mutable_buffers_type> bb;
        std::size_t bytes_used;
        int state = 0;

        data(Handler& handler, Stream& s_, DynamicBuffer& db_,
                basic_parser<isRequest, isDirect, Derived>& p_)
            : cont(beast_asio_helpers::
                is_continuation(handler))
            , s(s_)
            , db(db_)
            , p(p_)
        {
        }
    };

    handler_ptr<data, Handler> d_;

public:
    parse_some_buffer_op(parse_some_buffer_op&&) = default;
    parse_some_buffer_op(parse_some_buffer_op const&) = default;

    template<class DeducedHandler, class... Args>
    parse_some_buffer_op(DeducedHandler&& h,
            Stream& s, Args&&... args)
        : d_(std::forward<DeducedHandler>(h),
            s, std::forward<Args>(args)...)
    {
        (*this)(error_code{}, 0, false);
    }

    void
    operator()(error_code ec,
        std::size_t bytes_transferred, bool again = true);

    friend
    void*
    asio_handler_allocate(std::size_t size,
        parse_some_buffer_op* op)
    {
        return beast_asio_helpers::
            allocate(size, op->d_.handler());
    }

    friend
    void
    asio_handler_deallocate(
        void* p, std::size_t size,
            parse_some_buffer_op* op)
    {
        return beast_asio_helpers::
            deallocate(p, size, op->d_.handler());
    }

    friend
    bool
    asio_handler_is_continuation(
        parse_some_buffer_op* op)
    {
        return op->d_->cont;
    }

    template<class Function>
    friend
    void
    asio_handler_invoke(Function&& f,
        parse_some_buffer_op* op)
    {
        return beast_asio_helpers::
            invoke(f, op->d_.handler());
    }
};

template<class Stream, class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived,
        class Handler>
void
parse_some_buffer_op<Stream, DynamicBuffer,
    isRequest, isDirect, Derived, Handler>::
operator()(error_code ec,
    std::size_t bytes_transferred, bool again)
{
    auto& d = *d_;
    d.cont = d.cont || again;
    if(d.state == 99)
        goto upcall;
    for(;;)
    {
        switch(d.state)
        {
        case 0:
            if(d.db.size() == 0)
            {
                d.state = 2;
                break;
            }
            //[[fallthrough]]

        case 1:
        {
            BOOST_ASSERT(d.db.size() > 0);
            d.bytes_used =
                d.p.write(d.db.data(), ec);
            if(d.bytes_used > 0 || ec)
            {
                // call handler
                if(d.state == 1)
                    goto upcall;
                d.state = 99;
                d.s.get_io_service().post(
                    bind_handler(std::move(*this), ec, 0));
                return;
            }
            //[[fallthrough]]
        }

        case 2:
        case 3:
        {
            auto const size =
                read_size_helper(d.db, 65536);
            BOOST_ASSERT(size > 0);
            try
            {
                d.mb.emplace(d.db.prepare(size));
            }
            catch(std::length_error const&)
            {
                // call handler
                if(d.state == 3)
                    goto upcall;
                d.state = 99;
                d.s.get_io_service().post(
                    bind_handler(std::move(*this),
                        error::buffer_overflow, 0));
                return;
            }
            // read
            d.state = 4;
            d.s.async_read_some(*d.mb, std::move(*this));
            return;
        }

        case 4:
            if(ec == boost::asio::error::eof)
            {
                BOOST_ASSERT(bytes_transferred == 0);
                d.bytes_used = 0;
                if(! d.p.got_some())
                    goto upcall;
                // caller sees EOF on next read.
                ec = {};
                d.p.write_eof(ec);
                if(ec)
                    goto upcall;
                BOOST_ASSERT(d.p.is_complete());
                goto upcall;
            }
            else if(ec)
            {
                d.bytes_used = 0;
                goto upcall;
            }
            BOOST_ASSERT(bytes_transferred > 0);
            d.db.commit(bytes_transferred);
            d.state = 1;
            break;
        }
    }
upcall:
    // can't pass any members of `d` otherwise UB
    auto const bytes_used = d.bytes_used;
    d_.invoke(ec, bytes_used);
}

//------------------------------------------------------------------------------

template<class Stream, class DynamicBuffer,
    bool isRequest, class Derived, class Handler>
class parse_some_body_op
{
    struct data
    {
        bool cont;
        Stream& s;
        DynamicBuffer& db;
        basic_parser<isRequest, true, Derived>& p;
        boost::optional<typename
            Derived::mutable_buffers_type> mb;
        std::size_t bytes_used;
        int state = 0;

        data(Handler& handler, Stream& s_, DynamicBuffer& db_,
                basic_parser<isRequest, true, Derived>& p_)
            : cont(beast_asio_helpers::
                is_continuation(handler))
            , s(s_)
            , db(db_)
            , p(p_)
        {
        }
    };

    handler_ptr<data, Handler> d_;

public:
    parse_some_body_op(parse_some_body_op&&) = default;
    parse_some_body_op(parse_some_body_op const&) = default;

    template<class DeducedHandler, class... Args>
    parse_some_body_op(DeducedHandler&& h,
            Stream& s, Args&&... args)
        : d_(std::forward<DeducedHandler>(h),
            s, std::forward<Args>(args)...)
    {
        (*this)(error_code{}, 0, false);
    }

    void
    operator()(error_code ec,
        std::size_t bytes_transferred, bool again = true);

    friend
    void*
    asio_handler_allocate(std::size_t size,
        parse_some_body_op* op)
    {
        return beast_asio_helpers::
            allocate(size, op->d_.handler());
    }

    friend
    void
    asio_handler_deallocate(
        void* p, std::size_t size,
            parse_some_body_op* op)
    {
        return beast_asio_helpers::
            deallocate(p, size, op->d_.handler());
    }

    friend
    bool
    asio_handler_is_continuation(
        parse_some_body_op* op)
    {
        return op->d_->cont;
    }

    template<class Function>
    friend
    void
    asio_handler_invoke(Function&& f,
        parse_some_body_op* op)
    {
        return beast_asio_helpers::
            invoke(f, op->d_.handler());
    }
};

template<class Stream, class DynamicBuffer,
    bool isRequest, class Derived, class Handler>
void
parse_some_body_op<Stream, DynamicBuffer,
    isRequest, Derived, Handler>::
operator()(error_code ec,
    std::size_t bytes_transferred, bool again)
{
    auto& d = *d_;
    d.cont = d.cont || again;
    if(d.state == 99)
        goto upcall;
    for(;;)
    {
        switch(d.state)
        {
        case 0:
            if(d.db.size() > 0)
            {
                d.bytes_used = d.p.copy_body(d.db);
                // call handler
                d.state = 99;
                d.s.get_io_service().post(
                    bind_handler(std::move(*this),
                        ec, 0));
                return;
            }
            d.p.prepare_body(d.mb, 65536);
            // read
            d.state = 1;
            d.s.async_read_some(
                *d.mb, std::move(*this));
            return;

        case 1:
            d.bytes_used = 0;
            if(ec == boost::asio::error::eof)
            {
                BOOST_ASSERT(bytes_transferred == 0);
                // caller sees EOF on next read
                ec = {};
                d.p.write_eof(ec);
                if(ec)
                    goto upcall;
                BOOST_ASSERT(d.p.is_complete());
            }
            else if(! ec)
            {
                d.db.commit(bytes_transferred);
            }
            goto upcall;
        }
    }
upcall:
    d_.invoke(ec, d.bytes_used);
}

//------------------------------------------------------------------------------

template<class Stream, class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived,
        class Handler>
class parse_op
{
    struct data
    {
        bool cont;
        Stream& s;
        DynamicBuffer& db;
        basic_parser<isRequest, isDirect, Derived>& p;

        data(Handler& handler, Stream& s_, DynamicBuffer& db_,
            basic_parser<isRequest, isDirect, Derived>& p_)
            : cont(beast_asio_helpers::
                is_continuation(handler))
            , s(s_)
            , db(db_)
            , p(p_)
        {
            BOOST_ASSERT(! p.is_complete());
        }
    };

    handler_ptr<data, Handler> d_;

public:
    parse_op(parse_op&&) = default;
    parse_op(parse_op const&) = default;

    template<class DeducedHandler, class... Args>
    parse_op(DeducedHandler&& h,
            Stream& s, Args&&... args)
        : d_(std::forward<DeducedHandler>(h),
            s, std::forward<Args>(args)...)
    {
        (*this)(error_code{}, 0, false);
    }

    void
    operator()(error_code const& ec,
        std::size_t bytes_used, bool again = true);

    friend
    void*
    asio_handler_allocate(
        std::size_t size, parse_op* op)
    {
        return beast_asio_helpers::
            allocate(size, op->d_.handler());
    }

    friend
    void
    asio_handler_deallocate(
        void* p, std::size_t size,
            parse_op* op)
    {
        return beast_asio_helpers::
            deallocate(p, size, op->d_.handler());
    }

    friend
    bool
    asio_handler_is_continuation(
        parse_op* op)
    {
        return op->d_->cont;
    }

    template<class Function>
    friend
    void
    asio_handler_invoke(
        Function&& f, parse_op* op)
    {
        return beast_asio_helpers::
            invoke(f, op->d_.handler());
    }
};

template<class Stream, class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived,
        class Handler>
void
parse_op<Stream, DynamicBuffer,
    isRequest, isDirect, Derived, Handler>::
operator()(error_code const& ec,
    std::size_t bytes_used, bool again)
{
    auto& d = *d_;
    d.cont = d.cont || again;
    if(! ec)
    {
        d.db.consume(bytes_used);
        if(! d.p.is_complete())
            return async_parse_some(
                d.s, d.db, d.p, std::move(*this));
    }
    d_.invoke(ec);
}

//------------------------------------------------------------------------------

template<class SyncReadStream, class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived>
inline
std::size_t
parse_some_buffer(
    SyncReadStream& stream, DynamicBuffer& dynabuf,
        basic_parser<isRequest, isDirect, Derived>& parser,
            error_code& ec)
{
    std::size_t bytes_used;
    if(dynabuf.size() == 0)
        goto do_read;
    for(;;)
    {
        bytes_used = parser.write(
            dynabuf.data(), ec);
        if(ec)
            return 0;
        if(bytes_used > 0)
            goto do_finish;
    do_read:
        boost::optional<typename
            DynamicBuffer::mutable_buffers_type> mb;
        auto const size =
            read_size_helper(dynabuf, 65536);
        BOOST_ASSERT(size > 0);
        try
        {
            mb.emplace(dynabuf.prepare(size));
        }
        catch(std::length_error const&)
        {
            ec = error::buffer_overflow;
            return 0;
        }
        auto const bytes_transferred =
            stream.read_some(*mb, ec);
        if(ec == boost::asio::error::eof)
        {
            BOOST_ASSERT(bytes_transferred == 0);
            bytes_used = 0;
            if(parser.got_some())
            {
                // caller sees EOF on next read
                ec = {};
                parser.write_eof(ec);
                if(ec)
                    return 0;
                BOOST_ASSERT(parser.is_complete());
            }
            break;
        }
        else if(ec)
        {
            return 0;
        }
        BOOST_ASSERT(bytes_transferred > 0);
        dynabuf.commit(bytes_transferred);
    }
do_finish:
    return bytes_used;
}

template<class SyncReadStream, class DynamicBuffer,
    bool isRequest, class Derived>
inline
std::size_t
parse_some_body(SyncReadStream& stream, DynamicBuffer& dynabuf,
    basic_parser<isRequest, true, Derived>& parser,
        error_code& ec)
{
    if(dynabuf.size() > 0)
        return parser.copy_body(dynabuf);
    boost::optional<typename
        Derived::mutable_buffers_type> mb;
    // VFALCO Need try/catch for std::length_error here
    parser.prepare_body(mb, 65536);
    auto const bytes_transferred =
        stream.read_some(*mb, ec);
    if(ec == boost::asio::error::eof)
    {
        BOOST_ASSERT(bytes_transferred == 0);
        // caller sees EOF on next read
        ec = {};
        parser.write_eof(ec);
        if(ec)
            return 0;
        BOOST_ASSERT(parser.is_complete());
    }
    else if(! ec)
    {
        parser.commit_body(bytes_transferred);
        return 0;
    }
    return 0;
}

} // detail

//------------------------------------------------------------------------------

template<class SyncReadStream, class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived>
std::size_t
parse_some(SyncReadStream& stream, DynamicBuffer& dynabuf,
    basic_parser<isRequest, isDirect, Derived>& parser)
{
    static_assert(is_SyncReadStream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    error_code ec;
    auto const bytes_used =
        parse_some(stream, dynabuf, parser, ec);
    if(ec)
        throw system_error{ec};
    return bytes_used;
}

template<class SyncReadStream, class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
parse_some(SyncReadStream& stream, DynamicBuffer& dynabuf,
    basic_parser<isRequest, true, Derived>& parser,
        error_code& ec)
{
    static_assert(is_SyncReadStream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_ASSERT(! parser.is_complete());
    switch(parser.state())
    {
    case parse_state::header:
    case parse_state::chunk_header:
        return detail::parse_some_buffer(
            stream, dynabuf, parser, ec);

    default:
        return detail::parse_some_body(
            stream, dynabuf, parser, ec);
    }
}

template<class SyncReadStream, class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
parse_some(SyncReadStream& stream, DynamicBuffer& dynabuf,
    basic_parser<isRequest, false, Derived>& parser,
        error_code& ec)
{
    static_assert(is_SyncReadStream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    return detail::parse_some_buffer(
        stream, dynabuf, parser, ec);
}

template<class AsyncReadStream, class DynamicBuffer,
    bool isRequest, class Derived, class ReadHandler>
typename async_completion<
    ReadHandler, void(error_code, std::size_t)>::result_type
async_parse_some(AsyncReadStream& stream,
    DynamicBuffer& dynabuf, basic_parser<
        isRequest, true, Derived>& parser,
            ReadHandler&& handler)
{
    static_assert(is_AsyncReadStream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    beast::async_completion<ReadHandler,
        void(error_code, std::size_t)> completion{handler};
    switch(parser.state())
    {
    case parse_state::header:
    case parse_state::chunk_header:
        detail::parse_some_buffer_op<AsyncReadStream,
            DynamicBuffer, isRequest, true, Derived,
                decltype(completion.handler)>{
                    completion.handler, stream, dynabuf, parser};
        break;

    default:
        detail::parse_some_body_op<AsyncReadStream,
            DynamicBuffer, isRequest, Derived,
                decltype(completion.handler)>{
                    completion.handler, stream, dynabuf, parser};
        break;
    }
    return completion.result.get();
}

template<class AsyncReadStream, class DynamicBuffer,
    bool isRequest, class Derived, class ReadHandler>
typename async_completion<
    ReadHandler, void(error_code, std::size_t)>::result_type
async_parse_some(AsyncReadStream& stream,
    DynamicBuffer& dynabuf, basic_parser<
        isRequest, false, Derived>& parser,
            ReadHandler&& handler)
{
    static_assert(is_AsyncReadStream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    beast::async_completion<ReadHandler,
        void(error_code, std::size_t)> completion{handler};
    detail::parse_some_buffer_op<AsyncReadStream,
        DynamicBuffer, isRequest, false, Derived,
            decltype(completion.handler)>{
                completion.handler, stream, dynabuf, parser};
    return completion.result.get();
}

//------------------------------------------------------------------------------

template<class SyncReadStream, class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived>
void
parse(SyncReadStream& stream, DynamicBuffer& dynabuf,
    basic_parser<isRequest, isDirect, Derived>& parser)
{
    static_assert(is_SyncReadStream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    error_code ec;
    parse(stream, dynabuf, parser, ec);
    if(ec)
        throw system_error{ec};
}

template<class SyncReadStream, class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived>
void
parse(SyncReadStream& stream, DynamicBuffer& dynabuf,
    basic_parser<isRequest, isDirect, Derived>& parser,
        error_code& ec)
{
    static_assert(is_SyncReadStream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_ASSERT(! parser.is_complete());
    do
    {
        auto const bytes_used =
            parse_some(stream, dynabuf, parser, ec);
        if(ec)
            return;
        dynabuf.consume(bytes_used);
    }
    while(! parser.is_complete());
}

template<class AsyncReadStream, class DynamicBuffer,
    bool isRequest, bool isDirect, class Derived,
        class ReadHandler>
typename async_completion<
    ReadHandler, void(error_code)>::result_type
async_parse(AsyncReadStream& stream,
    DynamicBuffer& dynabuf, basic_parser<
        isRequest, isDirect, Derived>& parser,
            ReadHandler&& handler)
{
    static_assert(is_AsyncReadStream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    beast::async_completion<ReadHandler,
        void(error_code)> completion{handler};
    detail::parse_op<AsyncReadStream, DynamicBuffer,
        isRequest, isDirect, Derived, decltype(completion.handler)>{
            completion.handler, stream, dynabuf, parser};
    return completion.result.get();
}

} // http
} // beast

#endif
