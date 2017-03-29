//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <beast/core/flat_streambuf.hpp>
#include <beast/core/prepare_buffers.hpp>
#include <beast/http/chunk_encode.hpp>
#include <beast/http/read.hpp>
#include <beast/http/write.hpp>
#include <beast/http/string_body.hpp>
#include <beast/core/detail/clamp.hpp>
#include <beast/test/string_istream.hpp>
#include <beast/test/string_ostream.hpp>
#include <beast/test/yield_to.hpp>
#include <beast/unit_test/suite.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

namespace beast {
namespace http {

class design_test
    : public beast::unit_test::suite
    , public beast::test::enable_yield_to
{
public:
    //--------------------------------------------------------------------------
    /*
        Read a message with a direct Reader Body.
    */
    struct direct_body
    {
        using value_type = std::string;

        class reader
        {
            value_type& body_;
            std::size_t len_ = 0;

        public:
            static bool constexpr is_direct = true;

            using mutable_buffers_type =
                boost::asio::mutable_buffers_1;

            template<bool isRequest, class Fields>
            explicit
            reader(message<isRequest, direct_body, Fields>& m)
                : body_(m.body)
            {
            }

            void
            init()
            {
            }

            void
            init(std::uint64_t content_length)
            {
                if(content_length >
                        (std::numeric_limits<std::size_t>::max)())
                    throw std::length_error(
                        "Content-Length max exceeded");
                body_.reserve(static_cast<
                    std::size_t>(content_length));
            }

            mutable_buffers_type
            prepare(std::size_t n)
            {
                body_.resize(len_ + n);
                return {&body_[len_], n};
            }

            void
            commit(std::size_t n)
            {
                if(body_.size() > len_ + n)
                    body_.resize(len_ + n);
                len_ = body_.size();
            }

            void
            finish()
            {
                body_.resize(len_);
            }
        };
    };

    void
    testDirectBody()
    {
        // Content-Length
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Content-Length: 1\r\n"
                "\r\n"
                "*"
            };
            message<true, direct_body, fields> m;
            flat_streambuf sb{1024};
            read(is, sb, m);
            BEAST_EXPECT(m.body == "*");
        }

        // end of file
        {
            test::string_istream is{ios_,
                "HTTP/1.1 200 OK\r\n"
                "\r\n" // 19 byte header
                "*"
            };
            message<false, direct_body, fields> m;
            flat_streambuf sb{20};
            read(is, sb, m);
            BEAST_EXPECT(m.body == "*");
        }

        // chunked
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                "1\r\n"
                "*\r\n"
                "0\r\n\r\n"
            };
            message<true, direct_body, fields> m;
            flat_streambuf sb{10};
            read(is, sb, m);
            BEAST_EXPECT(m.body == "*");
        }
    }

    //--------------------------------------------------------------------------
    /*
        Read a message with an indirect Reader Body.
    */
    struct indirect_body
    {
        using value_type = std::string;

        class reader
        {
            value_type& body_;

        public:
            static bool constexpr is_direct = false;

            using mutable_buffers_type =
                boost::asio::null_buffers;

            template<bool isRequest, class Fields>
            explicit
            reader(message<isRequest, indirect_body, Fields>& m)
                : body_(m.body)
            {
            }

            void
            init(error_code& ec)
            {
            }
            
            void
            init(std::uint64_t content_length,
                error_code& ec)
            {
            }
            
            void
            write(boost::string_ref const& s,
                error_code& ec)
            {
                body_.append(s.data(), s.size());
            }

            void
            finish(error_code& ec)
            {
            }
        };
    };

    void
    testIndirectBody()
    {
        // Content-Length
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Content-Length: 1\r\n"
                "\r\n"
                "*"
            };
            message<true, indirect_body, fields> m;
            flat_streambuf sb{1024};
            read(is, sb, m);
            BEAST_EXPECT(m.body == "*");
        }

        // end of file
        {
            test::string_istream is{ios_,
                "HTTP/1.1 200 OK\r\n"
                "\r\n" // 19 byte header
                "*"
            };
            message<false, indirect_body, fields> m;
            flat_streambuf sb{20};
            read(is, sb, m);
            BEAST_EXPECT(m.body == "*");
        }


        // chunked
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                "1\r\n"
                "*\r\n"
                "0\r\n\r\n"
            };
            message<true, indirect_body, fields> m;
            flat_streambuf sb{1024};
            read(is, sb, m);
            BEAST_EXPECT(m.body == "*");
        }
    }

    //--------------------------------------------------------------------------
    /*
        Read a message header and manually read the body.
    */
    void
    testManualBody()
    {
        // Content-Length
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Content-Length: 5\r\n"
                "\r\n" // 37 byte header
                "*****"
            };
            header_parser<true, fields> p;
            flat_streambuf sb{38};
            parse(is, sb, p);
            BEAST_EXPECT(p.size() == 5);
            BEAST_EXPECT(sb.size() < 5);
            sb.commit(boost::asio::read(
                is, sb.prepare(5 - sb.size())));
            BEAST_EXPECT(sb.size() == 5);
        }

        // end of file
        {
            test::string_istream is{ios_,
                "HTTP/1.1 200 OK\r\n"
                "\r\n" // 19 byte header
                "*****"
            };
            header_parser<false, fields> p;
            flat_streambuf sb{20};
            parse(is, sb, p);
            BEAST_EXPECT(p.state() ==
                parse_state::body_to_eof);
            BEAST_EXPECT(sb.size() < 5);
            sb.commit(boost::asio::read(
                is, sb.prepare(5 - sb.size())));
            BEAST_EXPECT(sb.size() == 5);
        }

        // chunked
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                "5;x;y=1;z=\"-\"\r\n*****\r\n"
                "3\r\n---\r\n"
                "1\r\n+\r\n"
                "0\r\nMD5:xxx\r\n\r\n"
            };

            header_parser<true, fields> p;
            flat_streambuf sb{36};

            // Read the header
            parse(is, sb, p);
            BEAST_EXPECT(p.state() ==
                parse_state::chunk_header);
            
            // header_parser pauses after receiving the header
            p.resume();

            error_code ec;

            // Chunk 1
            parse_some(is, sb, p);
            BEAST_EXPECT(p.state() ==
                parse_state::chunk_body);
            BEAST_EXPECT(p.size() == 5);
            if(sb.size() < 5)
                sb.commit(boost::asio::read(
                    is, sb.prepare(5 - sb.size())));
            sb.consume(5);
            p.consume_body(ec);
            BEAST_EXPECTS(! ec, ec.message());

            // Chunk 2
            parse_some(is, sb, p);
            BEAST_EXPECT(p.state() ==
                parse_state::chunk_body);
            BEAST_EXPECT(p.size() == 3);
            if(sb.size() < 3)
                sb.commit(boost::asio::read(
                    is, sb.prepare(3 - sb.size())));
            sb.consume(3);
            p.consume_body(ec);
            BEAST_EXPECTS(! ec, ec.message());

            // Chunk 3
            parse_some(is, sb, p);
            BEAST_EXPECT(p.state() ==
                parse_state::chunk_body);
            BEAST_EXPECT(p.size() == 1);
            // Read 1 body byte
            if(sb.size() < 1)
                sb.commit(boost::asio::read(
                    is, sb.prepare(1 - sb.size())));
            sb.consume(1);
            p.consume_body(ec);
            BEAST_EXPECTS(! ec, ec.message());

            // Final chunk
            parse_some(is, sb, p);
            BEAST_EXPECT(p.is_complete());
            BEAST_EXPECT(p.get().fields["MD5"] == "xxx");
        }
    }

    //--------------------------------------------------------------------------
    /*
        Read a header, check for Expect: 100-continue,
        then conditionally read the body.
    */
    void
    testExpect100Continue()
    {
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Expect: 100-continue\r\n"
                "Content-Length: 5\r\n"
                "\r\n"
                "*****"
            };

            header_parser<true, fields> p;
            flat_streambuf sb{40};
            parse(is, sb, p);
            BEAST_EXPECT(p.got_header());
            BEAST_EXPECT(
                p.get().fields["Expect"] ==
                    "100-continue");
            message_parser<
                true, string_body, fields> p1{
                    std::move(p)};
            parse(is, sb, p1);
            BEAST_EXPECT(
                p1.get().body == "*****");
        }
    }

    //--------------------------------------------------------------------------
    /*
        Efficiently relay a message from one stream to another
    */
    template<
        bool isRequest,
        class SyncWriteStream,
        class DynamicBuffer,
        class SyncReadStream>
    void
    relay(
        SyncWriteStream& out,
        DynamicBuffer& sb,
        SyncReadStream& in)
    {
        using boost::asio::buffer_size;
        header_parser<isRequest, fields> p;
        parse(in, sb, p);
        write(out, p.get());
        switch(p.state())
        {
        case parse_state::body:
        {
            auto remain = p.size();
            while(remain > 0)
            {
                if(sb.size() == 0)
                    sb.commit(in.read_some(sb.prepare(
                        read_size_helper(sb, 65536))));
                auto const limit =
                    beast::detail::clamp(remain);
                auto const bytes_transferred =
                    out.write_some(prepare_buffers(
                        limit, sb.data()));
                sb.consume(bytes_transferred);
                remain -= bytes_transferred;
            }
            break;
        }
        case parse_state::body_to_eof:
        {
            error_code ec;
            for(;;)
            {
                if(sb.size() == 0)
                {
                    auto const n = in.read_some(
                        sb.prepare(read_size_helper(
                            sb, 65536)), ec);
                    if(ec == boost::asio::error::eof)
                    {
                        ec = {};
                        break;
                    }
                    if(! BEAST_EXPECTS(! ec, ec.message()))
                        return;
                    sb.commit(n);
                }
                auto const bytes_transferred =
                    out.write_some(sb.data());
                sb.consume(bytes_transferred);
            }
            break;
        }
        case parse_state::chunk_header:
        {
            error_code ec;
            p.resume();
            for(;;)
            {
                BEAST_EXPECT(p.state() ==
                    parse_state::chunk_header);
                parse_some(in, sb, p);
                if(p.is_complete())
                    break;
                BEAST_EXPECT(p.state() ==
                    parse_state::chunk_body);
                auto remain = p.size();
                BOOST_ASSERT(remain > 0);
                while(remain > 0)
                {
                    if(sb.size() == 0)
                        sb.commit(in.read_some(sb.prepare(
                            read_size_helper(sb, 65536))));
                    auto const limit =
                        beast::detail::clamp(remain);
                    auto const b =
                        prepare_buffers(limit, sb.data());
                    auto const n = buffer_size(b);
                    boost::asio::write(out,
                        chunk_encode(false, b));
                    sb.consume(n);
                    remain -= n;
                }
                p.consume_body(ec);
            }
            boost::asio::write(out,
                chunk_encode_final());
            break;
        }

        default:
            break;
        }
    }

    void
    testRelay()
    {
        // Content-Length
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Content-Length: 5\r\n"
                "\r\n" // 37 byte header
                "*****",
                3 // max_read
            };
            test::string_ostream os{ios_};
            flat_streambuf sb{16};
            relay<true>(os, sb, is);
        }

        // end of file
        {
            test::string_istream is{ios_,
                "HTTP/1.1 200 OK\r\n"
                "\r\n" // 19 byte header
                "*****",
                3 // max_read
            };
            test::string_ostream os{ios_};
            flat_streambuf sb{16};
            relay<false>(os, sb, is);
        }

        // chunked
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                "5;x;y=1;z=\"-\"\r\n*****\r\n"
                "3\r\n---\r\n"
                "1\r\n+\r\n"
                "0\r\n\r\n",
                2 // max_read
            };
            test::string_ostream os{ios_};
            flat_streambuf sb{16};
            relay<true>(os, sb, is);
        }
    }

    //--------------------------------------------------------------------------
    /*
        Read the request header, then read the request body content using
        a fixed-size buffer, i.e. read the body in chunks of 4k for instance.
        The end of the body should be indicated somehow and chunk-encoding
        should be decoded by beast.
    */
    template<bool isRequest,
        class SyncReadStream, class BodyCallback>
    void
    doFixedRead(SyncReadStream& stream, BodyCallback const& cb)
    {
        flat_streambuf buf{4096, 4096}; // 4K size, 4K limit
        header_parser<isRequest, fields> p;
        error_code ec;
        for(;;)
        {
            switch(p.state())
            {
            case parse_state::header:
            case parse_state::chunk_header:
            {
                auto const bytes_transferred =
                    stream.read_some(buf.prepare(
                        buf.max_size() - buf.size()), ec);
                if(ec == boost::asio::error::eof)
                {
                    if(! p.got_some())
                        goto do_finish;
                    ec = {};
                    p.write_eof(ec);
                    BEAST_EXPECTS(! ec, ec.message());
                }
                else
                {   
                    BEAST_EXPECTS(! ec, ec.message());
                    buf.commit(bytes_transferred);
                    auto const used = p.write(buf.data(), ec);
                    BEAST_EXPECTS(! ec, ec.message());
                    if(used == 0 && buf.size() == buf.max_size())
                        throw std::length_error{"buffer overflow"};
                    buf.consume(used);
                }
                break;
            }

            case parse_state::body:
            case parse_state::chunk_body:
            {
                if(buf.size() > 0)
                {
                    auto const len =
                        (std::min)(beast::detail::clamp(
                            p.size()), buf.size());
                    cb(buf, len);
                    p.consume(len);
                }
                else
                {
                    BOOST_ASSERT(p.size() > 0);
                    auto const len = (std::min)(
                        beast::detail::clamp(p.size()),
                            buf.max_size() - buf.size());
                    auto const bytes_transferred =
                        stream.read_some(buf.prepare(len), ec);
                    if(ec == boost::asio::error::eof)
                    {
                        ec = {};
                        p.write_eof(ec);
                        BEAST_EXPECTS(! ec, ec.message());
                    }
                    else
                    {
                        BEAST_EXPECTS(! ec, ec.message());
                        buf.commit(bytes_transferred);
                    }
                }
                break;
            }

            case parse_state::body_to_eof:
            {
                if(buf.size() > 0)
                {
                    cb(buf, buf.size());
                }
                auto const bytes_transferred =
                    stream.read_some(buf.prepare(
                        buf.max_size() - buf.size()), ec);
                if(ec == boost::asio::error::eof)
                {
                    ec = {};
                    p.write_eof(ec);
                    BEAST_EXPECTS(! ec, ec.message());
                }
                else
                {
                    BEAST_EXPECTS(! ec, ec.message());
                    buf.commit(bytes_transferred);
                }
                break;
            }

            case parse_state::complete:
                // all finished!
                goto do_finish;
            }
        }
    do_finish:
        ;
    }
    
    struct bodyHandler
    {
        template<class DynamicBuffer>
        void
        operator()(DynamicBuffer& buf, std::size_t len) const
        {
            // called for each piece of the body,
            // There are `size` bytes in buf.data()
            buf.consume(len);
        }
    };

    void
    testFixedBuffer()
    {
        using boost::asio::buffer;
        using boost::asio::buffer_cast;
        using boost::asio::buffer_size;

        // Content-Length
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Content-Length: 1\r\n"
                "\r\n"
                "*"
            };
            doFixedRead<true>(is, bodyHandler{});
        }

        // end of file
        {
            test::string_istream is{ios_,
                "HTTP/1.1 200 OK\r\n"
                "\r\n" // 19 byte header
                "*****"
            };
            doFixedRead<false>(is, bodyHandler{});
        }

        // chunked
        {
            test::string_istream is{ios_,
                "GET / HTTP/1.1\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                "5;x;y=1;z=\"-\"\r\n*****\r\n"
                "3\r\n---\r\n"
                "1\r\n+\r\n"
                "0\r\n\r\n",
                2 // max_read
            };
            doFixedRead<true>(is, bodyHandler{});
        }
    }

    //--------------------------------------------------------------------------

    void
    run()
    {
        testDirectBody();
        testIndirectBody();
        testManualBody();
        testExpect100Continue();
        testRelay();
        testFixedBuffer();
    }
};

BEAST_DEFINE_TESTSUITE(design,http,beast);

} // http
} // beast
