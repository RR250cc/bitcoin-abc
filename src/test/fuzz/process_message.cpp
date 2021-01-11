// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <banman.h>
#include <chainparams.h>
#include <config.h>
#include <consensus/consensus.h>
#include <net.h>
#include <net_processing.h>
#include <protocol.h>
#include <scheduler.h>
#include <script/script.h>
#include <streams.h>
#include <validationinterface.h>
#include <version.h>

#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>
#include <test/util/mining.h>
#include <test/util/net.h>
#include <test/util/setup_common.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iosfwd>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

#ifdef MESSAGE_TYPE
#define TO_STRING_(s) #s
#define TO_STRING(s) TO_STRING_(s)
const std::string LIMIT_TO_MESSAGE_TYPE{TO_STRING(MESSAGE_TYPE)};
#else
const std::string LIMIT_TO_MESSAGE_TYPE;
#endif

const std::map<std::string, std::set<std::string>>
    EXPECTED_DESERIALIZATION_EXCEPTIONS = {
        {"CDataStream::read(): end of data: iostream error",
         {"addr", "block", "blocktxn", "cmpctblock", "feefilter", "filteradd",
          "filterload", "getblocks", "getblocktxn", "getdata", "getheaders",
          "headers", "inv", "notfound", "ping", "sendcmpct", "tx"}},
        {"CompactSize exceeds limit of type: iostream error", {"cmpctblock"}},
        {"differential value overflow: iostream error", {"getblocktxn"}},
        {"index overflowed 16 bits: iostream error", {"getblocktxn"}},
        {"index overflowed 16-bits: iostream error", {"cmpctblock"}},
        {"indexes overflowed 16 bits: iostream error", {"getblocktxn"}},
        {"non-canonical ReadCompactSize(): iostream error",
         {"addr", "block", "blocktxn", "cmpctblock", "filteradd", "filterload",
          "getblocks", "getblocktxn", "getdata", "getheaders", "headers", "inv",
          "notfound", "tx"}},
        {"ReadCompactSize(): size too large: iostream error",
         {"addr", "block", "blocktxn", "cmpctblock", "filteradd", "filterload",
          "getblocks", "getblocktxn", "getdata", "getheaders", "headers", "inv",
          "notfound", "tx"}},
        {"Superfluous witness record: iostream error",
         {"block", "blocktxn", "cmpctblock", "tx"}},
        {"Unknown transaction optional data: iostream error",
         {"block", "blocktxn", "cmpctblock", "tx"}},
};

const TestingSetup *g_setup;
} // namespace

void initialize() {
    static TestingSetup setup{
        CBaseChainParams::REGTEST,
        {
            "-nodebuglogfile",
        },
    };
    g_setup = &setup;

    for (int i = 0; i < 2 * COINBASE_MATURITY; i++) {
        MineBlock(GetConfig(), g_setup->m_node, CScript() << OP_TRUE);
    }
    SyncWithValidationInterfaceQueue();
}

void test_one_input(const std::vector<uint8_t> &buffer) {
    const Config &config = GetConfig();
    FuzzedDataProvider fuzzed_data_provider(buffer.data(), buffer.size());
    ConnmanTestMsg &connman = *(ConnmanTestMsg *)g_setup->m_node.connman.get();
    const std::string random_message_type{
        fuzzed_data_provider.ConsumeBytesAsString(CMessageHeader::COMMAND_SIZE)
            .c_str()};
    if (!LIMIT_TO_MESSAGE_TYPE.empty() &&
        random_message_type != LIMIT_TO_MESSAGE_TYPE) {
        return;
    }
    CDataStream random_bytes_data_stream{
        fuzzed_data_provider.ConsumeRemainingBytes<uint8_t>(), SER_NETWORK,
        PROTOCOL_VERSION};
    CNode &p2p_node =
        *std::make_unique<CNode>(
             0, ServiceFlags(NODE_NETWORK | NODE_BLOOM), 0, INVALID_SOCKET,
             CAddress{CService{in_addr{0x0100007f}, 7777}, NODE_NETWORK}, 0, 0,
             0, CAddress{}, std::string{}, ConnectionType::OUTBOUND)
             .release();
    p2p_node.fSuccessfullyConnected = true;
    p2p_node.nVersion = PROTOCOL_VERSION;
    p2p_node.SetSendVersion(PROTOCOL_VERSION);
    connman.AddTestNode(p2p_node);
    g_setup->m_node.peer_logic->InitializeNode(config, &p2p_node);
    try {
        g_setup->m_node.peer_logic->ProcessMessage(
            config, p2p_node, random_message_type, random_bytes_data_stream,
            GetTimeMillis(), std::atomic<bool>{false});
    } catch (const std::ios_base::failure &e) {
        const std::string exception_message{e.what()};
        const auto p =
            EXPECTED_DESERIALIZATION_EXCEPTIONS.find(exception_message);
        if (p == EXPECTED_DESERIALIZATION_EXCEPTIONS.cend() ||
            p->second.count(random_message_type) == 0) {
            std::cout << "Unexpected exception when processing message type \""
                      << random_message_type << "\": " << exception_message
                      << std::endl;
            assert(false);
        }
    }
    SyncWithValidationInterfaceQueue();
    // See init.cpp for rationale for implicit locking order requirement
    LOCK2(::cs_main, g_cs_orphans);
    g_setup->m_node.connman->StopNodes();
}
