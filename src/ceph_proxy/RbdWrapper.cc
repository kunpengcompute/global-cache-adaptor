/* License:LGPL-2.1
 *
 * Copyright (c) 2021 Huawei Technologies Co., Ltd All rights reserved.
 *
 */

#include <string>
#include <map>
#include <vector>
#include <shared_mutex>
#include <atomic>
#include <regex>
#include "rbd/librbd.h"
#include "rados/librados.h"
#include "rados/librados.hpp"
#include "rbd/librbd.hpp"
#include "CephExport.h"
#include "CephProxyLog.h"

#define AUTH_CLUSTER_REQUIRED   "auth_cluster_required"
#define RADOS_CONNECT_RETRY     5
#define CONNECT_WAIT_TIME       5
#define PATH_MAX_LEN        128
const uint32_t MB = (1024 * 1024);

class NoProgressContext : public librbd::ProgressContext {
public:
    NoProgressContext() { }
    int update_progress(uint64_t offset, uint64_t src_size) override
    {
        return 0;
    }
};

struct ProxyCtx {
    bool init_flag = false;
    librados::Rados client;
    std::shared_mutex lock;
    int ref;
};

struct SigPoolInfo {
    int64_t poolId;
    std::string poolName;
    bool isEC;
    int k;
    int m;
    uint64_t usage;
    std::string ecProfileName;
};

static struct ProxyCtx gProxyCtx;
using shared_lock = std::shared_lock<std::shared_mutex>;
using unique_lock = std::unique_lock<std::shared_mutex>;

static void RadosShutdown()
{
    unique_lock l(gProxyCtx.lock);
    gProxyCtx.ref--;
    if (gProxyCtx.ref == 0) {
        gProxyCtx.client.shutdown();
        gProxyCtx.init_flag = false;
    }
}

static void FastInitRados(std::map<std::string, std::string> &conf_map)
{
    if (!gProxyCtx.init_flag) {
        ProxyDbgCrit("proxy Ctx not init!");
        return;
    }
    gProxyCtx.ref++;
    std::map<std::string, std::string>::iterator iter;
    int ret;
    for (iter = conf_map.begin(); iter != conf_map.end(); iter++) {
        ret = gProxyCtx.client.conf_set(iter->first.c_str(), iter->second.c_str());
        if (ret < 0) {
            ProxyDbgLogErr("rados conf set %s=%s failed", iter->first.c_str(), iter->second.c_str());
            continue;
        }
        ProxyDbgLogDebug("rados conf set %s=%s", iter->first.c_str(), iter->second.c_str());
    }
}

extern "C" {
int GetCfgItemCstr(char *dest, size_t destSize, const char *unit, const char *key);
}

static int GetCephPath(std::string &confPath, std::string &keyPath)
{
    char cephConfPath[PATH_MAX_LEN] = { '\0' };
    char cephKeyringPath[PATH_MAX_LEN] = { '\0' };
    int ret = GetCfgItemCstr(cephConfPath, PATH_MAX_LEN, "proxy", "ceph_conf_path");
    if (ret != 0) {
        ProxyDbgLogErr("Failed to read the proxy conf ceph_conf_path. ret=%d\n", ret);
        return ret;
    }

    ret = GetCfgItemCstr(cephKeyringPath, PATH_MAX_LEN, "proxy", "ceph_keyring_path");
    if (ret != 0) {
        ProxyDbgLogErr("Failed to read the proxy conf ceph_keyring_path. ret=%d\n", ret);
        return ret;
    }

    confPath = cephConfPath;
    keyPath = cephKeyringPath;
    return 0;
}

static int RadosInit(std::map<std::string, std::string> &conf_map)
{
    uint32_t retryCount = 0;
    {
        unique_lock l(gProxyCtx.lock);
        if (gProxyCtx.init_flag) {
            FastInitRados(conf_map);
            return 0;       // auto unlock
        }
    }

    unique_lock l(gProxyCtx.lock);
    if (gProxyCtx.init_flag) {
        FastInitRados(conf_map);
        return 0;       // auto unlock
    }

    int ret = gProxyCtx.client.init(NULL);
    if (ret != 0) {
        ProxyDbgLogErr("rados create failed: %d", ret);
        return ret;
    }

    std::string confPath, keyPath;
    std::string anthName;
    std::map<std::string, std::string>::iterator iter;

    ret = GetCephPath(confPath, keyPath);
    if (ret != 0) {
        ProxyDbgLogErr("rados ceph path failed: %d", ret);
        goto shutdown;
    }
    ret = gProxyCtx.client.conf_read_file(confPath.c_str());
    if (ret != 0) {
        ProxyDbgLogErr("rados read file failed: %d", ret);
        goto shutdown;
    }
    ret = gProxyCtx.client.conf_get(AUTH_CLUSTER_REQUIRED, anthName);
    if (ret != 0) {
        ProxyDbgLogErr("rados conf get keyring failed, ret=%d", ret);
        goto shutdown;
    }

    if (strcmp(anthName.c_str(), "cephx") == 0) {
        ret = gProxyCtx.client.conf_set("keyring", keyPath.c_str());
        if (ret < 0) {
            ProxyDbgLogErr("rados conf set keyring=%s failed, ret=%d", keyPath.c_str(), ret);
            goto shutdown;
        }
    }

    for (iter = conf_map.begin(); iter != conf_map.end(); iter++) {
        ret = gProxyCtx.client.conf_set(iter->first.c_str(), iter->second.c_str());
        if (ret < 0) {
            ProxyDbgLogErr("rados conf set %s=%s failed", iter->first.c_str(), iter->second.c_str());
            gProxyCtx.client.shutdown();
            return ret;
        }
        ProxyDbgLogDebug("rados conf set %s=%s", iter->first.c_str(), iter->second.c_str());
    }

    while (retryCount < RADOS_CONNECT_RETRY) {
        ret = gProxyCtx.client.connect();
        if (ret < 0) {
            ProxyDbgLogErr("connect ceph monitor failed: %d, retry:[%u/%d]", ret, retryCount + 1, RADOS_CONNECT_RETRY);
            retryCount++;
            sleep(CONNECT_WAIT_TIME * (1 << retryCount));
            continue;
        } else {
            break;
        }
    }

    if (ret < 0) {
        ProxyDbgLogErr("rados connect timeout failed: %d", ret);
        gProxyCtx.client.shutdown();
        return ret;
    }

    gProxyCtx.ref = 0;
    gProxyCtx.ref++;
    gProxyCtx.init_flag = true;

    ProxyDbgLogDebug("rados init success");
    return 0;
shutdown:
    gProxyCtx.client.shutdown();
    return ret;
}

static void IoCtxDestroy(librados::IoCtx &ioctx)
{
    ioctx.close();
}

static int IoctxSetNamespace(librados::IoCtx &io_ctx, const std::string &namespace_name)
{
    if (!namespace_name.empty()) {
        librbd::RBD rbd;
        bool exists = false;
        int r = rbd.namespace_exists(io_ctx, namespace_name.c_str(), &exists);
        if (r < 0) {
            ProxyDbgLogErr("namespace_exists interface return %d", r);
            return r;
        }
        if (!exists) {
            ProxyDbgLogErr("name space %s not exists", namespace_name.c_str());
            return -ENOENT;
        }
    }
    io_ctx.set_namespace(namespace_name);
    return 0;
}

static int IoctxInit(librados::IoCtx *ioctx, const std::string &pool_name,
                    int64_t pool_id, const std::string &namespace_name)
{
    int ret;
    if (!gProxyCtx.init_flag) {
        ProxyDbgCrit("proxy ctx not init!");
        return -1;
    }
    if (!pool_name.empty()) {
        ret = gProxyCtx.client.ioctx_create(pool_name.c_str(), *ioctx);
        if (ret < 0) {
            ProxyDbgLogErr("Rados ioctx Init failed, pool_name %s, ret %d", pool_name.c_str(), ret);
            return ret;
        }
    } else {
        ret = gProxyCtx.client.ioctx_create2(pool_id, *ioctx);
        if (ret < 0) {
            ProxyDbgLogErr("Rados ioctx Init failed, pool_id %ld, ret %d", pool_id, ret);
            return ret;
        }
    }

    ret = IoctxSetNamespace(*ioctx, namespace_name);
    if (ret < 0) {
        ProxyDbgLogErr("rados set namespace failed, ns %s, ret %d", namespace_name.c_str(), ret);
        IoCtxDestroy(*ioctx);
        return ret;
    }

    return ret;
}

static void IoctxSetOsdmapFullTry(librados::IoCtx &io_ctx)
{
    io_ctx.set_osdmap_full_try();
}

static void IoctxCloseImage(librbd::Image &image)
{
    image.close();
}

static int IoctxOpenImage(librados::IoCtx &io_ctx, const std::string &image_name,
                        const std::string &image_id, librbd::Image *image)
{
    int ret;
    librbd::RBD rbd;

    if (!image_id.empty()) {
        ret = rbd.open_by_id(io_ctx, *image, image_id.c_str());
        if (ret < 0) {
            ProxyDbgLogErr("rbd open image id %s failed %d", image_id.c_str(), ret);
            return ret;
        }
    } else {
        ret = rbd.open(io_ctx, *image, image_name.c_str());
        if (ret < 0) {
            ProxyDbgLogErr("rbd open image name %s failed %d", image_name.c_str(), ret);
            return ret;
        }
    }

    return ret;
}

static int ImageState(librbd::Image &image, rbd_image_info_t &info)
{
    int ret;
    librbd::RBD rbd;

    ret = image.stat(info, 1);
    if (ret < 0) {
        ProxyDbgLogErr("rbd stat failed, ret=%d", ret);
    }
    return ret;
}

static int ImageRemoveSnap(librbd::Image &image, const std::string &snap_name, uint64_t snap_id, bool force)
{
    int ret;

    if (!snap_name.empty()) {
        uint32_t flags = force ? RBD_SNAP_REMOVE_FORCE : 0;
        NoProgressContext prog_ctx;
        ret = image.snap_remove2(snap_name.c_str(), flags, prog_ctx);
        if (ret < 0) {
            ProxyDbgLogErr("rbd snap remove failed. snap_name %s force %d ret %d",
                            snap_name.c_str(), force, ret);
            return ret;
        }
    } else {
        ret = image.snap_remove_by_id(snap_id);
        if (ret < 0) {
            ProxyDbgLogErr("rbd snap remove failed. snap_id %llu ret %d", snap_id, ret);
            return ret;
        }
    }

    return ret;
}

int CephLibrbdSnapRemove(int64_t pool_id,
                        const char *_namespace_name,
                        const char *_image_id,
                        uint64_t snap_id,
                        bool force)
{
    if (_namespace_name == nullptr || _image_id == nullptr) {
        ProxyDbgLogErr("namespace_name %p or image_id %p should not nullptr", _namespace_name, _image_id);
        return -EINVAL;
    }
    int ret;
    librados::IoCtx ioctx;
    librbd::Image image;
    std::string pool_name = "";
    std::string namespace_name = _namespace_name;
    std::string image_name = "";
    std::string image_id = _image_id;
    std::string snap_name = "";

    std::map<std::string, std::string> confMap;
    confMap["rbd_cache_writethrough_until_flush"] = "false";

    if (pool_id == 0 && pool_name.empty()) {
        ProxyDbgLogErr("both empty with pool id and pool name");
        return -EINVAL;
    } else if (image_id.empty() && image_name.empty()) {
        ProxyDbgLogErr("both empty with image id and image name");
        return -EINVAL;
    } else if (snap_id == 0 && snap_name.empty()) {
        ProxyDbgLogErr("both empty with snap id and snap name");
        return -EINVAL;
    }

    ProxyDbgLogInfo("image remove snap start pool %lld:%s image %s:%s snap %llu:%s",
        pool_id, pool_name.c_str(), image_id.c_str(), image_name.c_str(), snap_id, snap_name.c_str());

    ret = RadosInit(confMap);
    if (ret < 0) {
        ProxyDbgLogErr("rados client Init failed: %d", ret);
        return ret;
    }

    ret = IoctxInit(&ioctx, pool_name, pool_id, namespace_name);
    if (ret < 0) {
        if (ret == -ENOENT) {
            ret = 0;
        } else {
            ProxyDbgLogErr("ioctx init failed: %d", ret);
        }
        goto shutdown;
    }

    IoctxSetOsdmapFullTry(ioctx);

    ret = IoctxOpenImage(ioctx, image_name, image_id, &image);
    if (ret < 0) {
        if (ret == -ENOENT) {
            ret = 0;
        } else {
            ProxyDbgLogErr("image open failed: %d", ret);
        }
        goto close_ioctx;
    }

    ret = ImageRemoveSnap(image, snap_name, snap_id, force);
    if (ret < 0) {
        if (ret == -ENOENT) {
            ret = 0;
        } else {
            ProxyDbgLogErr("image remove snap failed: %d", ret);
        }
        goto close_image;
    }

    ProxyDbgLogInfo("image remove snap success pool %lld:%s image %s:%s snap %llu:%s",
        pool_id, pool_name.c_str(), image_id.c_str(), image_name.c_str(), snap_id, snap_name.c_str());

close_image:
    IoctxCloseImage(image);

close_ioctx:
    IoCtxDestroy(ioctx);

shutdown:
    RadosShutdown();
    return ret;
}


int CephLibrbdGetImageInfo(int64_t pool_id,
                           const char *_image_id,
                           int32_t *num_objs)
{
    if (num_objs == nullptr || _image_id == nullptr) {
        ProxyDbgLogErr("num_objs %p or image_id %p should not nullptr", num_objs, _image_id);
        return -EINVAL;
    }
    int ret;
    librados::IoCtx ioctx;
    librbd::Image image;
    std::string image_id = _image_id;
    rbd_image_info_t info;

    std::map<std::string, std::string> confMap;
    ret = RadosInit(confMap);
    if (ret < 0) {
        ProxyDbgLogErr("rados client Init failed: %d", ret);
        return ret;
    }
    ret = IoctxInit(&ioctx, "", pool_id, "");
    if (ret < 0) {
        ProxyDbgLogErr("ioctx %lld Init failed: %d", pool_id, ret);
        goto shutdown;
    }

    ret = IoctxOpenImage(ioctx, "", image_id, &image);
    if (ret < 0) {
        ProxyDbgLogErr("image %s open failed: %d", _image_id, ret);
        goto close_ioctx;
    }

    ret = ImageState(image, info);
    if (ret < 0) {
        ProxyDbgLogErr("get image order failed! image=%s, ret=%d", _image_id, ret);
        goto close_image;
    }

    *num_objs = info.num_objs;

close_image:
    IoctxCloseImage(image);

close_ioctx:
    IoCtxDestroy(ioctx);

shutdown:
    RadosShutdown();
    return ret;
}

static int MonCommand(std::string cmd, std::string &_outs)
{
    int ret;
    if (!gProxyCtx.init_flag) {
        ProxyDbgCrit("proxy ctx not init!");
        return -1;
    }
    std::string outs;
    bufferlist inbl;
    bufferlist outbl;
    ret = gProxyCtx.client.mon_command(cmd, inbl, &outbl, &outs);
    if (ret < 0) {
        ProxyDbgLogWarn("mon_command failed: %s, ret=%d", cmd.c_str(), ret);
        return ret;
    }
    _outs = outbl.to_str().c_str();
    return 0;
}
// 解析ceph osd lspools 命令，并将pool都存入poolMap
static int ParsePoolMap(std::string &outs, std::map<int64_t, struct SigPoolInfo> &poolMap)
{
    std::string pattern = "\\s*(\\d+)\\s+(\\S+)";
    std::vector<std::string> preVec;
    char *strs = new(std::nothrow) char[outs.length() + 1];
    if (strs == nullptr) {
        ProxyDbgLogErr("alloc memory failed");
        return -1;
    }
    strcpy(strs, outs.c_str());

    char *savep;
    char *p = strtok_r(strs, "\n", &savep);
    while (p) {
        std::string s = p;
        preVec.push_back(s);
        p = strtok_r(nullptr, "\n", &savep);
    }

    for (size_t i = 0; i < preVec.size(); i++) {
        std::regex expression(pattern);
        std::smatch result;
        bool flag = std::regex_match(preVec[i], result, expression);
        if (flag) {
            errno = 0;
            char *end = nullptr;
            const char *p = result[1].str().c_str();
            int64_t poolId = (int64_t)strtol(p, &end, 10);
            if (errno == ERANGE || end == p) {
                ProxyDbgLogErr("get poolId from str failed.");
                break;
            }
            struct SigPoolInfo info;
            info.poolId = poolId;
            info.poolName = result[2].str().c_str();        // 第2个匹配项
            info.usage = 0;
            poolMap[poolId] = info;
            ProxyDbgLogInfo("pool %s, id %lld", info.poolName.c_str(), info.poolId);
        }
    }

    if (strs) {
        delete[] strs;
        strs = nullptr;
    }
    return 0;
}
// 获取指定ecProfile的 k 和 m
static int GetECPoolScale(struct SigPoolInfo &info)
{
    std::string cmd("{\"prefix\": \"osd erasure-code-profile get\", \"name\": \"");
    std::string outs;
    cmd.append(info.ecProfileName);
    cmd.append(std::string("\"}"));

    int ret = MonCommand(cmd, outs);    // ceph osd erasure-code-profile get xxxx
    if (ret != 0) {
        ProxyDbgLogErr("Get erasure code profile failed, profile=%s, ret=%d", info.ecProfileName.c_str(), ret);
        return ret;
    }

    std::vector<std::string> preVec;
    char *strs = new(std::nothrow) char[outs.length() + 1];
    if (strs == nullptr) {
        ProxyDbgLogErr("alloc memory failed");
        return -1;
    }
    strcpy(strs, outs.c_str());

    char *savep;
    char *p = strtok_r(strs, "\n", &savep);
    while (p) {
        std::string s = p;
        preVec.push_back(s);
        p = strtok_r(nullptr, "\n", &savep);
    }

    for (uint32_t i = 0; i < preVec.size(); i++) {
        std::regex expression("(\\w+)=(\\w+)");
        std::smatch result;
        bool flag = std::regex_match(preVec[i], result, expression);
        if (!flag) {
            continue;
        }
        errno = 0;
        char *end = nullptr;
        const char *p = result[2].str().c_str();
        if (result[1].str().compare("k") == 0) {
            info.k = (int)strtol(p, &end, 10); // 获取 k
            if (errno == ERANGE || end == p) {
                ProxyDbgLogErr("get k from str failed.");
                break;
            }
        } else if (result[1].str().compare("m") == 0) {
            info.m = (int)strtol(p, &end, 10); // 获取 m
            if (errno == ERANGE || end == p) {
                ProxyDbgLogErr("get m from str failed.");
                break;
            }
        }
    }
    if (strs) {
        delete[] strs;
        strs = nullptr;
    }
    return 0;
}
// 遍历 poolMap 中的 pool，获取 pool 的 k 和 m
// 3副本固定k=1，m=2，EC按照配置的 erasure_code_profile 来获取
static int CalPoolProperty(std::map<int64_t, struct SigPoolInfo> &poolMap)
{
    for (std::map<int64_t, struct SigPoolInfo>::iterator p = poolMap.begin(); p != poolMap.end(); p++) {
        struct SigPoolInfo& info = p->second;
        std::string cmd("{\"var\": \"erasure_code_profile\", \"prefix\": \"osd pool get\", \"pool\": \"");
        std::string outs;
        cmd.append(info.poolName);
        cmd.append(std::string("\"}"));
        int ret = MonCommand(cmd, outs);        // ceph osd pool get xxx erasure_code_profile
        if (ret < 0) {
            if (ret == -EACCES) {       // replica
                info.isEC = false;
                info.k = 1;
                info.m = 2;             // 3副本相当于1个源块和2个校验块
                ProxyDbgLogInfo("Non EC pool %s, k=%d, m=%d", info.poolName.c_str(), info.k, info.m);
            } else {
                ProxyDbgLogErr("Get erasure code profile failed, name=%s, ret=%d", info.poolName.c_str(), ret);
                return ret;
            }
        } else {        // EC
            info.isEC = true;
            info.k = info.m = 0;
            std::regex expression("erasure_code_profile:\\s(\\S+)\\n?");
            std::smatch result;
            std::string outStr(outs.c_str());
            bool flag = std::regex_match(outStr, result, expression);
            if (flag) {
                info.ecProfileName = result[1].str().c_str();
            } else {
                ProxyDbgLogErr("regex erasure code profile failed, name=%s, out=%s",
                    info.poolName.c_str(), outs.c_str());
                return -1;
            }
            ret = GetECPoolScale(info);
            if (ret < 0) {
                ProxyDbgLogErr("get EC pool size failed! ret=%d", ret);
                return ret;
            }

            if (info.k == 0 || info.m == 0) {
                ProxyDbgCrit("unexpected EC result k %d m %d", info.k, info.m);
                return -1;
            }
            ProxyDbgLogInfo("EC pool %s, k=%d, m=%d", info.poolName.c_str(), info.k, info.m);
        }
    }
    return 0;
}

static int DiffCallback(uint64_t offset, size_t len, int exists, void *arg)
{
    uint64_t *used = static_cast<uint64_t *>(arg);
    if (exists) {
        (*used) += len;
    }
    return 0;
}
// 计算image的磁盘使用率，包括下属快照
static int CalImageUsage(librados::IoCtx &ioctx, struct SigPoolInfo& info, librbd::image_spec_t& imageSpec)
{
    int ret;
    librbd::RBD rbd;
    librbd::Image image;
    bool exact = false;

    ret = rbd.open_read_only(ioctx, image, imageSpec.name.c_str(), NULL);
    if (ret < 0) {
        ProxyDbgLogErr("image open failed, image=%s/%s, ret=%d",
            info.poolName.c_str(), imageSpec.name.c_str(), ret);
        return ret;
    }

    librbd::image_info_t state;
    std::string lastSnap;
    std::vector<librbd::snap_info_t> snapList;
    uint64_t used;
    const char *snapFrom = nullptr;

    ret = image.stat(state, sizeof(state));     // 获取image.size
    if (ret < 0) {
        ProxyDbgLogErr("image stat failed, image=%s/%s, ret=%d",
            info.poolName.c_str(), imageSpec.name.c_str(), ret);
        goto image_close;
    }

    ret = image.snap_list(snapList);       // 获取快照列表
    if (ret < 0) {
        ProxyDbgLogErr("image snap list failed, image=%s/%s, ret=%d",
            info.poolName.c_str(), imageSpec.name.c_str(), ret);
        goto image_close;
    }
    // 需要记录上次访问的snap，即last_snap，之后的snap或者image都上次的snap进行diff
    for (librbd::snap_info_t& snap : snapList) {
        librbd::Image snap_image;
        ret = rbd.open_read_only(ioctx, snap_image, imageSpec.name.c_str(), snap.name.c_str());
        if (ret < 0) {
            ProxyDbgLogErr("snap open failed, image=%s/%s@%s, ret=%d",
                info.poolName.c_str(), imageSpec.name.c_str(), snap.name.c_str(), ret);
            goto image_close;
        }

        if (!lastSnap.empty()) {
            snapFrom = lastSnap.c_str();
        }
        used = 0;
        ret = snap_image.diff_iterate2(snapFrom, 0, snap.size, false, !exact, &DiffCallback, &used);   // diff snap
        if (ret < 0) {
            ProxyDbgLogErr("snap diff failed, image=%s/%s@%s, ret=%d",
                info.poolName.c_str(), imageSpec.name.c_str(), snap.name.c_str(), ret);
            snap_image.close();
            goto image_close;
        }
        info.usage += used;
        lastSnap = snap.name.c_str();      // update last snap
        ProxyDbgLogInfo("snap %s/%s@%s, used=%llu",
            info.poolName.c_str(), imageSpec.name.c_str(), snap.name.c_str(), used);
    }

    if (!lastSnap.empty()) {
        snapFrom = lastSnap.c_str();
    }
    used = 0;
    ret = image.diff_iterate2(snapFrom, 0, state.size, false, !exact, &DiffCallback, &used);   // diff image
    if (ret < 0) {
        ProxyDbgLogErr("image diff failed, image=%s/%s, ret=%d",
            info.poolName.c_str(), imageSpec.name.c_str(), ret);
        goto image_close;
    }
    info.usage += used;
    ProxyDbgLogInfo("image %s/%s, used=%llu", info.poolName.c_str(), imageSpec.name.c_str(), used);
image_close:
    image.close();
    return ret;
}
// 获取池的磁盘使用率
static int CalPoolUsage(std::map<int64_t, struct SigPoolInfo> &poolMap)
{
    int ret = 0;
    for (std::map<int64_t, struct SigPoolInfo>::iterator p = poolMap.begin(); p != poolMap.end(); p++) {
        struct SigPoolInfo& info = p->second;
        librados::IoCtx ioctx;
        ret = IoctxInit(&ioctx, info.poolName, 0, "");
        if (ret < 0) {
            ProxyDbgLogErr("ioctx Init failed, poolName=%s, ret=%d", info.poolName.c_str(), ret);
            return ret;
        }

        librbd::RBD rbd;
        std::vector<librbd::image_spec_t> images;
        ret = rbd.list2(ioctx, &images);        // 获取所有的image
        if (ret < 0) {
            ProxyDbgLogErr("ioctx list image failed, poolName=%s, ret=%d", info.poolName.c_str(), ret);
            IoCtxDestroy(ioctx);
            return ret;
        }
        for (librbd::image_spec_t& imageSpec: images) {
            ret = CalImageUsage(ioctx, info, imageSpec);
            if (ret < 0) {
                ProxyDbgLogErr("cal image usage failed, image=%s/%s, ret=%d",
                    info.poolName.c_str(), imageSpec.name.c_str(), ret);
                IoCtxDestroy(ioctx);
                return ret;
            }
        }
        IoCtxDestroy(ioctx);
    }
    return ret;
}

int CephLibrbdDiskUsage(uint64_t *usage)
{
    if (usage == nullptr) {
        ProxyDbgLogErr("usage %p should not nullptr", usage);
        return -EINVAL;
    }

    std::map<std::string, std::string> confMap;
    confMap["rbd_cache_writethrough_until_flush"] = "false";
    std::map<int64_t, struct SigPoolInfo> poolMap;
    std::map<int64_t, struct SigPoolInfo>::iterator p;

    int ret = RadosInit(confMap);
    if (ret < 0) {
        ProxyDbgLogErr("rados client Init failed: %d", ret);
        return ret;
    }
    std::string cmd("{\"prefix\":\"osd lspools\"}");
    std::string outs;
    ret = MonCommand(cmd, outs);        // ceph osd lspools
    if (ret < 0) {
        ProxyDbgLogErr("lspools failed: %d", ret);
        goto shutdown;
    }

    ret = ParsePoolMap(outs, poolMap);
    if (ret < 0) {
        goto shutdown;
    }

    ret = CalPoolProperty(poolMap);
    if (ret < 0) {
        goto shutdown;
    }

    ret = CalPoolUsage(poolMap);
    if (ret < 0) {
        goto shutdown;
    }

    *usage = 0;
    for (p = poolMap.begin(); p != poolMap.end(); p++) {
        struct SigPoolInfo& info = p->second;
        *usage += info.usage * (info.k + info.m) / info.k;
    }
    *usage /= MB;

shutdown:
    RadosShutdown();
    return ret;
}
