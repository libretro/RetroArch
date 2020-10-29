#include "rpc_connection.h"
#include "serialization.h"

#include <atomic>

#define RPC_VERSION 1

static RpcConnection Instance;

/*static*/ RpcConnection* RpcConnection::Create(const char* applicationId)
{
    Instance.connection = BaseConnection::Create();
    StringCopy(Instance.appId, applicationId);
    return &Instance;
}

/*static*/ void RpcConnection::Destroy(RpcConnection*& c)
{
    c->Close();
    BaseConnection::Destroy(c->connection);
    c = nullptr;
}

void RpcConnection::Open()
{
    if (state == State::Connected)
        return;

    if (state == State::Disconnected)
    {
        if (!connection->Open())
           return;
    }

    if (state == State::SentHandshake)
    {
       JsonDocument message;
       if (Read(message))
       {
          bool cmdDispatch = false;
          bool evtReady    = false;

          for (JsonReader r(message); r.NextKey();)
          {
              if (r.depth == 1 && !strcmp(r.key, "cmd"))
                  cmdDispatch = !strcmp(r.NextString(""), "DISPATCH");
              else if (r.depth == 1 && !strcmp(r.key, "evt"))
                  evtReady = !strcmp(r.NextString(""), "READY");
          }

          if (cmdDispatch && evtReady)
          {
             state = State::Connected;
             if (onConnect)
                onConnect(message);
          }
       }
    }
    else
    {
        sendFrame.opcode = Opcode::Handshake;
        sendFrame.length = (uint32_t)JsonWriteHandshakeObj(
          sendFrame.message, sizeof(sendFrame.message), RPC_VERSION, appId);

        if (connection->Write(&sendFrame,
                 sizeof(MessageFrameHeader) + sendFrame.length))
            state = State::SentHandshake;
        else
            Close();
    }
}

void RpcConnection::Close()
{
    if (onDisconnect && (state == State::Connected || state == State::SentHandshake))
        onDisconnect(lastErrorCode, lastErrorMessage);
    connection->Close();
    state = State::Disconnected;
}

bool RpcConnection::Write(const void* data, size_t length)
{
    sendFrame.opcode = Opcode::Frame;
    memcpy(sendFrame.message, data, length);
    sendFrame.length = (uint32_t)length;
    if (!connection->Write(&sendFrame, sizeof(MessageFrameHeader) + length))
    {
        Close();
        return false;
    }
    return true;
}

bool RpcConnection::Read(JsonDocument& message)
{
    MessageFrame readFrame;

    if (state != State::Connected && state != State::SentHandshake)
        return false;

    for (;;)
    {
        bool didRead = connection->Read(
              &readFrame, sizeof(MessageFrameHeader));

        if (!didRead)
        {
            if (!connection->isOpen)
            {
                lastErrorCode = (int)ErrorCode::PipeClosed;
                StringCopy(lastErrorMessage, "Pipe closed");
                Close();
            }
            return false;
        }

        if (readFrame.length > 0)
        {
            didRead = connection->Read(readFrame.message, readFrame.length);
            if (!didRead)
            {
                lastErrorCode = (int)ErrorCode::ReadCorrupt;
                StringCopy(lastErrorMessage, "Partial data in frame");
                Close();
                return false;
            }
            readFrame.message[readFrame.length] = 0;
        }

        switch (readFrame.opcode)
        {
           case Opcode::Close:
              message.ParseInsitu(readFrame.message);
              lastErrorCode = 0;
              lastErrorMessage[0] = '\0';
              for (JsonReader r(message); r.NextKey();)
              {
                  if (r.depth == 1 && !strcmp(r.key, "code"))
                      r.NextInt(&lastErrorCode);
                  else if (r.depth == 1 && !strcmp(r.key, "message"))
                      StringCopy(lastErrorMessage, r.NextString(""));
              }
              Close();
              return false;
           case Opcode::Frame:
              message.ParseInsitu(readFrame.message);
              return true;
           case Opcode::Ping:
              readFrame.opcode = Opcode::Pong;
              if (!connection->Write(&readFrame, sizeof(MessageFrameHeader) + readFrame.length))
                 Close();
              break;
           case Opcode::Pong:
              break;
           case Opcode::Handshake:
           default:
              /* something bad happened */
              lastErrorCode = (int)ErrorCode::ReadCorrupt;
              StringCopy(lastErrorMessage, "Bad ipc frame");
              Close();
              return false;
        }
    }
}
