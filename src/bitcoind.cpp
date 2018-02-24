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
    CScheduler scheduler;//zhu要是用来管理后台任务，主要的两个函数是scheduleFromNow和scheduleEvery，分别表示从现在开始是过一段时间执行某函数一次，和从现在开始每隔几秒执行某函数一次。也可创建一个新的线程去执行任务，而不影响主线程的执行

    bool fRet = false;

    //
    // Parameters
    //
    // If Qt is used, parameters/bitcoin.conf are parsed in qt/bitcoin.cpp's main()
    gArgs.ParseParameters(argc, argv);//类型是ArgsManager，ParseParameters()是该类中的一个主要成员函数，功能是将传入的参数进行解析并存入到两个map当中
    //解析完参数之后，下面就开始进行一系列参数设置
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
/*接下来是这句SelectParams(ChainNameFromCommandLine());，首先通过ChainNameFromCommandLine()获取命令行中设置的当前程序运行的网络，包括以下三种：
Main：表示主网，也就是当前比特币所有用户交易的网络，bitcoind中的默认值。
Testnet：测试网，测试网中专门有一条测试链，所有的交易都只是用于测试，并且测试网中的币可以方便的获取，主要目的就是模拟真实交易环境测试新的功能。
Regtest：回归测试，又称为私有网，用于个人开发测试，挖矿难度较低，并且参数都可以自行设置。

所以一般在本地环境开始时使用Regtest，本地开发完成后，进入Testnet进行大规模实际环境测试，运行正常后再进入主网，这也是目前众多区块链（ICO）项目的主流开发路线。

回到代码中，获取到当前的网络之后通过SelectParams()根据不同的网络创建不同的共识参数，实现的方式是使用三个继承类CMainParams，CTestNetParams，CRegTestParams继承基类CChainParams，然后根据选择的不同的网络返回不同的继承类，返回值由一个CChainParams类型的智能指针(unique_ptr)globalChainParams来jieshou,最后使用时就用这个智能指针来访问相应的共识参数。所谓智能指针就是当指针离开作用域时自动的删除(使用delete)所指向的对象。

she置好网络后，下面一部分代码是用来判断命令行中是否存在错误的参数，判断方法是看每一个参数的第一个字母是否为-或者在windows环境中- or /，如果不是就报错然后退出程序。
*/
    try
    {
        if (!fs::is_directory(GetDataDir(false)))//检查数据目录是否合法，数据目录在Ubuntu下默认的路径是~/.bitcoin/，当然也能通过-datadir参数进行设置，该目录下主要保存同步的区块信息，钱包信息，配置信息等等几乎所有的区块链运行信息都保存在这里。然后开始读取配置文件，配置文件的默认名称是~/.bitcoin/bitcoinf.conf也是在数据目录下，不过默认是没有这个文件的，进入ReadConfigFile可以看到文件不存在也是可以的
        {
            fprintf(stderr, "Error: Specified data directory \"%s\" does not exist.\n", gArgs.GetArg("-datadir", "").c_str());
            return false;
        }
        try
        {
            gArgs.ReadConfigFile(gArgs.GetArg("-conf", BITCOIN_CONF_FILENAME));//读取bitcoin.conf配置文件
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
        gArgs.SoftSetBoolArg("-server", true);//首先通过SoftSetBoolArg()设置了-server参数为true，SoftSetBoolArg()首先判断参数是否已经设置过了，如果是，返回false；否则就设置对应的值，返回true。而-server参数表示是否接收RPC命令，这里因为是bitcoind，默认作为核心服务器接收bitcoin-cli以及bitcoin-tx传送的命令
        // Set this early so that parameter interactions go to console
        InitLogging();//初始化日志记录以及打印方式
        InitParameterInteraction();//初始化网络参数
        if (!AppInitBasicSetup())//注册相应的消息以及处理方式
        {
            // InitError will have been called with detailed error, which ends up on console
            exit(EXIT_FAILURE);
        }
        if (!AppInitParameterInteraction())//设置区块链运行参数，例如最大连接数等，注册所有的命令
        {
            // InitError will have been called with detailed error, which ends up on console
            exit(EXIT_FAILURE);
        }
        if (!AppInitSanityChecks())//Sanity Check是用来检查比特币运行时所需要的所有的库是否都运行正常
        {
            // InitError will have been called with detailed error, which ends up on console
            exit(EXIT_FAILURE);
        }//在AppInitSanityChecks()之后，程序获取了-daemon参数，如果设置了这个参数，表示bitcoind运行后将以守护进程（后台进程）的方式运行，其中daemon()函数的参数描述如下，
        /*意思是说daemon()可以将当前进程脱离终端的控制，并转为系统后台进程，函数传入两个参数，第一个是nochdir，为0表示将工作目录改为系统根目录/；为1表示将当前路径设为工作目录。第二个参数noclose为0表示重定向stdin、stdout、stderr到/dev/null，即不显示任何信息；为1表示不改变这些文件描述符。

进程后台化之后，通过AppInitLockDataDirectory()来锁定数据目录，防止程序运行期间随意修改数据目录中的内容。在AppInitMain()结束之后，如果返回值fRet为false，那么强制结束所有线程；否则就等待所有线程运行结束。最后通过ShutDown()完成清理工作
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
        fRet = AppInitMain(threadGroup, scheduler);//初始化主程序
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
    SetupEnvironment();//这个函数设置了本地运行环境。此函数的作用是根据不同国家设置不同的编码，用于国际化的

    // Connect bitcoind signal handlers
    noui_connect();//连接bitcoind客户端的信号处理程序。我的理解是这个信号处理程序就是侦听程序的各种消息包括异常消息，也可能是硬件的异常消息或程序的普通信号消息。

    return (AppInit(argc, argv) ? EXIT_SUCCESS : EXIT_FAILURE);
}
