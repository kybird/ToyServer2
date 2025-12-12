#include "System/Network/ASIO/Components/Reader.h"
#include "System/Network/ASIO/AsioSession.h"
#include "System/Pch.h"

#include <boost/asio/bind_allocator.hpp>
#include <boost/asio/recycling_allocator.hpp>

namespace System {

void Reader::Init(std::shared_ptr<boost::asio::ip::tcp::socket> socket, AsioSession *owner)
{
    _socket = socket;
    _owner = owner;
}

void Reader::ReadSome(void *buffer, size_t length)
{
    if (!_socket || !_socket->is_open())
        return;

    // [Lifetime Safety]
    // Capture 'self' (shared_ptr<AsioSession>) to keep session alive during async read.
    // Reader is owned by Session, so keeping Session alive keeps Reader alive.
    if (!_owner)
        return;
    std::shared_ptr<AsioSession> self = _owner->shared_from_this();

    _socket->async_read_some(
        boost::asio::buffer(buffer, length),
        boost::asio::bind_allocator(
            boost::asio::recycling_allocator<void>(),
            [this, self](const boost::system::error_code &ec, size_t bytesTransferred)
            {
                OnReadComplete(ec, bytesTransferred);
            }
        )
    );
}

void Reader::OnReadComplete(const boost::system::error_code &ec, size_t bytesTransferred)
{
    if (ec)
    {
        if (ec != boost::asio::error::eof && ec != boost::asio::error::connection_reset)
        {
            LOG_ERROR("Read Error: {}", ec.message());
        }
        if (_owner)
            _owner->Close();
        return;
    }

    if (_owner)
    {
        _owner->OnRead(bytesTransferred);
    }
}

} // namespace System
