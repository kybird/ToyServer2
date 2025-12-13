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
    // Manual RefCount for Async Dispatcher-controlled lifetime
    if (!_owner)
        return;

    _owner->IncRef();
    AsioSession *rawOwner = _owner;

    _socket->async_read_some(
        boost::asio::buffer(buffer, length),
        boost::asio::bind_allocator(
            boost::asio::recycling_allocator<void>(),
            [this, rawOwner](const boost::system::error_code &ec, size_t bytesTransferred)
            {
                // Note: 'this' (Reader) is member of 'rawOwner' (Session).
                // If Session is alive, Reader is alive.
                OnReadComplete(ec, bytesTransferred);
                rawOwner->DecRef();
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
