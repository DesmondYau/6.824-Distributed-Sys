#include "kvserver.hpp"
#include <iostream>

// Constructor / Factory
KVServer::KVServer(labrpc::Server* srv, int me, raft::Persister* persister, int maxraftstate)
    : me(me)
    , maxraftstate(maxraftstate)
{
    // YOU WILL HAVE TO ADD CODE HERE
    // (create Raft, register RPC handlers, start apply goroutine, etc.)
}

void KVServer::Get(const GetArgs& args, GetReply& reply)
{
    // YOU WILL HAVE TO MODIFY THIS FUNCTION
    reply.wrongLeader = true;   // placeholder
    reply.value = "";
}

void KVServer::PutAppend(const PutAppendArgs& args, PutAppendReply& reply)
{
    // YOU WILL HAVE TO MODIFY THIS FUNCTION
    reply.wrongLeader = true;   // placeholder
}

void KVServer::Kill()
{
    dead = true;
    if (rf) rf->kill();
}

bool KVServer::isKilled() const
{
    return dead.load();
}

// Factory (called by config)
KVServer* StartKVServer(labrpc::Server* srv, int me, raft::Persister* persister, int maxraftstate)
{
    return new KVServer(srv, me, persister, maxraftstate);
}