// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "chainparams.h"
#include "clientversion.h"
#include "compat.h"
#include "fs.h"
#include "rpc/server.h"
#include "init.h"
#include "noui.h"
#include "scheduler.h"
#include "util.h"
#include "httpserver.h"
#include "httprpc.h"
#include "utilstrencodings.h"

#include <boost/thread.hpp>

#include <stdio.h>

/* Introduction text for doxygen: */

/*! \mainpage Developer documentation
 *
 * \section intro_sec Introduction
 *
 * This is the developer documentation of the reference client for an experimental new digital currency called Bitcoin (https://www.bitcoin.org/),
 * which enables instant payments to anyone, anywhere in the world. Bitcoin uses peer-to-peer technology to operate
 * with no central authority: managing transactions and issuing money are carried out collectively by the network.
 *
 * The software is a community-driven open source project, released under the MIT license.
 *
 * \section Navigation
 * Use the buttons <code>Namespaces</code>, <code>Classes</code> or <code>Files</code> at the top of the page to start navigating the code.
 */

void WaitForShutdown(boost::thread_group* threadGroup)
{
    bool fShutdown = ShutdownRequested();
    // Tell the main threads to shutdown.
    while (!fShutdown)
    {
        MilliSleep(200);
        fShutdown = ShutdownRequested();
    }
    if (threadGroup)
    {
        Interrupt(*threadGroup);
        threadGroup->join_all();
    }
}

//////////////////////////////////////////////////////////////////////////////
//
// Start
//
bool AppInit(int argc, char* argv[])
{
    boost::thread_group threadGroup;
    CScheduler scheduler;//zhuҪ�����������̨������Ҫ������������scheduleFromNow��scheduleEvery���ֱ��ʾ�����ڿ�ʼ�ǹ�һ��ʱ��ִ��ĳ����һ�Σ��ʹ����ڿ�ʼÿ������ִ��ĳ����һ�Ρ�Ҳ�ɴ���һ���µ��߳�ȥִ�����񣬶���Ӱ�����̵߳�ִ��

    bool fRet = false;

    //
    // Parameters
    //
    // If Qt is used, parameters/bitcoin.conf are parsed in qt/bitcoin.cpp's main()
    gArgs.ParseParameters(argc, argv);//������ArgsManager��ParseParameters()�Ǹ����е�һ����Ҫ��Ա�����������ǽ�����Ĳ������н��������뵽����map����
    //���������֮������Ϳ�ʼ����һϵ�в�������
    // Process help and version before taking care about datadir
    if (gArgs.IsArgSet("-?") || gArgs.IsArgSet("-h") ||  gArgs.IsArgSet("-help") || gArgs.IsArgSet("-version"))
    {
        std::string strUsage = strprintf(_("%s Daemon"), _(PACKAGE_NAME)) + " " + _("version") + " " + FormatFullVersion() + "\n";

        if (gArgs.IsArgSet("-version"))
        {
            strUsage += FormatParagraph(LicenseInfo());
        }
        else
        {
            strUsage += "\n" + _("Usage:") + "\n" +
                  "  bitcoind [options]                     " + strprintf(_("Start %s Daemon"), _(PACKAGE_NAME)) + "\n";

            strUsage += "\n" + HelpMessage(HMM_BITCOIND);
        }

        fprintf(stdout, "%s", strUsage.c_str());
        return true;
    }
/*�����������SelectParams(ChainNameFromCommandLine());������ͨ��ChainNameFromCommandLine()��ȡ�����������õĵ�ǰ�������е����磬�����������֣�
Main����ʾ������Ҳ���ǵ�ǰ���ر������û����׵����磬bitcoind�е�Ĭ��ֵ��
Testnet������������������ר����һ�������������еĽ��׶�ֻ�����ڲ��ԣ����Ҳ������еıҿ��Է���Ļ�ȡ����ҪĿ�ľ���ģ����ʵ���׻��������µĹ��ܡ�
Regtest���ع���ԣ��ֳ�Ϊ˽���������ڸ��˿������ԣ��ڿ��ѶȽϵͣ����Ҳ����������������á�

����һ���ڱ��ػ�����ʼʱʹ��Regtest�����ؿ�����ɺ󣬽���Testnet���д��ģʵ�ʻ������ԣ������������ٽ�����������Ҳ��Ŀǰ�ڶ���������ICO����Ŀ����������·�ߡ�

�ص������У���ȡ����ǰ������֮��ͨ��SelectParams()���ݲ�ͬ�����紴����ͬ�Ĺ�ʶ������ʵ�ֵķ�ʽ��ʹ�������̳���CMainParams��CTestNetParams��CRegTestParams�̳л���CChainParams��Ȼ�����ѡ��Ĳ�ͬ�����緵�ز�ͬ�ļ̳��࣬����ֵ��һ��CChainParams���͵�����ָ��(unique_ptr)globalChainParams��jieshou,���ʹ��ʱ�����������ָ����������Ӧ�Ĺ�ʶ��������ν����ָ����ǵ�ָ���뿪������ʱ�Զ���ɾ��(ʹ��delete)��ָ��Ķ���

she�ú����������һ���ִ����������ж����������Ƿ���ڴ���Ĳ������жϷ����ǿ�ÿһ�������ĵ�һ����ĸ�Ƿ�Ϊ-������windows������- or /��������Ǿͱ���Ȼ���˳�����
*/
    try
    {
        if (!fs::is_directory(GetDataDir(false)))//�������Ŀ¼�Ƿ�Ϸ�������Ŀ¼��Ubuntu��Ĭ�ϵ�·����~/.bitcoin/����ȻҲ��ͨ��-datadir�����������ã���Ŀ¼����Ҫ����ͬ����������Ϣ��Ǯ����Ϣ��������Ϣ�ȵȼ������е�������������Ϣ�����������Ȼ��ʼ��ȡ�����ļ��������ļ���Ĭ��������~/.bitcoin/bitcoinf.confҲ��������Ŀ¼�£�����Ĭ����û������ļ��ģ�����ReadConfigFile���Կ����ļ�������Ҳ�ǿ��Ե�
        {
            fprintf(stderr, "Error: Specified data directory \"%s\" does not exist.\n", gArgs.GetArg("-datadir", "").c_str());
            return false;
        }
        try
        {
            gArgs.ReadConfigFile(gArgs.GetArg("-conf", BITCOIN_CONF_FILENAME));//��ȡbitcoin.conf�����ļ�
        } catch (const std::exception& e) {
            fprintf(stderr,"Error reading configuration file: %s\n", e.what());
            return false;
        }
        // Check for -testnet or -regtest parameter (Params() calls are only valid after this clause)
        try {
            SelectParams(ChainNameFromCommandLine());
        } catch (const std::exception& e) {
            fprintf(stderr, "Error: %s\n", e.what());
            return false;
        }

        // Error out when loose non-argument tokens are encountered on command line
        for (int i = 1; i < argc; i++) {
            if (!IsSwitchChar(argv[i][0])) {
                fprintf(stderr, "Error: Command line contains unexpected token '%s', see bitcoind -h for a list of options.\n", argv[i]);
                exit(EXIT_FAILURE);
            }
        }

        // -server defaults to true for bitcoind but not for the GUI so do this here
        gArgs.SoftSetBoolArg("-server", true);//����ͨ��SoftSetBoolArg()������-server����Ϊtrue��SoftSetBoolArg()�����жϲ����Ƿ��Ѿ����ù��ˣ�����ǣ�����false����������ö�Ӧ��ֵ������true����-server������ʾ�Ƿ����RPC���������Ϊ��bitcoind��Ĭ����Ϊ���ķ���������bitcoin-cli�Լ�bitcoin-tx���͵�����
        // Set this early so that parameter interactions go to console
        InitLogging();//��ʼ����־��¼�Լ���ӡ��ʽ
        InitParameterInteraction();//��ʼ���������
        if (!AppInitBasicSetup())//ע����Ӧ����Ϣ�Լ�����ʽ
        {
            // InitError will have been called with detailed error, which ends up on console
            exit(EXIT_FAILURE);
        }
        if (!AppInitParameterInteraction())//�������������в�������������������ȣ�ע�����е�����
        {
            // InitError will have been called with detailed error, which ends up on console
            exit(EXIT_FAILURE);
        }
        if (!AppInitSanityChecks())//Sanity Check�����������ر�����ʱ����Ҫ�����еĿ��Ƿ���������
        {
            // InitError will have been called with detailed error, which ends up on console
            exit(EXIT_FAILURE);
        }//��AppInitSanityChecks()֮�󣬳����ȡ��-daemon��������������������������ʾbitcoind���к����ػ����̣���̨���̣��ķ�ʽ���У�����daemon()�����Ĳ����������£�
        /*��˼��˵daemon()���Խ���ǰ���������ն˵Ŀ��ƣ���תΪϵͳ��̨���̣���������������������һ����nochdir��Ϊ0��ʾ������Ŀ¼��Ϊϵͳ��Ŀ¼/��Ϊ1��ʾ����ǰ·����Ϊ����Ŀ¼���ڶ�������nocloseΪ0��ʾ�ض���stdin��stdout��stderr��/dev/null��������ʾ�κ���Ϣ��Ϊ1��ʾ���ı���Щ�ļ���������

���̺�̨��֮��ͨ��AppInitLockDataDirectory()����������Ŀ¼����ֹ���������ڼ������޸�����Ŀ¼�е����ݡ���AppInitMain()����֮���������ֵfRetΪfalse����ôǿ�ƽ��������̣߳�����͵ȴ������߳����н��������ͨ��ShutDown()���������
*/
        if (gArgs.GetBoolArg("-daemon", false))
        {
#if HAVE_DECL_DAEMON
            fprintf(stdout, "Bitclassic Coin server starting\n");

            // Daemonize
            if (daemon(1, 0)) { // don't chdir (1), do close FDs (0)
                fprintf(stderr, "Error: daemon() failed: %s\n", strerror(errno));
                return false;
            }
#else
            fprintf(stderr, "Error: -daemon is not supported on this operating system\n");
            return false;
#endif // HAVE_DECL_DAEMON
        }
        // Lock data directory after daemonization
        if (!AppInitLockDataDirectory())
        {
            // If locking the data directory failed, exit immediately
            exit(EXIT_FAILURE);
        }
        fRet = AppInitMain(threadGroup, scheduler);//��ʼ��������
    }
    catch (const std::exception& e) {
        PrintExceptionContinue(&e, "AppInit()");
    } catch (...) {
        PrintExceptionContinue(nullptr, "AppInit()");
    }

    if (!fRet)
    {
        Interrupt(threadGroup);
        threadGroup.join_all();
    } else {
        WaitForShutdown(&threadGroup);
    }
    Shutdown();

    return fRet;
}

int main(int argc, char* argv[])
{
    SetupEnvironment();//������������˱������л������˺����������Ǹ��ݲ�ͬ�������ò�ͬ�ı��룬���ڹ��ʻ���

    // Connect bitcoind signal handlers
    noui_connect();//����bitcoind�ͻ��˵��źŴ�������ҵ����������źŴ�����������������ĸ�����Ϣ�����쳣��Ϣ��Ҳ������Ӳ�����쳣��Ϣ��������ͨ�ź���Ϣ��

    return (AppInit(argc, argv) ? EXIT_SUCCESS : EXIT_FAILURE);
}
