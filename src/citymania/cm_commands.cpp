#include "../stdafx.h"

#include "cm_commands.hpp"

#include "../command_func.h"
#include "../debug.h"
#include "../network/network.h"
#include "../network/network_client.h"

#include <queue>
#include <vector>


namespace citymania {

const uint32 MAX_CALLBACK_LIFETIME = 30;  // it should be executed within few frames so 30 more than enough

std::map<size_t, std::pair<uint32, std::vector<CommandCallback>>> _command_callbacks;
std::queue<std::pair<size_t, uint32>> _command_sent;
CommandCallback _current_callback = nullptr;
bool _auto_command = false;
std::queue<std::pair<size_t, CommandCallback>> _callback_queue;

uint GetCurrentQueueDelay();

template <typename T, typename... Rest>
void hash_combine(std::size_t& seed, const T& v, const Rest&... rest)
{
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    (hash_combine(seed, rest), ...);
}

}  // namespace citymania
namespace std {
    template<typename T>
    struct hash<std::vector<T>> {
        typedef std::vector<T> argument_type;
        typedef std::size_t result_type;
        result_type operator()(argument_type const& in) const {
            size_t size = in.size();
            size_t seed = 0;
            for (size_t i = 0; i < size; i++)
                citymania::hash_combine(seed, in[i]);
            return seed;
        }
    };
}  // namespace std
namespace citymania {

size_t GetCommandHash(Commands cmd, CompanyID company_id, StringID err_msg, ::CommandCallback callback, TileIndex tile, const CommandDataBuffer &data) {
    size_t res = 0;
    hash_combine(res, cmd, (uint16)company_id, err_msg, callback, (uint32)tile, data);
    return res;
}

void ExecuteCurrentCallback(const CommandCost &cost) {
    if (_current_callback != nullptr) {
        _current_callback(cost.Succeeded());
        _current_callback == nullptr;
    }
}

void BeforeNetworkCommandExecution(const CommandPacket* cp) {
    if (!cp->my_cmd) return;
    size_t hash = GetCommandHash(cp->cmd, cp->company, cp->err_msg, cp->callback, cp->tile, cp->data);
    Debug(misc, 5, "CM BeforeNetworkCommandExecution: cmd={} hash={}", cp->cmd, hash);
    while (!_callback_queue.empty() && _callback_queue.front().first != hash) {
        Debug(misc, 0, "CM Dismissing command from callback queue: hash={}", _callback_queue.front().first);
        _callback_queue.pop();
    }
    if (_callback_queue.empty()) {
        Debug(misc, 0, "CM Received unexpected network command: cmd={}", cp->cmd);
        return;
    }
    _current_callback = _callback_queue.front().second;
    _callback_queue.pop();
    return;
}

void AfterNetworkCommandExecution(const CommandPacket* cp) {
    _current_callback = nullptr;
}

void AddCommandCallback(const CommandPacket *cp) {
    size_t hash = GetCommandHash(cp->cmd, cp->company, cp->err_msg, cp->callback, cp->tile, cp->data);
    Debug(misc, 5, "CM Added callback: cmd={} hash={}", cp->cmd, hash);
    _callback_queue.push(std::make_pair(hash, _current_callback));
    _current_callback = nullptr;
}

/*
void AddCommandCallback(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, const std::string &text, CommandCallback callback) {
    if (!_networking) {
        callback(true);
        return;
    }
    auto hash = GetCommandHash(tile, p1, p2, cmd & CMD_ID_MASK, text);
    auto sent_frame = _frame_counter + GetCurrentQueueDelay();
    _command_callbacks[hash].first = sent_frame;
    _command_callbacks[hash].second.push_back(callback);
    _command_sent.push(std::make_pair(hash, sent_frame));
    // fprintf(stderr, "CALLBACK %lu (%u %u %u %u %s)\n", hash, tile, p1, p2, (uint)(cmd & CMD_ID_MASK), text.c_str());
}

*/
//void HandleCommandExecution(bool res, TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, const std::string &text) {
    /* FIXME
    auto hash = GetCommandHash(tile, p1, p2, cmd & CMD_ID_MASK, text);
    auto p = _command_callbacks.find(hash);
    // fprintf(stderr, "EXECUTED %lu (%u %u %u %u %s) %u\n", hash, tile, p1, p2, (uint)(cmd & CMD_ID_MASK), text.c_str(), (int)(p == _command_callbacks.end()));
    if (p == _command_callbacks.end()) return;
    for (auto &cb : p->second.second)
        cb(res);
    _command_callbacks.erase(p); */
//}

void ClearOldCallbacks() {
    while(!_command_sent.empty() && _command_sent.front().second + MAX_CALLBACK_LIFETIME < _frame_counter) {
        auto hash = _command_sent.front().first;
        _command_sent.pop();
        auto p = _command_callbacks.find(hash);
        if (p != _command_callbacks.end() && p->first + MAX_CALLBACK_LIFETIME < _frame_counter) {
            _command_callbacks.erase(p);
        }
    }
}

std::queue<CommandPacket> _outgoing_queue;
uint _commands_this_frame;

void InitCommandQueue() {
    _commands_this_frame = 0;
    std::queue<CommandPacket>().swap(_outgoing_queue);  // clear queue
    _command_callbacks.clear();
}

bool CanSendCommand() {
    return _commands_this_frame < 2;
}

uint GetCurrentQueueDelay() {
    return _outgoing_queue.size() / 2;
}

void FlushCommandQueue() {
    while (!_outgoing_queue.empty() && CanSendCommand()) {
        MyClient::SendCommand(&_outgoing_queue.front());
        _outgoing_queue.pop();
        _commands_this_frame++;
    }
}

void HandleNextClientFrame() {
    _commands_this_frame = 0;
    FlushCommandQueue();
    // ClearOldCallbacks();
}

void SendClientCommand(const CommandPacket *cp) {
    AddCommandCallback(cp);
    if (_outgoing_queue.empty() && CanSendCommand()) {
        MyClient::SendCommand(cp);
        _commands_this_frame++;
        return;
    }
    _outgoing_queue.push(*cp);
}

}  // namespace citymania
