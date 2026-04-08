#pragma once

#include <iostream>
#include <thread>
#include <queue>
#include <map>
#include <functional>

#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>

#include "Singleton.h"
#include "Session.h"
#include "MsgNode.h"
#include "const.h"

class LogicNode{
    friend class LogicSystem;
public:
    LogicNode(shared_ptr<Session>, shared_ptr<RecvNode>);
private:
    shared_ptr<Session> _session;
    shared_ptr<RecvNode> _recvnode;
};

typedef function<void(shared_ptr<Session>, short msg_id, string msg_data)> FunCallBack;
class LogicSystem:public Singleton<LogicSystem>
{
    friend class Singleton<LogicSystem>;
public:
    ~LogicSystem();
    void PostMsgToQue(shared_ptr <LogicNode> msg);
private:
    LogicSystem();
    void DealMsg();
    void RegisterCallBacks();
    void HelloWordCallBack(shared_ptr<Session>, short msg_id, string msg_data);
    std::thread _worker_thread;
    std::queue<shared_ptr<LogicNode>> _msg_que;
    std::mutex _mutex;
    std::condition_variable _consume;
    bool _b_stop;
    std::map<short, FunCallBack> _fun_callbacks;
};