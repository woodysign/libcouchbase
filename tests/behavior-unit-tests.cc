/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2012 Couchbase, Inc.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#include "config.h"
#include <gtest/gtest.h>
#include <libcouchbase/couchbase.h>
#include <map>

#define ENV_VAR_NAME "LIBCOUCHBASE_EVENT_PLUGIN_NAME"
#define ENV_VAR_SYM "LIBCOUCHBASE_EVENT_PLUGIN_SYMBOL"


#ifdef _WIN32
#define EXPECTED_DEFAULT LCB_IO_OPS_WINIOCP
#define setenv(k, v, o) SetEnvironmentVariable(k, v)
#else
#define EXPECTED_DEFAULT LCB_IO_OPS_LIBEVENT
#endif

static void setPluginEnv(const std::string &name, const std::string &sym)
{
    setenv(ENV_VAR_NAME, name.c_str(), 1);
    setenv(ENV_VAR_SYM, sym.c_str(), 1);
}

static void clearPluginEnv()
{
    setPluginEnv("", "");
}

typedef std::map<std::string, lcb_io_ops_type_t> plugin_map;
class PluginMap
{
public:
    plugin_map kv;

    PluginMap() {
        kv["select"] = LCB_IO_OPS_SELECT;
        kv["libevent"] = LCB_IO_OPS_LIBEVENT;
        kv["libev"] = LCB_IO_OPS_LIBEV;
#ifdef _WIN32
        kv["iocp"] = LCB_IO_OPS_WINIOCP;
        kv["winsock"] = LCB_IO_OPS_WINSOCK;
#endif
    }

};

static PluginMap plugins;

class Behavior : public ::testing::Test
{
public:
    virtual void SetUp() {
        const char *tmp;
        if ((tmp = getenv(ENV_VAR_NAME)) != NULL) {
            origPluginName = tmp;
        }

        if ((tmp = getenv(ENV_VAR_SYM)) != NULL) {
            origPluginSymbol = tmp;
        }

        // Clear it
        clearPluginEnv();

        ASSERT_EQ(LCB_SUCCESS, lcb_create(&instance, NULL));
    }

    virtual void TearDown() {
        lcb_destroy(instance);
        setPluginEnv(origPluginName, origPluginSymbol);
    }

protected:
    lcb_t instance;
    std::string origPluginName;
    std::string origPluginSymbol;
};


TEST_F(Behavior, CheckDefaultValues)
{
    EXPECT_EQ(LCB_ASYNCHRONOUS, lcb_behavior_get_syncmode(instance));
    EXPECT_EQ(LCB_IPV6_DISABLED, lcb_behavior_get_ipv6(instance));
    return;
}

TEST_F(Behavior, CheckSyncmode)
{
    lcb_behavior_set_syncmode(instance, LCB_SYNCHRONOUS);
    EXPECT_EQ(LCB_SYNCHRONOUS, lcb_behavior_get_syncmode(instance));
    lcb_behavior_set_syncmode(instance, LCB_ASYNCHRONOUS);
    EXPECT_EQ(LCB_ASYNCHRONOUS, lcb_behavior_get_syncmode(instance));
}

TEST_F(Behavior, CheckIPv6)
{
    lcb_behavior_set_ipv6(instance, LCB_IPV6_ONLY);
    EXPECT_EQ(LCB_IPV6_ONLY, lcb_behavior_get_ipv6(instance));

    lcb_behavior_set_ipv6(instance, LCB_IPV6_ALLOW);
    EXPECT_EQ(LCB_IPV6_ALLOW, lcb_behavior_get_ipv6(instance));

    lcb_behavior_set_ipv6(instance, LCB_IPV6_DISABLED);
    EXPECT_EQ(LCB_IPV6_DISABLED, lcb_behavior_get_ipv6(instance));
}

TEST_F(Behavior, PluginDefaults)
{
    lcb_error_t err;
    struct lcb_cntl_iops_info_st info;
    memset(&info, 0, sizeof(info));

    err = lcb_cntl(NULL, LCB_CNTL_GET, LCB_CNTL_IOPS_DEFAULT_TYPES, &info);

    ASSERT_EQ(LCB_SUCCESS, err);
    ASSERT_EQ(EXPECTED_DEFAULT, info.v.v0.os_default);
    ASSERT_EQ(EXPECTED_DEFAULT, info.v.v0.effective);
}

TEST_F(Behavior, PluginEnvironment)
{

    for (plugin_map::iterator iter = plugins.kv.begin();
            iter != plugins.kv.end(); iter++) {

        setPluginEnv(iter->first, "");

        lcb_io_ops_type_t iotype;
        lcb_error_t err;
        struct lcb_cntl_iops_info_st info;
        memset(&info, 0, sizeof(info));

        err = lcb_cntl(NULL, LCB_CNTL_GET, LCB_CNTL_IOPS_DEFAULT_TYPES, &info);
        ASSERT_EQ(LCB_SUCCESS, err);
        ASSERT_EQ(EXPECTED_DEFAULT, info.v.v0.os_default);
        ASSERT_EQ(iter->second, info.v.v0.effective) << iter->first;
    }
}

TEST_F(Behavior, PluginOverrides)
{
    // Environment is cleared
    struct lcb_create_io_ops_st options;
    struct lcb_cntl_iops_info_st ioinfo;

    memset(&options, 0, sizeof(options));
    memset(&ioinfo, 0, sizeof(ioinfo));

    setPluginEnv("select", "");
    options.version = 0;
    options.v.v0.type = LCB_IO_OPS_LIBEV;
    lcb_error_t err;

    ioinfo.v.v0.options = &options;
    err = lcb_cntl(NULL, LCB_CNTL_GET, LCB_CNTL_IOPS_DEFAULT_TYPES, &ioinfo);
    ASSERT_EQ(LCB_SUCCESS, err);
    ASSERT_EQ(ioinfo.v.v0.effective, LCB_IO_OPS_LIBEV);

    setPluginEnv("select", "");
    options.v.v0.type = LCB_IO_OPS_DEFAULT;
    err = lcb_cntl(NULL, LCB_CNTL_GET, LCB_CNTL_IOPS_DEFAULT_TYPES, &ioinfo);
    ASSERT_EQ(LCB_SUCCESS, err);
    ASSERT_EQ(ioinfo.v.v0.effective, LCB_IO_OPS_SELECT);

    memset(&options, 0, sizeof(options));
    options.version = 1;
    options.v.v1.sofile = "libfoo";
    options.v.v1.symbol = "abort";
    err = lcb_cntl(NULL, LCB_CNTL_GET, LCB_CNTL_IOPS_DEFAULT_TYPES, &ioinfo);
    ASSERT_EQ(LCB_SUCCESS, err);
    ASSERT_EQ(ioinfo.v.v0.effective, 0);

}

TEST_F(Behavior, BadPluginEnvironment)
{
    lcb_error_t err;
    struct lcb_cntl_iops_info_st info;
    memset(&info, 0, sizeof(info));

    setPluginEnv("foobarbaz", "non_existent_symbol");
    err = lcb_cntl(NULL, LCB_CNTL_GET, LCB_CNTL_IOPS_DEFAULT_TYPES, &info);
    ASSERT_EQ(LCB_SUCCESS, err);
    ASSERT_EQ(info.v.v0.os_default, EXPECTED_DEFAULT);
    ASSERT_EQ(info.v.v0.effective, 0);

    lcb_t instance2;
    ASSERT_EQ(lcb_create(&instance2, NULL), LCB_DLOPEN_FAILED);

    setPluginEnv("foobarbaz", "");
    ASSERT_EQ(lcb_create(&instance2, NULL), LCB_BAD_ENVIRONMENT);

    // Find a DLL that we know can be loaded, but doesn't have the symbols
    // we need. For windows, we use the unqualified name,
#ifdef _WIN32
    const char *dllname = "kernel32.dll.";
#elif __APPLE__
    const char *dllname = "libm.dylib";
#else
    const char *dllname = "libm.so";
#endif

    setPluginEnv(dllname, "nonexist-symbol");
    ASSERT_EQ(lcb_create(&instance2, NULL), LCB_DLSYM_FAILED);

}
