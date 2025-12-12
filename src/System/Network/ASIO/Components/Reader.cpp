#include "System/Network/ASIO/Components/Reader.h"
#include "System/Network/ASIO/AsioSession.h"
#include "System/Pch.h"

namespace System {

void Reader::Init(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
                  AsioSession *owner) {
  _socket = socket;
  _owner = owner;
}

void Reader::ReadSome(void *buffer, size_t length) {
  if (!_socket || !_socket->is_open())
    return;

  _socket->async_read_some(
      boost::asio::buffer(buffer, length),
      [this](const boost::system::error_code &ec, size_t bytesTransferred) {
        OnReadComplete(ec, bytesTransferred);
      });
}

void Reader::OnReadComplete(const boost::system::error_code &ec,
                            size_t bytesTransferred) {
  if (ec) {
    if (ec != boost::asio::error::eof &&
        ec != boost::asio::error::connection_reset) {
      LOG_ERROR("Read Error: {}", ec.message());
    }
    if (_owner)
      _owner->Close();
    return;
  }

  if (_owner) {
    _owner->OnRead(bytesTransferred);
  }
}

} // namespace System
