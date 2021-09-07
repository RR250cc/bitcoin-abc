// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <addrdb.h>

#include <addrman.h>
#include <chainparams.h>
#include <clientversion.h>
#include <hash.h>
#include <logging/timer.h>
#include <random.h>
#include <streams.h>
#include <tinyformat.h>
#include <util/system.h>

#include <cstdint>

namespace {

template <typename Stream, typename Data>
bool SerializeDB(const CChainParams &chainParams, Stream &stream,
                 const Data &data) {
    // Write and commit header, data
    try {
        CHashWriter hasher(SER_DISK, CLIENT_VERSION);
        stream << chainParams.DiskMagic() << data;
        hasher << chainParams.DiskMagic() << data;
        stream << hasher.GetHash();
    } catch (const std::exception &e) {
        return error("%s: Serialize or I/O error - %s", __func__, e.what());
    }

    return true;
}

template <typename Data>
bool SerializeFileDB(const CChainParams &chainParams, const std::string &prefix,
                     const fs::path &path, const Data &data) {
    // Generate random temporary filename
    uint16_t randv = 0;
    GetRandBytes((uint8_t *)&randv, sizeof(randv));
    std::string tmpfn = strprintf("%s.%04x", prefix, randv);

    // open temp output file, and associate with CAutoFile
    fs::path pathTmp = gArgs.GetDataDirNet() / tmpfn;
    FILE *file = fsbridge::fopen(pathTmp, "wb");
    CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull()) {
        fileout.fclose();
        remove(pathTmp);
        return error("%s: Failed to open file %s", __func__,
                     fs::PathToString(pathTmp));
    }

    // Serialize
    if (!SerializeDB(chainParams, fileout, data)) {
        fileout.fclose();
        remove(pathTmp);
        return false;
    }
    if (!FileCommit(fileout.Get())) {
        fileout.fclose();
        remove(pathTmp);
        return error("%s: Failed to flush file %s", __func__,
                     fs::PathToString(pathTmp));
    }
    fileout.fclose();

    // replace existing file, if any, with new file
    if (!RenameOver(pathTmp, path)) {
        remove(pathTmp);
        return error("%s: Rename-into-place failed", __func__);
    }

    return true;
}

template <typename Stream, typename Data>
bool DeserializeDB(const CChainParams &chainParams, Stream &stream, Data &data,
                   bool fCheckSum = true) {
    try {
        CHashVerifier<Stream> verifier(&stream);
        // de-serialize file header (network specific magic number) and ..
        uint8_t pchMsgTmp[4];
        verifier >> pchMsgTmp;
        // ... verify the network matches ours
        if (memcmp(pchMsgTmp, std::begin(chainParams.DiskMagic()),
                   sizeof(pchMsgTmp))) {
            return error("%s: Invalid network magic number", __func__);
        }

        // de-serialize data
        verifier >> data;

        // verify checksum
        if (fCheckSum) {
            uint256 hashTmp;
            stream >> hashTmp;
            if (hashTmp != verifier.GetHash()) {
                return error("%s: Checksum mismatch, data corrupted", __func__);
            }
        }
    } catch (const std::exception &e) {
        return error("%s: Deserialize or I/O error - %s", __func__, e.what());
    }

    return true;
}

template <typename Data>
bool DeserializeFileDB(const CChainParams &chainParams, const fs::path &path,
                       Data &data) {
    // open input file, and associate with CAutoFile
    FILE *file = fsbridge::fopen(path, "rb");
    CAutoFile filein(file, SER_DISK, CLIENT_VERSION);
    if (filein.IsNull()) {
        LogPrintf("Missing or invalid file %s\n", fs::PathToString(path));
        return false;
    }

    return DeserializeDB(chainParams, filein, data);
}

} // namespace

CBanDB::CBanDB(fs::path ban_list_path, const CChainParams &_chainParams)
    : m_ban_list_path(std::move(ban_list_path)), chainParams(_chainParams) {}

bool CBanDB::Write(const banmap_t &banSet) {
    return SerializeFileDB(chainParams, "banlist", m_ban_list_path, banSet);
}

bool CBanDB::Read(banmap_t &banSet) {
    return DeserializeFileDB(chainParams, m_ban_list_path, banSet);
}

bool DumpPeerAddresses(const CChainParams &chainParams, const ArgsManager &args,
                       const CAddrMan &addr) {
    const auto pathAddr = args.GetDataDirNet() / "peers.dat";
    return SerializeFileDB(chainParams, "peers", pathAddr, addr);
}

bool ReadPeerAddresses(const CChainParams &chainParams, const ArgsManager &args,
                       CAddrMan &addr) {
    const auto pathAddr = args.GetDataDirNet() / "peers.dat";
    return DeserializeFileDB(chainParams, pathAddr, addr);
}

bool ReadFromStream(const CChainParams &chainParams, CAddrMan &addr,
                    CDataStream &ssPeers) {
    bool ret = DeserializeDB(chainParams, ssPeers, addr, false);
    if (!ret) {
        // Ensure addrman is left in a clean state
        addr.Clear();
    }
    return ret;
}

void DumpAnchors(const CChainParams &chainParams,
                 const fs::path &anchors_db_path,
                 const std::vector<CAddress> &anchors) {
    LOG_TIME_SECONDS(strprintf(
        "Flush %d outbound block-relay-only peer addresses to anchors.dat",
        anchors.size()));
    SerializeFileDB(chainParams, "anchors", anchors_db_path, anchors);
}

std::vector<CAddress> ReadAnchors(const CChainParams &chainParams,
                                  const fs::path &anchors_db_path) {
    std::vector<CAddress> anchors;
    if (DeserializeFileDB(chainParams, anchors_db_path, anchors)) {
        LogPrintf("Loaded %i addresses from %s\n", anchors.size(),
                  fs::quoted(fs::PathToString(anchors_db_path.filename())));
    } else {
        anchors.clear();
    }

    fs::remove(anchors_db_path);
    return anchors;
}
