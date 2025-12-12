#pragma once

#include "System/ILog.h"
#include "System/Pch.h"

namespace System {

class AsioSession; // Forward Decl

class Reader {
public:
  void Init(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
            AsioSession *owner);

  void ReadSome(void *buffer, size_t length);

private:
  void OnReadComplete(const boost::system::error_code &ec,
                      size_t bytesTransferred);

private:
  std::shared_ptr<boost::asio::ip::tcp::socket> _socket;
  AsioSession *_owner = nullptr;
};

} // namespace System
