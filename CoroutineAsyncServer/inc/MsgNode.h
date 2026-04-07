#pragma once
#include <string>
#include <iostream>
#include <boost/asio.hpp>
#include "const.h"

using boost::asio::ip::tcp;


class MsgNode
{
public:
  short _total_len;
  short _cur_len;
  char* _data;
public:
  MsgNode(short max_len):_total_len(max_len), _cur_len(0)
  {
    _data = new char[_total_len + 1]();
    _data[_total_len] = '\0';
  }
  ~MsgNode(){
    std::cout << "destruct Node" << std::endl;
    delete [] _data;
  }

  void Clear();
};

class RecvNode :public MsgNode {
public:
    RecvNode(short max_len, short msg_id);
    short GetMsgId() const {
        return _msg_id;
    }
private:
    short _msg_id;
};


class SendNode:public MsgNode {
public:
    SendNode(const char* msg, short max_len, short msg_id);
private:
    short _msg_id;
};